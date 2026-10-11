/* Approved Lab pixels, composited over the actual tactical view. */
static unsigned char g_ehBar[EH_WIDTH*EH_MAX_H*4];
static unsigned char g_ehHeader[EH_MAIN_W*H6_TAB_H*4];
struct EH_Progress {float from,to,at;};
static std::map<std::string,EH_Progress> g_ehProgress;
static float eh_smooth_progress(const SbEntry& e,float time){
    std::map<std::string,EH_Progress>::iterator found=g_ehProgress.find(e.name);
    if(found==g_ehProgress.end()){EH_Progress p={e.progress,e.progress,time};g_ehProgress[e.name]=p;return e.progress;}
    EH_Progress& p=found->second;
    float u=(time-p.at)/.12f;if(u<0)u=0;if(u>1)u=1;
    float value=p.from+(p.to-p.from)*u*u*(3-2*u);
    if(time<p.at||e.progress<p.to){p.from=p.to=e.progress;p.at=time;return e.progress;}
    if(e.progress!=p.to){p.from=value;p.to=e.progress;p.at=time;}
    return value;
}
static void eh_cameo(unsigned char *dst,const char *name,const SbEntry *entry)
{
    memset(dst,0,64*48*4);
    const char *art=sb_art_name(name);
    const unsigned char *tdr=h6_tdr_find(art);
    const DB_Pack *cp=g_h6CamPack;
    const DB_Shape *cam=cp?db_shape(cp,art):0;
    if(!cam){cp=g_dbPack;cam=cp?db_shape(cp,art):0;}
    if(tdr)memcpy(dst,tdr,64*48*4);
    else if(cam&&cp){
        unsigned char pixels[64*48];memset(pixels,0,sizeof pixels);
        DB_Surface surf;db_surface_init(&surf,cam->w,cam->h,pixels);
        db_draw_shape(&surf,cam,0,0,0);
        int zoom=cam->w<=32?2:1;
        for(int y=0;y<48;y++)for(int x=0;x<64;x++){
            int sx=x/zoom,sy=y/zoom;if(sx>=cam->w||sy>=cam->h)continue;
            unsigned char pi=pixels[sy*cam->w+sx];if(!pi)continue;
            unsigned char *q=dst+(y*64+x)*4;
            memcpy(q,cp->pal8+pi*3,3);q[3]=255;
        }
    }
    if(!entry){hud_enhanced_gray(dst);return;}
    if(!entry->completed&&(entry->constructing||entry->onHold||entry->busy))
        hud_enhanced_clock(dst,eh_smooth_progress(*entry,g_sb.EffectTime?g_sb.EffectTime():0),entry->busy&&!entry->constructing&&!entry->onHold,g_sb.nod,g_sb.EffectTime?g_sb.EffectTime():0);
    if(cp&&(entry->completed||entry->onHold)){
        const DB_Shape *pip=db_shape(cp,"PIPS");
        if(pip){
            unsigned char pixels[64*48];memset(pixels,0,sizeof pixels);
            int w=cam?cam->w:64,h=cam?cam->h:48,z=w<=32?2:1;
            DB_Surface surf;db_surface_init(&surf,w,h,pixels);
            db_draw_shape_centered(&surf,pip,entry->completed?DB_PIP_READY:DB_PIP_HOLDING,w/2,h-pip->h-8);
            for(int y=0;y<48;y++)for(int x=0;x<64;x++)if(y/z<h&&x/z<w){
                unsigned char pi=pixels[(y/z)*w+x/z];if(pi){unsigned char *q=dst+(y*64+x)*4;memcpy(q,cp->pal8+pi*3,3);q[3]=255;}
            }
        }
    }
}
/* ---- THE PRODUCTION QUEUE STRIP ----------------------------------------------------
   A column of small cards down the left edge of the panel, beside the power meter: one
   per thing being built or queued, its cameo at half size, the build clock over it, the
   percentage in the middle and, when more than one of it is in hand, how many in the
   corner. Ten Minigunners (one building, nine queued) are one card reading 10.

   ORDER. By category in the order of the category buttons (structures, infantry,
   vehicles, aircraft), so a card never jumps between columns of meaning. Within a
   category the item the factory is working on comes first, then the queued items in
   the order the queue pump will start them, which is the queue map's own key order.

   WHERE THE NUMBERS COME FROM. The engine builds one item per category at a time and has
   no queue (see BUILD QUEUEING above), so "in hand" is the engine's constructing or held
   entry plus the renderer's own queue count for the same buildable. A start the pump has
   sent and the factory has already taken, but which the pump has not yet counted down,
   is subtracted once so the figure never reads one high for a frame.

   MOTION, on a clock of its own. A card slides out leftwards from under the panel when
   its item enters production, slides back under the panel when it finishes or is
   cancelled, and the cards below a departed one then rise to close the gap. Every move is
   a smoothstep ease over a fixed duration in SECONDS of presentation time, so the motion
   is the same at any game speed. In play that time is the real time between drawn
   frames; in an automated run it is a fixed step per drawn frame plus the engine time
   that passed since the last one, which keeps every scripted picture reproducible.
   Positions snap to whole native units, as the drawer does.

   The strip is drawn BEFORE the panel, so a card entering or leaving is covered by the
   panel's edge rather than drawn over it. It takes no input: a click on a card reaches
   the battlefield underneath, as it did before the strip existed. */
#define EHQ_X 21                 /* settled left edge, bar units: two units clear of the panel */
#define EHQ_HIDE 40              /* travel to the right that puts a card wholly under the panel */
#define EHQ_TOP 275              /* first card level with the top of the meter and the grid */
#define EHQ_PITCH 32             /* card height plus a two-unit gap */
#define EHQ_STRIP_W 104          /* bar columns the strip texture covers */
#define EHQ_OUT_S .32f           /* sliding out */
#define EHQ_IN_S .28f            /* sliding back under */
#define EHQ_MOVE_S .30f          /* rising to close a gap */
struct EHQ_Item { std::pair<int,int> key; char name[16]; float progress; int count, state; };
struct EHQ_Card {
    std::pair<int,int> key; char name[16]; float progress; int count, state;
    float progFrom, progAt;               /* the figure glides to each new engine reading */
    bool leaving;
    float outFrom, outTo, outAt, outDur;  /* 0 under the panel .. 1 in its place */
    float slotFrom, slotTo, slotAt;       /* slot index, eased */
    int index;                            /* where it was last laid out */
};
static std::vector<EHQ_Card> g_ehqCards;
static float g_ehqNow = 0;               /* presentation seconds, see above */
static float g_ehqStep = -1;             /* >= 0: automated, seconds per drawn frame */
static float g_ehqLastEffect = -1;
static Uint64 g_ehqLastCounter = 0;
static bool g_ehqLog = false;            /* hudqlog: one HUDQ line per drawn frame */
static std::string g_ehqLine;            /* the last frame's line, for hudqdump */
static unsigned char g_ehqStrip[EHQ_STRIP_W*EH_MAX_H*4];
static unsigned char g_ehqCard[EHQ_W*EHQ_H*4];
static unsigned char g_ehqCameo[64*48*4];
static GLuint g_ehqTex = 0;

static float ehq_ease(float from,float to,float at,float dur)
{
    float u=dur>0?(g_ehqNow-at)/dur:1;if(u<0)u=0;if(u>1)u=1;
    return from+(to-from)*u*u*(3-2*u);
}
static float ehq_out(const EHQ_Card& c){return ehq_ease(c.outFrom,c.outTo,c.outAt,c.outDur);}
#define EHQ_PROGRESS_S (1.0f/15)  /* one engine tick */
static float ehq_progress(const EHQ_Card& c)
{
    float u=(g_ehqNow-c.progAt)/EHQ_PROGRESS_S;if(u<0)u=0;if(u>1)u=1;
    return c.progFrom+(c.progress-c.progFrom)*u;
}
static float ehq_slot(const EHQ_Card& c){return ehq_ease(c.slotFrom,c.slotTo,c.slotAt,EHQ_MOVE_S);}

static void ehq_clock_step(void)
{
    const float effect=g_sb.EffectTime?g_sb.EffectTime():0;
    float dt;
    if(g_ehqStep>=0){
        float ran=g_ehqLastEffect<0?0:effect-g_ehqLastEffect;if(ran<0)ran=0;
        dt=(g_ehqLastEffect<0?0:g_ehqStep)+ran;
    }else{
        const Uint64 now=SDL_GetPerformanceCounter();
        dt=g_ehqLastCounter?(float)((double)(now-g_ehqLastCounter)/(double)SDL_GetPerformanceFrequency()):0;
        g_ehqLastCounter=now;
        /* a stall (a save, a window drag) finishes a move rather than replaying it */
        if(dt<0)dt=0;
        if(dt>.25f)dt=.25f;
    }
    g_ehqLastEffect=effect;
    g_ehqNow+=dt;
}

/* Things a factory builds. A superweapon's recharge also reads as "constructing" in the
   sidebar export, but nothing is being built, so it has no card. */
static bool ehq_built(const SbEntry& e)
{
    return e.type==BUILDING_TYPE||e.type==INFANTRY_TYPE||e.type==UNIT_TYPE||e.type==AIRCRAFT_TYPE;
}

/* What should be on the strip this frame, in order. */
static void ehq_wanted(std::vector<EHQ_Item>& out)
{
    out.clear();
    if(g_sbSpectator||!g_sbState.valid)return;
    for(int cat=0;cat<4;cat++){
        int active=-1;std::pair<int,int> activeKey(-1,-1);
        for(size_t i=0;i<g_sbState.entry.size();i++){
            const SbEntry& e=g_sbState.entry[i];
            if(!ehq_built(e))continue;
            if(eh_category(e)==cat&&(e.constructing||e.onHold)&&!e.completed){active=(int)i;break;}
        }
        if(active>=0){
            const SbEntry& e=g_sbState.entry[active];
            EHQ_Item it;it.key=activeKey=std::make_pair(e.btype,e.bid);
            snprintf(it.name,sizeof it.name,"%s",e.name);
            it.progress=e.progress;
            it.state=e.onHold?EHQ_HOLD:EHQ_BUILDING;
            int queued=sb_queued(e);
            if(queued>0&&g_sbQueueOpen.count(it.key))queued--;
            it.count=1+queued;
            out.push_back(it);
        }
        for(std::map<std::pair<int,int>,int>::iterator q=g_sbQueue.begin();q!=g_sbQueue.end();++q){
            if(q->second<=0)continue;
            if(q->first==activeKey)continue;
            for(size_t i=0;i<g_sbState.entry.size();i++){
                const SbEntry& e=g_sbState.entry[i];
                if(e.btype!=q->first.first||e.bid!=q->first.second||eh_category(e)!=cat||!ehq_built(e))continue;
                EHQ_Item it;it.key=q->first;snprintf(it.name,sizeof it.name,"%s",e.name);
                it.progress=0;it.state=EHQ_WAITING;it.count=q->second;
                out.push_back(it);
                break;
            }
        }
    }
}

/* How many cards fit between the top of the meter and the foot of the panel. */
static int ehq_capacity(int fbh)
{
    int bottom=hud_enhanced_height(g_h6Rows)-30;
    const int screen=(int)(fbh/g_h6Scale)-4;
    if(screen<bottom)bottom=screen;
    int n=(bottom-EHQ_TOP+(EHQ_PITCH-EHQ_H))/EHQ_PITCH;
    return n<1?1:n;
}

static void ehq_update(int fbh)
{
    std::vector<EHQ_Item> want;ehq_wanted(want);
    /* OVERFLOW: the last place on the strip becomes a "+N" card counting the items that
       did not fit, so nothing in production is ever silently missing from it. */
    const int cap=ehq_capacity(fbh);
    if((int)want.size()>cap){
        int more=0;for(size_t i=cap-1;i<want.size();i++)more+=want[i].count;
        want.resize(cap-1);
        EHQ_Item it;it.key=std::make_pair(-1,-1);strcpy(it.name,"+");it.progress=0;it.state=EHQ_MORE;it.count=more;
        want.push_back(it);
    }
    /* departures first: anything no longer wanted turns round and slides back in */
    for(size_t c=0;c<g_ehqCards.size();c++){
        EHQ_Card& k=g_ehqCards[c];
        bool found=false;
        for(size_t w=0;w<want.size();w++)if(want[w].key==k.key){found=true;break;}
        if(!found&&!k.leaving){
            /* finished rather than cancelled: it leaves full. A building waits completed
               for placement, so the engine says so; a unit is out of the factory the tick
               after it completes and is caught only when a frame lands on that tick, so
               otherwise it leaves on its last reading, which in play is the high nineties */
            for(size_t i=0;i<g_sbState.entry.size();i++){
                const SbEntry& e=g_sbState.entry[i];
                if(e.btype==k.key.first&&e.bid==k.key.second&&e.completed){k.progress=k.progFrom=1;k.progAt=g_ehqNow-1;}
            }
            k.leaving=true;k.outFrom=ehq_out(k);k.outTo=0;k.outAt=g_ehqNow;k.outDur=EHQ_IN_S;
        }
    }
    /* arrivals, and returns: a card already on its way back in comes straight back out */
    std::vector<EHQ_Card> next;
    for(size_t w=0;w<want.size();w++){
        const EHQ_Item& it=want[w];
        EHQ_Card *old=0;
        for(size_t c=0;c<g_ehqCards.size();c++)if(g_ehqCards[c].key==it.key){old=&g_ehqCards[c];break;}
        EHQ_Card k;
        if(old){
            k=*old;
            if(k.leaving){k.leaving=false;k.outFrom=ehq_out(k);k.outTo=1;k.outAt=g_ehqNow;k.outDur=EHQ_OUT_S;}
        }else{
            k=EHQ_Card();k.key=it.key;k.outFrom=0;k.outTo=1;k.outAt=g_ehqNow;k.outDur=EHQ_OUT_S;
            k.slotFrom=k.slotTo=-1;k.index=-1;
        }
        /* the engine moves progress fifteen times a second; the clock and the figure
           glide between readings on the presentation clock, and snap when it goes back
           (the next item of a kind started) */
        if(!old||it.progress<ehq_progress(k)){k.progFrom=it.progress;k.progAt=g_ehqNow-1;}
        else if(it.progress!=k.progress){k.progFrom=ehq_progress(k);k.progAt=g_ehqNow;}
        snprintf(k.name,sizeof k.name,"%s",it.name);k.progress=it.progress;k.count=it.count;k.state=it.state;
        next.push_back(k);
    }
    /* a leaving card keeps its place in the column until it is gone, so the cards below
       wait for the space and then close it, rather than sliding under a card still there */
    for(size_t c=0;c<g_ehqCards.size();c++){
        const EHQ_Card& k=g_ehqCards[c];
        if(!k.leaving)continue;
        if(ehq_out(k)<=0&&g_ehqNow>=k.outAt+k.outDur)continue;   /* home: gone */
        size_t at=k.index<0?next.size():(size_t)k.index;if(at>next.size())at=next.size();
        next.insert(next.begin()+at,k);
    }
    for(size_t i=0;i<next.size();i++){
        EHQ_Card& k=next[i];
        if(k.slotTo<0){k.slotFrom=k.slotTo=(float)i;k.slotAt=g_ehqNow;}
        else if(k.slotTo!=(float)i){k.slotFrom=ehq_slot(k);k.slotTo=(float)i;k.slotAt=g_ehqNow;}
        k.index=(int)i;
    }
    g_ehqCards.swap(next);
}

static void ehq_draw(float barX,float barY,float sc,int fbh)
{
    ehq_update(fbh);
    char head[96];
    snprintf(head,sizeof head,"HUDQ|t=%.4f|cap=%d|n=%d",g_ehqNow,ehq_capacity(fbh),(int)g_ehqCards.size());
    g_ehqLine=head;
    if(g_ehqCards.empty()){if(g_ehqLog){printf("%s\n",head);fflush(stdout);}return;}
    const int h=hud_enhanced_height(g_h6Rows);
    memset(g_ehqStrip,0,(size_t)EHQ_STRIP_W*h*4);
    for(size_t c=0;c<g_ehqCards.size();c++){
        const EHQ_Card& k=g_ehqCards[c];
        const float out=ehq_out(k),slot=ehq_slot(k);
        const int cx=EHQ_X+(int)floorf((1-out)*EHQ_HIDE+.5f);
        const int cy=EHQ_TOP+(int)floorf(slot*EHQ_PITCH+.5f);
        const unsigned char *cameo=0;
        if(k.state!=EHQ_MORE){
            SbEntry plain;memset(plain.name,0,sizeof plain.name);snprintf(plain.name,sizeof plain.name,"%s",k.name);
            plain.progress=0;plain.completed=plain.constructing=plain.onHold=plain.busy=false;
            eh_cameo(g_ehqCameo,k.name,&plain);cameo=g_ehqCameo;
        }
        hud_enhanced_queue_card(g_ehqCard,g_h6Pack,cameo,ehq_progress(k),k.state,k.count,g_sb.nod,g_sb.EffectTime?g_sb.EffectTime():0);
        for(int y=0;y<EHQ_H;y++){
            const int ty=cy+y;if(ty<0||ty>=h)continue;
            for(int x=0;x<EHQ_W;x++){
                const int tx=cx+x;if(tx<0||tx>=EHQ_STRIP_W)continue;
                const unsigned char *q=g_ehqCard+(y*EHQ_W+x)*4;
                if(q[3])memcpy(g_ehqStrip+((size_t)ty*EHQ_STRIP_W+tx)*4,q,4);
            }
        }
        char one[96];
        const int pct=k.state==EHQ_BUILDING||k.state==EHQ_HOLD?(int)floorf(ehq_progress(k)*100):0;
        snprintf(one,sizeof one,"|%s:%d:%d:%c:x=%d:y=%d:out=%.3f%s",k.name,k.count,pct,"BHWM"[k.state],
                 (int)(barX+cx*sc),(int)(barY+cy*sc),out,k.leaving?":leaving":"");
        g_ehqLine+=one;
    }
    if(g_ehqLog){printf("%s\n",g_ehqLine.c_str());fflush(stdout);}
    h6_upload(&g_ehqTex,g_ehqStrip,EHQ_STRIP_W,h);
    h6_tex_quad(g_ehqTex,EHQ_STRIP_W,h,barX,barY,barX+EHQ_STRIP_W*sc,barY+h*sc);
}

static void eh_draw_panel(const H6_State& original,int fbw,int fbh)
{
    EH_State state;memset(&state,0,sizeof state);state.base=original;state.rows=g_h6Rows;
    memset(state.base.slot,0,sizeof state.base.slot);
    eh_refresh();
    for(int row=0;row<g_h6Rows;row++)for(int col=0;col<2;col++){
        int at=2*(g_ehTop[g_ehCategory]+row)+col;if(at>=(int)g_ehItems.size())continue;
        const EH_Display& d=g_ehItems[at];int slot=row*2+col;
        const SbEntry *e=d.engine>=0?&g_sbState.entry[d.engine]:0;
        eh_cameo(g_h6Cameo[slot],d.name,e);
        state.base.slot[slot].used=1;state.base.slot[slot].rgba=g_h6Cameo[slot];state.base.slot[slot].progress=-1;
    }
    state.tool_frame[0]=g_sbSpectator?-1:sb_frame_for(SBH_REPAIR,-1,g_sbRepairOn);
    state.tool_frame[1]=g_sbSpectator?-1:sb_frame_for(SBH_SELL,-1,g_sbSellOn);
    state.tool_frame[2]=g_sbSpectator?-1:sb_frame_for(SBH_POWER,-1,g_sbPowerOn);
    state.tool_hover[0]=g_sbHover==SBH_REPAIR;state.tool_hover[1]=g_sbHover==SBH_SELL;state.tool_hover[2]=g_sbHover==SBH_POWER;
    for(int i=0;i<4;i++){state.category_frame[i]=sb_frame_for(SBH_CATEGORY,i,g_ehCategory==i);state.category_hover[i]=g_sbHover==SBH_CATEGORY&&g_sbHoverCol==i;}
    hud_enhanced_bar(g_ehBar,g_h6Pack,&state);
    int h=hud_enhanced_height(g_h6Rows);h6_upload(&g_h6TexBar,g_ehBar,EH_WIDTH,h);
    /* Half-opacity cameos must blend even after a world cutout alpha test. */
    GLboolean alphaTest=glIsEnabled(GL_ALPHA_TEST);glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor4f(1,1,1,1);
    float x=g_dbX0-73*g_h6Scale,y=g_dbY0,sc=g_h6Scale;
    /* the queue strip first, so the panel's edge covers a card sliding under it */
    ehq_clock_step();
    ehq_draw(x,y,sc,fbh);
    h6_tex_quad(g_h6TexBar,EH_WIDTH,h,x,y,x+EH_WIDTH*sc,y+h*sc);
    struct Header {const char *label;int value,frame,width;float x;GLuint *texture;};
    const int leftWidth=fbw<664*sc?148:160;
    Header tabs[]={
        {0,original.credits,original.credits_frame,160,fbw-344*sc,&g_h6TexCred},
        {"OPTIONS",0,original.options_frame,leftWidth,0,&g_h6TexOpt},
        {"DATABASE",0,original.database_frame,leftWidth,leftWidth*sc,&g_h6TexDb},
        {"SIDEBAR",0,original.sidebar_frame,184,fbw-184*sc,&g_h6TexSide}
    };
    for(int i=0;i<4;i++){
        if(i==2&&!codex_available())continue;
        Header& t=tabs[i];hud_enhanced_tab(g_ehHeader,g_h6Pack,t.label,t.value,t.frame,t.width==184?1:t.width==148?148:0);
        h6_upload(t.texture,g_ehHeader,t.width,17);
        if(i==3&&g_sbCodexUp)glColor4f(.42f,.42f,.44f,1);
        h6_tex_quad(*t.texture,t.width,17,t.x,y,t.x+t.width*sc,y+17*sc);
        glColor4f(1,1,1,1);
    }
    /* Retain the meter diagnostics used by the power gates. */
    int *counts=g_sbDrawnHudCount;counts[0]=counts[1]=counts[2]=0;
    for(int py=275;py<h-30;py++)for(int px=73;px<90;px++){
        const unsigned char *q=g_ehBar+(py*EH_WIDTH+px)*4;if(!q[3])continue;int r=q[0],g=q[1],b=q[2];
        if(g>r+24&&g>b+24)counts[0]++;
        else if(r>b+24&&abs(r-g)<=8)counts[1]++;
        else if(r>g*2+24&&r>b*2+24)counts[2]++;
    }
    g_sbDrawnHudPower=counts[0]+counts[1]+counts[2]==0?-1:counts[2]>=counts[1]&&counts[2]>=counts[0]?2:counts[1]>=counts[0]?1:0;
    glDisable(GL_TEXTURE_2D);if(alphaTest)glEnable(GL_ALPHA_TEST);
}

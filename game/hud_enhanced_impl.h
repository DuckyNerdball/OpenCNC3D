/* Included in the portable HUD compositor, after its source raster helpers. */
#include "hud_enhanced.h"
#include <math.h>

static unsigned char eh_byte(double value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (unsigned char)floor(value + .5);
}

int hud_enhanced_ok(const H6_Pack *p)
{
    static const char *names[] = {"eh_body","eh_map","eh_wing","eh_header","eh_plate",
        "eh_repair","eh_sell","eh_power","eh_structures","eh_infantry","eh_vehicles","eh_aircraft"};
    static const int sizes[12][3]={{184,488,1},{184,187,1},{85,159,1},{184,17,3},{35,35,4},{19,19,1},{19,19,1},{19,19,1},{19,19,1},{19,19,1},{19,19,1},{19,19,1}};
    int i;
    for(i=0;i<12;i++){const H6_Asset *a=hud640_asset(p,names[i]);if(!a||a->w!=sizes[i][0]||a->h!=sizes[i][1]||a->frames!=sizes[i][2])return 0;}
    return 1;
}

int hud_enhanced_height(int rows) { return 309 + rows * 48; }

void hud_enhanced_gray(unsigned char *rgba)
{
    int i;
    for (i=0;i<64*48;i++) {
        unsigned char *q=rgba+i*4;
        q[0]=q[1]=q[2]=eh_byte(.2126*q[0]+.7152*q[1]+.0722*q[2]);
        q[3]=eh_byte(q[3]*.5);
    }
}

/* The build clock over any cameo-shaped raster: the full cameo and the queue strip's
   half-size one share this, so the two wipes are one drawing at one texel density. */
static void eh_clock_wh(unsigned char *rgba, int w, int h, float progress, int busy, int nod, float time)
{
    const double tau=6.283185307179586;
    const double tint[2][3]={{91,68,32},{96,16,12}};
    const double light[2][3]={{255,242,193},{255,147,109}};
    const double halo[2][3]={{250,191,91},{245,47,26}};
    double front;
    int x,y,c;
    if (progress<0) progress=0;
    if (progress>1) progress=1;
    if (busy) progress=0;
    front=progress*tau;
    nod=nod?1:0;
    for (y=0;y<h;y++) for (x=0;x<w;x++) {
        unsigned char *q=rgba+(y*w+x)*4;
        double dx=x+.5-w*.5,dy=y+.5-h*.5,r=sqrt(dx*dx+dy*dy);
        double angle=fmod(atan2(dx,-dy)+tau,tau);
        double distance=(angle-front)*(r>1?r:1);
        double coverage=progress<=0?1:progress>=1?0:.5+distance/.9;
        double cloud=.5+.25*sin(x/10.0+y/13.0-time*.9)+.25*sin(y/8.0-x/16.0+time*.65);
        double opacity,core=0,glow=0;
        if (!q[3]) continue;
        if (coverage<0) coverage=0;
        if (coverage>1) coverage=1;
        opacity=coverage*(busy?.48:.78+cloud*.08);
        if (!busy && progress>0 && progress<1) {
            core=exp(-distance*distance/(2*1.15*1.15))*(.82+.18*sin(r*.53-time*8));
            glow=exp(-distance*distance/(2*2.6*2.6))*.28;
        }
        for (c=0;c<3;c++) {
            double v=eh_byte(q[c]*(1-opacity)+tint[nod][c]*(.78+cloud*.22)*opacity);
            q[c]=eh_byte(v*(1-core*.76)+light[nod][c]*core*.76+halo[nod][c]*glow);
        }
    }
}

void hud_enhanced_clock(unsigned char *rgba, float progress, int busy, int nod, float time)
{
    eh_clock_wh(rgba,64,48,progress,busy,nod,time);
}

/* Match Uint8ClampedArray rounding used by the authored pixel controls. */
static unsigned char eh_clamped_byte(double value)
{
    double lo;
    if (value < 0) return 0;
    if (value > 255) return 255;
    lo=floor(value);
    if (value-lo==.5) return (unsigned char)((int)lo%2 ? lo+1 : lo);
    return (unsigned char)floor(value+.5);
}

static void eh_button(unsigned char *dst,const H6_Pack *p,const char *icon,int frame,int hover)
{
    int x,y,c;
    static const double gain[3]={1,.92,.73};
    memset(dst,0,35*35*4);
    h6_blit(dst,35,35,hud640_asset(p,"eh_plate"),frame,0,0);
    h6_blit(dst,35,35,hud640_asset(p,icon),0,8,8+(frame==2?1:0));
    if (!hover) return;
    for (y=0;y<35;y++) for (x=0;x<35;x++) {
        unsigned char *q=dst+(y*35+x)*4;
        int edge=x<34-x?x:34-x,vy=y<34-y?y:34-y;
        int lift;
        if (vy<edge) edge=vy;
        if (!q[3]) continue;
        lift=edge>=2&&edge<=4?28:17;
        for (c=0;c<3;c++) q[c]=eh_clamped_byte(q[c]*1.65+lift*gain[c]);
    }
}

static void eh_raw(unsigned char *dst,int dw,int dh,const unsigned char *rgba,int sw,int sh,int x,int y)
{
    H6_Asset a;
    memset(&a,0,sizeof a);a.w=sw;a.h=sh;a.frames=1;a.rgba=rgba;
    h6_blit(dst,dw,dh,&a,0,x,y);
}

void hud_enhanced_bar(unsigned char *dst,const H6_Pack *p,const EH_State *s)
{
    static unsigned char base[160*624*4],button[35*35*4],map[160*144*4];
    static const char *tools[3]={"eh_repair","eh_sell","eh_power"};
    static const char *cats[4]={"eh_structures","eh_infantry","eh_vehicles","eh_aircraft"};
    static const int tx[4]={7,44,81,118};
    H6_State bs=s->base;
    const H6_Asset *frame=hud640_asset(p,"eh_body"),*off,*lit,*mark;
    int rows=s->rows,h=hud_enhanced_height(rows),footer=h-30,body=275,bodyh=footer-body;
    int x,y,k,segments=(238+(rows-5)*48)/12,level,on,meterFloor=449+(rows-5)*48+66;
    memset(dst,0,(size_t)EH_WIDTH*h*4);
    bs.rows=8;bs.power_level=bs.power_drain_level=0;
    bs.repair_frame=bs.sell_frame=-1;bs.map_frame=0;
    hud640_draw_bar(base,p,&bs);
    /* Continuous source grid at native density, cropped only on shorter displays. */
    for (y=0;y<bodyh;y++) memcpy(dst+((size_t)(body+y)*EH_WIDTH+EH_INNER_X)*4,
        base+((size_t)(209+y)*160)*4,160*4);
    /* Meter uses the same watt scale and full-height channel for every category. */
    level=s->base.power_level;if(level<0)level=0;if(level>100)level=100;
    on=(segments*level+50)/100;lit=hud640_asset(p,"meter_lit");
    for(k=0;k<on;k++) h6_blit(dst,EH_WIDTH,h,lit,0,EH_INNER_X,meterFloor-(k+1)*12);
    h6_rotate_green(dst,EH_WIDTH,h,EH_INNER_X,meterFloor-on*12,EH_INNER_X+17,meterFloor,s->base.power_color);
    mark=hud640_asset(p,"meter_drain");
    if(mark&&s->base.power_drain_level>0){
        int drain=s->base.power_drain_level,my;
        if(drain>100)drain=100;
        my=meterFloor-(segments*drain+50)/100*12-mark->h/2;
        if(my<meterFloor-segments*12)my=meterFloor-segments*12;
        if(my>meterFloor-mark->h)my=meterFloor-mark->h;
        h6_blit(dst,EH_WIDTH,h,mark,0,EH_INNER_X,my);
    }
    /* Wing first, minimap rail above it: the painted contact shadow stays underneath. */
    h6_blit(dst,EH_WIDTH,h,hud640_asset(p,"eh_wing"),0,0,32);
    for(y=0;y<144;y++)for(x=0;x<160;x++){
        unsigned char *q=map+(y*160+x)*4;q[0]=8;q[1]=9;q[2]=11;q[3]=255;
    }
    if(s->base.radar_active&&s->base.radar_rgba)eh_raw(map,160,144,s->base.radar_rgba,160,144,0,0);
    else {
        off=hud640_asset(p,s->base.nod?"radar_off_nod":"radar_off");
        /* Enlarge only faction artwork, never the authored frame or corner pixels. */
        if(off&&off->rgba)for(y=0;y<144;y++)for(x=0;x<160;x++) {
            int sx=x*off->w/160,sy=y*off->h/144;
            memcpy(map+(y*160+x)*4,off->rgba+(sy*off->w+sx)*4,4);
        }
    }
    eh_raw(dst,EH_WIDTH,h,map,160,144,72,31);
    h6_blit(dst,EH_WIDTH,h,hud640_asset(p,"eh_map"),0,EH_MAIN_X,18);
    /* Retain both corner joins when the housing is shortened, never scale an edge. */
    if(rows==8)h6_blit(dst,EH_WIDTH,h,frame,0,EH_MAIN_X,205);
    else {
        int height=h-205;
        for(y=0;y<height-48;y++)for(x=0;x<184;x++){
            const unsigned char *q=frame->rgba+(y*184+x)*4;
            if(q[3]){unsigned char *d=dst+((size_t)(205+y)*EH_WIDTH+EH_MAIN_X+x)*4;memcpy(d,q,4);}
        }
        for(y=0;y<48;y++)for(x=0;x<184;x++){
            const unsigned char *q=frame->rgba+((440+y)*184+x)*4;
            if(q[3]){unsigned char *d=dst+((size_t)(h-48+y)*EH_WIDTH+EH_MAIN_X+x)*4;memcpy(d,q,4);}
        }
    }
    for(k=0;k<3;k++)if(s->tool_frame[k]>=0){
        eh_button(button,p,tools[k],s->tool_frame[k],s->tool_hover[k]);
        eh_raw(dst,EH_WIDTH,h,button,35,35,EH_TOOL_X,EH_TOOL_Y+k*42);
    }
    for(k=0;k<4;k++){
        eh_button(button,p,cats[k],s->category_frame[k],s->category_hover[k]);
        eh_raw(dst,EH_WIDTH,h,button,35,35,EH_INNER_X+tx[k],EH_TAB_Y);
    }
}

void hud_enhanced_tab(unsigned char *dst,const H6_Pack *p,const char *label,int value,int frame,int wide)
{
    int w=wide>1?wide:wide?184:160,x,y,c;
    const H6_Asset *font=hud640_asset(p,"font_big");
    memset(dst,0,(size_t)w*17*4);
    if(!label)hud640_draw_tab(dst,p,NULL,value,frame);
    else {
        const H6_Asset *plate=hud640_asset(p,wide==1?"eh_header":"tab_plate");
        if(w==148&&plate){
            /* Short-window headers retain both authored bolt caps without scaling. */
            for(y=0;y<17;y++)for(x=0;x<w;x++){
                int sx=x<74?x:x+12;
                const unsigned char *q=plate->rgba+((size_t)y*plate->w*plate->frames+frame*plate->w+sx)*4;
                memcpy(dst+(y*w+x)*4,q,4);
            }
        }else h6_blit(dst,w,17,plate,frame,0,0);
        if(font){
            unsigned char text[184*17*4];
            memset(text,0,sizeof text);
            h6_print(text,w,17,font,label,(w-h6_text_width(font,label))/2,(17-font->h)/2+(frame==2?1:0));
            if(frame==2)for(y=0;y<w*17;y++)for(c=0;c<3;c++)text[y*4+c]=(unsigned char)(text[y*4+c]*.55);
            eh_raw(dst,w,17,text,w,17,0,0);
        }
    }
    for(y=0;y<17;y++)for(x=0;x<w;x++){
        int xx=x<w-1-x?x:w-1-x,yy=y<16-y?y:16-y;
        if(xx+yy<4)dst[(y*w+x)*4+3]=0;
    }
}

/* ---- the production queue strip --------------------------------------------------
   One card per thing being built or queued, hung off the left edge of the panel beside
   the power meter. Everything here is authored at one texel per native HUD unit, like
   the rest of the panel: the half-size cameo, the clock wipe over it and the lettering
   are composed into the card at native size and the shared HUD zoom is applied once by
   the caller. The frame is drawn rather than cut from the cameo well, because the well's
   frame is part of the chassis (square outside, chamfered inside) and a floating card
   needs its outer corners cut as well; its tones are the chassis's own, sampled from the
   panel rim, the well bevel and the well's dark lip, lit from the upper left. */
static int eh_queue_depth(int x,int y,int *face)
{
    int d[8],k,best=0;
    d[0]=x;d[1]=y;d[2]=EHQ_W-1-x;d[3]=EHQ_H-1-y;
    d[4]=x+y-EHQ_CUT;d[5]=(EHQ_W-1-x)+y-EHQ_CUT;
    d[6]=x+(EHQ_H-1-y)-EHQ_CUT;d[7]=(EHQ_W-1-x)+(EHQ_H-1-y)-EHQ_CUT;
    for(k=1;k<8;k++)if(d[k]<d[best])best=k;
    if(face)*face=best;
    return d[best];
}

/* Light ink with a dark one-pixel halo, so a figure reads over any cameo. */
static void eh_queue_text(unsigned char *dst,const H6_Asset *font,const char *text,int x,int y,
                          const double tint[3],int clip)
{
    static unsigned char ink[EHQ_W*EHQ_H*4];
    int px,py,c;
    memset(ink,0,sizeof ink);
    h6_print(ink,EHQ_W,EHQ_H,font,text,x,y);
    for(py=0;py<EHQ_H;py++)for(px=0;px<EHQ_W;px++){
        unsigned char *d=dst+(py*EHQ_W+px)*4;
        const unsigned char *q=ink+(py*EHQ_W+px)*4;
        if(clip&&eh_queue_depth(px,py,0)<3)continue;
        if(q[3]){
            /* the font's own grey ramp, carried into the tint: lit strokes stay lit and
               the baked one-pixel shadow stays dark */
            double lum=(q[0]+q[1]+q[2])/(3.0*255.0),a=q[3]/255.0;
            for(c=0;c<3;c++)d[c]=eh_byte(d[c]*(1-a)+tint[c]*lum*a);
        }else{
            int nx,ny,near=0;
            for(ny=py-1;ny<=py+1&&!near;ny++)for(nx=px-1;nx<=px+1;nx++){
                if(nx<0||ny<0||nx>=EHQ_W||ny>=EHQ_H)continue;
                if(ink[(ny*EHQ_W+nx)*4+3]){near=1;break;}
            }
            if(near)for(c=0;c<3;c++)d[c]=eh_byte(d[c]*.3+6*.7);
        }
    }
}

void hud_enhanced_queue_card(unsigned char *dst,const H6_Pack *p,const unsigned char *cameo,
                             float progress,int state,int count,int nod,float time)
{
    static const unsigned char lit[2][3]={{127,123,124},{70,69,70}};
    static const unsigned char mid[2][3]={{67,65,66},{48,47,49}};
    static const unsigned char shade[2][3]={{37,36,38},{21,21,22}};
    static const double light[2][3]={{255,242,193},{255,147,109}};
    static const double amber[3]={250,191,91};
    static unsigned char mini[EHQ_CAMEO_W*EHQ_CAMEO_H*4];
    const H6_Asset *font=hud640_asset(p,"font_small");
    char text[16];
    int x,y,c,face,k;
    nod=nod?1:0;
    memset(dst,0,EHQ_W*EHQ_H*4);
    /* the picture first, at half size: each native pixel is the mean of a 2x2 block of
       the 64x48 cameo, the same reduction the selection card's smaller wells use */
    for(y=0;y<EHQ_CAMEO_H;y++)for(x=0;x<EHQ_CAMEO_W;x++){
        unsigned char *m=mini+(y*EHQ_CAMEO_W+x)*4;
        m[0]=8;m[1]=9;m[2]=11;m[3]=255;
        if(cameo&&state!=EHQ_MORE){
            const unsigned char *s0=cameo+((y*2)*64+x*2)*4,*s1=s0+64*4;
            int a=s0[3]+s0[7]+s1[3]+s1[7];
            for(c=0;c<3;c++){
                int sum=s0[c]*s0[3]+s0[4+c]*s0[7]+s1[c]*s1[3]+s1[4+c]*s1[7];
                double v=a?(double)sum/a:0,cov=a/(4.0*255.0);
                m[c]=eh_byte(m[c]*(1-cov)+v*cov);
            }
        }
    }
    /* A queued item has not started, so it carries no clock: a fully closed clock would
       hide WHAT is queued, which is the one thing the card is for. It is dimmed and half
       desaturated instead, so the item at work stands out from those waiting behind it. */
    if(state==EHQ_WAITING)for(x=0;x<EHQ_CAMEO_W*EHQ_CAMEO_H;x++){
        unsigned char *m=mini+x*4;
        double l=.2126*m[0]+.7152*m[1]+.0722*m[2];
        for(c=0;c<3;c++)m[c]=eh_byte((m[c]*.5+l*.5)*.62);
    }
    else if(state!=EHQ_MORE)
        eh_clock_wh(mini,EHQ_CAMEO_W,EHQ_CAMEO_H,progress,state==EHQ_HOLD,nod,time);
    for(y=0;y<EHQ_H;y++)for(x=0;x<EHQ_W;x++){
        unsigned char *d=dst+(y*EHQ_W+x)*4;
        k=eh_queue_depth(x,y,&face);
        if(k<0)continue;
        d[3]=255;
        if(k==0){d[0]=8;d[1]=8;d[2]=10;continue;}
        if(k<=2){
            /* left, top and the top-left cut face the light; right, bottom and the
               bottom-right cut face away; the two remaining cuts are half lit */
            const unsigned char *t=face==0||face==1||face==4?lit[k-1]:face==5||face==6?mid[k-1]:shade[k-1];
            d[0]=t[0];d[1]=t[1];d[2]=t[2];
            continue;
        }
        memcpy(d,mini+((y-EHQ_CAMEO_Y)*EHQ_CAMEO_W+(x-EHQ_CAMEO_X))*4,3);
        /* the well's dark lip, one pixel inside the bevel, as on the full-size cameos */
        if(k==3)for(c=0;c<3;c++)d[c]=eh_byte(d[c]*.45);
    }
    if(!font)return;
    if(state==EHQ_MORE)sprintf(text,"+%d",count);
    else if(state==EHQ_HOLD)strcpy(text,"HOLD");
    else{
        int pct=(int)floor((state==EHQ_WAITING?0:progress)*100.0);
        if(pct<0)pct=0;
        if(pct>100)pct=100;
        sprintf(text,"%d%%",pct);
    }
    eh_queue_text(dst,font,text,EHQ_CAMEO_X+(EHQ_CAMEO_W-h6_text_width(font,text))/2,10,
                  state==EHQ_HOLD?amber:light[nod],0);
    /* how many of this one are in hand, in the corner the clock's sweep ends in */
    if(count>1&&state!=EHQ_MORE){
        int tw,bx,by=19;
        sprintf(text,"%d",count>99?99:count);
        tw=h6_text_width(font,text);
        bx=EHQ_CAMEO_X+EHQ_CAMEO_W-3-tw-2;
        for(y=by;y<EHQ_H;y++)for(x=bx;x<EHQ_W;x++){
            unsigned char *d=dst+(y*EHQ_W+x)*4;
            if(eh_queue_depth(x,y,0)<3)continue;
            if(x==bx&&y==by)continue;
            for(c=0;c<3;c++)d[c]=eh_byte(d[c]*.18+10*.82);
        }
        eh_queue_text(dst,font,text,bx+2,by,amber,1);
    }
}

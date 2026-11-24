/* Approved RA v06 presentation. All gameplay state/orders remain in cnc_sidebar.h. */
static std::map<std::string,std::vector<unsigned char> > g_rahCameos;
static unsigned char g_rahBar[RAH_W*RAH_H*4],g_rahWing[RAH_WING_W*RAH_WING_H*4],g_rahHeader[RAH_W*17*4];
static GLuint g_rahTex=0,g_rahWingTex=0,g_rahHeadTex[5]={0};
/* The names no pack has a cameo for: rah_cameo_plain hands them a clear 64x48, which
   the build strip and the spy panel draw as nothing, and rah_cameo_art says there is
   no picture at all, so the unit card does not report one it cannot show. */
static std::set<std::string> g_rahCameoNone;
static const unsigned char *rah_cameo_plain(const char *name){
 const char *art=sb_art_name(name);std::vector<unsigned char>& out=g_rahCameos[art];if(!out.empty())return out.data();
 out.assign(64*48*4,0);const DB_Pack *p=g_ra95Pack;const DB_Shape *a=p?db_shape(p,art):0;
 if(!a){p=g_dbPack;a=p?db_shape(p,art):0;}if(!a){g_rahCameoNone.insert(art);return out.data();}
 std::vector<unsigned char> ix(a->w*a->h);DB_Surface surf;db_surface_init(&surf,a->w,a->h,ix.data());db_draw_shape(&surf,a,0,0,0);
 for(int y=0;y<48;y++)for(int x=0;x<64;x++){unsigned c=ix[(y*a->h/48)*a->w+x*a->w/64];unsigned char *q=out.data()+(y*64+x)*4;memcpy(q,p->pal8+c*3,3);q[3]=255;}
 return out.data();
}
static const unsigned char *rah_cameo_art(const char *name){
 const unsigned char *px=rah_cameo_plain(name);return g_rahCameoNone.count(sb_art_name(name))?NULL:px;
}
static int rah_skin(SbHit hit,int index=-1,bool active=false,bool disabled=false){
 if(disabled)return 4;if(g_sbPress==hit&&(index<0||g_sbPressCol==index))return 2;
 if(g_sbHover==hit&&(index<0||g_sbHoverCol==index))return active?5:1;return active?3:0;
}
static void rah_cameo(RAH_Surface dst,const EH_Display& d,int x,int y){
 const SbEntry *e=d.engine>=0?&g_sbState.entry[d.engine]:0;unsigned char image[RAH_CW*RAH_CH*4];RAH_Surface c={image,RAH_CW,RAH_CH};memset(image,0,sizeof image);
 rah_raw(c,rah_cameo_plain(d.name),64,48,0,0,RAH_CW,RAH_CH);
 if(!e){for(int k=0;k<RAH_CW*RAH_CH;k++){unsigned char *q=image+k*4;q[0]=q[1]=q[2]=(q[0]*3+q[1]*6+q[2])/10;q[3]=128;}}
 else if(!e->completed&&(e->constructing||e->onHold||e->busy)){
  const float tm=g_sb.EffectTime?g_sb.EffectTime():0;const float progress=e->constructing||e->onHold?eh_smooth_progress(*e,tm):0;
  const double tau=6.283185307179586,angle=std::max(0.f,std::min(1.f,progress))*tau;
  for(int yy=0;yy<RAH_CH;yy++)for(int xx=0;xx<RAH_CW;xx++){
   double dx=xx+.5-RAH_CW*.5,dy=yy+.5-RAH_CH*.5,a=atan2(dy,dx)+tau*.25;if(a<0)a+=tau;if(a>=tau)a-=tau;
   unsigned char *q=image+(yy*RAH_CW+xx)*4;
   if(a>=angle){double v=(xx+yy)/(double)(RAH_CW+RAH_CH);const double tint[2][3]={{10+7*v,28+11*v,65+13*v},{65+21*v,7+7*v,5+2*v}};double alpha=.90-.08*sin(v*3.14159);for(int k=0;k<3;k++)q[k]=(unsigned char)(q[k]*(1-alpha)+tint[g_sb.nod?1:0][k]*alpha);}
   double vx=sin(angle),vy=-cos(angle),along=dx*vx+dy*vy,dist=fabs(dx*vy-dy*vx);
   if(progress>0&&along>=0&&dist<2.5){double a=(dist<1.5?.75:.22)*(0.85+.15*sin(tm*4));const int col[2][3]={{141,173,212},{236,102,82}};for(int k=0;k<3;k++)q[k]=(unsigned char)(q[k]*(1-a)+col[g_sb.nod?1:0][k]*a);}
  }
 }
 rah_raw(dst,image,RAH_CW,RAH_CH,x,y,RAH_CW,RAH_CH);
 if(e&&(e->completed||e->onHold))rah_text(dst,e->completed?"READY":"HOLD",x+40,y+24,"gold",true);
 if(g_sbHoverCol>=0&&g_sbHoverSlot>=0){const EH_Display *h=eh_display(g_sbHoverCol,g_sbHoverSlot);if(h&&!strcmp(h->name,d.name)&&(g_sbHover==SBH_ITEM||g_sbHover==SBH_LOCKED))rah_outline(dst,x,y,RAH_CW,RAH_CH);}
}
/* ---- COMPOSED ONLY ON CHANGE (hud_compose.h) ---------------------------------------
   The bar has three kinds of region. The radar opening (30,34) 156x151 is painted opaque
   black and then the radar or the emblem, and nothing drawn after it reaches into it: the
   body, the glass, the corner, the power bars and the tabs all start below row 205 or
   left of column 30. The build cells (RAH_GRID, pitch RAH_DX x RAH_DY, RAH_CW x RAH_CH
   each) do not overlap one another, and nothing drawn after them reaches into them: the
   readout well and the scroll arrows start at foot = height - 33, four rows under the
   last cell. So the bar is described by a frame key (everything else), one key per cell
   and a radar key. While the frame key holds, a cell or the radar that changed is drawn
   again on its own, by the same functions the whole compose uses: a cell over what lay
   under it before the cells were drawn (g_rahUnder), the radar over nothing, because its
   black fill is opaque. */
static unsigned char g_rahUnder[RAH_W*RAH_H*4],g_rahVerify[RAH_W*RAH_H*4];
static HudSurface g_rahBarSurf,g_rahWingSurf,g_rahHeadSurf[5];
static HudKey g_rahKey,g_rahCellKey[RAH_ROWS*2],g_rahWingKey,g_rahHeadKey;
static std::vector<unsigned char> g_rahCellShown[RAH_ROWS*2];
struct RAH_RadarShown {bool valid;int active,nod;unsigned char px[RAH_RADAR_W*RAH_RADAR_H*4];};
static RAH_RadarShown g_rahRadarShown;
static bool rah_radar_shown_same(const H6_State& st){
 const int active=st.radar_active&&st.radar_rgba?1:0;
 if(!g_rahRadarShown.valid||g_rahRadarShown.active!=active)return false;
 if(!active)return g_rahRadarShown.nod==(g_sb.nod?1:0);
 return !memcmp(g_rahRadarShown.px,st.radar_rgba,sizeof g_rahRadarShown.px);
}
static void rah_radar_shown_note(const H6_State& st){
 g_rahRadarShown.valid=true;g_rahRadarShown.active=st.radar_active&&st.radar_rgba?1:0;g_rahRadarShown.nod=g_sb.nod?1:0;
 if(g_rahRadarShown.active)memcpy(g_rahRadarShown.px,st.radar_rgba,sizeof g_rahRadarShown.px);
}
static void rah_bar_radar(RAH_Surface s,const H6_State& st){
 rah_rect(s,30,34,156,151,0,0,0);if(st.radar_active&&st.radar_rgba)rah_raw(s,st.radar_rgba,156,151,30,34,156,151);else rah_art(s,g_sb.nod?"soviet":"allies",30,34,0,156,151);
}
static void rah_bar_cell(RAH_Surface s,int k){
 int at=g_ehTop[g_ehCategory]*2+k,x=RAH_GRID_X+(k%2)*RAH_DX,y=RAH_GRID_Y+(k/2)*RAH_DY;if(at<(int)g_ehItems.size()){EH_Display d=g_ehItems[at];rah_cameo(s,d,x,y);}else rah_rect(s,x,y,80,60,0,0,0,56);
}
/* The name of the item whose cell rah_cameo outlines this frame, or 0. */
static const char *rah_outlined_name(void){
 if(g_sbHoverCol>=0&&g_sbHoverSlot>=0){const EH_Display *h=eh_display(g_sbHoverCol,g_sbHoverSlot);if(h&&(g_sbHover==SBH_ITEM||g_sbHover==SBH_LOCKED))return h->name;}
 return 0;
}
/* Exactly what rah_cameo (or the empty cell) reads for cell k. The smoothed progress is
   asked for under the very condition rah_cameo asks for it, and eh_smooth_progress gives
   the same answer to a second question in the same frame, so the compose that may follow
   sees the same value. The time goes in only where the sweep line's pulse is drawn. */
static void rah_cell_key(HudKey& key,int k,const char *outlined){
 key.clear();const int at=g_ehTop[g_ehCategory]*2+k;
 if(at>=(int)g_ehItems.size()){key.i(-2);return;}
 const EH_Display& d=g_ehItems[at];key.s(d.name);key.i(g_sb.nod?1:0);
 const SbEntry *e=d.engine>=0?&g_sbState.entry[d.engine]:0;
 if(!e)key.i(-1);
 else{
  key.i(e->completed?1:0);key.i(e->constructing?1:0);key.i(e->onHold?1:0);key.i(e->busy?1:0);
  if(!e->completed&&(e->constructing||e->onHold||e->busy)){
   const float tm=g_sb.EffectTime?g_sb.EffectTime():0;const float progress=e->constructing||e->onHold?eh_smooth_progress(*e,tm):0;
   key.f(progress);if(progress>0)key.f(tm);
  }
 }
 key.i(outlined&&!strcmp(outlined,d.name)?1:0);
}
static void rah_bar_compose(const H6_State& st,bool keepUnder){
 memset(g_rahBar,0,sizeof g_rahBar);const int height=rah_height(g_h6Rows),base=height-49,foot=height-33;RAH_Surface s={g_rahBar,RAH_W,height};
 rah_art(s,"map",0,18);rah_bar_radar(s,st);
 char bodyName[16]="body",glassName[16]="glass";if(g_h6Rows<6){snprintf(bodyName,sizeof bodyName,"body%d",g_h6Rows);snprintf(glassName,sizeof glassName,"glass%d",g_h6Rows);}const H6_Asset *glass=hud640_asset(g_raHudPack,glassName);const int gy=height-glass->h,limit=base-gy-22;rah_art(s,bodyName,0,205);rah_art(s,glassName,0,gy);
 int produced=std::max(0,g_sbState.powerProduced),drained=std::max(0,g_sbState.powerDrained),status=drained>produced*2?2:drained>produced?1:0;
 const int cols[3][2][3]={{{89,255,85},{0,170,0}},{{239,174,85},{215,121,16}},{{255,0,0},{178,0,0}}};int ph=rah_power_height(produced,limit),dh=rah_power_height(drained,limit);
 for(int k=0;k<2;k++)if(ph)rah_rect(s,10+k*2,base-ph,2,ph+1,cols[status][k][0],cols[status][k][1],cols[status][k][2]);
 rah_art(s,"demand",2,base-dh-4);rah_art(s,"corner",0,205);
 static const char *cats[]={"STRUCTURES","INFANTRY","VEHICLES","AIRCRAFT","SHIPS"},*icons[]={"structures","infantry","vehicles","aircraft","ships"};
 for(int k=0;k<5;k++){rah_art(s,"tab",32+31*k,229,rah_skin(SBH_CATEGORY,k,g_ehCategory==k));rah_art(s,icons[k],36+31*k,234);}
 rah_text(s,cats[g_ehCategory],112,263,"gold",true);eh_refresh();
 if(keepUnder)memcpy(g_rahUnder+(size_t)RAH_GRID_Y*RAH_W*4,g_rahBar+(size_t)RAH_GRID_Y*RAH_W*4,(size_t)(foot-RAH_GRID_Y)*RAH_W*4);
 for(int k=0;k<g_h6Rows*2;k++)rah_bar_cell(s,k);
 char readout[64];snprintf(readout,sizeof readout,"%d/%d",produced,drained);rah_well(s,30,foot,114,16);rah_text(s,readout,87,foot+4,"gold",true,110);
 const int maxTop=std::max(0,((int)g_ehItems.size()+1)/2-g_h6Rows);
 if(maxTop>0)for(int k=0;k<2;k++){SbHit hit=k?SBH_DOWN:SBH_UP;bool disabled=k?g_ehTop[g_ehCategory]>=maxTop:g_ehTop[g_ehCategory]<=0;int x=k?176:152;rah_art(s,"scroll",x,foot,rah_skin(hit,0,false,disabled));rah_scroll_arrow(s,x+9,foot+5,k!=0,disabled?"muted":"font");}
}
/* A TOOL THAT IS ON IS LIT: its plate takes the approved active frame and its icon the
   HUD's gold ink, the pair the mission timer's plate lights with. The active plate alone is
   a lighter grey, and a lightning button that only turned a shade lighter did not read as
   on beside Tiberian Dawn's, whose plate turns orange. The icon keeps its own dark outline
   and shading: only its light body takes the gold, scaled by how light it was. */
static void rah_tool_icon(RAH_Surface s,const char *name,int x,int y,bool lit){
 const H6_Asset *a=hud640_asset(g_raHudPack,name);if(!a)return;
 if(!lit||a->w>32||a->h>32){rah_art(s,name,x,y);return;}
 unsigned char px[32*32*4];
 for(int yy=0;yy<a->h;yy++)for(int xx=0;xx<a->w;xx++){
  const unsigned char *q=a->rgba+((size_t)yy*a->w*a->frames+xx)*4;unsigned char *o=px+(yy*a->w+xx)*4;memcpy(o,q,4);
  const int lum=(q[0]+q[1]+q[2])/3;
  if(q[3]&&lum>=96){o[0]=(unsigned char)std::min(255,231*lum/224);o[1]=(unsigned char)std::min(255,167*lum/224);o[2]=0;}
 }
 rah_raw(s,px,a->w,a->h,x,y,a->w,a->h);
}
static void rah_wing_compose(void){
 memset(g_rahWing,0,sizeof g_rahWing);RAH_Surface wing={g_rahWing,85,165};rah_art(wing,"wing",0,0);
 static const char *tools[]={"repair","sell","power"};bool modes[]={g_sbRepairOn,g_sbSellOn,g_sbPowerOn};SbHit hits[]={SBH_REPAIR,SBH_SELL,SBH_POWER};
 for(int k=0;k<3;k++){const bool disabled=g_sbSpectator||(k==0&&!g_sbState.repairEnabled)||(k==1&&!g_sbState.sellEnabled);rah_art(wing,"tool",21,23+42*k,rah_skin(hits[k],-1,modes[k],disabled));rah_tool_icon(wing,tools[k],29,31+42*k,modes[k]&&!disabled);}
}
/* One header plate (k 0..3) or the mission timer (k 4) into g_rahHeader. */
static void rah_header_compose(const char *art,int w,int frame,const char *text,const char *font,int maxw){
 memset(g_rahHeader,0,sizeof g_rahHeader);RAH_Surface hs={g_rahHeader,w,17};rah_art(hs,art,0,0,frame);rah_text(hs,text,w/2,4,font,true,maxw);
}
/* Bring the plate's texture to what (art, w, frame, text, font) describe, composing only
   when the texture does not already show it. */
static void rah_header_plate(int k,const char *art,int w,int frame,const char *text,const char *font,int maxw){
 HudSurface& surf=g_rahHeadSurf[k];hud_surface_register(surf,"rah header",&g_rahHeadTex[k]);
 HudKey& key=g_rahHeadKey;key.clear();key.p(g_raHudPack);key.s(art);key.i(w);key.i(frame);key.s(text);key.s(font);key.i(maxw);
 const size_t n=(size_t)w*17*4;
 if(hud_surface_fresh(surf,key)){
  g_hudSkipped++;
  if(hud_compose_verify()){rah_header_compose(art,w,frame,text,font,maxw);hud_verify_compare("rah header",surf.pixels.data(),g_rahHeader,n);}
  return;
 }
 rah_header_compose(art,w,frame,text,font,maxw);h6_upload(&g_rahHeadTex[k],g_rahHeader,w,17);g_hudComposed++;hud_surface_store(surf,key,g_rahHeader,n);
}
static void rah_draw_panel(const H6_State& st,int fbw,int fbh){
 const int height=rah_height(g_h6Rows),base=height-49;
 eh_refresh();
 /* What the power gates read (G69), and the readout, worked out whether or not the bar is
    composed this frame. */
 char glassName[16]="glass";if(g_h6Rows<6)snprintf(glassName,sizeof glassName,"glass%d",g_h6Rows);const H6_Asset *glass=hud640_asset(g_raHudPack,glassName);const int gy=height-glass->h,limit=base-gy-22;
 const int produced=std::max(0,g_sbState.powerProduced),drained=std::max(0,g_sbState.powerDrained),status=drained>produced*2?2:drained>produced?1:0,ph=rah_power_height(produced,limit);
 char readout[64];snprintf(readout,sizeof readout,"%d/%d",produced,drained);
 /* the bar's frame key: everything rah_bar_compose reads outside the cells and the radar */
 const int cells=g_h6Rows*2,maxTop=std::max(0,((int)g_ehItems.size()+1)/2-g_h6Rows);
 HudKey& key=g_rahKey;key.clear();key.p(g_raHudPack);key.i(g_h6Rows);key.i(height);key.i(produced);key.i(drained);key.i(g_sb.nod?1:0);key.i(g_ehCategory);
 for(int k=0;k<5;k++)key.i(rah_skin(SBH_CATEGORY,k,g_ehCategory==k));
 key.i(maxTop);
 if(maxTop>0)for(int k=0;k<2;k++){SbHit hit=k?SBH_DOWN:SBH_UP;bool disabled=k?g_ehTop[g_ehCategory]>=maxTop:g_ehTop[g_ehCategory]<=0;key.i(disabled?1:0);key.i(rah_skin(hit,0,false,disabled));}
 const char *outlined=rah_outlined_name();
 for(int k=0;k<cells;k++)rah_cell_key(g_rahCellKey[k],k,outlined);
 HudSurface& bar=g_rahBarSurf;hud_surface_register(bar,"rah bar",&g_rahTex);
 const bool barFresh=hud_surface_fresh(bar,key);
 int dirty0=height,dirty1=0;
 if(!barFresh){
  rah_bar_compose(st,true);rah_radar_shown_note(st);
  for(int k=0;k<cells;k++)g_rahCellShown[k]=g_rahCellKey[k].b;
  g_hudComposed++;
 }else{
  RAH_Surface s={g_rahBar,RAH_W,height};
  if(!rah_radar_shown_same(st)){rah_bar_radar(s,st);rah_radar_shown_note(st);g_hudRadarPatches++;dirty0=std::min(dirty0,RAH_RADAR_Y);dirty1=std::max(dirty1,RAH_RADAR_Y+RAH_RADAR_H);}
  for(int k=0;k<cells;k++){
   if(g_rahCellShown[k]==g_rahCellKey[k].b)continue;
   const int x=RAH_GRID_X+(k%2)*RAH_DX,y=RAH_GRID_Y+(k/2)*RAH_DY;
   for(int yy=y;yy<y+RAH_CH;yy++)memcpy(g_rahBar+((size_t)yy*RAH_W+x)*4,g_rahUnder+((size_t)yy*RAH_W+x)*4,(size_t)RAH_CW*4);
   rah_bar_cell(s,k);g_rahCellShown[k]=g_rahCellKey[k].b;g_hudCellPatches++;dirty0=std::min(dirty0,y);dirty1=std::max(dirty1,y+RAH_CH);
  }
  g_hudSkipped++;
  if(hud_compose_verify()){
   const size_t n=(size_t)height*RAH_W*4;memcpy(g_rahVerify,g_rahBar,n);rah_bar_compose(st,false);
   hud_verify_compare("rah bar",g_rahVerify,g_rahBar,n);memcpy(g_rahBar,g_rahVerify,n);
  }
 }
 /* the wing: the three tool plates, as rah_skin draws them */
 HudKey& wkey=g_rahWingKey;wkey.clear();wkey.p(g_raHudPack);
 {bool modes[]={g_sbRepairOn,g_sbSellOn,g_sbPowerOn};SbHit hits[]={SBH_REPAIR,SBH_SELL,SBH_POWER};
  for(int k=0;k<3;k++){wkey.i(modes[k]?1:0);wkey.i(rah_skin(hits[k],-1,modes[k],g_sbSpectator||(k==0&&!g_sbState.repairEnabled)||(k==1&&!g_sbState.sellEnabled)));}}
 HudSurface& wingSurf=g_rahWingSurf;hud_surface_register(wingSurf,"rah wing",&g_rahWingTex);
 const bool wingFresh=hud_surface_fresh(wingSurf,wkey);
 if(!wingFresh)rah_wing_compose();
 else{g_hudSkipped++;if(hud_compose_verify()){unsigned char kept[sizeof g_rahWing];memcpy(kept,g_rahWing,sizeof kept);rah_wing_compose();hud_verify_compare("rah wing",kept,g_rahWing,sizeof kept);memcpy(g_rahWing,kept,sizeof kept);}}
 glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_TEXTURE_BIT|GL_CURRENT_BIT);glDisable(GL_ALPHA_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor4f(1,1,1,1);
 const float sc=g_h6Scale,x=g_dbX0,y=g_dbY0;
 if(!wingFresh){h6_upload(&g_rahWingTex,g_rahWing,85,165);g_hudComposed++;hud_surface_store(wingSurf,wkey);}
 h6_tex_quad(g_rahWingTex,85,165,x-63*sc,y+27*sc,x+22*sc,y+192*sc);
 if(!barFresh){h6_upload(&g_rahTex,g_rahBar,216,height);hud_surface_store(bar,key);}
 else if(dirty0<dirty1){h6_upload_rows(&g_rahTex,g_rahBar,216,height,dirty0,dirty1);hud_surface_store(bar,key);}
 h6_tex_quad(g_rahTex,216,height,x,y,x+216*sc,y+height*sc);
 const char *labels[]={"OPTIONS","DATABASE",readout,"SIDEBAR"};char credits[32];snprintf(credits,sizeof credits,"%d",st.credits);labels[2]=credits;
 const int left=fbw<696*sc?132:160;const int widths[]={left,left,160,216};const float xs[]={0,left*sc,fbw-376*sc,fbw-216*sc};SbHit hh[]={SBH_OPTIONS,SBH_DATABASE,SBH_CREDITS,SBH_SIDEBAR};
 for(int k=0;k<4;k++){if(k==1&&!codex_available())continue;int w=widths[k];rah_header_plate(k,k==3?"sidebar":w==132?"header132":"header",w,rah_skin(hh[k],-1,k==3&&g_h6SlideTarget),labels[k],"font",w-12);h6_tex_quad(g_rahHeadTex[k],w,17,xs[k],0,xs[k]+w*sc,17*sc);}
 /* The mission timer: a header plate directly left of the credits plate, as tab.cpp:154 puts
    the 1995 timer tab directly left of the credits tab. Lit (the active frame, gold letters)
    under two minutes and briefly at each spoken reminder, when the 1995 tab lights. A window
    too narrow for it to clear OPTIONS and DATABASE hangs it under the credits plate instead. */
 {char tt[24];bool lit=false;if(sb_mission_timer(tt,sizeof tt,&lit)){float tx=fbw-536*sc,ty=0;if(tx<(codex_available()?2:1)*left*sc){tx=fbw-376*sc;ty=17*sc;}rah_header_plate(4,"header",160,lit?3:0,tt,lit?"gold":"font",148);h6_tex_quad(g_rahHeadTex[4],160,17,tx,ty,tx+160*sc,ty+17*sc);g_sbDrawnTimer=g_sbState.missionTimer;g_sbDrawnTimerLight=lit?1:0;snprintf(g_sbDrawnTimerText,sizeof g_sbDrawnTimerText,"%s",tt);}}
 glPopAttrib();g_sbDrawnHudPower=ph?status:-1;for(int k=0;k<3;k++)g_sbDrawnHudCount[k]=status==k?ph*4:0;
}
static void rah_free(void){hud_gen_bump();g_rahCameos.clear();g_rahCameoNone.clear();glDeleteTextures(1,&g_rahTex);glDeleteTextures(1,&g_rahWingTex);glDeleteTextures(5,g_rahHeadTex);g_rahTex=g_rahWingTex=0;memset(g_rahHeadTex,0,sizeof g_rahHeadTex);if(g_raHudPack)hud640_free(g_raHudPack);g_raHudPack=0;}

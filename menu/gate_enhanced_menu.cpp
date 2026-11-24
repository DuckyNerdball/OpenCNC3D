/* Hidden native-render/control integration test, not a playable UI fixture. */
#include "enhanced_menu.cpp"
#include <cassert>
#include "menu_language_text.h"
static int applied=0;
static int sounds[2]={};
extern "C" void game_enhanced_menu_sound(int click){sounds[click?1:0]++;}
static std::string shotdir;
static void settings_apply(void *,const DOPT_Settings *){applied++;}
static void check_seats(){
 for(size_t i=0;i<v.seats.size();i++){const Seat &a=v.seats[i];assert(a.x>=20&&a.y>=60&&a.x+a.w<=v.w-20&&a.y+a.h<=v.h-20);
  if(a.kind==BUTTON){std::string id=art_id("button-"+std::to_string(a.w)+"-"+std::to_string(a.h)+"-normal");assert(arts.count(id));}
  for(size_t j=0;j<i;j++){const Seat &b=v.seats[j];bool overlap=a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;
   if(overlap&&v.drop==-1){fprintf(stderr,"overlap: %s / %s\n",a.label.c_str(),b.label.c_str());assert(!overlap);}}
 }
}
static void sound_seats(SDL_Window *win){
 SDL_SetWindowSize(win,1280,720);em_viewport(1280,720);sound_hover.clear();
 /* the first live button a pointer can reach: with a list open, the buttons it covers
    (and those it does not) answer to the list, not to the pointer */
 auto ontop=[](int i){const Seat &q=v.seats[i];return hit(q.x+q.w/2,q.y+q.h/2)==i;};
 const Seat *button=nullptr;for(int i=0;i<int(v.seats.size());i++){const Seat &q=v.seats[i];if(!q.disabled&&q.kind!=FIELD&&ontop(i)){button=&q;break;}}
 assert(button);SDL_Event e={};e.type=SDL_MOUSEMOTION;e.motion.x=v.x+(button->x+button->w/2)*v.scale;e.motion.y=v.y+(button->y+button->h/2)*v.scale;
 int h=sounds[0],c=sounds[1];em_sound_event(&e,win);em_sound_event(&e,win);assert(sounds[0]==h+1&&sounds[1]==c);
 e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;em_sound_event(&e,win);assert(sounds[1]==c+1);
 e.type=SDL_MOUSEBUTTONUP;em_sound_event(&e,win);assert(sounds[1]==c+1);
 e.type=SDL_MOUSEMOTION;e.motion.x=e.motion.y=0;em_sound_event(&e,win);assert(sounds[0]==h+1);
 for(int i=0;i<int(v.seats.size());i++){const Seat &q=v.seats[i];if(!(q.disabled||q.kind==FIELD))continue;
  const int top=hit(q.x+q.w/2,q.y+q.h/2);if(top>=0&&top!=i)continue;   /* under an open list */
  e.motion.x=v.x+(q.x+q.w/2)*v.scale;e.motion.y=v.y+(q.y+q.h/2)*v.scale;em_sound_event(&e,win);assert(sounds[0]==h+1);}
}
static void save(const char *name){check_seats();assert(em_write_shot((shotdir+"/"+name+".png").c_str(),1280,720));
 sound_seats(SDL_GL_GetCurrentWindow());
 printf("ENMENU|feedback|%s|hover/click, release, repeated motion, disabled/empty/field=PASS\n",name);}
static int click(DOPT_State *s,int item,int fraction=500){int x,y,w,h;assert(em_item_rect(item,&x,&y,&w,&h));int dx,dy;em_to_legacy(x+(w-1)*fraction/1000,y+h/2,&dx,&dy);em_opt_press(s,dx,dy);return em_opt_release(s,dx,dy);}
static int clicksk(SK_State *s,int item){int x,y,w,h;assert(em_item_rect(item,&x,&y,&w,&h));int dx,dy;em_to_legacy(x+w/2,y+h/2,&dx,&dy);em_sk_press(s,dx,dy);return em_sk_release(s,dx,dy);}
/* A TIBERIAN DAWN MAP WITH NO BAKED PICTURE (a user map) is drawn as its rectangle with its
   starts numbered, not "No preview available": the preview's square holds the plate's grey
   and the markers' ink, which the same map without start cells does not. */
static void user_map_preview(const DB_Pack *pack){
 SK_Map maps[3]={{"USR01EA","Drawn Here","TEMPERATE",40,30,4,1},{"USR02EA","No Starts Known","TEMPERATE",40,30,4,1},{"IMP03EA","Imported","TEMPERATE",40,30,4,1}};
 /* the third has the importer's picture: a 4 x 3 field of one red, standing in for terrain */
 std::vector<unsigned char> pic(4*3*4);for(size_t q=0;q<pic.size();q+=4){pic[q]=0xc8;pic[q+1]=0x10;pic[q+2]=0x20;pic[q+3]=255;}
 maps[2].pic=pic.data();maps[2].pic_w=4;maps[2].pic_h=3;
 maps[0].x=10;maps[0].y=12;maps[0].start_cells=4;const short sx[4]={12,47,12,47},sy[4]={14,14,39,39};
 for(int k=0;k<4;k++){maps[0].start_x[k]=sx[k];maps[0].start_y[k]=sy[k];}
 int ink[3]={0,0,0},red=0;
 for(int i=0;i<3;i++){SK_State st;sk_init(&st,maps,3,nullptr);st.sel=i;v.sksection=0;em_lobby(&st,pack);em_viewport(1280,720);save(i==2?"usermap-picture":i?"usermap-nostarts":"usermap-preview");
  std::vector<unsigned char> px((size_t)1280*720*4);glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
  /* the map well sits at the right of the Players & Map page: count the plate's mid grey,
     and on the third the picture's red */
  for(size_t q=0;q<px.size();q+=4){if(px[q]==0x5c&&px[q+1]==0x5c&&px[q+2]==0x5c)ink[i]++;if(i==2&&px[q]==0xc8&&px[q+1]==0x10&&px[q+2]==0x20)red++;}}
 assert(ink[0]>200&&ink[1]==0&&ink[2]==0&&red>2000);
 printf("ENMENU|usermap-preview|plate %d px with starts, %d without; the imported picture %d px=PASS\n",ink[0],ink[1],red);
}
/* Exercise the actual Back/Done pointer actions and persisted offline controller,
   including changes which swap another seat's colour. */
static void skirmish_preferences(const DB_Pack *pack,const char *prefix){
 const SK_Map maps[]={{"SCM01EA","Eight Commanders","TEMPERATE",62,62,8,0},{"SCM02EA","Two Commanders","TEMPERATE",62,62,2,0}};
 SK_State st;sk_init(&st,maps,2,nullptr);if(theme.ra)sk_set_red_alert(&st);
 assert(sk_row_mode(&st,1)==SK_SEAT_BOT);for(int i=2;i<8;i++)assert(sk_row_mode(&st,i)==SK_SEAT_OPEN);
 auto draw=[&](){em_lobby(&st,pack);em_viewport(1280,720);check_seats();};
 auto label=[&](const char *name){int x,y,lx,ly;assert(em_seat_point(name,&x,&y));em_to_legacy(x,y,&lx,&ly);em_sk_press(&st,lx,ly);em_sk_release(&st,lx,ly);};
 const int original=sk_row_colour(&st,0),other=sk_row_colour(&st,1);draw();clicksk(&st,SK_I_ROW0);draw();clicksk(&st,SK_I_POP_C1+other);assert(sk_row_colour(&st,0)==other&&sk_row_colour(&st,1)==original);draw();label("Back");assert(st.popup<0&&sk_row_colour(&st,0)==original&&sk_row_colour(&st,1)==other);
 draw();clicksk(&st,SK_I_ROW0);draw();clicksk(&st,SK_I_POP_C1+other);draw();label("Done");assert(st.popup<0&&sk_row_colour(&st,0)==other&&sk_row_colour(&st,1)==original);
 {const int credits=st.credits,want=credits==10000?500:10000;draw();label("Match Rules");draw();sk_enh_control(&st,SK_I_CREDITS,want);assert(st.credits==want);label("Back");assert(st.credits==credits);
  draw();label("Match Rules");draw();sk_enh_control(&st,SK_I_CREDITS,want);label("Done");assert(st.credits==want);sk_enh_control(&st,SK_I_CREDITS,credits);draw();}
 draw();clicksk(&st,SK_I_ROW0+1);draw();sk_enh_set_level(&st,1,3);sk_key(&st,SK_KEY_ESC);assert(st.popup<0&&sk_row_level(&st,1)==1);
 st.mode[2]=SK_SEAT_BLOCK;st.ai_levels[1]=3;
 sk_enh_action(&st,SK_I_CHANGEMAP);sk_enh_pick_row(&st,1);sk_enh_action(&st,SK_I_PICK_OK);assert(st.sel==1&&sk_players(&st)==2&&st.mode[2]==SK_SEAT_BLOCK&&sk_row_level(&st,1)==3);
 sk_enh_action(&st,SK_I_CHANGEMAP);sk_enh_pick_row(&st,0);sk_enh_action(&st,SK_I_PICK_OK);assert(st.sel==0&&sk_players(&st)==8&&st.mode[2]==SK_SEAT_BLOCK&&st.mode[3]==SK_SEAT_OPEN);
 if(theme.ra)for(int c=0;c<8;c++){sk_enh_set_country(&st,0,c);draw();assert(seat_side(&st,0)==ra_country[c]);assert(st.side==(c==2||c==4));sk_enh_action(&st,SK_I_ROW0);assert(st.status&&strstr(st.status,ra_country[c]));sk_seat_edit_finish(&st,1);}
 assert(st.tiberium==1);
 assert(sk_enh_action(&st,SK_I_TIBERIUM)==SK_ACT_NONE&&st.tiberium==2);
 const std::string path=shotdir+"/"+prefix+"-skirmish.cfg";assert(sk_prefs_write(path.c_str(),&st));
 SK_Map reordered[]={maps[1],maps[0]};SK_State fresh;sk_init(&fresh,reordered,2,nullptr);if(theme.ra)sk_set_red_alert(&fresh);assert(sk_prefs_read(path.c_str(),&fresh));assert(fresh.sel==1&&fresh.mode[2]==SK_SEAT_BLOCK&&fresh.mode[3]==SK_SEAT_OPEN&&sk_row_level(&fresh,1)==3&&sk_row_colour(&fresh,0)==other&&fresh.tiberium==2);if(theme.ra)assert(sk_row_country(&fresh,0)==7);
 const SK_State before=fresh;FILE *f=fopen(path.c_str(),"wb");assert(f);fputs(theme.ra?"CNC3D_SKIRMISH 1 td SCM01EA\n":"CNC3D_SKIRMISH 1 ra SCM01EA\n",f);fclose(f);assert(!sk_prefs_read(path.c_str(),&fresh)&&!memcmp(&fresh,&before,sizeof fresh));
 f=fopen(path.c_str(),"wb");assert(f);fputs(theme.ra?"CNC3D_SKIRMISH 1 ra SCM01EA\n":"CNC3D_SKIRMISH 1 td SCM01EA\n",f);fclose(f);assert(!sk_prefs_read(path.c_str(),&fresh)&&!memcmp(&fresh,&before,sizeof fresh));
 assert(sk_prefs_write(path.c_str(),&st));fresh.net=1;assert(!sk_prefs_read(path.c_str(),&fresh)&&!sk_prefs_write(path.c_str(),&fresh));
 draw();assert(em_write_shot((shotdir+"/"+prefix+"-roster.png").c_str(),1280,720));
 printf("ENMENU|%s|skirmish-prefs|one AI, Empty/Blocked retained across maps, pointer Back rolls back colour swap, Done commits, Escape cancels, map-name restart roundtrip, invalid/foreign files and network isolation=PASS\n",prefix);
}
static void option_point(DOPT_State *state,int pointX,int pointY){
 int legacyX,legacyY;em_to_legacy(v.x+pointX*v.scale,v.y+pointY*v.scale,&legacyX,&legacyY);em_opt_press(state,legacyX,legacyY);
 em_options(state);em_viewport(v.fbw,v.fbh);em_to_legacy(v.x+pointX*v.scale,v.y+pointY*v.scale,&legacyX,&legacyY);em_opt_release(state,legacyX,legacyY);
}
static void dropdown_layer(DOPT_State *state,int item){
 em_options(state);em_viewport(v.fbw,v.fbh);click(state,item);em_options(state);assert(v.popupFirst>=0&&v.popupOwner>=0);
 const Seat first=v.seats[v.popupFirst];const int gapY=first.y+first.h;
 if(item==DOPT_VE_RESOLUTION){assert(!v.popupHint.empty());assert(v.popupY+v.popupH-26>=v.seats.back().y+v.seats.back().h);}
 for(int column=first.x+4;column<first.x+first.w-4;column++){
  const unsigned char *pixel=&v.rgba[(gapY*v.w+column)*4];assert(pixel[0]==0x10&&pixel[1]==0x13&&pixel[2]==0x10&&pixel[3]==255);
 }
 const DOPT_Visuals before=state->vis;
 const int scrollBefore=state->advTop;em_opt_scroll(state,1);assert(state->advTop==scrollBefore);assert(!memcmp(&before,&state->vis,sizeof before));
 if(item==DOPT_VE_RESOLUTION){assert(state->restop==1);em_opt_scroll(state,-1);assert(state->restop==0);}
 option_point(state,first.x+first.w/2,gapY);assert(v.drop==item&&state->texdrop==item);assert(!memcmp(&before,&state->vis,sizeof before));
 em_options(state);
 int disabled=-1;for(int index=v.popupFirst;index<int(v.seats.size());index++)if(v.seats[index].disabled){disabled=index;break;}
 if(disabled>=0){const Seat seat=v.seats[disabled];option_point(state,seat.x+seat.w/2,seat.y+seat.h/2);assert(v.drop==item&&state->texdrop==item);assert(!memcmp(&before,&state->vis,sizeof before));}
 em_options(state);
 int blocked=-1;for(int index=0;index<v.popupFirst;index++)if(index!=v.popupOwner&&!v.seats[index].disabled&&v.seats[index].action==OPTITEM){const Seat &seat=v.seats[index];if(!popup_contains(seat.x+seat.w/2,seat.y+seat.h/2)){blocked=index;break;}}
 assert(blocked>=0);const Seat underneath=v.seats[blocked];option_point(state,underneath.x+underneath.w/2,underneath.y+underneath.h/2);assert(v.drop==-1&&state->texdrop==-1);assert(!memcmp(&before,&state->vis,sizeof before));
 em_options(state);click(state,item);em_options(state);assert(v.popupFirst>=0);
 int allowed=-1;for(int index=v.popupFirst;index<int(v.seats.size());index++)if(!v.seats[index].disabled){allowed=index;break;}
 assert(allowed>=0);const Seat choice=v.seats[allowed];option_point(state,choice.x+choice.w/2,choice.y+choice.h/2);assert(v.drop==-1&&state->texdrop==-1);assert(dopt_enh_drop_value(state,item)==choice.arg);
 em_options(state);click(state,item);em_options(state);state->selected=DOPT_VE_WATER;const DOPT_Visuals keyboardBefore=state->vis;
 assert(em_opt_key(state,DOPT_KEY_DOWN)==DOPT_ACT_NONE&&state->selected==DOPT_VE_WATER&&state->page==DOPT_PAGE_ADVANCED&&v.drop==item);assert(!memcmp(&keyboardBefore,&state->vis,sizeof keyboardBefore));
 assert(v.popupChoice>=0&&dopt_enh_drop_allowed(state,item,v.popupChoice));const int keyboardChoice=v.popupChoice;
 assert(em_opt_key(state,DOPT_KEY_ENTER)==DOPT_ACT_NONE&&v.drop==-1&&state->texdrop==-1);assert(dopt_enh_drop_value(state,item)==keyboardChoice);assert(state->vis.prof[2].elem[DOPT_VE_WATER]==keyboardBefore.prof[2].elem[DOPT_VE_WATER]);
 em_options(state);click(state,item);em_options(state);const DOPT_Visuals escapeBefore=state->vis;assert(em_opt_key(state,DOPT_KEY_ESC)==DOPT_ACT_NONE&&state->page==DOPT_PAGE_ADVANCED&&v.drop==-1&&state->texdrop==-1);assert(!memcmp(&escapeBefore,&state->vis,sizeof escapeBefore));
 printf("ENMENU|dropdown|item=%d|opaque gaps, disabled rows, outside dismissal, no click-through, keyboard choice/escape, wheel isolation=PASS\n",item);
}
static void compact_sound_rows(DOPT_State *state){
 DOPT_Track tracks[16]={};for(int index=0;index<16;index++){tracks[index].base="fixture";tracks[index].fullname="Fixture track";tracks[index].index=index;}
 state->page=DOPT_PAGE_SOUND;dopt_set_tracks(state,tracks,16,0);state->selected=DOPT_S_LIST;state->trkSel=state->trkTop=0;em_options(state);assert(v.compact);
 em_opt_scroll(state,100);assert(state->trkTop==7);em_options(state);check_seats();bool finalVisible=false;int count=0;
 for(const Seat &seat:v.seats)if(seat.action==OPTROW){count++;if(seat.arg==15)finalVisible=true;}assert(count==9&&finalVisible);
 em_opt_scroll(state,-100);assert(state->trkTop==0);state->trkSel=0;
 for(int index=0;index<15;index++)assert(em_opt_key(state,DOPT_KEY_DOWN)==DOPT_ACT_NONE);
 assert(state->trkSel==15&&state->trkTop==7);em_options(state);check_seats();finalVisible=false;
 for(const Seat &seat:v.seats)if(seat.action==OPTROW&&seat.arg==15&&seat.active)finalVisible=true;assert(finalVisible);
 assert(em_write_shot((shotdir+"/title-sound-last-row-"+std::to_string(v.fbw)+".png").c_str(),v.fbw,v.fbh));dopt_set_tracks(state,nullptr,0,-1);
 puts("ENMENU|compact-sound|nine visible rows, final track reachable by wheel and keyboard=PASS");
}
static void compact_slot_rows(DOPT_State *state){
 state->page=DOPT_PAGE_SLOTS;state->sl.nrows=16;
 for(int index=0;index<state->sl.nrows;index++){state->sl.rows[index].slot=index;snprintf(state->sl.rows[index].text,sizeof state->sl.rows[index].text,"Fixture save %d",index+1);}
 for(int mode:{DOPT_SL_LOAD,DOPT_SL_SAVE,DOPT_SL_DELETE}){
  state->sl.mode=mode;state->selected=DOPT_SL_LIST;state->sl.sel=state->sl.top=0;em_options(state);assert(v.compact);
  for(int index=0;index<15;index++)assert(em_opt_key(state,DOPT_KEY_DOWN)==DOPT_ACT_NONE);
  assert(state->sl.sel==15);em_options(state);check_seats();bool finalVisible=false;int count=0;
  for(const Seat &seat:v.seats)if(seat.action==OPTROW){count++;if(seat.arg==15&&seat.active)finalVisible=true;}assert(count==(mode==DOPT_SL_SAVE?9:10)&&finalVisible);
  assert(em_write_shot((shotdir+"/title-slots-last-row-"+std::to_string(mode)+"-"+std::to_string(v.fbw)+".png").c_str(),v.fbw,v.fbh));
 }
 puts("ENMENU|compact-slots|load/save/delete populated lists fit and keyboard reaches final row=PASS");
}
static void retired_relief(DOPT_State *state){
 const DOPT_State savedState=*state;const int savedSection=v.section;state->page=DOPT_PAGE_ADVANCED;
 for(int mode=0;mode<DOPT_MODE_COUNT;mode++){
  state->advTab=mode;state->vis.prof[mode].elem[DOPT_VE_RELIEF]=1;state->selected=DOPT_VE_RELIEF;
  assert(!dopt_vis_in_mode(DOPT_VE_RELIEF,mode)&&dopt_item_disabled(state,DOPT_VE_RELIEF));
  const DOPT_Visuals before=state->vis;assert(dopt_key(state,DOPT_KEY_ENTER)==DOPT_ACT_NONE);assert(!memcmp(&before,&state->vis,sizeof before));
  int rows[DOPT_VE_COUNT];const int count=dopt_vis_mode_rows(mode,rows);for(int index=0;index<count;index++)assert(rows[index]!=DOPT_VE_RELIEF);
 }
 state->advTab=DOPT_MODE_ENHANCED;
 for(int section=0;section<5;section++){v.section=section;em_options(state);assert(!em_item_rect(DOPT_VE_RELIEF,nullptr,nullptr,nullptr,nullptr));for(const Seat &seat:v.seats)assert(seat.item!=DOPT_VE_RELIEF);}
 *state=savedState;v.section=savedSection;em_options(state);
 puts("ENMENU|retired-relief|absent from every Visuals section and controller mode; legacy profile value preserved=PASS");
}
static void ai_difficulty_rows(SK_State *state,const DB_Pack *pack,const char *prefix){
 const SK_State saved=*state;const int section=v.sksection;v.sksection=0;state->popup=-1;state->net=0;
 for(int i=1;i<sk_players(state);i++){state->mode[i]=SK_SEAT_BOT;state->ai_levels[i]=(unsigned char)((i-1)%4);}
 em_lobby(state,pack);em_viewport(1280,720);check_seats();int cells=0;
 for(const Seat &q:v.seats)if(SK_IS_LEVEL(q.item)){cells++;int seat=q.item-SK_I_LEVEL0;assert(q.disabled==(seat==0));assert(q.label==(seat==0?std::string("-"):sk_level_name(sk_row_level(state,seat))));assert(textwidth(q.label)<=q.w-8);}
 assert(cells==sk_players(state));
 for(int i=0;i<sk_players(state);i++){std::string summary=seat_side(state,i)+" / "+std::to_string(sk_row_team(state,i)+1)+" / Random";assert(textwidth(summary)<=264);}
 state->ai_levels[1]=1;
 for(int want:{2,3,0,1}){em_lobby(state,pack);em_viewport(1280,720);clicksk(state,SK_I_LEVEL0+1);assert(sk_row_level(state,1)==want);}
 state->popup=1;em_lobby(state,pack);em_viewport(1280,720);clicksk(state,-14);em_lobby(state,pack);assert(v.popupFirst>=0);assert(em_write_shot((shotdir+"/"+prefix+"-difficulty-options.png").c_str(),1280,720));
 for(int want=0;want<4;want++){em_lobby(state,pack);em_viewport(1280,720);clicksk(state,SK_I_POP_EASY+want);assert(sk_row_level(state,1)==want);if(want<3){em_lobby(state,pack);em_viewport(1280,720);clicksk(state,-14);}}
 em_lobby(state,pack);assert(em_write_shot((shotdir+"/"+prefix+"-difficulty-setup.png").c_str(),1280,720));
 state->popup=-1;em_lobby(state,pack);for(const auto &size:std::vector<std::pair<int,int>>{{1280,720},{2560,1440}}){
  SDL_Window *window=SDL_GL_GetCurrentWindow();SDL_SetWindowSize(window,size.first,size.second);SDL_PumpEvents();int dw,dh;SDL_GL_GetDrawableSize(window,&dw,&dh);assert(dw==size.first&&dh==size.second);
  em_viewport(size.first,size.second);assert(em_write_shot((shotdir+"/"+prefix+"-difficulty-"+std::to_string(size.first)+".png").c_str(),size.first,size.second));
  unsigned char corner[4];glReadPixels(size.first-1,size.second-1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,corner);assert(corner[3]==255);
 }
 SDL_SetWindowSize(SDL_GL_GetCurrentWindow(),1280,720);SDL_PumpEvents();em_viewport(1280,720);
 SK_Lobby result;sk_result(state,&result);for(int i=0;i<8;i++)assert(result.ai_levels[i]==state->ai_levels[i]);
 state->selected=SK_I_LEVEL0+1;sk_key(state,SK_KEY_LEFT);assert(sk_row_level(state,1)==2);sk_key(state,SK_KEY_RIGHT);assert(sk_row_level(state,1)==3);
 state->net=1;state->net_seat=0;state->net_seats=sk_players(state);state->net_taken[0]=1;state->net_taken[1]=0;
 sk_enh_set_level(state,1,0);assert(sk_row_level(state,1)==0);em_lobby(state,pack);assert(em_write_shot((shotdir+"/"+prefix+"-difficulty-network-host.png").c_str(),1280,720));state->net=2;state->net_seat=0;const int before=sk_row_level(state,1);
 sk_enh_set_level(state,1,3);assert(sk_row_level(state,1)==before);assert(sk_item_disabled(state,SK_I_LEVEL0+1));
 em_lobby(state,pack);check_seats();for(const Seat &q:v.seats)if(SK_IS_LEVEL(q.item))assert(q.disabled);
 assert(em_write_shot((shotdir+"/"+prefix+"-difficulty-network-joiner.png").c_str(),1280,720));
 if(!strcmp(prefix,"td")){
  assert(sk_check_layout(pack,state)==0);DB_Surface classic;unsigned char pixels[320*200],rgba[320*200*4];db_surface_init(&classic,320,200,pixels);sk_draw(&classic,pack,state);db_surface_to_rgba(&classic,pack->pal8,rgba,0);assert(png_write_rgba((shotdir+"/classic-difficulty.png").c_str(),rgba,320,200));
  SK_State classicState=*state;classicState.net=0;
  for(int seat=1;seat<sk_players(&classicState);seat++){classicState.popup=seat;
   for(int level=0;level<4;level++){int x,y,w,h;assert(sk_item_rect(&classicState,SK_I_POP_EASY+level,&x,&y,&w,&h));assert(sk_hit_test(&classicState,x+w/2,y+h/2)==SK_I_POP_EASY+level);sk_press(&classicState,x+w/2,y+h/2);sk_release(&classicState,x+w/2,y+h/2);assert(sk_row_level(&classicState,seat)==level);}
  }
  classicState.popup=1;sk_draw(&classic,pack,&classicState);db_surface_to_rgba(&classic,pack->pal8,rgba,0);assert(png_write_rgba((shotdir+"/classic-difficulty-options.png").c_str(),rgba,320,200));
 }
 *state=saved;v.sksection=section;em_lobby(state,pack);
 printf("ENMENU|%s|difficulty|four named levels, per-seat pointer/dropdown/keyboard, answer roundtrip, host-only edits, 720p/1440p fit=PASS\n",prefix);
}
/* THE SETTINGS SCREEN'S GAMEPLAY AND AUDIO TABS: drawn with their sliders and switches, a
   slider dragged to each end lands on that end of its setting through the bound apply, a
   switch toggles through the bound gameplay apply, and the screen stays on its own page. */
static int tabapplied=0,tabgp=0;
/* THE KEY BINDINGS PAGE against stand-in answers: the Gameplay tab's button opens it, a
   key box waits for a key and hands it to the game's assign, Escape leaves the binding,
   a refusal is said in the header, and the drag button's four choices reach the game. */
static int fakekey[FXK_COUNT],fakegrab=0,fakeassigns=0,fakeresets=0;
static const char *fake_key(int c){static char n[8];if(c<0||c>=FXK_COUNT)return "";n[0]=(char)toupper(fakekey[c]&127);n[1]=0;return n;}
static int fake_assign(int c,int code,int,char *msg,int len){fakeassigns++;if(code=='1'){snprintf(msg,len,"refused");return 0;}fakekey[c]=code;snprintf(msg,len,"%s is now %c",FX_KEYS[c].label,(char)toupper(code));return 1;}
static void fake_reset(void){fakeresets++;for(int k=0;k<FXK_COUNT;k++)fakekey[k]=FX_KEYS[k].code;fakegrab=0;}
static int fake_grab(void){return fakegrab;}
static void fake_set_grab(int b){fakegrab=b;}
static void keys_page(DOPT_State *state,const char *prefix){
 static const EM_KeyHooks hooks={fake_key,fake_assign,fake_reset,fake_grab,fake_set_grab};
 fake_reset();fakeresets=0;fakeassigns=0;em_set_key_hooks(&hooks);
 state->page=DOPT_PAGE_ADVANCED;v.section=5;em_options(state);em_viewport(1280,720);check_seats();
 click(state,EM_KEYS_OPEN);em_options(state);em_viewport(1280,720);check_seats();assert(v.section==7&&state->page==DOPT_PAGE_ADVANCED);
 /* every command has a box, and none of them is outside the panel or under another seat */
 for(int c=0;c<FXK_COUNT;c++){int x,y,w,h;assert(em_item_rect(EM_KEYS_SEAT-c,&x,&y,&w,&h));}
 for(size_t a=0;a<v.seats.size();a++)for(size_t b=a+1;b<v.seats.size();b++){const Seat &p=v.seats[a],&q=v.seats[b];
  assert(!(p.x<q.x+q.w&&q.x<p.x+p.w&&p.y<q.y+q.h&&q.y<p.y+p.h));}
 for(const Seat &q:v.seats)assert(q.x>=0&&q.y>=0&&q.x+q.w<=v.w&&q.y+q.h<=v.h);
 assert(em_write_shot((shotdir+"/"+prefix+"-settings-keys.png").c_str(),1280,720));
 click(state,EM_KEYS_SEAT-FXK_STOP);em_options(state);assert(em_keys_capturing());
 assert(em_write_shot((shotdir+"/"+prefix+"-settings-keys-waiting.png").c_str(),1280,720));
 em_keys_capture('k',14);em_options(state);assert(!em_keys_capturing()&&fakekey[FXK_STOP]=='k'&&fakeassigns==1);
 click(state,EM_KEYS_SEAT-FXK_GUARD);em_keys_capture(27,41);assert(!em_keys_capturing()&&fakeassigns==1&&fakekey[FXK_GUARD]=='g');
 click(state,EM_KEYS_SEAT-FXK_SCATTER);em_keys_capture('1',30);em_options(state);assert(fakeassigns==2&&fakekey[FXK_SCATTER]=='x');
 for(int b=FX_GRAB_COUNT-1;b>=0;b--){click(state,EM_KEYS_DRAG-b);em_options(state);assert(fakegrab==b);}
 click(state,EM_KEYS_DRAG-FX_GRAB_SIDE1);em_options(state);em_viewport(1280,720);
 assert(em_write_shot((shotdir+"/"+prefix+"-settings-keys-after.png").c_str(),1280,720));
 for(const Seat &q:v.seats)if(q.action==RESET){int dx,dy;em_to_legacy(v.x+(q.x+q.w/2)*v.scale,v.y+(q.y+q.h/2)*v.scale,&dx,&dy);em_opt_press(state,dx,dy);em_opt_release(state,dx,dy);}
 assert(fakeresets==1&&fakekey[FXK_STOP]=='s'&&fakegrab==0);
 em_set_key_hooks(nullptr);v.section=5;em_options(state);
 for(const Seat &q:v.seats)assert(q.item!=EM_KEYS_OPEN);
 v.section=0;printf("ENMENU|%s|key-bindings|page opened from Gameplay, %d boxes clear of each other, key taken, Escape kept, refusal kept, four drag buttons, reset=PASS\n",prefix,FXK_COUNT);
}
static void count_apply(void*,const DOPT_Settings*){tabapplied++;}
static void count_gp(void*,const DOPT_Gameplay*){tabgp++;}
static void settings_tabs(DOPT_State *state,const char *prefix){
 dopt_bind(state,NULL,count_apply);dopt_bind_gameplay(state,count_gp);state->page=DOPT_PAGE_ADVANCED;em_options(state);
 const int sliders[2][4]={{DOPT_ES_SPEED,DOPT_ES_SCROLL,DOPT_ES_SCROLLPUSH,DOPT_ES_SCROLLGRAB},{DOPT_ES_MUSIC,DOPT_ES_SOUND,DOPT_ES_VOICES,DOPT_ES_AMBIENCE}};
 /* the three scroll speeds run past the 1995 slider's seven steps */
 assert(dopt_enh_setting_max(DOPT_ES_SCROLL)==DOPT_MAX_SCROLL_EXT-1&&dopt_enh_setting_max(DOPT_ES_SCROLLPUSH)==DOPT_MAX_SCROLL_EXT-1&&dopt_enh_setting_max(DOPT_ES_SCROLLGRAB)==DOPT_MAX_SCROLL_EXT-1&&DOPT_MAX_SCROLL_EXT>DOPT_MAX_SCROLL);
 for(int tab=0;tab<2;tab++){
  v.section=5+tab;em_options(state);em_viewport(1280,720);check_seats();
  assert(em_write_shot((shotdir+"/"+prefix+"-settings-"+(tab?"audio":"gameplay")+".png").c_str(),1280,720));
  for(int k=0;k<4&&sliders[tab][k]>=0;k++){const int which=sliders[tab][k],before=tabapplied;
   click(state,EM_SETTING_SEAT-which,0);em_options(state);assert(dopt_enh_setting_value(state,which)==0);
   click(state,EM_SETTING_SEAT-which,1000);em_options(state);assert(dopt_enh_setting_value(state,which)==dopt_enh_setting_max(which));
   assert(tabapplied>before&&state->page==DOPT_PAGE_ADVANCED&&v.section==5+tab);}
  if(!tab){
   /* the mouse scheme's three buttons, each pressed once, each landing on its own value */
   for(int k=0;k<DOPT_G_MOUSE_COUNT;k++){const int want=(state->gp.on[DOPT_G_MOUSE]+1)%DOPT_G_MOUSE_COUNT,before=tabgp;click(state,EM_SCHEME_SEAT-want);em_options(state);assert(state->gp.on[DOPT_G_MOUSE]==want&&tabgp==before+1);}
   for(int i=1;i<DOPT_G_TOGGLES;i++){const int was=state->gp.on[i],before=tabgp;click(state,EM_GAMEPLAY_SEAT-i);em_options(state);assert(state->gp.on[i]!=was&&tabgp==before+1);}
  }
 }
 v.section=0;printf("ENMENU|%s|settings-tabs|gameplay and audio drawn, sliders to both ends, mouse scheme set three ways, switches toggled, page kept=PASS\n",prefix);
 keys_page(state,prefix);
}
/* THE LANGUAGE ROW AND ITS BOX, on stand-in answers: English on the disk, Deutsch not; the
   list opens and offers both, picking Deutsch opens the box, Download starts it, the bar is
   drawn part way, and a finished download is used and closed with OK. */
static int fakecur=0,fakestate=0,fakechose=-1;
static int fk_count(){return 2;}
static const char *fk_name(int i){return i?"Deutsch":"English";}
static int fk_current(){return fakecur;}
static int fk_installed(int i){return i==0||fakestate==2;}
static long long fk_missing(int i){return i?385447221LL:0;}
static int fk_choose(int i){fakecur=i;fakechose=i;return 1;}
static int fk_download(int){fakestate=1;return 1;}
static int fk_progress(long long *d,long long *t,char *m,int n){if(d)*d=154000000;if(t)*t=385447221;if(m&&n)snprintf(m,n,"Downloading...");return fakestate;}
static void fk_cancel(){fakestate=4;}
static const EM_LangHooks fakehooks={fk_count,fk_name,fk_current,fk_installed,fk_missing,fk_choose,fk_download,fk_progress,fk_cancel};
static void language_box(DOPT_State *state,const char *prefix){
 em_set_language_hooks(&fakehooks);fakecur=0;fakestate=0;fakechose=-1;state->page=DOPT_PAGE_ADVANCED;em_options(state);v.section=5;v.langBox=-1;
 em_options(state);em_viewport(1280,720);check_seats();
 click(state,EM_LANG_SEAT);em_options(state);assert(v.drop==EM_LANG_DROP);check_seats();
 assert(em_write_shot((shotdir+"/"+prefix+"-language-list.png").c_str(),1280,720));
 click(state,EM_LANG_SEAT-2);em_options(state);assert(v.langBox==1&&!v.langStarted);check_seats();
 assert(em_write_shot((shotdir+"/"+prefix+"-language-ask.png").c_str(),1280,720));
 click(state,EM_LANG_SEAT-20);em_options(state);assert(v.langStarted&&fakestate==1);check_seats();
 assert(em_write_shot((shotdir+"/"+prefix+"-language-bar.png").c_str(),1280,720));
 fakestate=2;em_options(state);assert(fakechose==1);check_seats();
 assert(em_write_shot((shotdir+"/"+prefix+"-language-ready.png").c_str(),1280,720));
 click(state,EM_LANG_SEAT-21);em_options(state);assert(v.langBox==-1);
 em_set_language_hooks(nullptr);v.section=0;
 printf("ENMENU|%s|language|row, list, box, bar, ready and used=PASS\n",prefix);
}
/* THE PAUSE MENU'S AUDIO: Game Controls carries the speed and the scroll rate only, the
   Audio button under Gameplay opens the jukebox page with all four volumes (the voices
   among them), and its Back returns to the pause menu on Audio. */
static int pauseapplied=0;
static void count_pause(void*,const DOPT_Settings*){pauseapplied++;}
static void pause_audio(const DB_Pack *pack,const char *prefix){
 DOPT_State o={};dopt_settings_init(&o.set);dopt_open(&o,pack);dopt_bind(&o,nullptr,count_pause);em_title_mode(0);
 em_options(&o);em_viewport(1280,720);check_seats();
 int audio=0;for(const Seat &q:v.seats)if(q.item==DOPT_AUDIO){audio++;assert(q.label=="Audio");}assert(audio==1);
 assert(em_write_shot((shotdir+"/"+prefix+"-pause.png").c_str(),1280,720));
 o.page=DOPT_PAGE_CONTROLS;em_options(&o);check_seats();
 assert(!em_item_rect(DOPT_C_MUSIC,nullptr,nullptr,nullptr,nullptr)&&!em_item_rect(DOPT_C_SOUNDCTRL,nullptr,nullptr,nullptr,nullptr));
 assert(em_write_shot((shotdir+"/"+prefix+"-game-controls.png").c_str(),1280,720));
 click(&o,DOPT_C_SPEED,1000);assert(o.set.speed==6);em_options(&o);click(&o,DOPT_C_SPEED,0);assert(o.set.speed==0);
 /* the three scroll speeds, each its own slider and each to its own top, touching no other */
 {const int which[3]={DOPT_ES_SCROLL,DOPT_ES_SCROLLPUSH,DOPT_ES_SCROLLGRAB};
  for(int k=0;k<3;k++){em_options(&o);const DOPT_Settings was=o.set;click(&o,EM_SETTING_SEAT-which[k],1000);
   assert(dopt_enh_setting_value(&o,which[k])==DOPT_MAX_SCROLL_EXT-1);
   for(int j=0;j<3;j++)if(j!=k)assert(dopt_enh_setting_value(&o,which[j])==((which[j]==DOPT_ES_SCROLL)?was.scrollrate:(which[j]==DOPT_ES_SCROLLPUSH)?was.scrollpush:was.scrollgrab));
   em_options(&o);click(&o,EM_SETTING_SEAT-which[k],0);assert(dopt_enh_setting_value(&o,which[k])==0);}}
 o.page=DOPT_PAGE_OPTIONS;em_options(&o);click(&o,DOPT_AUDIO);em_options(&o);assert(o.page==DOPT_PAGE_SOUND);check_seats();
 assert(em_write_shot((shotdir+"/"+prefix+"-audio.png").c_str(),1280,720));
 const int sliders[4]={DOPT_S_MUSIC,DOPT_S_SOUND,DOPT_S_SPEECH,DOPT_S_AMBIENCE};
 for(int k=0;k<4;k++){const int before=pauseapplied;click(&o,sliders[k],0);em_options(&o);click(&o,sliders[k],1000);em_options(&o);assert(pauseapplied>before);}
 assert(o.set.speech==239&&o.set.music==239);
 click(&o,DOPT_S_OK);assert(o.page==DOPT_PAGE_OPTIONS&&o.selected==DOPT_AUDIO);
 printf("ENMENU|%s|pause-audio|Audio under Gameplay, Game Controls speed and three scroll speeds only, four volumes with the voices, back to the pause menu=PASS\n",prefix);
}
static void title_settings(SDL_Window *window,const DB_Pack *pack){
 DOPT_State state={};dopt_open(&state,pack);state.vis.mode=state.advTab=DOPT_MODE_ENHANCED;state.vis.enh_tex_ok=1;state.vis.dos_tex_ok=0;state.vis.prof[2].elem[DOPT_VE_NEWHUD]=1;
 state.vis.nres=10;for(int index=0;index<state.vis.nres;index++){state.vis.res_w[index]=640+index*160;state.vis.res_h[index]=480+index*90;state.vis.res_fit[index]=index<5;}
 const int pages[]={DOPT_PAGE_OPTIONS,DOPT_PAGE_CONTROLS,DOPT_PAGE_GAMEPLAY,DOPT_PAGE_ADVANCED,DOPT_PAGE_SLOTS,DOPT_PAGE_SOUND,DOPT_PAGE_CHEATS,DOPT_PAGE_RESTATE,DOPT_PAGE_CONFIRM,DOPT_PAGE_NOTICE};
 for(const auto &size:std::vector<std::pair<int,int>>{{1280,720},{1920,1440},{2560,1440}}){
  const int width=size.first,height=size.second;SDL_SetWindowSize(window,width,height);em_title_mode(0);DM_State mainState;dm_state_init(&mainState);em_main(&mainState);em_viewport(width,height);em_draw(width,height,1);
  const int panelY=v.y,panelHeight=v.h,panelScale=v.scale;std::vector<unsigned char> mainPixels(width*height*4),settingsPixels(mainPixels.size());glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,mainPixels.data());
  em_title_mode(1);
  /* Every page stands in the main menu's panel, except a confirmation and a notice: those
     keep their own frame's height, centred, so three lines of text do not sit at the top of
     a frame running most of the way down the screen. The wordmark comes down with the box
     and stands above it; the backdrop does not move. */
  const Art &mark=arts.at("wordmark");const int markW=mark.w*panelScale*2,markH=mark.h*panelScale*2,markX=(width-markW)/2,markTop=panelY-markH-16*panelScale;assert(markX>=0&&markTop>=0);
  for(int page:pages){state.page=page;em_options(&state);em_viewport(width,height);check_seats();
   const bool own=page==DOPT_PAGE_CONFIRM||page==DOPT_PAGE_NOTICE;
   if(own)assert(v.title&&!v.compact&&v.h==arts.at(art_id("confirm-frame")).h&&v.h<panelHeight&&v.scale==panelScale&&v.x==(width-v.w*v.scale)/2&&v.y>=panelY&&v.y+v.h*v.scale<=height);
   else assert(v.y==panelY&&v.h==panelHeight&&v.scale==panelScale&&v.y+v.h*v.scale<=height);
   em_draw(width,height,2);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,settingsPixels.data());
   if(!own)for(int row=0;row<panelY;row++)assert(!memcmp(&mainPixels[(height-1-row)*width*4],&settingsPixels[(height-1-row)*width*4],width*4));
   else{const int shift=v.y-panelY;
    /* above the main panel, beside the wordmark's columns, the main menu's backdrop exactly */
    for(int row=0;row<panelY;row++){const unsigned char *a=&mainPixels[(height-1-row)*width*4],*b=&settingsPixels[(height-1-row)*width*4];
     assert(!memcmp(a,b,markX*4)&&!memcmp(a+(markX+markW)*4,b+(markX+markW)*4,(width-markX-markW)*4));}
    /* and the main menu's wordmark, whole, the same distance above the box as above the panel */
    for(int row=markTop;row<panelY;row++)assert(!memcmp(&mainPixels[((height-1-row)*width+markX)*4],&settingsPixels[((height-1-row-shift)*width+markX)*4],markW*4));
   }
   assert(em_write_shot((shotdir+"/title-settings-"+std::to_string(page)+"-"+std::to_string(width)+".png").c_str(),width,height));
  }
  state.page=DOPT_PAGE_ADVANCED;state.texdrop=-1;em_options(&state);
  const int items[]={DOPT_VE_RESOLUTION,DOPT_VE_TEXSET,DOPT_VE_INFSET,DOPT_VE_UISCALE,DOPT_VE_PERSPECTIVE};
  for(int item:items){v.section=item==DOPT_VE_RESOLUTION?0:item==DOPT_VE_TEXSET||item==DOPT_VE_INFSET?1:3;em_options(&state);em_viewport(width,height);dropdown_layer(&state,item);
   em_options(&state);click(&state,item);em_options(&state);check_seats();assert(em_write_shot((shotdir+"/title-dropdown-"+std::to_string(item)+"-"+std::to_string(width)+".png").c_str(),width,height));state.texdrop=v.drop=-1;
  }
  printf("ENMENU|title-settings|%dx%d|matching panel, confirm and notice at their own height under the wordmark, fixed backdrop, all pages/dropdowns=PASS\n",width,height);
  compact_sound_rows(&state);compact_slot_rows(&state);
 }
 SDL_SetWindowSize(window,1280,720);em_title_mode(1);settings_tabs(&state,"td");language_box(&state,"td");
 em_title_mode(0);state.page=DOPT_PAGE_ADVANCED;em_options(&state);assert(v.h==636&&!v.compact&&!v.title);SDL_SetWindowSize(window,1280,720);em_viewport(1280,720);
}
static void skirmish_dropdown(SK_State *state,const DB_Pack *pack){
 state->net=0;state->popup=0;em_lobby(state,pack);em_viewport(1280,720);
 int legacyX,legacyY;const int colour=sk_row_colour(state,0);em_to_legacy(v.x+190*v.scale,v.y+290*v.scale,&legacyX,&legacyY);em_sk_press(state,legacyX,legacyY);em_sk_release(state,legacyX,legacyY);em_lobby(state,pack);assert(v.drop==-11&&v.popupFirst>=0);
 em_to_legacy(v.x+430*v.scale,v.y+284*v.scale,&legacyX,&legacyY);em_sk_press(state,legacyX,legacyY);em_lobby(state,pack);em_to_legacy(v.x+430*v.scale,v.y+284*v.scale,&legacyX,&legacyY);em_sk_release(state,legacyX,legacyY);assert(v.drop==-1&&sk_row_colour(state,0)==colour);
 puts("ENMENU|skirmish-dropdown|outside click closes without choosing underlying colour=PASS");state->popup=-1;
}

/* THE MATCH DEBRIEF'S TABLE, eight commanders with every page's figures and a timeline,
   shared by Tiberian Dawn's leg and Red Alert's. */
static void debrief_sample(EM_Debrief &d){
 memset(&d,0,sizeof d);d.nrows=8;d.games=2;d.win=1;d.decided=1;d.seconds=1182;d.net=1;snprintf(d.mapname,sizeof d.mapname,"Green Acres");
 d.killed=41;d.razed=12;d.lost=7;d.harvested=9200;d.score=41*2+12*5+9200/10;d.place=1;d.places=8;
 const char *names[8]={"COMMANDER","VEGA","GREINER","MOEBIUS","NIKOOMBA","SEPH","GIDEON","COMPUTER"};
 for(int r=0;r<8;r++){EM_DebriefRow &o=d.row[r];snprintf(o.name,sizeof o.name,"%s",names[r]);o.country=r&1;o.colour=r;o.wins=r<4?2:0;
  o.kills[0]=9+r;o.kills[1]=53-6*r;o.kills[2]=o.kills[3]=-1;o.defeated=r>=4;o.is_local=r==0;
  o.killed=41-5*r;o.razed=12-r;o.units_lost=5+3*r;o.structures_lost=2+r;o.harvested=9200-1100*r;o.score=o.killed*2+o.razed*5+o.harvested/10;
  o.units_built=30-r;o.structures_built=14-r;o.army_peak=26-2*r;o.army_end=r>=4?0:20-2*r;o.structures_end=r>=4?0:12-r;
  o.credits_start=5000;o.credits_end=r>=4?0:1800+300*r;o.spent=o.credits_start+o.harvested-o.credits_end;o.leadership=90-9*r;o.efficiency=20+3*r;}
 d.samples=48;d.sample_ticks=75;
 for(int r=0;r<8;r++)for(int sr=0;sr<EM_SERIES;sr++)for(int k=0;k<48;k++)d.timeline[r][sr][k]=(sr+1)*(k*(60-5*r))/4+(sr==4?3000:0);
}
/* TIBERIAN DAWN'S MATCH DEBRIEF: Red Alert's TOP SCORES layout in Tiberian Dawn's own kit,
   with its pages. The frame is this kit's visuals frame, this machine's row is filled in the
   dark green a selected list row is, the side column names GDI and Nod (never a Red Alert
   country), and under the result line the sum is spelt with tiberium, not ore. Six tabs, one
   active; the Score page's bars measured against the leader; the Units, Structures and
   Economy pages' figures right-aligned under their headings; the Graph page's lines in the
   player colours, this machine's the thickest, its axes drawn, and another series another
   picture. With no row this machine's there is no sum; with no samples there is no line. */
static void td_debrief(){
 assert(!theme.ra);
 static EM_Debrief d;debrief_sample(d);
 em_match_debrief(&d,-1);
 {int buttons=0,tabs=0,active=0;for(const Seat &q:v.seats){if(q.item==EM_DEBRIEF_CONTINUE)buttons++;if(q.item>=EM_DEBRIEF_TAB&&q.item<EM_DEBRIEF_TAB+EM_PAGES){tabs++;if(q.active){active++;assert(q.item==EM_DEBRIEF_TAB);}}}
  assert(buttons==1&&tabs==EM_PAGES&&active==1);}
 assert(v.frame=="visuals-frame"&&arts.count("visuals-frame"));
 auto at=[&](int x,int y){const unsigned char *q=&v.rgba[(y*v.w+x)*4];return unsigned(q[0])<<16|unsigned(q[1])<<8|q[2];};
 auto inks=[&](int y0,unsigned col){int n=0;for(int y=y0;y<y0+18;y++)for(int x=38;x<740;x++)n+=at(x,y)==col;return n;};
 /* the leftmost and rightmost pixel of one ink in a box */
 auto span=[&](int x0,int x1,int y0,unsigned col,int &lo,int &hi){lo=1<<20;hi=-1;for(int y=y0;y<y0+18;y++)for(int x=x0;x<x1;x++)if(at(x,y)==col){lo=std::min(lo,x);hi=std::max(hi,x);}};
 /* a figure right-aligned at `right`: its ink starts where the number's own width says */
 auto cell=[&](const char *where,int right,int y,long long n,unsigned col){const int want=right-textwidth(std::to_string(n));int lo,hi;span(want-20,right+4,y,col,lo,hi);
  if(hi<0||lo<want-2||lo>want+3||hi>right+1){fprintf(stderr,"td debrief %s: figure %lld at y %d ink %d..%d, want from %d to %d\n",where,n,y,lo,hi,want,right);assert(0);}};
 /* this machine's row, in Tiberian Dawn's selected-row green */
 const unsigned rowfill=at(50,210-6+2);
 if(rowfill!=0x273521u){fprintf(stderr,"td debrief: local row fill %06x, want 273521\n",rowfill);assert(0);}
 /* the side column: GDI on the first row, Nod on the second, right-aligned at 320 */
 for(int r=0;r<2;r++){int lo,hi;span(200,330,210+r*36,MUTED,lo,hi);const int want=320-textwidth(r?"Nod":"GDI");
  if(hi<0||lo<want-2||lo>want+3||hi>321){fprintf(stderr,"td debrief: side column row %d ink %d..%d, want from %d (%s)\n",r,lo,hi,want,r?"Nod":"GDI");assert(0);}}
 /* the result line, this machine's numbers, and the sum spelt with tiberium, all inside the frame */
 char line[160];const int kind=em_match_debrief_result(&d,line,sizeof line);
 const int bright=inks(536,TEXT),howto=inks(588,MUTED);int flo,fhi;span(38,760,588,MUTED,flo,fhi);
 const int fwant=38+textwidth("Score = units x2 + structures x5 + tiberium/10");
 if(kind!=1||strcmp(line,"Your score: 1062 - 1st of 8")||bright<=0||howto<=0||fhi<fwant-3||fhi>fwant+1||fhi>=v.w-40){
  fprintf(stderr,"td debrief: [%s] kind=%d score-ink=%d formula-ink=%d formula ends %d, want %d\n",line,kind,bright,howto,fhi,fwant);assert(0);}
 save("debrief-td-8");
 /* THE SCORE PAGE: every commander's bar, 280 pixels for the leader and in proportion
    below, in their own colour */
 d.page=EM_PAGE_SCORE;em_match_debrief(&d,-1);
 {int best=1;for(int r=0;r<8;r++)best=std::max(best,int(d.row[r].score));
  for(int r=0;r<8;r++){const unsigned col=player_colour(r);int n=0;for(int x=340;x<640;x++)n+=at(x,210+r*36+8)==col;const int want=int(280LL*d.row[r].score/best);
   if(n<want-1||n>want+1){fprintf(stderr,"td debrief score page: row %d bar %d px, want %d\n",r,n,want);assert(0);}}
  cell("score",700,210,d.row[0].score,TEXT);save("debrief-td-score");}
 /* THE UNITS, STRUCTURES AND ECONOMY PAGES: the first and the last commander's figures,
    right-aligned under their headings (the beaten in the muted ink) */
 d.page=EM_PAGE_UNITS;em_match_debrief(&d,-1);
 cell("units built",560,210,d.row[0].units_built,TEXT);cell("units destroyed",690,210,d.row[0].killed,TEXT);cell("units lost",800,210,d.row[0].units_lost,TEXT);cell("army peak",960,210,d.row[0].army_peak,TEXT);
 cell("units built, beaten",560,210+7*36,d.row[7].units_built,MUTED);save("debrief-td-units");
 d.page=EM_PAGE_STRUCTURES;em_match_debrief(&d,-1);
 cell("structures built",560,210,d.row[0].structures_built,TEXT);cell("structures destroyed",690,210,d.row[0].razed,TEXT);cell("structures lost",800,210,d.row[0].structures_lost,TEXT);cell("structures standing",960,210,d.row[0].structures_end,TEXT);save("debrief-td-structures");
 d.page=EM_PAGE_ECONOMY;em_match_debrief(&d,-1);
 cell("money at the start",560,210,d.row[0].credits_start,TEXT);cell("tiberium harvested",690,210,d.row[0].harvested,TEXT);cell("spent",800,210,d.row[0].spent,TEXT);cell("held",960,210,d.row[0].credits_end,TEXT);
 {int lo,hi;span(560,700,182,MUTED,lo,hi);const int want=690-textwidth("Tiberium");if(lo<want-2||lo>want+3){fprintf(stderr,"td debrief economy: harvest heading ink from %d, want %d (Tiberium)\n",lo,want);assert(0);}}
 save("debrief-td-economy");
 /* THE GRAPH: the plot's two axes, every commander's line in their colour, this machine's
    the thickest, and another series another picture */
 d.page=EM_PAGE_GRAPH;d.series=0;em_match_debrief(&d,-1);
 int lines[8]={0},axis=0;
 for(int y=230;y<=462;y++){axis+=at(128,y)==MUTED;for(int x=129;x<=952;x++){const unsigned c=at(x,y);for(int r=0;r<8;r++)if(c==player_colour(r))lines[r]++;}}
 {int series=0;for(const Seat &q:v.seats)if(q.item>=EM_DEBRIEF_SERIES&&q.item<EM_DEBRIEF_SERIES+EM_SERIES)series++;assert(series==EM_SERIES);}
 for(int r=1;r<8;r++)if(lines[r]<300||lines[0]<=lines[r]){fprintf(stderr,"td debrief graph: row %d line %d px, this machine's %d\n",r,lines[r],lines[0]);assert(0);}
 if(axis<200){fprintf(stderr,"td debrief graph: y axis %d px\n",axis);assert(0);}
 std::vector<unsigned char> s0=v.rgba;save("debrief-td-graph");
 d.series=4;em_match_debrief(&d,-1);
 {int moved=0;for(int y=230;y<=462;y++)for(int x=129;x<=952;x++)moved+=memcmp(&s0[(y*v.w+x)*4],&v.rgba[(y*v.w+x)*4],3)!=0;
  if(moved<500){fprintf(stderr,"td debrief graph: another series moved %d pixels\n",moved);assert(0);}}
 save("debrief-td-graph-credits");
 d.samples=0;em_match_debrief(&d,-1);
 {int any=0;for(int y=230;y<=462;y++)for(int x=129;x<=952;x++){const unsigned c=at(x,y);for(int r=0;r<8;r++)any+=c==player_colour(r);}assert(any==0);}
 d.samples=48;d.series=0;d.page=EM_PAGE_SUMMARY;
 d.nrows=2;em_match_debrief(&d,-1);const int two=inks(588,MUTED);assert(two==howto);save("debrief-td-2");
 d.nrows=8;d.place=0;em_match_debrief(&d,-1);const int plain=inks(536,TEXT),plainhow=inks(588,MUTED);
 if(plainhow!=0||plain<=0){fprintf(stderr,"td debrief no local row: title=%d formula=%d\n",plain,plainhow);assert(0);}
 save("debrief-td-nolocal");
 d.place=5;d.win=0;d.row[0].defeated=1;em_match_debrief(&d,-1);save("debrief-td-defeat");
 printf("ENMENU|td|debrief|%s|frame=%s|local-row=%06x|sides=GDI,Nod|score-ink=%d|formula=tiberium|formula-ink=%d|formula-end=%d|two-rows-formula-ink=%d|no-local-formula-ink=%d|tabs=%d|score-bars=8|units,structures,economy=figures|graph-lines=8|local-line=%d|axis=%d=PASS\n",
        line,v.frame.c_str(),rowfill,bright,howto,fhi,two,plainhow,(int)EM_PAGES,lines[0],axis);
}
/* ---- RED ALERT'S KIT ----------------------------------------------------------------------
   The same screens drawn from ra3d/enhanced-menu.pack, the Red Alert module's own kit, in the
   same process after Tiberian Dawn's pass (whose pictures are written first and are not
   touched by this). Skipped, and said so, when no Red Alert module is staged. */
static const char *td_words[]={"GDI","Nod","Tiberium","Covert","Command & Conquer"};
static void ra_words(){for(const Seat &q:v.seats)for(const char *w:td_words)if(q.label.find(w)!=std::string::npos){fprintf(stderr,"Tiberian Dawn word in a Red Alert seat: %s\n",q.label.c_str());assert(0);}}
static void ra_art(){for(const Seat &q:v.seats)if(q.kind==BUTTON){std::string id=art_id("button-"+std::to_string(q.w)+"-"+std::to_string(q.h)+"-normal");if(!arts.count(id)){fprintf(stderr,"no art %s\n",id.c_str());assert(0);}}}
/* The frame's own field, just inside the left rail, is red at the title, blue in a mission. */
static void ra_palette(bool blue){const unsigned char *p=&v.rgba[(70*v.w+31)*4];int r=p[0],b=p[2];if(blue?!(b>r):!(r>b)){fprintf(stderr,"palette %s (screen %s, theme %s): field %d,%d,%d\n",blue?"blue":"red",v.frame.c_str(),theme.blue?"blue":"red",p[0],p[1],p[2]);assert(0);}}
static int ra_shots=0;
/* two saved missions, so the pause page's Load, Save and Delete are live */
static int ra_slot_rows(void *,DOPT_SlotRow *out,int max){const char *t[]={"Allied 1 - In the thick of it","Soviet 1 - Lesson in Blood"};int n=0;for(;n<2&&n<max;n++){out[n].slot=n;snprintf(out[n].text,sizeof out[n].text,"%s",t[n]);}return n;}
static void ra_save(const char *name,bool blue){
 check_seats();ra_words();ra_art();ra_palette(blue);
 for(const auto &size:std::vector<std::pair<int,int>>{{1280,720},{2560,1440}}){SDL_SetWindowSize(SDL_GL_GetCurrentWindow(),size.first,size.second);em_viewport(size.first,size.second);
  assert(em_write_shot((shotdir+"/ra-"+name+"-"+std::to_string(size.second)+".png").c_str(),size.first,size.second));ra_shots++;}
 /* (a screen with a list open over it is sounded through the list, not the seats below) */
 if(v.popupFirst<0)sound_seats(SDL_GL_GetCurrentWindow());printf("ENMENU|ra|%s|%s palette, Red Alert art and words%s=PASS\n",name,blue?"blue":"red",v.popupFirst<0?", hover/click":", list open");
}
/* RED ALERT'S START MARKERS on a lobby preview. Each start's frame pixel is written here as
   tools/ra3d/bake-ra-map-previews.py computes it, and is first checked to be the white
   centre of the dot baked into the picture, so the menu's own arithmetic is not taken on
   trust. The frame is then painted here as the menu must paint it: every idle diamond in
   the idle grey, then every held one in its holder's roster colour, each inside a black
   rim, so a held start sharing a cell with an idle one still shows its holder. A start is
   held only with ra_start_holders on, and only on the map in play (never on a candidate in
   the picker). Every diamond pixel on the canvas must match that painting; no pure white,
   which only the baked dots carry, may be left anywhere in the frame; and each start's
   number must be drawn near it. bx, by is the frame's corner on the canvas and k the size
   of a frame pixel there. Answers how many starts were drawn in a holder's colour. */
struct RaStart{int fx,fy;};
static const std::vector<RaStart> ra_isle={{91,104},{28,73},{98,15},{21,16},{59,46},{96,58},{25,100},{61,83}},ra_hoppers={{38,43},{85,81},{88,39},{36,81}},
 ra_equal={{31,29},{88,92},{83,22},{32,93},{32,93}};
static int ra_markers(const SK_State *st,int ix,const std::vector<RaStart> &want,int bx,int by,int k,const char *where){
 const Art &art=arts.at(std::string("preview-")+st->maps[ix].scen);assert(art.w==120&&art.h==120);
 auto at=[&](int x,int y){const unsigned char *q=&v.rgba[(y*v.w+x)*4];return unsigned(q[0])<<16|unsigned(q[1])<<8|q[2];};
 std::vector<int> paint(120*120,-1);int held=0,ink=0;
 for(int pass=0;pass<2;pass++)for(int i=0;i<int(want.size());i++){const int fx=want[i].fx,fy=want[i].fy;
  const int holder=ra_start_holders&&ix==st->sel?sk_start_holder(st,i):-1;if((holder>=0)!=(pass==1))continue;
  const unsigned char *b=&art.pixels[(fy*art.w+fx)*4];assert(b[0]==255&&b[1]==255&&b[2]==255);
  const unsigned fill=holder>=0?player_colour(sk_row_colour(st,holder)):0x545454u;held+=holder>=0;
  for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++){const int d=abs(dx)+abs(dy);if(d>3||fx+dx<0||fy+dy<0||fx+dx>=120||fy+dy>=120)continue;paint[(fy+dy)*120+fx+dx]=int(d==3?0u:fill);}
 }
 for(int fy=0;fy<120;fy++)for(int fx=0;fx<120;fx++){if(paint[fy*120+fx]<0)continue;const unsigned w=unsigned(paint[fy*120+fx]);
  for(int yy=0;yy<k;yy++)for(int xx=0;xx<k;xx++){const unsigned c=at(bx+fx*k+xx,by+fy*k+yy);
   if(c!=w){fprintf(stderr,"%s: frame pixel %d,%d is %06x, want %06x\n",where,fx,fy,c,w);assert(0);}}}
 for(int i=0;i<int(want.size());i++){const int fx=want[i].fx,fy=want[i].fy;
  int beside=0;for(int y=std::max(0,fy-14);y<std::min(120,fy+15);y++)for(int x=std::max(0,fx-14);x<std::min(120,fx+15);x++)beside+=at(bx+x*k,by+y*k)==TEXT;
  if(!beside){fprintf(stderr,"%s: start %d has no number beside it\n",where,i+1);assert(0);}ink+=beside;
 }
 int white=0;for(int y=0;y<120*k;y++)for(int x=0;x<120*k;x++)white+=at(bx+x,by+y)==0xffffffu;
 if(white){fprintf(stderr,"%s: %d pixels of the baked white dots are still showing\n",where,white);assert(0);}
 printf("ENMENU|ra|start-markers|%s|%d starts, %d in a holder's colour (holder colours %s), black rims, no baked white left, numbers drawn (%d ink pixels)=PASS\n",where,int(want.size()),held,ra_start_holders?"on":"off",ink);
 return held;
}
static void ra_pass(){
 FILE *f=fopen("ra3d/enhanced-menu.pack","rb");if(!f){puts("ENMENU|ra|SKIP|no Red Alert module staged (ra3d/enhanced-menu.pack)");return;}fclose(f);
 /* a fresh kit: the pack, the textures made from the last one, every screen */
 em_set_red_alert("OpenCNC3D gate","OpenCNC3D is an unofficial fan project, not affiliated with or endorsed by Electronic Arts.\nCommand & Conquer: Red Alert is a trademark of Electronic Arts Inc. Created by freschism");
 texture=bgtexture=logotexture=legaltexture=bgtexture_lo=logotexture_lo=0;em_reset();assert(em_available()&&em_red_alert());
 char err[256];DB_Pack *p=db_pack_load("ra3d/dosmenu.pack",err,sizeof err);assert(p);em_menu_overlay(p,-1000,-1000);
 DM_State m;dm_state_init(&m);m.red_alert=1;m.version="RA3D";m.copyright="";m.special_missing=0;m.selected=DM_START;em_main(&m);em_viewport(1280,720);
 {int seen=0;for(const Seat &q:v.seats){if(q.item==DM_SPECIAL){seen++;assert(!q.disabled&&q.label=="Mission Disks");}assert(textwidth(q.label)<=q.w-16);}assert(seen==1);assert(em_main_next(&m,DM_START,1)==DM_SPECIAL);
  m.special_missing=1;em_main(&m);int missing=0;for(const Seat&q:v.seats)if(q.item==DM_SPECIAL){missing++;assert(q.disabled&&q.label=="Mission Disks");}assert(missing==1);assert(em_main_next(&m,DM_START,1)==DM_USERMAPS);m.special_missing=0;em_main(&m);}
 ra_save("main",false);
 /* RA3D cleanup: a legal string must not produce any main-menu footer pixels. */
 for(const auto &size:std::vector<std::pair<int,int>>{{1280,720},{2560,1440}}){int w=size.first,h=size.second;SDL_SetWindowSize(SDL_GL_GetCurrentWindow(),w,h);em_main(&m);em_viewport(w,h);
  std::vector<unsigned char> a(w*h*4),b(a.size());em_draw(w,h,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,a.data());
  std::string keep=theme.legal;theme.legal="";em_draw(w,h,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,b.data());theme.legal=keep;
  int changed=0;for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(memcmp(&a[(y*w+x)*4],&b[(y*w+x)*4],4)){changed++;assert(h-1-y>=v.y+v.h*v.scale);}
  assert(changed==0);printf("ENMENU|ra|legal|%dx%d|changed=%d (footer absent)\n",w,h,changed);}
 /* the two emblems turn inside their own seats */
 {SDL_SetWindowSize(SDL_GL_GetCurrentWindow(),1280,720);em_main(&m);em_viewport(1280,720);std::vector<unsigned char> a(1280*720*4),b(a.size());
  em_draw(1280,720,1);glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,a.data());faction_angle_override=1;em_draw(1280,720,1);glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,b.data());faction_angle_override=-1;
  int c[2]={};for(int yy=0;yy<720;yy++)for(int xx=0;xx<1280;xx++)if(memcmp(&a[(yy*1280+xx)*4],&b[(yy*1280+xx)*4],4)){int y=719-yy;bool l=xx>=77&&xx<277&&y>=484&&y<684,r=xx>=1003&&xx<1203&&y>=484&&y<684;assert(l||r);c[r?1:0]++;}
  assert(c[0]>1000&&c[1]>1000);printf("ENMENU|ra|main-spin|Allied=%d|Soviet=%d|outside-seats=0\n",c[0],c[1]);}
 for(int i=0;i<2;i++){em_choice(i,1,-1);ra_save(i?"intro":"exit",false);}
 /* the side choice: both plates with an emblem, the sides, the difficulty, Start */
 em_side_choice(1,2,-1);em_viewport(1280,720);
 {int plates=0;for(const Seat &q:v.seats)if(q.kind==PLATE){plates++;assert(q.disabled&&q.badge>=0);}assert(plates==2);
  int x,y,w,h;assert(em_item_rect(EM_SIDE_SOVIET,&x,&y,&w,&h)&&em_pick(x+w/2,y+h/2)==EM_SIDE_SOVIET);assert(em_item_rect(EM_SIDE_HARD,&x,&y,&w,&h)&&em_pick(x+w/2,y+h/2)==EM_SIDE_HARD);
  assert(em_item_rect(EM_SIDE_START,&x,&y,&w,&h)&&em_pick(x+w/2,y+h/2)==EM_SIDE_START);
  for(const Seat &q:v.seats){if(q.item==EM_SIDE_SOVIET)assert(q.active);if(q.item==EM_SIDE_ALLIES)assert(!q.active);if(q.item==EM_SIDE_HARD)assert(q.active);}}
 em_side_choice(0,1,-1);ra_save("side",false);
 /* the dialogs, at the title (red) and in a mission (blue) */
 DOPT_State o={};dopt_settings_init(&o.set);dopt_open(&o,p);dopt_bind(&o,nullptr,settings_apply);dopt_bind_slots(&o,ra_slot_rows);o.scenario="SCG01EA";dopt_set_n64_unavailable(&o,1);
 o.vis.mode=o.advTab=DOPT_MODE_ENHANCED;o.vis.enh_tex_ok=1;
 em_title_mode(0);em_options(&o);ra_save("pause",true);
 /* Load, Save and Delete answer with the notice until Red Alert's saves have their own index,
    and the notice's OK goes back to the pause page */
 em_viewport(1280,720);click(&o,DOPT_SAVE);assert(o.page==DOPT_PAGE_NOTICE&&o.nback&&!strcmp(o.ncaption,"NOTICE"));em_options(&o);ra_save("notice-saves",true);
 em_viewport(1280,720);click(&o,0);assert(o.page==DOPT_PAGE_OPTIONS&&o.selected==DOPT_SAVE);
 em_set_route_ready(EM_ROUTE_SAVES,1);em_options(&o);em_viewport(1280,720);click(&o,DOPT_LOAD);assert(o.page==DOPT_PAGE_SLOTS);em_set_route_ready(EM_ROUTE_SAVES,0);
 em_options(&o);ra_save("load",true);o.page=DOPT_PAGE_OPTIONS;
 puts("ENMENU|ra|saves|notice until the split, OK back to the pause page, the slot list once ready=PASS");
 const struct{int page;const char *name;}pages[]={{DOPT_PAGE_CONTROLS,"controls"},{DOPT_PAGE_GAMEPLAY,"gameplay"},{DOPT_PAGE_CHEATS,"cheats"},{DOPT_PAGE_CONFIRM,"confirm"}};
 for(auto &pg:pages){o.page=pg.page;em_options(&o);ra_save(pg.name,true);}
 /* a match's box asks to surrender, and once surrendered asks to leave */
 dopt_set_match(&o,1,0);o.page=DOPT_PAGE_CONFIRM;em_options(&o);ra_save("confirm-surrender",true);
 dopt_set_match(&o,1,1);o.page=DOPT_PAGE_CONFIRM;em_options(&o);ra_save("confirm-leave",true);dopt_set_match(&o,0,0);
 DOPT_Track tracks[12]={};const char *names[]={"Bigfoot","Crush","Face the Enemy 1","Face the Enemy 2","Hell March","Run for Your Life","Smash","Trenches","Workmen","Await","Dense","Fogger"};
 for(int i=0;i<12;i++){tracks[i].base="fixture";tracks[i].fullname=names[i];tracks[i].index=i;}
 o.page=DOPT_PAGE_SOUND;dopt_set_tracks(&o,tracks,12,4);em_options(&o);{int icons=0;for(const Seat &q:v.seats)if(!q.icon.empty()){icons++;assert(arts.count(art_id(q.icon)));}assert(icons==2);}ra_save("sound",true);dopt_set_tracks(&o,nullptr,0,-1);
 /* the mission briefing stands on its side's paper: ALIPAPER for an Allied mission, SOVPAPER for a Soviet one */
 std::vector<unsigned char> paper[2];
 for(const char *scen:{"SCG01EA","SCU01EA"}){o.page=DOPT_PAGE_RESTATE;o.scenario=scen;em_set_mission_side(scen[2]=='U');strcpy(o.brief,"Rescue Einstein from the Headquarters inside this Soviet complex. Once found, evacuate him via the helicopter at the signal flare.");em_options(&o);
  for(int y=400;y<450;y++)paper[scen[2]=='U'].insert(paper[scen[2]=='U'].end(),&v.rgba[(y*v.w+60)*4],&v.rgba[(y*v.w+600)*4]);
  ra_save(scen[2]=='U'?"briefing-soviet":"briefing-allied",true);}
 assert(paper[0]!=paper[1]);em_set_mission_side(0);
 o.page=DOPT_PAGE_VISUALS;o.vis.mode=DOPT_MODE_ENHANCED;
 for(int i=0;i<5;i++){v.section=i;em_options(&o);if(i==0)v.section=0;
  for(const Seat &q:v.seats){if(q.item==DOPT_VE_TIB3D&&q.kind==CHECK)assert(q.label=="3D Ore"&&!q.disabled);
   if(q.kind==CHECK&&(q.item==DOPT_VE_FOLIAGE||q.item==DOPT_VE_ROCKS||q.item==DOPT_VE_PATCHES))assert(!q.disabled&&q.value==o.vis.prof[o.advTab].elem[q.item]);
   if(q.action==MODE&&q.arg==1)assert(q.disabled);}
  ra_save(("visuals-"+std::to_string(i)).c_str(),true);}
 /* RA's implemented features must be clickable in the actual Enhanced seat geometry. */
 for(int item:{DOPT_VE_TREES,DOPT_VE_GRASS,DOPT_VE_TIB3D,DOPT_VE_FOLIAGE,DOPT_VE_ROCKS,DOPT_VE_PATCHES,DOPT_VE_NEWHUD}){
  v.section=item==DOPT_VE_NEWHUD?3:(item==DOPT_VE_FOLIAGE||item==DOPT_VE_ROCKS||item==DOPT_VE_PATCHES)?4:1;em_options(&o);em_viewport(1280,720);
  int before=o.vis.prof[DOPT_MODE_ENHANCED].elem[item];click(&o,item);assert(o.vis.prof[DOPT_MODE_ENHANCED].elem[item]!=before);
  em_options(&o);click(&o,item);assert(o.vis.prof[DOPT_MODE_ENHANCED].elem[item]==before);
 }
 /* the arrows step over the rows Red Alert greys */
 v.section=1;em_options(&o);o.selected=DOPT_VE_INFSET;for(int k=0;k<6;k++){em_opt_key(&o,DOPT_KEY_DOWN);assert(!ra_unavailable(o.selected));}
 puts("ENMENU|ra|visuals|N64 and the rows Red Alert does not draw greyed, stepped over by the arrows=PASS");
 /* the resolution list open over the Display page */
 o.page=DOPT_PAGE_VISUALS;o.vis.nres=7;{const int rw[]={1280,1600,1920,1920,2560,2560,3840},rh[]={720,900,1080,1200,1440,1600,2160};for(int i=0;i<7;i++){o.vis.res_w[i]=rw[i];o.vis.res_h[i]=rh[i];o.vis.res_fit[i]=1;}}
 v.section=0;em_options(&o);em_viewport(1280,720);click(&o,DOPT_VE_RESOLUTION);em_options(&o);assert(v.popupFirst>=0);ra_save("visuals-resolution",true);v.drop=-1;o.texdrop=-1;
 /* the same dialogs opened from the title are red, at the main menu's height */
 em_title_mode(1);o.page=DOPT_PAGE_VISUALS;v.section=0;em_options(&o);assert(v.compact&&v.h==564);ra_save("title-visuals",false);
 {DOPT_State t=o;settings_tabs(&t,"ra");t.page=DOPT_PAGE_ADVANCED;v.section=6;em_options(&t);ra_save("title-settings-audio",false);v.section=0;}
 dopt_open_slots(&o,p,DOPT_SL_LOAD);em_options(&o);ra_save("title-load",false);em_title_mode(0);
 dopt_open_notice(&o,p,"NOTICE",em_route_notice(EM_ROUTE_USERMAPS));em_title_mode(1);em_options(&o);ra_save("notice-usermaps",false);em_title_mode(0);
 /* skirmish and the network lobby: Red Alert's sides, ore, ten tech levels, its colours */
 /* (the playable rectangle's corner and the start cells, as the preview baker lists them) */
 const SK_Map maps[]={{"SCM06EA","Isle of Fury (6-8)","TEMPERATE",96,96,8,0,14,17,8,{87,36,92,31,61,91,34,63},{100,75,29,30,54,63,97,83}},
  {"SCM11EA","Island Hoppers (2-6)","SNOW",96,96,6,0,11,19,4,{41,79,81,40},{53,84,50,84}}};
 SK_State sst;sk_init(&sst,maps,2,nullptr);sk_set_red_alert(&sst);em_lobby(&sst,p);
 for(int i=0;i<8;i++)assert(sk_row_colour(&sst,i)==sk_row_country(&sst,i));ai_difficulty_rows(&sst,p,"ra");ra_save("skirmish",false);
 /* Red Alert's Game Mode dropdown: Free for All, with Gun Game and Rally greyed and the reason */
 {v.drop=-1;em_lobby(&sst,p);clicksk(&sst,EM_SK_MODE_DROP);em_lobby(&sst,p);int greyed=0;for(const Seat &q:v.seats)if(q.action==SKMODE){if(q.arg==SK_MODE_FFA)assert(!q.disabled&&q.active);else{assert(q.disabled);greyed++;}}
  assert(greyed==2&&v.popupHint=="Dawn3D only for now"&&textwidth(v.popupHint)<=v.popupW-32);ra_save("mode-open",false);
  assert(!sk_set_mode(&sst,SK_MODE_GUN_GAME)&&!sk_set_mode(&sst,SK_MODE_RALLY)&&sk_mode(&sst)==SK_MODE_FFA);em_sk_drop_close();em_lobby(&sst,p);}
 /* the starts on the skirmish preview, with the player on start 3, the computer on start 1
    and the rest Random: starts 3 and 1 in those seats' colours, the rest idle, every one
    numbered. Then the same picture with holder colours off, every start idle, and Equal
    Opportunity, whose starts 4 and 5 share a cell, with a seat on 4: start 5 is held by
    that seat too, so both diamonds are its colour and no other seat may take 5. The picks
    are put back afterwards so every screen below is drawn as before. */
 int ra_starts[SK_ROSTER_ROWS];memcpy(ra_starts,sst.start,sizeof ra_starts);
 if(!ra_start_holders){fputs("Red Alert's start markers are not drawn in the colour of the seat holding them\n",stderr);assert(0);}
 v.sksection=0;sst.start[0]=2;sst.start[1]=0;em_lobby(&sst,p);assert(sk_start_holder(&sst,2)==0&&sk_start_holder(&sst,0)==1);assert(ra_markers(&sst,0,ra_isle,698,202,2,"skirmish")==2);
 ra_start_holders=false;em_lobby(&sst,p);assert(ra_markers(&sst,0,ra_isle,698,202,2,"skirmish-idle")==0);ra_start_holders=true;
 {const SK_Map equal[]={{"SCM03EA","Equal Opportunity (2)","SNOW",71,71,5,0,28,26,5,{46,80,77,47,47},{43,80,39,81,81}}};
  SK_State e;sk_init(&e,equal,1,nullptr);sk_set_red_alert(&e);for(int i=0;i<SK_ROSTER_ROWS;i++)e.start[i]=-1;e.start[0]=3;
  em_lobby(&e,p);assert(sk_start_holder(&e,3)==0&&sk_start_holder(&e,4)==0&&sk_start_holder(&e,2)<0);assert(ra_markers(&e,0,ra_equal,698,202,2,"skirmish-shared-cell")==2);}
 memcpy(sst.start,ra_starts,sizeof ra_starts);em_lobby(&sst,p);
 v.sksection=1;em_lobby(&sst,p);{bool ore=false,shroud=false,ctf=false,computers=false,super=false;for(const Seat &q:v.seats){if(q.item==SK_I_TIBERIUM&&q.label=="Ore Regrowth Rate   1x")ore=true;if(q.label=="Shroud Regrows")shroud=true;if(q.label=="Capture The Flag")ctf=true;if(q.label=="New AI default")computers=true;if(q.label=="Superweapons")super=true;if(q.item==SK_I_BUILD)assert(q.max==10);}assert(ore&&shroud&&ctf&&computers&&!super);}ra_save("skirmish-rules",false);
 /* Red Alert's own rules pressed as a hand presses them: Shroud Regrows and Capture The Flag
    ticked, the computers' slider to its right end (Hard) and its left (Easy); the lobby's
    answer carries all three */
 {assert(sst.shroud==0&&sst.ctf==0&&sst.ai_difficulty==1);em_viewport(1280,720);clicksk(&sst,-50-SK_RA_SHROUD);em_lobby(&sst,p);clicksk(&sst,-50-SK_RA_CTF);em_lobby(&sst,p);assert(sst.shroud==1&&sst.ctf==1);
  int x,y,w,h,lx,ly;assert(em_item_rect(-50-SK_RA_DIFFICULTY,&x,&y,&w,&h));em_to_legacy(x+w-2,y+h/2,&lx,&ly);em_sk_press(&sst,lx,ly);em_sk_release(&sst,lx,ly);assert(sst.ai_difficulty==3);
  em_lobby(&sst,p);ra_save("skirmish-rules-set",false);em_viewport(1280,720);assert(em_item_rect(-50-SK_RA_DIFFICULTY,&x,&y,&w,&h));em_to_legacy(x+1,y+h/2,&lx,&ly);em_sk_press(&sst,lx,ly);em_sk_release(&sst,lx,ly);assert(sst.ai_difficulty==0);
  SK_Lobby answer;sk_result(&sst,&answer);assert(answer.shroud==1&&answer.ctf==1&&answer.ai_difficulty==0);
  sst.shroud=sst.ctf=0;sst.ai_difficulty=1;}
 puts("ENMENU|ra|rules|Shroud Regrows, Capture The Flag and the computers' Easy / Normal / Hard / Expert pressed and in the lobby's answer=PASS");
 v.sksection=0;
 sst.popup=1;em_lobby(&sst,p);for(const Seat &q:v.seats)if(q.kind==COLOUR)assert(q.value==int(player_colour(q.item-SK_I_POP_C1)));ra_save("player-setup",false);
 v.drop=-11;em_lobby(&sst,p);assert(v.popupFirst>=0);
 /* the country list: all eight, the Allies first; England chosen for seat 2 makes it Allied */
 {int n=0,england=-1;for(int i=v.popupFirst;i<int(v.seats.size());i++){n++;if(v.seats[i].label=="England")england=i;}assert(n==8&&england>=0);
  ra_save("player-setup-country",false);em_viewport(1280,720);const Seat e=v.seats[england];int lx,ly;em_to_legacy(v.x+(e.x+e.w/2)*v.scale,v.y+(e.y+e.h/2)*v.scale,&lx,&ly);em_sk_press(&sst,lx,ly);em_sk_release(&sst,lx,ly);
  assert(sk_row_country(&sst,1)==3&&sk_row_house(&sst,1)==0&&v.drop==-1);}
 /* the line under the seat speaks Red Alert too: a team pressed names the seat's country, a
    seat type pressed splits the computers into Allied and Soviet, and neither says GDI or Nod */
 {const int team=sst.team[1];assert(sk_enh_action(&sst,SK_I_POP_T1)==SK_ACT_NONE&&sst.status&&strstr(sst.status,"England"));
  for(const char *w:td_words)assert(!strstr(sst.status,w));em_lobby(&sst,p);ra_save("player-setup-status",false);
  assert(sk_enh_action(&sst,SK_I_POP_BOT)==SK_ACT_NONE&&sst.status&&strstr(sst.status,"Allied")&&strstr(sst.status,"Soviet"));
  for(const char *w:td_words)assert(!strstr(sst.status,w));sst.team[1]=team;sst.status=nullptr;}
 puts("ENMENU|ra|countries|eight countries in the seat's list, the choice sets its side, each seat its own colour, the seat's line names its country=PASS");
 sst.popup=-1;
 em_lobby(&sst,p);em_viewport(1280,720);clicksk(&sst,SK_I_CHANGEMAP);em_lobby(&sst,p);ra_save("maps",false);
 /* the picker: the map in play shows who holds its starts, and a candidate shows every
    start idle */
 sst.start[0]=2;sst.start[1]=0;em_lobby(&sst,p);assert(sst.pick_sel==0);assert(ra_markers(&sst,0,ra_isle,698,204,2,"picker")==2);
 sk_enh_pick_row(&sst,1);em_lobby(&sst,p);assert(sst.pick_sel==1);assert(ra_markers(&sst,1,ra_hoppers,698,204,2,"picker-candidate")==0);
 sk_enh_pick_row(&sst,0);memcpy(sst.start,ra_starts,sizeof ra_starts);em_lobby(&sst,p);em_viewport(1280,720);clicksk(&sst,SK_I_PICK_CANCEL);
 sst.net=1;sst.net_seat=0;sst.net_seats=8;sst.net_humans=1;sst.net_taken[0]=1;strcpy(sst.name[0],"Commander");sst.chat_count=0;sk_chat_push(&sst,-1,"Waiting for other commanders to join.");em_lobby(&sst,p);ra_save("lobby",false);
 /* the room's preview, a pixel per frame pixel */
 sst.start[0]=2;sst.start[1]=0;em_lobby(&sst,p);assert(ra_markers(&sst,0,ra_isle,600,204,1,"lobby")==2);memcpy(sst.start,ra_starts,sizeof ra_starts);em_lobby(&sst,p);
 v.sksection=1;em_lobby(&sst,p);ra_save("lobby-rules",false);
 /* a joiner reads the host's rules and cannot move them */
 sst.net=2;sst.net_seat=1;em_lobby(&sst,p);{int ra=0;for(const Seat &q:v.seats)if(q.action==RACHECK||q.action==RARANGE){ra++;assert(q.disabled);}assert(ra==3);}
 sk_enh_ra_rule(&sst,SK_RA_SHROUD,1);sk_enh_ra_rule(&sst,SK_RA_DIFFICULTY,2);assert(sst.shroud==0&&sst.ai_difficulty==1);ra_save("lobby-rules-joiner",false);sst.net=1;sst.net_seat=0;
 puts("ENMENU|ra|room-rules|a joiner's Red Alert rules are drawn read only and refuse a change=PASS");
 v.sksection=0;
 MP_State b;mp_init(&b);em_browser(&b);ra_save("host",false);b.tab=MP_TAB_JOIN;em_browser(&b);ra_save("join",false);
 b.prompt=1;em_browser(&b);ra_save("player-name",false);b.prompt=0;b.page=MP_PAGE_WAIT;em_browser(&b);ra_save("wait-room",false);
 /* Special Ops: Counterstrike and Aftermath tabs, Allied before Soviet, plates; User Maps empty */
 const DO_Mission ops[]={{"","COUNTERSTRIKE",0,1},{"SCG20EA","Sarin Gas 1: Crackdown",0,0},{"SCU20EA","Soviet Soldier Volkov and Chitzkoi",1,0},{"","AFTERMATH",0,1},{"SCG40EA","Harbor Reclamation",0,0},{"SCU40EA","Shock Therapy",1,0}};
 DO_State dst;do_state_init(&dst,ops,6);em_ops(&dst);{int tabs=0,plates=0;for(const Seat &q:v.seats){if(q.action==OPSFILTER){tabs++;assert(q.label=="Counterstrike"||q.label=="Aftermath");}if(q.kind==PLATE)plates++;}assert(tabs==2&&plates==1);}ra_save("special-ops",false);
 v.opsfilter=1;em_ops(&dst);ra_save("special-ops-aftermath",false);
 const DO_Mission none[]={{"","COUNTERSTRIKE",0,1},{"","AFTERMATH",0,1}};DO_State noops;do_state_init(&noops,none,2);em_ops(&noops);ra_save("special-ops-empty",false);
 /* the match debrief: the session's table after a third match, this machine's commander
    first and the beaten computers in their own colours */
 {EM_Debrief d;memset(&d,0,sizeof d);d.nrows=4;d.games=3;d.win=1;d.decided=1;d.seconds=754;snprintf(d.mapname,sizeof d.mapname,"Coastal Influence (4-6)");
  d.killed=17;d.razed=6;d.lost=9;d.harvested=12400;d.score=17*2+6*5+12400/10;d.place=1;d.places=4;
  const char *names[4]={"Commander","COMPUTER","COMPUTER","COMPUTER"};const int country[4]={1,2,3,4},colour[4]={1,2,3,4},wins[4]={2,1,0,1};
  const int kills[4][4]={{14,9,23,-1},{8,17,11,-1},{-1,5,7,-1},{3,12,4,-1}};
  for(int r=0;r<4;r++){snprintf(d.row[r].name,sizeof d.row[r].name,"%s",names[r]);d.row[r].country=country[r];d.row[r].colour=colour[r];d.row[r].wins=wins[r];for(int k=0;k<4;k++)d.row[r].kills[k]=kills[r][k];d.row[r].defeated=r>0;d.row[r].is_local=r==0;}
  em_match_debrief(&d,-1);{int buttons=0;for(const Seat &q:v.seats)if(q.item==EM_DEBRIEF_CONTINUE)buttons++;assert(buttons==1);}
  /* THE PLAYER'S OWN SCORE, on the result line under the table: "Your score: 1304 - 1st of
     4" in the bright ink, and how it is added up two lines below in the muted one. With no
     row this machine's, the plain title in the bright ink and no sum under it. */
  auto inks=[&](int y0,unsigned col){int n=0;for(int y=y0;y<y0+18;y++)for(int x=38;x<740;x++){const unsigned char *q=&v.rgba[(y*v.w+x)*4];n+=(unsigned(q[0])<<16|unsigned(q[1])<<8|q[2])==col;}return n;};
  {char line[160],plainline[160],third[160];const int kind=em_match_debrief_result(&d,line,sizeof line);
   const int bright=inks(536,TEXT),howto=inks(588,MUTED);
   if(kind!=1||strcmp(line,"Your score: 1304 - 1st of 4")||bright<=0||howto<=0){fprintf(stderr,"debrief score line: [%s] kind=%d bright=%d howto=%d\n",line,kind,bright,howto);assert(0);}
   ra_save("debrief",false);
   d.place=0;const int plainkind=em_match_debrief_result(&d,plainline,sizeof plainline);em_match_debrief(&d,-1);const int plain=inks(536,TEXT),plainhow=inks(588,MUTED);
   if(plainkind!=2||strcmp(plainline,"Final standings")||plain<=0||plain>=bright||plainhow!=0){fprintf(stderr,"debrief no local row: [%s] title=%d (score line %d) howto=%d\n",plainline,plain,bright,plainhow);assert(0);}
   ra_save("debrief-nolocal",false);
   /* the teens take th, whatever their last digit: 11th, 12th, 13th, then 21st again */
   d.place=12;d.places=12;em_match_debrief_result(&d,third,sizeof third);
   if(strcmp(third,"Your score: 1304 - 12th of 12")){fprintf(stderr,"debrief ordinal: [%s]\n",third);assert(0);}
   d.place=1;d.places=4;
   printf("ENMENU|ra|debrief-score|%s|score-ink=%d|formula-ink=%d|no-local=%s|no-local-title-ink=%d|no-local-formula-ink=%d=PASS\n",line,bright,howto,plainline,plain,plainhow);}
  d.win=0;d.row[0].defeated=1;d.row[1].defeated=0;d.place=4;em_match_debrief(&d,-1);ra_save("debrief-defeat",false);
  /* AND ITS PAGES, in Red Alert's kit: the tabs and the graph's series buttons from Red
     Alert's own button art, Red Alert's words (ore, never tiberium) */
  {static EM_Debrief e;debrief_sample(e);const char *pages[EM_PAGES]={"debrief-summary","debrief-score","debrief-units","debrief-structures","debrief-economy","debrief-graph"};
   for(int pg=0;pg<EM_PAGES;pg++){e.page=pg;em_match_debrief(&e,-1);int tabs=0;for(const Seat &q:v.seats)if(q.item>=EM_DEBRIEF_TAB&&q.item<EM_DEBRIEF_TAB+EM_PAGES)tabs++;assert(tabs==EM_PAGES);ra_save(pages[pg],false);}
   printf("ENMENU|ra|debrief-pages|%d pages in Red Alert's kit=PASS\n",(int)EM_PAGES);}}
 DO_State empty;do_state_init(&empty,nullptr,0);em_ops(&empty);ra_save("user-maps",false);
 /* the Database, a mission tab: blue, five categories with Ships */
 const char *roster[]={"Construction Yard","Power Plant","Tesla Coil"};EM_Database db={};db.count=3;db.house=2;db.names=roster;db.name=roster[2];db.code="TSLA";db.brief="";db.armour="Heavy";db.hp=400;db.cost=1500;db.damage=db.speed=db.rof=db.range256=-1;db.model_available=1;db.selected=2;
 em_database(&db);{int cats=0;for(const Seat &q:v.seats)if(q.item>=1000&&q.item<1010){cats++;}assert(cats==5);}ra_save("database",true);
 db_pack_free(p);printf("ENMENU|ra|PASS|%d Red Alert pictures: main, side, pause, notices, every options page, lobby, browser, ops, database\n",ra_shots);
}
static void importer_views(const char *prefix){
 DMS_MapImportState st={};st.selected_row=-1;st.pressed=MI_NONE;
 strcpy(st.path,"/Maps/Commander/Downloads/Original fan maps");strcpy(st.location,st.path);
 strcpy(st.message,"Choose an original C&C or Red Alert map. Folders open with Enter or a double-click.");
 st.total=27;st.row_count=MI_VISIBLE_ROWS;
 for(int i=0;i<MI_VISIBLE_ROWS;i++){snprintf(st.rows[i],sizeof st.rows[i],i<2?"Map collection %d":"A very long original fan mission filename %d.ini",i+1);st.row_directory[i]=i<2;}
 auto witness=[&](const char *tag){em_import_map(&st);check_seats();
  for(const Seat &q:v.seats)if(q.kind==BUTTON)assert(textwidth(q.label)<=q.w-16);
  for(int h:{720,1440}){SDL_SetWindowSize(SDL_GL_GetCurrentWindow(),h*16/9,h);SDL_PumpEvents();em_viewport(h*16/9,h);for(const Seat &q:v.seats){int x,y,w,hh;assert(em_item_rect(q.item,&x,&y,&w,&hh));assert(em_pick(x+w/2,y+hh/2)==(q.disabled?-1:q.item));}
   char name[160];snprintf(name,sizeof name,"/%s-import-%s-%dp.png",prefix,tag,h);assert(em_write_shot((shotdir+name).c_str(),h*16/9,h));}
 };
 witness("browser");st.focus=1;strcpy(st.location,"/Maps/Commander/Documents/A folder path long enough to exercise clipping without painting over the Go control/Maps");witness("location");st.focus=0;
 st.selected_row=3;strcpy(st.selected_file,st.rows[3]);st.busy=1;strcpy(st.message,"Reading map and building preview...");witness("inspect");
 st.busy=0;st.ready=1;strcpy(st.info.name,"Original fan mission - very long map title to check fitting");strcpy(st.info.game,theme.ra?"Red Alert":"Tiberian Dawn");strcpy(st.info.kind,"Multiplayer");strcpy(st.info.theater,"TEMPERATE");st.info.width=126;st.info.height=94;st.info.starts=8;st.info.preview_w=128;st.info.preview_h=96;
 for(int y=0;y<96;y++)for(int x=0;x<128;x++){unsigned char *p=&st.info.preview[(y*128+x)*4];bool water=x<20+(y/10)%3*6;p[0]=water?40:90+(x+y)%16;p[1]=water?75:108+(x+y)%16;p[2]=water?100:55;p[3]=255;}
 strcpy(st.message,"Multiplayer map detected. Import prepares its terrain and Auto Heightmap for Skirmish and Multiplayer.");witness("ready");
 st.pressed=MI_IMPORT;witness("pressed");st.pressed=MI_NONE;st.busy=st.importing=1;st.ready=0;st.progress=47;strcpy(st.message,"Generating Auto Heightmap from the map's original terrain...");witness("progress");
 st.busy=0;st.completed=1;st.progress=100;strcpy(st.message,"Import complete. Your multiplayer map is available in Skirmish and Multiplayer.");witness("complete");
 st.completed=st.importing=st.ready=0;st.info={};strcpy(st.message,"The map's terrain file is missing. Keep the original INI and matching BIN together, then choose the map again.");witness("error");
 DO_State list;do_state_init(&list,nullptr,0);list.user_maps=1;em_ops(&list);check_seats();em_viewport(1280,720);
 int x,y,w,h,lx,ly;assert(em_item_rect(DO_HIT_IMPORT,&x,&y,&w,&h));em_to_legacy(x+w/2,y+h/2,&lx,&ly);assert(em_ops_hit(&list,lx,ly)==DO_HIT_IMPORT);
 assert(em_write_shot((shotdir+"/"+prefix+"-user-maps-empty.png").c_str(),1280,720));
 assert(do_hit_test(&list,DO_IMPORT_X+DO_IMPORT_W/2,DO_BTN_Y+DO_BTN_H/2)==DO_HIT_IMPORT);
 list.user_maps=0;assert(do_hit_test(&list,DO_IMPORT_X+DO_IMPORT_W/2,DO_BTN_Y+DO_BTN_H/2)!=DO_HIT_IMPORT);
 printf("ENMENU|%s|importer|8 states, 720p/1440p seats, clipped long labels, explicit User Maps import capability=PASS\n",prefix);
}
static const char* german_menu_text(const char* t){return cnc_menu_language_text("de",t);}
static void german_menu(const DB_Pack* pack,const char* game){
 EM_LangHooks hooks={};hooks.translate=german_menu_text;em_set_language_hooks(&hooks);
 DM_State state;dm_state_init(&state);em_main(&state);em_viewport(1280,720);check_seats();
 int translated=0;for(const Seat& q:v.seats){assert(textwidth(q.label)<=q.w-16);if(q.item==DM_START){assert(q.label=="Neues Spiel");translated++;}}
 assert(translated==1);int x,y;assert(em_seat_point("Start New Game",&x,&y));assert(em_seat_point("Neues Spiel",&x,&y));
 assert(em_write_shot((shotdir+"/"+game+"-german-main.png").c_str(),1280,720));
 DOPT_State opts={};dopt_open(&opts,pack);em_options(&opts);em_viewport(1280,720);check_seats();
 assert(em_seat_point("Resume Mission",&x,&y));for(const Seat& q:v.seats){if(textwidth(q.label)>q.w-16)fprintf(stderr,"German overflow: %s (%d > %d)\n",q.label.c_str(),textwidth(q.label),q.w-16);assert(textwidth(q.label)<=q.w-16);}
 assert(em_write_shot((shotdir+"/"+game+"-german-pause.png").c_str(),1280,720));
 opts.page=DOPT_PAGE_VISUALS;opts.vis.mode=DOPT_MODE_ENHANCED;em_options(&opts);em_viewport(1280,720);check_seats();
 assert(em_seat_point("Reset to Defaults",&x,&y));for(const Seat& q:v.seats)if(q.kind==BUTTON){if(textwidth(q.label)>q.w-16)fprintf(stderr,"German button overflow: %s (%d > %d)\n",q.label.c_str(),textwidth(q.label),q.w-16);assert(textwidth(q.label)<=q.w-16);}
 assert(em_write_shot((shotdir+"/"+game+"-german-settings.png").c_str(),1280,720));
 em_set_language_hooks(nullptr);printf("ENMENU|%s|german|main/pause/settings localized, native font, label fit, English/German pointer aliases=PASS\n",game);
}
int main(int argc,char **argv){
 assert(argc==2||argc==3);shotdir=argv[1];
 assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);SDL_Window *win=SDL_CreateWindow("Enhanced menu gate",0,0,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(win);SDL_GLContext gl=SDL_GL_CreateContext(win);assert(gl);assert(em_available());
 char err[256];DB_Pack *p=db_pack_load("dosmenu.pack",err,sizeof err);assert(p);
 if(argc==3&&!strcmp(argv[2],"--importer-only")){
  importer_views("td");db_pack_free(p);
  em_set_red_alert("Map importer review","");texture=bgtexture=logotexture=legaltexture=bgtexture_lo=logotexture_lo=0;em_reset();assert(em_available());
  p=db_pack_load("ra3d/dosmenu.pack",err,sizeof err);assert(p);importer_views("ra");db_pack_free(p);
  SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();puts("ENMENU|IMPORTER|PASS");return 0;
 }
 if(argc==3&&!strcmp(argv[2],"--german-only")){
  german_menu(p,"td");db_pack_free(p);
  em_set_red_alert("Language regression","");texture=bgtexture=logotexture=legaltexture=bgtexture_lo=logotexture_lo=0;em_reset();assert(em_available());
  p=db_pack_load("ra3d/dosmenu.pack",err,sizeof err);assert(p);german_menu(p,"ra");db_pack_free(p);
  SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();puts("ENMENU|GERMAN|PASS");return 0;
 }
 if(argc==3&&!strcmp(argv[2],"--skirmish-prefs-only")){
  user_map_preview(p);skirmish_preferences(p,"td");db_pack_free(p);
  em_set_red_alert("Skirmish regression","");texture=bgtexture=logotexture=legaltexture=bgtexture_lo=logotexture_lo=0;em_reset();assert(em_available());
  p=db_pack_load("ra3d/dosmenu.pack",err,sizeof err);assert(p);skirmish_preferences(p,"ra");db_pack_free(p);
  SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();puts("ENMENU|SKIRMISH-PREFS|PASS");return 0;
 }
 if(argc==3&&!strcmp(argv[2],"--ra-only")){ra_pass();db_pack_free(p);SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();puts("ENMENU|RA-ONLY|PASS");return 0;}
 if(argc==3&&!strcmp(argv[2],"--difficulty-only")){
  const SK_Map maps[]={{"SCM01EA","Eight Commanders","TEMPERATE",62,62,8,0}};
  SK_State state;sk_init(&state,maps,1,nullptr);ai_difficulty_rows(&state,p,"td");db_pack_free(p);
  em_set_red_alert("Fair AI review","");texture=bgtexture=logotexture=legaltexture=bgtexture_lo=logotexture_lo=0;em_reset();assert(em_available());
  p=db_pack_load("ra3d/dosmenu.pack",err,sizeof err);assert(p);sk_init(&state,maps,1,nullptr);sk_set_red_alert(&state);ai_difficulty_rows(&state,p,"ra");db_pack_free(p);
  SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();puts("ENMENU|DIFFICULTY|PASS");return 0;
 }
 /* --settings-only: the title's Settings screen alone, both games, for a quick look */
 if(argc==3&&!strcmp(argv[2],"--settings-only")){
  title_settings(win,p);pause_audio(p,"td");db_pack_free(p);
  FILE *f=fopen("ra3d/dosmenu.pack","rb");
  if(f){fclose(f);em_set_red_alert("Settings review","");texture=bgtexture=logotexture=legaltexture=bgtexture_lo=logotexture_lo=0;em_reset();assert(em_available());
   p=db_pack_load("ra3d/dosmenu.pack",err,sizeof err);assert(p);DOPT_State o={};dopt_settings_init(&o.set);dopt_open(&o,p);em_title_mode(1);
   settings_tabs(&o,"ra");language_box(&o,"ra");pause_audio(p,"ra");em_title_mode(1);o.page=DOPT_PAGE_ADVANCED;v.section=6;em_options(&o);em_viewport(1280,720);assert(em_write_shot((shotdir+"/ra-title-settings-audio.png").c_str(),1280,720));db_pack_free(p);}
  SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();puts("ENMENU|SETTINGS|PASS");return 0;
 }
 DM_State m;dm_state_init(&m);m.selected=DM_START;m.version="v0.8.1";em_menu_overlay(p,-1000,-1000);em_main(&m);em_viewport(1280,720);save("main");
 /* Display order, keyboard navigation and the original controller actions agree. */
 for(int n=0;n<DM_ITEM_COUNT;n++){
  const Seat &q=v.seats[n];assert(q.item==main_order[n]&&q.y==main_y[n]);assert(textwidth(q.label)<=q.w-16);
  if(!q.disabled){int x,y,w,h,lx,ly;assert(em_item_rect(q.item,&x,&y,&w,&h));em_to_legacy(x+w/2,y+h/2,&lx,&ly);assert(dm_hit_test(&m,lx,ly)==q.item);}
  int next=em_main_next(&m,q.item,1);assert(!dm_state_item_disabled(&m,next));
 }
 assert(em_main_next(&m,DM_EXIT,1)==DM_START);assert(em_main_next(&m,DM_START,-1)==DM_EXIT);
 m.intro_missing=1;assert(em_main_next(&m,DM_VISUALS,1)==DM_EXIT);
 /* An install without the movies (the default until Setup downloads them) greys Intro on the Enhanced menu too. */
 em_main(&m);{int seen=0;for(const Seat &q:v.seats)if(q.item==DM_INTRO){seen++;assert(q.disabled);}assert(seen==1);}
 m.intro_missing=0;em_main(&m);for(const Seat &q:v.seats)if(q.item==DM_INTRO)assert(!q.disabled);
 printf("ENMENU|intro|greyed without the movies, live with them=PASS\n");
 for(int y:{246,354}){int lx,ly;em_to_legacy(v.x+v.w*v.scale/2,v.y+y*v.scale,&lx,&ly);assert(dm_hit_test(&m,lx,ly)<0);}
 printf("ENMENU|main-order|grouped labels, keyboard, controller actions and inert dividers=PASS\n");
 user_map_preview(p);
 /* Compare actual framebuffer pixels with the legal text removed, then with
    the pointer added. Every difference must stay in its own screen seat. */
 for(const auto &size:std::vector<std::pair<int,int>>{{1280,720},{1600,960},{2560,1440}}){
  int w=size.first,h=size.second;SDL_SetWindowSize(win,w,h);em_main(&m);em_viewport(w,h);
  std::vector<unsigned char> legal(w*h*4),plain(legal.size()),cursor(legal.size());
  em_draw(w,h,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,legal.data());
  const char *notice=m.copyright;m.copyright="";em_draw(w,h,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,plain.data());m.copyright=notice;
  int changed=0;for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(memcmp(&plain[(y*w+x)*4],&legal[(y*w+x)*4],4)){changed++;assert(h-1-y>=v.y+v.h*v.scale);}
  assert(changed>100);assert(emcursor::load());assert(emcursor::mesh.size()==108);
  em_menu_overlay(p,60,100);em_draw(w,h,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,cursor.data());
  int green=0,ink=0;for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(memcmp(&cursor[(y*w+x)*4],&legal[(y*w+x)*4],4)){
   int sy=h-1-y;assert(x>=60-10*v.scale&&x<=60+40*v.scale&&sy>=100-10*v.scale&&sy<=100+50*v.scale);ink++;
   const unsigned char *px=&cursor[(y*w+x)*4];green+=px[1]>px[0]+20&&px[1]>px[2]+20;
  }assert(ink>100&&green>100);
  assert(em_write_shot((shotdir+"/main-overlay-"+std::to_string(w)+".png").c_str(),w,h));
  printf("ENMENU|overlay|%dx%d|legal=%d|cursor=%d|green=%d|outside-seats=0\n",w,h,changed,ink,green);em_menu_overlay(p,-1000,-1000);
 }
 SDL_SetWindowSize(win,1280,720);em_main(&m);em_viewport(1280,720);save("main");
 /* Main menu animation stays in the two requested outer seats. */
 std::vector<unsigned char> titleStill(1280*720*4),titleSpin(titleStill.size());glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,titleStill.data());
 faction_angle_override=1;save("main-spin");faction_angle_override=-1;glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,titleSpin.data());
 int titlechanged[2]={};for(int yy=0;yy<720;yy++)for(int xx=0;xx<1280;xx++)if(memcmp(&titleStill[(yy*1280+xx)*4],&titleSpin[(yy*1280+xx)*4],4)){
  int y=719-yy;bool left=xx>=77&&xx<277&&y>=484&&y<684,right=xx>=1003&&xx<1203&&y>=484&&y<684;
  assert(left||right);titlechanged[right?1:0]++;
 }assert(titlechanged[0]>1000&&titlechanged[1]>1000);printf("ENMENU|main-spin|GDI=%d|Nod=%d|outside-seats=0\n",titlechanged[0],titlechanged[1]);
 const char *roster[]={"Power Plant","Communications Center"};EM_Database db={};db.count=2;db.house=1;db.names=roster;db.name=roster[0];db.code="NUKE";db.brief="Supplies 100 power; unarmed; wood armour.";db.armour="Wood";db.hp=200;db.cost=300;db.damage=db.speed=db.rof=db.range256=-1;db.model_available=1;db.unlocks="Barracks, refinery";
 for(int detail=0;detail<4;detail++){db.detail=detail;em_database(&db);save(("database-"+std::to_string(detail)).c_str());
  for(const Seat &q:v.seats)if(q.kind==BUTTON)assert(textwidth(q.label)<=q.w-(q.badge>=0?52:16));}
 em_viewport(1280,720);int dbx,dby,dbw,dbh;assert(em_database_view(&dbx,&dby,&dbw,&dbh));assert(dbx==420&&dby==244&&dbw==332&&dbh==296);
 assert(em_pick(1036,148)==4000&&em_pick(1162,148)==4001&&em_pick(920,262)==5001);printf("ENMENU|database|fittings/text/model-well/hit-seats=PASS\n");
 DOPT_State o={};dopt_settings_init(&o.set);dopt_open(&o,p);dopt_bind(&o,nullptr,settings_apply);em_options(&o);save("pause");
 /* A title dialog must leave visible backdrop around its wide frame; a pause
    dialog must retain the battlefield behind it instead of clearing it. */
 o.page=DOPT_PAGE_VISUALS;o.vis.mode=DOPT_MODE_ENHANCED;em_options(&o);
 for(const auto &size:std::vector<std::pair<int,int>>{{1280,720},{2560,1440}}){
  int w=size.first,h=size.second;SDL_SetWindowSize(win,w,h);em_title_mode(1);em_options(&o);em_viewport(w,h);em_draw(w,h,2);
  std::vector<unsigned char> rgba(w*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  int red=0;for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(x<v.x||x>=v.x+v.w*v.scale||h-1-y<v.y||h-1-y>=v.y+v.h*v.scale){const unsigned char *px=&rgba[(y*w+x)*4];red+=px[0]>80&&px[0]>px[1]+20&&px[0]>px[2]+40;}
  assert(red>1000);printf("ENMENU|settings-backdrop|%dx%d|outside-frame-red=%d\n",w,h,red);
  em_title_mode(0);em_options(&o);em_viewport(w,h);
  glClearColor(.12f,.24f,.36f,1);glClear(GL_COLOR_BUFFER_BIT);em_draw(w,h,0);unsigned char corner[4];glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,corner);assert(corner[0]>15&&corner[0]<30&&corner[1]>35&&corner[1]<55&&corner[2]>55&&corner[2]<75);
 }
 SDL_SetWindowSize(win,1280,720);em_viewport(1280,720);
 o.page=DOPT_PAGE_CONTROLS;em_options(&o);em_viewport(1280,720);save("controls");assert(!em_item_rect(DOPT_C_MUSIC,nullptr,nullptr,nullptr,nullptr)&&!em_item_rect(DOPT_C_SOUNDCTRL,nullptr,nullptr,nullptr,nullptr));
 click(&o,DOPT_C_SPEED,0);assert(o.set.speed==0);em_options(&o);click(&o,DOPT_C_SPEED,1000);assert(o.set.speed==6);
 /* the volumes are the pause menu's Audio page, the voices among them */
 o.page=DOPT_PAGE_OPTIONS;em_options(&o);click(&o,DOPT_AUDIO);em_options(&o);assert(o.page==DOPT_PAGE_SOUND);save("audio");
 click(&o,DOPT_S_MUSIC,0);assert(o.set.music==0);em_options(&o);click(&o,DOPT_S_MUSIC,1000);assert(o.set.music==239&&applied>=2);
 em_options(&o);click(&o,DOPT_S_SPEECH,0);assert(o.set.speech==0);em_options(&o);click(&o,DOPT_S_SPEECH,1000);assert(o.set.speech==239);
 em_options(&o);click(&o,DOPT_S_OK);assert(o.page==DOPT_PAGE_OPTIONS&&o.selected==DOPT_AUDIO);
 o.page=DOPT_PAGE_VISUALS;o.vis.mode=DOPT_MODE_ENHANCED;em_options(&o);assert(!em_item_rect(DOPT_VE_SUPERSAMPLE,nullptr,nullptr,nullptr,nullptr));save("visuals-display");
 /* Vertical sync clicks through off, on and adaptive; on and adaptive are both ticked, so the label must tell them apart. */
 o.vis.vsync=0;for(int want:{1,2,0}){em_options(&o);em_viewport(1280,720);click(&o,DOPT_VE_VSYNC);assert(o.vis.vsync==want);
  em_options(&o);int seen=0;for(const Seat &q:v.seats)if(q.item==DOPT_VE_VSYNC){seen++;assert(q.value==want&&q.label==(want==2?std::string("Adaptive sync"):std::string(dopt_vis_elem_label(DOPT_VE_VSYNC))));}assert(seen==1);
  if(want==2)save("visuals-adaptive");}
 printf("ENMENU|vsync|off, on, adaptive, off; adaptive labelled apart=PASS\n");
 for(int i=1;i<5;i++){v.section=i;em_options(&o);save((std::string("visuals-")+std::to_string(i)).c_str());}
 retired_relief(&o);
 v.section=2;em_options(&o);em_viewport(1280,720);int before=o.vis.prof[2].elem[DOPT_VE_SHADOWS];click(&o,DOPT_VE_SHADOWS);assert(o.vis.prof[2].elem[DOPT_VE_SHADOWS]!=before);
 o.vis.from_menu=1;em_options(&o);assert(click(&o,DOPT_A_OK)==DOPT_ACT_RESUME);
 o.page=DOPT_PAGE_VISUALS;o.vis.from_menu=0;em_options(&o);assert(em_opt_key(&o,DOPT_KEY_ESC)==DOPT_ACT_NONE&&o.page==DOPT_PAGE_OPTIONS);
 o.page=DOPT_PAGE_VISUALS;o.vis.from_menu=1;em_options(&o);int bx=0,by=0;for(const Seat &q:v.seats)if(q.action==BACK){em_to_legacy(v.x+(q.x+q.w/2)*v.scale,v.y+(q.y+q.h/2)*v.scale,&bx,&by);}
 em_opt_press(&o,bx,by);assert(em_opt_release(&o,bx,by)==DOPT_ACT_RESUME);
 o.page=DOPT_PAGE_GAMEPLAY;em_options(&o);save("gameplay");before=o.gp.on[DOPT_G_MOUSE];click(&o,EM_SCHEME_SEAT-(before+1)%DOPT_G_MOUSE_COUNT);assert(o.gp.on[DOPT_G_MOUSE]==(before+1)%DOPT_G_MOUSE_COUNT);
 o.page=DOPT_PAGE_CONFIRM;em_options(&o);save("confirm");click(&o,DOPT_CF_CANCEL);assert(o.page==DOPT_PAGE_OPTIONS);
 o.page=DOPT_PAGE_RESTATE;strcpy(o.brief,"Secure the beachhead and establish a base. Build a refinery and harvest Tiberium. Eliminate the Nod forces in the area.");em_options(&o);save("briefing");
 dopt_open_slots(&o,p,DOPT_SL_LOAD);assert(o.sl.nrows==0&&o.sl.sel==-1);em_options(&o);
 for(const Seat &q:v.seats)assert(q.action!=OPTROW);save("load-empty");
 const SK_Map maps[]={{"SCM01EA","Green Acres","TEMPERATE",58,44,8,0},{"SCM02EA","Sand Trap","DESERT",62,62,8,0}};
 SK_Prev *pr=sk_prev_load("mappreview.pack",err,sizeof err);SK_State s;sk_init(&s,maps,2,pr);em_lobby(&s,p);em_viewport(1280,720);save("skirmish");clicksk(&s,SK_I_ROW0);assert(s.popup==0);em_lobby(&s,p);save("seat");clicksk(&s,SK_I_POP_C1+2);assert(sk_row_colour(&s,0)==2);s.popup=-1;
 v.sksection=1;em_lobby(&s,p);save("skirmish-rules");v.sksection=0;em_lobby(&s,p);clicksk(&s,SK_I_CHANGEMAP);assert(s.picker);em_lobby(&s,p);save("maps");clicksk(&s,SK_I_PICK_CANCEL);assert(!s.picker);
 s.net=1;s.net_seat=0;s.net_seats=8;s.net_humans=1;s.net_taken[0]=1;strcpy(s.name[0],"Commander");s.chat_count=0;
 sk_chat_push(&s,-1,"Waiting for other commanders to join.");sk_chat_push(&s,0,"This is a long message that must wrap cleanly into several lines without hiding the message input or losing the newest messages at the bottom of the transcript.");
 em_lobby(&s,p);save("lobby");strcpy(s.chat_in,"Ready when you are.");int sendx=0,sendy=0;for(const Seat &q:v.seats)if(q.action==SEND){em_to_legacy(v.x+q.x*v.scale+q.w*v.scale/2,v.y+q.y*v.scale+q.h*v.scale/2,&sendx,&sendy);}
 em_sk_press(&s,sendx,sendy);assert(em_sk_release(&s,sendx,sendy)==SK_ACT_SAY);
 s.net=2;v.sksection=1;em_lobby(&s,p);save("lobby-rules-joiner");before=s.credits;sk_enh_control(&s,SK_I_CREDITS,10000);assert(s.credits==before);assert(sk_enh_action(&s,SK_I_CHANGEMAP)==0&&!s.picker);
 /* THE GAME MODE DROPDOWN at the right end of the lobby's tab row: a fresh lobby is Free for
    All; the list's three rows; Gun Game keeps a skirmish's computer seats; Rally moves to a
    course and back; a network room's computer seats empty for Gun Game; a relayed room greys
    Gun Game with its reason; a room being started and a joiner cannot change it; an open list
    is the one thing Escape reaches. */
 {const SK_Map gm[]={{"SCM01EA","Green Acres","TEMPERATE",58,44,8,0,0,0,0,{0},{0},SK_COURSE_NONE},
   {"SCM70EA","Tiberium Run","TEMPERATE",62,62,8,0,0,0,0,{0},{0},SK_COURSE_CIRCUIT}};
  auto modeseat=[](){const Seat *found=nullptr;for(const Seat &q:v.seats)if(q.action==DROP&&q.arg==EM_SK_MODE_DROP)found=&q;assert(found);return *found;};
  SK_State g;sk_init(&g,gm,2,pr);v.sksection=0;v.drop=-1;em_lobby(&g,p);
  assert(sk_mode(&g)==SK_MODE_FFA&&modeseat().label=="Free for All"&&!modeseat().disabled);save("mode-closed");
  assert(sk_row_mode(&g,1)==SK_SEAT_BOT);
  clicksk(&g,EM_SK_MODE_DROP);em_lobby(&g,p);assert(v.popupFirst>=0&&em_sk_drop_open());
  {int rows=0;for(const Seat &q:v.seats)if(q.action==SKMODE){rows++;assert(!q.disabled&&q.active==(q.arg==SK_MODE_FFA));}assert(rows==3);}save("mode-open");
  clicksk(&g,EM_SK_MODE_BASE+SK_MODE_GUN_GAME);assert(sk_mode(&g)==SK_MODE_GUN_GAME&&!em_sk_drop_open()&&sk_row_mode(&g,1)==SK_SEAT_BOT);
  em_lobby(&g,p);assert(modeseat().label=="Gun Game");v.sksection=1;em_lobby(&g,p);save("mode-gungame-rules");v.sksection=0;em_lobby(&g,p);
  clicksk(&g,EM_SK_MODE_DROP);em_lobby(&g,p);clicksk(&g,EM_SK_MODE_BASE+SK_MODE_RALLY);assert(sk_mode(&g)==SK_MODE_RALLY&&g.sel==1&&!g.gun_game);
  em_lobby(&g,p);assert(modeseat().label=="Rally");save("mode-rally");
  clicksk(&g,EM_SK_MODE_DROP);em_lobby(&g,p);clicksk(&g,EM_SK_MODE_BASE+SK_MODE_FFA);assert(sk_mode(&g)==SK_MODE_FFA&&g.sel==0);
  {SK_Lobby out;sk_result(&g,&out);assert(!out.gun_game&&out.rally_laps==0);}
  g.net=1;g.net_seat=0;g.net_seats=8;g.net_humans=1;g.net_taken[0]=1;g.mode[1]=SK_SEAT_BOT;
  assert(sk_set_mode(&g,SK_MODE_GUN_GAME)&&g.mode[1]==SK_SEAT_OPEN);{SK_Lobby out;sk_result(&g,&out);assert(out.gun_game);}sk_set_mode(&g,SK_MODE_FFA);
  em_lobby(&g,p);save("mode-host");
  g.net_relayed=1;assert(!sk_mode_offered(&g,SK_MODE_GUN_GAME));em_lobby(&g,p);clicksk(&g,EM_SK_MODE_DROP);em_lobby(&g,p);
  {bool greyed=false;for(const Seat &q:v.seats)if(q.action==SKMODE&&q.arg==SK_MODE_GUN_GAME)greyed=q.disabled;assert(greyed&&v.popupHint=="Gun Game: LAN only"&&textwidth(v.popupHint)<=v.popupW-32);}save("mode-relayed");
  em_sk_drop_close();assert(!em_sk_drop_open());g.net_relayed=0;
  g.net_starting=1;em_lobby(&g,p);assert(modeseat().disabled&&!sk_set_mode(&g,SK_MODE_RALLY)&&sk_mode(&g)==SK_MODE_FFA);g.net_starting=0;
  g.net=2;g.net_seat=1;em_lobby(&g,p);assert(modeseat().disabled&&!sk_set_mode(&g,SK_MODE_GUN_GAME));save("mode-joiner");
  printf("ENMENU|game-mode|dropdown opens on Free for All, three rows, Gun Game keeps skirmish bots and empties room bots, Rally to a course and back, relayed greys Gun Game, starting and joiner locked=PASS\n");}
 /* RALLY: a lobby in the Game Mode dropdown's third mode lists the courses and only them, defaults to the
    first, offers the laps (not on a point-to-point course) and none of the economy, and
    the laps reach the lobby's answer; every other mode hides the courses. */
 {const SK_Map rmaps[]={{"SCM01EA","Green Acres","TEMPERATE",58,44,8,0,0,0,0,{0},{0},SK_COURSE_NONE},
   {"SCM70EA","Tiberium Run","TEMPERATE",62,62,8,0,0,0,0,{0},{0},SK_COURSE_CIRCUIT},{"SCM71EA","Nod Gauntlet","TEMPERATE",62,62,8,0,0,0,0,{0},{0},SK_COURSE_RUN}};
  SK_State r;sk_init(&r,rmaps,3,pr);assert(r.sel==0&&!r.rally&&sk_first_offered(&r)==0&&!sk_map_offered(&r,1)&&!sk_map_offered(&r,2));
  sk_set_rally(&r,1);assert(r.rally&&r.sel==1&&r.laps==SK_LAPS_DEFAULT&&!sk_map_offered(&r,0)&&sk_map_offered(&r,1)&&sk_map_offered(&r,2));
  v.sksection=0;em_lobby(&r,p);save("rally-skirmish");
  /* the roster prints a race's seats as the race has them: the side alone, under a heading
     that names only the side, never "GDI / 1 / Random" for a team and a start it never reads;
     a Free for All lobby keeps all three */
  assert(roster_heading(&r)=="Side"&&sk_row_mode(&r,0)!=SK_SEAT_OPEN&&roster_summary(&r,0)==seat_side(&r,0)&&roster_summary(&r,0).find('/')==std::string::npos);
  {SK_State f;sk_init(&f,rmaps,3,pr);assert(roster_heading(&f)=="Side / Team / Start"&&roster_summary(&f,0).find(" / ")!=std::string::npos);}
  printf("ENMENU|rally-roster|heading=%s|seat0=%s|PASS\n",roster_heading(&r).c_str(),roster_summary(&r,0).c_str());
  /* a seat in a Rally lobby: Team and Start position shown as the race has them and not offered
     (the grid is drawn at random); the side stays offered */
  clicksk(&r,SK_I_ROW0);assert(r.popup==0);em_lobby(&r,p);
  {int drops=0;bool side=false;for(const Seat &q:v.seats){if(q.action==DROP&&(q.arg==-12||q.arg==-13)){drops++;assert(q.disabled);}
    if(q.action==DROP&&q.arg==-11)side=!q.disabled;}assert(drops==2&&side);}
  assert(sk_item_disabled(&r,SK_I_POP_S1)&&sk_item_disabled(&r,SK_I_POP_T1));save("rally-seat");r.popup=-1;em_lobby(&r,p);
  clicksk(&r,SK_I_CHANGEMAP);assert(r.picker);em_lobby(&r,p);{int rows=0;for(const Seat &q:v.seats)if(q.action==MAPROW){rows++;assert(q.arg!=0);}assert(rows==2);}save("rally-maps");clicksk(&r,SK_I_PICK_CANCEL);assert(!r.picker);
  v.sksection=1;em_lobby(&r,p);{bool laps=false,economy=false;for(const Seat &q:v.seats){if(q.item==SK_I_LAPS){laps=true;assert(!q.disabled&&q.min==SK_LAPS_MIN&&q.max==SK_LAPS_MAX&&q.value==SK_LAPS_DEFAULT);}
   if(q.item==SK_I_CREDITS||q.item==SK_I_BUILD||q.item==SK_I_UNITS||q.item==SK_I_SUPER||q.item==SK_I_CRATES||q.item==SK_I_TIBERIUM)economy=true;}assert(laps&&!economy);}save("rally-rules");
  sk_enh_control(&r,SK_I_LAPS,5);assert(r.laps==5);sk_enh_control(&r,SK_I_LAPS,99);assert(r.laps==SK_LAPS_MAX);sk_enh_control(&r,SK_I_LAPS,5);
  {SK_Lobby out;sk_result(&r,&out);assert(out.rally_laps==5&&out.map==1&&!out.gun_game);}
  const std::string rpath=shotdir+"/rally-skirmish.cfg";assert(sk_prefs_write(rpath.c_str(),&r));
  {SK_State back;sk_init(&back,rmaps,3,pr);sk_set_rally(&back,1);assert(sk_prefs_read(rpath.c_str(),&back)&&back.sel==1&&back.laps==5);
   SK_State ffa;sk_init(&ffa,rmaps,3,pr);assert(sk_prefs_read(rpath.c_str(),&ffa)&&ffa.sel==0);}
  sk_enh_action(&r,SK_I_CHANGEMAP);sk_enh_pick_row(&r,0);assert(r.pick_sel!=0);sk_enh_pick_row(&r,2);sk_enh_action(&r,SK_I_PICK_OK);assert(r.sel==2);
  /* a lobby opens in Free for All, so the course raced last (here the second) waits for
     Rally to be chosen and is selected then, rather than the first course */
  {const std::string cpath=shotdir+"/rally-second-course.cfg";assert(sk_prefs_write(cpath.c_str(),&r));
   SK_State next;sk_init(&next,rmaps,3,pr);assert(sk_prefs_read(cpath.c_str(),&next)&&next.sel==0&&next.remembered_course==2);
   assert(sk_set_mode(&next,SK_MODE_RALLY)&&next.sel==2);}
  em_lobby(&r,p);{bool seen=false;for(const Seat &q:v.seats)if(q.item==SK_I_LAPS){seen=true;assert(q.disabled&&q.value==1);}assert(seen);}save("rally-rules-run");
  {SK_Lobby out;sk_result(&r,&out);assert(out.rally_laps==5);}
  r.net=1;r.net_seat=0;r.net_seats=8;r.net_humans=1;r.net_taken[0]=1;r.speed=4;sk_enh_action(&r,SK_I_CHANGEMAP);sk_enh_pick_row(&r,1);sk_enh_action(&r,SK_I_PICK_OK);
  em_lobby(&r,p);{bool ai=false,speed=false;for(const Seat &q:v.seats){if(q.item==SK_I_AITAKE)ai=true;if(q.item==SK_I_SPEED)speed=true;}assert(ai&&speed);}save("rally-lobby-rules-host");
  r.net=2;r.net_seat=1;em_lobby(&r,p);for(const Seat &q:v.seats)if(q.item==SK_I_LAPS)assert(q.disabled);const int keep=r.laps;sk_enh_control(&r,SK_I_LAPS,1);assert(r.laps==keep);save("rally-lobby-rules-joiner");
  r.net=0;sk_set_rally(&r,0);assert(!r.rally&&r.sel==0);v.sksection=0;
  printf("ENMENU|rally|courses only under Rally and hidden otherwise, laps 1..9 to the answer and the prefs, one run disables laps, host speed and takeover, joiner refused=PASS\n");}
 MP_State b;mp_init(&b);em_browser(&b);save("host");b.tab=MP_TAB_JOIN;em_browser(&b);save("join-empty");
 DO_Mission missions[]={{"SCG22EA","Blackout",0,0},{"SCB20EA","Liberation",1,0}};DO_State ops;do_state_init(&ops,missions,2);em_ops(&ops);save("missions");
 const DO_Mission categories[]={{"","NOD COVERT OPERATIONS",1,1},{"SCB20EA","Nod Covert",1,0},{"","GDI COVERT OPERATIONS",0,1},{"SCG22EA","GDI Covert",0,0},{"","GDI SPECIAL OPS",0,1},{"SCG30EA","GDI Special",0,0},{"","NOD SPECIAL OPS",1,1},{"SCB21EA","Nod Special",1,0},{"","TEST MAPS",0,1},{"TESTMAP","Test Map",0,0}};
 DO_State combined;do_state_init(&combined,categories,10);em_ops(&combined);save("special-covert");
 std::vector<int> badgeOrder;int filters=0;for(const Seat &q:v.seats){if(q.action==OPSFILTER)filters++;if(q.badge>=0)badgeOrder.push_back(q.badge);assert(q.label!="Test Map"&&q.label!="Test Maps");}
 assert(filters==2&&(badgeOrder==std::vector<int>{0,1}));em_ops_move(&combined,-10000);assert(combined.selected==3);em_ops_move(&combined,1);assert(combined.selected==1);em_ops_move(&combined,10000);assert(combined.selected==1);
 int filter=-1;for(size_t i=0;i<v.seats.size();i++)if(v.seats[i].action==OPSFILTER&&v.seats[i].arg==1)filter=int(i);assert(filter>=0);em_ops_action(&combined,-1000-filter);em_ops(&combined);assert(combined.selected==5);save("special-console");em_ops_move(&combined,10000);assert(combined.selected==7);em_ops_move(&combined,1);assert(combined.selected==7);
 std::vector<unsigned char> still(1280*720*4),spin(still.size());glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,still.data());
 faction_angle_override=1.0f;save("special-spin");faction_angle_override=-1;glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,spin.data());
 int changed[2]={};for(int y=0;y<720;y++)for(int x=0;x<1280;x++)if(memcmp(&still[(y*1280+x)*4],&spin[(y*1280+x)*4],4)){
  bool seated=false;for(const Seat &q:v.seats)if(q.badge>=0){float bx=v.x+(q.x+6)*v.scale,by=v.y+(q.y+2)*v.scale;
   if(x>=int(bx)&&x<int(ceilf(bx+28*v.scale))&&720-1-y>=int(by)&&720-1-y<int(ceilf(by+28*v.scale))){changed[q.badge]++;seated=true;}}
  assert(seated);
 }assert(changed[0]>10&&changed[1]>10);printf("ENMENU|spin|GDI=%d|Nod=%d|outside-seats=0\n",changed[0],changed[1]);
 /* The Funpark section brings a third tab, and only where it is in the list: RIGHT steps to
    it, its rows carry no faction badge (either side is picked when one starts), and the
    lists above without it kept their two tabs. */
 {const DO_Mission fun[]={{"","GDI COVERT OPERATIONS",0,1},{"SCG22EA","GDI Covert",0,0},{"","GDI SPECIAL OPS",0,1},{"SCG30EA","GDI Special",0,0},{"","FUNPARK",0,1},{"SCJ01EA","Funpark 1",0,0},{"SCJ02EA","Funpark 2",0,0},{"","TEST MAPS",0,1},{"TESTMAP","Test Map",0,0}};
  DO_State f;do_state_init(&f,fun,9);em_ops(&f);int tabs=0;for(const Seat &q:v.seats)if(q.action==OPSFILTER){tabs++;if(q.arg==2)assert(q.label=="Funpark");}assert(tabs==3);
  em_ops_filter(&f,0);em_ops_step(&f,1);em_ops_step(&f,1);em_ops(&f);assert(f.selected==5);em_ops_step(&f,1);assert(f.selected==5);em_ops_move(&f,1);assert(f.selected==6);
  for(const Seat &q:v.seats)if(q.action==OPTROW)assert(q.badge<0);save("special-funpark");em_ops_step(&f,-1);assert(f.selected==3);
  printf("ENMENU|funpark|third tab only with the FUNPARK section, RIGHT reaches it and stops there, no faction badge on its rows=PASS\n");}
 for(int i=0;i<3;i++){em_choice(i,0,-1);save((std::string("choice-")+std::to_string(i)).c_str());}
 title_settings(win,p);skirmish_dropdown(&s,p);ai_difficulty_rows(&s,p,"td");
 td_debrief();
 sk_prev_free(pr);db_pack_free(p);
 ra_pass();
 SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();puts("ENMENU|PASS|native seats, all screens, source sliders/actions, joiner permissions");return 0;
}

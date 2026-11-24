/* Enhanced menu v06. Accepted native artwork, original font, existing controllers.
   No Lab room/save fixtures and no replacement network or save implementation. */
#include "enhanced_menu.h"
#include "map_import_ui.h"
#include "enhanced_menu_font.h"
extern "C" {
#include "pngwrite.h"
}
#include <SDL.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
/* GL 1.2's edge clamp, which every Windows driver supports but Windows' own gl.h, frozen at
   GL 1.1, does not name. */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include "../art/interface/enhanced-menu-logos-v01/menu_faction_meshes.h"
#include "../art/interface/ra-enhanced-menu-v01/emblems/menu_ra_emblem_meshes.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "enhanced_menu_cursor.h"
#include "fx_input.h"

namespace {
/* fixed: one of the kit's own pictures, loaded once and never changed after; a picture
   built for one frame (a map preview) is not, and is told apart by its content. */
struct Art { int w=0,h=0; std::vector<unsigned char> pixels; bool fixed=false; };
struct Seat {
 int item=0,action=0,arg=0,x=0,y=0,w=0,h=0,lx=0,ly=0,lw=0,lh=0,kind=0;
 int value=0,min=0,max=1,step=1,badge=-1; bool disabled=false,active=false;
 /* drawn greyed like a disabled seat, but still pressed and selected: a saved game this
    build cannot load, which the player may still pick to be told why, save over or delete */
 bool muted=false;
 std::string label,sourceLabel,icon;
};
/* PLATE: a faction emblem on its steel tile (Red Alert's details plates), never pressed. */
enum {BUTTON, CHECK, RANGE, FIELD, ROW, COLOUR, PLATE, ICON};
enum {OPT=1,MAIN,OPS,LOBBY,BROWSER,DATABASE,CHOICE,SIDE,DEBRIEF,IMPORTMAP};
enum {TAB=1,RESET,BACK,RANGESET,DROP,DROPCHOICE,MODE,DISP,OPSTOP,OPSFILTER,
 MAPROW,SEATDONE,SEND,MPSETUP,MPHANDLE,OPTROW,SKRANGE,OPTITEM,SKITEM,SKRANDOM,SKCOUNTRY,RARANGE,RACHECK,
 SETRANGE,GPCHECK,LANGDROP,LANGPICK,LANGGO,LANGCANCEL,LANGCLOSE,SKMODE,KEYCAP,KEYDRAG,GPSCHEME};
struct View {
 int kind=0,w=0,h=0,scale=1,x=0,y=0,fbw=0,fbh=0,mousex=-1000,mousey=-1000;
 int held=-1,drag=-1,section=0,sksection=0,opsfilter=-1,opstop=0,drop=-1;
 int page=-1,lastselected=-1; const void *owner=nullptr;
 int popupFirst=-1,popupOwner=-1,popupChoice=-1,popupX=0,popupY=0,popupW=0,popupH=0;
 bool title=false,compact=false;
 /* the language box: the language it offers (-1 none), and whether its download was started */
 int langBox=-1; bool langStarted=false;
 std::vector<Seat> seats; std::vector<unsigned char> rgba;
 std::string frame,popupHint;
 /* the drawing rgba holds: its key, and the panel writes counter when it was made */
 unsigned long long rasterKey=0,rasterWrites=0; bool rasterValid=false;
} v;
bool option_title=false;
const EM_LangHooks *langhooks=nullptr;   /* the language row's answers (em_set_language_hooks) */
const EM_KeyHooks *keyhooks=nullptr;     /* the Key Bindings page's answers (em_set_key_hooks) */
/* THE KEY BINDINGS PAGE'S OWN STATE: the command whose box is waiting for a key (-1 none),
   and the line its header shows, which says what the last key press did. */
int kbcap=-1;std::string kbmsg;
std::map<std::string,Art> arts;
int loadstate=0; GLuint texture=0,bgtexture=0,logotexture=0,legaltexture=0;
const DB_Pack *menu_pack=nullptr;
int pointerx=-1000,pointery=-1000;
std::string sound_hover;
std::string legal_text;const DB_Pack *legal_pack=nullptr;
/* Body text, secondary text and accents. Tiberian Dawn's are these three; Red Alert's
   come from its pack, per palette (see Theme). */
unsigned TEXT=0xe6e5da, MUTED=0xaeb5ac, ACCENT=0x6ddf4c;
/* WHICH GAME'S KIT. One process draws one game's menus: Tiberian Dawn's v06 art, or Red
   Alert's (the ra-enhanced-menu-v01 design, baked by bake_ra_enhanced_menu.py from the 1996
   data into ra3d/enhanced-menu.pack). Red Alert's dialogs are red at the title and blue in a
   mission, as the 1996 game draws them in its title and scenario palettes, so each of its
   frame and button arts carries the palette it was baked in. */
struct Theme {
 bool ra=false,blue=false,soviet=false;   // soviet: the mission being played is a Soviet one
 unsigned slot[16]={};          // the pack's theme-red / theme-blue colours, in Slot order
 std::string version,legal;     // the main menu's version line and the footer's notice
};
enum Slot{S_TEXT,S_MUTED,S_ACCENT,S_WELLLIT,S_ROWON,S_ROWHOVER,S_RAMP0,S_OVERLAY=14,S_TRACK=15};
Theme theme;
std::vector<int> pack_advance;  // Red Alert's GRAD6FNT advance table, read from its pack
const int *advance=font_advance;int glyphs=226;
std::string art_id(const std::string &id){return theme.ra?id+(theme.blue?"-blue":"-red"):id;}
void theme_colours(){
 if(!theme.ra)return;
 auto it=arts.find(theme.blue?"theme-blue":"theme-red");if(it==arts.end())return;
 for(int i=0;i<16&&i<it->second.w;i++){const unsigned char *p=&it->second.pixels[i*4];theme.slot[i]=(p[0]<<16)|(p[1]<<8)|p[2];}
 TEXT=theme.slot[S_TEXT];MUTED=theme.slot[S_MUTED];ACCENT=theme.slot[S_ACCENT];
}
unsigned art_gen=0;   /* moves each time the kit is loaded, so a reloaded picture is a new one */
unsigned read32(FILE *f){unsigned char b[4];if(fread(b,1,4,f)!=4)return 0;return b[0]|unsigned(b[1])<<8|unsigned(b[2])<<16|unsigned(b[3])<<24;}
bool load(){
 if(loadstate)return loadstate>0; loadstate=-1;
 FILE *f=fopen(theme.ra?"ra3d/enhanced-menu.pack":"enhanced-menu.pack","rb");if(!f)return false;
 char magic[8];if(fread(magic,1,8,f)!=8||memcmp(magic,theme.ra?"ENMENURA":"ENMENU06",8)){fclose(f);return false;}
 unsigned count=read32(f);if(count>(theme.ra?1024u:256u)){fclose(f);return false;}
 std::map<std::string,Art> loaded;
 for(unsigned i=0;i<count;i++){
  char name[64];if(fread(name,1,64,f)!=64||!memchr(name,0,64)){fclose(f);return false;}
  Art a;a.w=read32(f);a.h=read32(f);unsigned bytes=read32(f);
  if(a.w<1||a.h<1||a.w>4096||a.h>2048||bytes!=unsigned(a.w*a.h*4)){fclose(f);return false;}
  a.pixels.resize(bytes);if(fread(a.pixels.data(),1,bytes,f)!=bytes){fclose(f);return false;}
  loaded[name]=std::move(a);
 }
 fclose(f);if(!loaded.count("font_menu")||!loaded.count(theme.ra?"lobby-frame-red":"lobby-frame"))return false;
 if(theme.ra){
  auto ad=loaded.find("font_advance");if(ad==loaded.end()||ad->second.w*8!=loaded.at("font_menu").w)return false;
  pack_advance.assign(ad->second.w,0);for(int i=0;i<ad->second.w;i++)pack_advance[i]=ad->second.pixels[i*4];
  advance=pack_advance.data();glyphs=ad->second.w;
 }
 for(auto &kv:loaded)kv.second.fixed=true;
 arts.swap(loaded);loadstate=1;art_gen++;fprintf(stderr,"ENMENU|loaded|%s|assets=%zu\n",theme.ra?"red-alert-v01":"v06",arts.size());return true;
}
/* REDRAW ON CHANGE. The panel is painted in software, the whole frame art and every
   button, and it was painted afresh on every pass of the menu loop and sent to the card
   again, twice per pass on the main menu, whether anything had changed or not; on an idle
   main menu that was most of the time a pass took that was not spent asleep.

   Every drawing call below now does two things: it folds its own arguments into a running
   key (rhash), and, unless this is a layout pass (recording), it draws. A screen builder is
   run once as a layout pass, which places every seat (so hover and clicks hit the current
   screen) and works out the key of what it would draw, and only when that key differs from
   the key of the picture already in rgba is it run again to draw. The key is built from
   the drawing calls themselves, not from a list of the inputs a builder happens to read,
   so a hover, a held button, the caret's blink, a translated label or a new map preview
   changes the key exactly when it changes a pixel, and nothing has to be remembered to
   keep it honest. The kit's own pictures are keyed by identity (they never change after
   loading, and art_gen moves when they are reloaded); a picture built for the frame is
   keyed by its content.

   writes counts every drawing call that reaches the panel, from anything, so a screen not
   drawn this way (the pause dialog's options, the database) invalidates the stored
   picture just by drawing, and em_draw sends the panel to the card only when it has
   changed since the last time. */
bool recording=false;          /* a layout pass: place seats and key the drawing, draw nothing */
bool layout_only=false;        /* em_layout_pass: the next builder places seats and stops */
unsigned long long rhash=0,writes=0,uploaded_writes=~0ULL;
unsigned em_rasters_n=0,em_uploads_n=0;
void mix(unsigned long long x){rhash^=x+0x9e3779b97f4a7c15ULL+(rhash<<6)+(rhash>>2);}
void mixbytes(const void *data,size_t n){
 const unsigned char *b=(const unsigned char*)data;unsigned long long h=1469598103934665603ULL;
 for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}mix(h);mix(n);
}
void mixart(const Art &a){if(a.fixed){mix((unsigned long long)(uintptr_t)&a);mix(art_gen);}else{mix(a.w);mix(a.h);mixbytes(a.pixels.data(),a.pixels.size());}}
void pixel_(int x,int y,unsigned col,int alpha){
 if(x<0||y<0||x>=v.w||y>=v.h)return;
 unsigned char *d=&v.rgba[(y*v.w+x)*4];unsigned r=(col>>16)&255,g=(col>>8)&255,b=col&255;
 if(alpha==255){d[0]=r;d[1]=g;d[2]=b;d[3]=255;return;}
 int a=d[3],out=alpha+a*(255-alpha)/255;if(!out)return;
 d[0]=(r*alpha+d[0]*a*(255-alpha)/255)/out;
 d[1]=(g*alpha+d[1]*a*(255-alpha)/255)/out;
 d[2]=(b*alpha+d[2]*a*(255-alpha)/255)/out;d[3]=out;
}
void rect_(int x,int y,int w,int h,unsigned col,int alpha){for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)pixel_(xx,yy,col,alpha);}
void pixel(int x,int y,unsigned col,int alpha=255){mix(1);mix(x);mix(y);mix(col);mix(alpha);if(recording)return;writes++;pixel_(x,y,col,alpha);}
void rect(int x,int y,int w,int h,unsigned col,int alpha=255){mix(2);mix(x);mix(y);mix(w);mix(h);mix(col);mix(alpha);if(recording)return;writes++;rect_(x,y,w,h,col,alpha);}
void blit(const Art &a,int x,int y,int scale=1){
 mix(3);mixart(a);mix(x);mix(y);mix(scale);if(recording)return;writes++;
 for(int yy=0;yy<a.h;yy++)for(int xx=0;xx<a.w;xx++){
  const unsigned char *p=&a.pixels[(yy*a.w+xx)*4];if(!p[3])continue;
  rect_(x+xx*scale,y+yy*scale,scale,scale,(p[0]<<16)|(p[1]<<8)|p[2],p[3]);
 }
}
void image(const std::string &id,int x,int y){auto it=arts.find(id);if(it!=arts.end())blit(it->second,x,y);}
std::string localized(const std::string& s){return langhooks&&langhooks->translate?langhooks->translate(s.c_str()):s;}
int textwidth(const std::string &s){int w=0;for(unsigned char c:localized(s))if(c<glyphs)w+=std::max(0,advance[c])*2;return w;}
std::string fitted(std::string s,int w){if(textwidth(s)<=w)return s;while(!s.empty()&&textwidth(s+"..")>w)s.pop_back();return s+"..";}
void text(const std::string &source,int x,int y,unsigned col=TEXT,bool center=false){
 const std::string s=localized(source);
 mix(4);mixbytes(s.data(),s.size());mix(x);mix(y);mix(col);mix(center);if(recording)return;writes++;
 if(center)x-=textwidth(s)/2;const Art &a=arts.at("font_menu");
 for(unsigned char c:s){if(c>=glyphs)continue;
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){
   const unsigned char *p=&a.pixels[(yy*a.w+c*8+xx)*4];if(!p[3])continue;
   bool shadow=p[0]<64&&p[1]<64&&p[2]<64;rect_(x+xx*2,y+yy*2,2,2,shadow?0:col,p[3]);
  }
  x+=std::max(0,advance[c])*2;
 }
}
std::vector<std::string> wrapped(const std::string &s,int width){
 std::vector<std::string> out;std::string line,word;
 auto flushword=[&](){if(word.empty())return;
  if(!line.empty()&&textwidth(line+" "+word)>width){out.push_back(line);line.clear();}
  while(textwidth(word)>width){size_t n=1;while(n<word.size()&&textwidth(word.substr(0,n+1))<=width)n++;
   if(!line.empty()){out.push_back(line);line.clear();}out.push_back(word.substr(0,n));word.erase(0,n);}
  if(!word.empty()){if(!line.empty())line+=' ';line+=word;}word.clear();};
 for(char c:s){if(c==' '||c=='\n'||c=='\r'||c=='\t'){flushword();if(c=='\n'){out.push_back(line);line.clear();}}else word+=c;}
 flushword();if(!line.empty())out.push_back(line);return out;
}
void note(const std::string &s,int x,int y,int width,int maxlines=99,unsigned col=MUTED,int pitch=26,bool newest=false){
 auto lines=wrapped(s,width);int start=newest?std::max(0,int(lines.size())-maxlines):0;
 for(int i=start;i<int(lines.size())&&i<start+maxlines;i++)text(lines[i],x,y+(i-start)*pitch,col);
}
/* Red Alert's well: the field shows through, darkened, inside the same cut-corner liner;
   a popup's well is solid so the list it carries reads over whatever it covers. */
void ra_well(const char *title,int x,int y,int w,int h,bool overlay){
 const unsigned dark=0x060202,lit=theme.slot[S_WELLLIT];
 mix(5);mix(x);mix(y);mix(w);mix(h);mix(overlay);
 if(overlay)rect(x,y,w,h,theme.slot[S_OVERLAY]);
 else if(!recording&&++writes)for(int yy=std::max(0,y);yy<std::min(v.h,y+h);yy++)for(int xx=std::max(0,x);xx<std::min(v.w,x+w);xx++){
  unsigned char *d=&v.rgba[(yy*v.w+xx)*4];for(int c=0;c<3;c++)d[c]=(unsigned char)(d[c]*.36f);}
 rect(x+4,y,w-8,2,dark);rect(x,y+4,2,h-8,dark);rect(x+w-2,y+4,2,h-8,dark);rect(x+4,y+h-2,w-8,2,dark);
 for(int i=0;i<4;i++){pixel(x+3-i,y+i,dark);pixel(x+w-4+i,y+i,dark);pixel(x+3-i,y+h-1-i,dark);pixel(x+w-4+i,y+h-1-i,dark);}
 rect(x+4,y+h-2,w-8,1,lit,200);rect(x+w-2,y+5,1,h-10,lit,200);
 if(title&&*title){text(title,x+18,y+14,ACCENT);rect(x+16,y+40,w-32,1,0x040202);rect(x+16,y+41,w-32,1,lit,160);}
}
void well(const char *title,int x,int y,int w,int h,bool overlay=false){
 if(theme.ra){ra_well(title,x,y,w,h,overlay);return;}
 if(overlay)rect(x,y,w,h,0x101310);
 rect(x+4,y,w-8,2,0x080a09);rect(x,y+4,2,h-8,0x080a09);
 rect(x+w-2,y+4,2,h-8,0x080a09);rect(x+4,y+h-2,w-8,2,0x080a09);
 for(int i=0;i<4;i++){pixel(x+3-i,y+i,0x080a09);pixel(x+w-4+i,y+i,0x080a09);pixel(x+3-i,y+h-1-i,0x080a09);pixel(x+w-4+i,y+h-1-i,0x080a09);}
 rect(x+4,y+h-2,w-8,1,0x4a4b42);rect(x+w-2,y+5,1,h-10,0x4a4b42);
 if(title&&*title){text(title,x+18,y+14,ACCENT);rect(x+16,y+40,w-32,1,0x0b0d0b);rect(x+16,y+41,w-32,1,0x353930);}
}
void begin(int kind,const void *owner,const char *frame,const std::string &title){
 if(!load())return;
 if(v.kind!=kind||v.owner!=owner){v.held=v.drag=-1;v.drop=v.popupChoice=-1;v.page=-1;v.lastselected=-1;
  if(kind==OPT)v.section=0;if(kind==LOBBY)v.sksection=0;
  if(kind==OPS){v.opsfilter=-1;v.opstop=0;}}
 v.kind=kind;v.owner=owner;v.frame=frame;v.title=kind==MAIN||(kind==OPT&&option_title);
 /* Red Alert's small boxes (a confirmation, a notice) keep their own size at the title and
    stand in the middle of the screen, as its exit box does. */
 if(theme.ra&&kind==OPT&&!strcmp(frame,"confirm-frame"))v.title=false;
 /* A confirmation or notice box keeps its own height at the title in Tiberian Dawn too:
    stretched to the main menu's height, three lines of text sat at the top of a frame that
    ran most of the way down the screen. The wordmark above it stays. */
 v.compact=kind==OPT&&v.title&&strcmp(frame,"confirm-frame")!=0;
 /* In a mission: the pause dialog and everything it opens, and the Database tab. */
 theme.blue=theme.ra&&((kind==OPT&&!option_title)||kind==DATABASE);theme_colours();
 /* Red Alert bakes its title dialogs at the main menu's height rather than stretching rows,
    so its grid stays whole. */
 const std::string id=theme.ra&&v.compact?std::string(frame)+"-compact-red":art_id(frame);
 const Art &a=arts.at(id);v.w=a.w;v.h=v.compact?arts.at(art_id("main-frame")).h:a.h;
 v.seats.clear();v.popupFirst=v.popupOwner=-1;v.popupHint.clear();
 mix(7);mixbytes(id.data(),id.size());mix(v.w);mix(v.h);mix(art_gen);
 /* a layout pass leaves the last picture alone: it may be the one still wanted */
 if(!recording){v.rgba.assign(v.w*v.h*4,0);writes++;}
 if(v.h==a.h)image(id,0,0);
 else if(!recording)for(int row=0;row<v.h;row++){
  int sourceRow=row;
  if(v.h<a.h){if(row>=v.h/2)sourceRow+=a.h-v.h;}
  else if(row>=a.h/2)sourceRow=row<a.h/2+v.h-a.h?a.h/2+(row-a.h/2)%64:row-v.h+a.h;
  memcpy(&v.rgba[row*v.w*4],&a.pixels[sourceRow*a.w*4],v.w*4);
 }
 text(title,v.w/2,32,TEXT,true);
}
void popup(int owner,int left,int top,int width,int height){v.popupFirst=int(v.seats.size());v.popupOwner=owner;v.popupX=left;v.popupY=top;v.popupW=width;v.popupH=height;}
bool popup_contains(int pointX,int pointY){return pointX>=v.popupX&&pointY>=v.popupY&&pointX<v.popupX+v.popupW&&pointY<v.popupY+v.popupH;}
Seat &add(int item,const std::string &label,int x,int y,int w,int h=40,int kind=BUTTON,int action=0,int arg=0){
 Seat q;q.item=item;q.sourceLabel=label;q.label=localized(label);q.x=x;q.y=y;q.w=w;q.h=h;q.kind=kind;q.action=action;q.arg=arg;
 v.seats.push_back(q);return v.seats.back();
}
void legacy(Seat &q,int x,int y,int w,int h){q.lx=x;q.ly=y;q.lw=w;q.lh=h;}
Seat &opt(DOPT_State *st,int item,const std::string &label,int x,int y,int w,int h=40,int kind=BUTTON){
 Seat &q=add(item,label,x,y,w,h,kind);int rx=0,ry=0,rw=0,rh=0;
 if(st->page==DOPT_PAGE_ADVANCED&&item<DOPT_VE_COUNT&&item>=DOPT_VE_GLOBAL_COUNT)dopt_adv_reveal(st,item);
 if(dopt_item_rect(st,item,&rx,&ry,&rw,&rh))legacy(q,rx,ry,rw,rh);else q.disabled=true;
 q.disabled=q.disabled||dopt_item_disabled(st,item);q.active=st->selected==item;return q;
}
Seat &sk(const SK_State *st,int item,const std::string &label,int x,int y,int w,int h=40,int kind=BUTTON){
 Seat &q=add(item,label,x,y,w,h,kind);int rx=0,ry=0,rw=0,rh=0;
 if(sk_item_rect(st,item,&rx,&ry,&rw,&rh))legacy(q,rx,ry,rw,rh);else q.disabled=true;
 q.disabled=q.disabled||sk_item_disabled(st,item);q.active=st->selected==item;return q;
}
Seat &mp(const MP_State *st,int item,const std::string &label,int x,int y,int w,int h=40,int kind=BUTTON){
 Seat &q=add(item,label,x,y,w,h,kind);int rx=0,ry=0,rw=0,rh=0;
 if(mp_item_rect(st,item,&rx,&ry,&rw,&rh))legacy(q,rx,ry,rw,rh);else q.disabled=true;
 q.disabled=q.disabled||mp_item_disabled(st,item);q.active=st->hover==item;return q;
}
/* Red Alert's fields, rows, checks and sliders: the Tiberian Dawn geometry in the dialog's
   own ramp, steel and gold. Returns false for the kinds both games draw from button art. */
bool paint_ra(const Seat &q,bool hover,bool down,unsigned col){
 const unsigned *T=theme.slot;
 if(q.kind==PLATE){image("plate",q.x,q.y);return true;}
 if(q.kind==FIELD){rect(q.x,q.y,q.w,q.h,0x060303);rect(q.x,q.y,q.w,1,q.active?ACCENT:0x807870);rect(q.x,q.y,1,q.h,0x807870);rect(q.x,q.y+q.h-1,q.w,1,0x342826);rect(q.x+q.w-1,q.y,1,q.h,0x342826);
  text(fitted(q.label,q.w-24),q.x+12,q.y+12,col);if(q.active&&SDL_GetTicks()%800<400)rect(std::min(q.x+12+textwidth(q.label),q.x+q.w-14),q.y+10,1,19,ACCENT);return true;}
 if(q.kind==ROW){if(q.active||hover){rect(q.x,q.y,q.w,q.h,q.active?T[S_ROWON]:T[S_ROWHOVER]);rect(q.x,q.y,2,q.h,ACCENT);}text(fitted(q.label,q.w-(q.badge<0?24:56)),q.x+(q.badge<0?12:44),q.y+(q.h-16)/2,col);return true;}
 if(q.kind==CHECK){if(hover&&!q.disabled)rect(q.x,q.y,q.w,q.h,T[S_ROWHOVER]);int x=q.x+2,y=q.y+7;
  rect(x,y,22,22,0x969696);rect(x+1,y+1,20,20,0x080404);rect(x+1,y+1,20,1,0x3c3c3c);rect(x+2,y+20,18,1,0x281e1e);rect(x+20,y+2,1,18,0x281e1e);
  if(q.value){int pts[][2]={{5,10},{7,12},{9,14},{11,12},{13,10},{15,8},{17,6}};for(auto &p:pts)rect(x+p[0]-1,y+p[1]-1,3,3,q.disabled?0x968264:ACCENT);}
  text(fitted(q.label,q.w-44),q.x+38,q.y+10,col);return true;}
 if(q.kind==RANGE){int fill=int((q.w-28)*double(q.value-q.min)/std::max(1,q.max-q.min));int x=q.x+14,y=q.y+14,w=q.w-28;
  rect(x-2,y-2,w+4,12,0x060202);rect(x,y+9,w,1,0x5a3c38);rect(x+w+1,y,1,9,0x5a3c38);rect(x,y,w,8,T[S_TRACK]);
  rect(x,y,fill,8,q.disabled?0x5a4642:hover?T[S_RAMP0+2]:T[S_RAMP0+4]);rect(x,y,fill,1,q.disabled?0x968c82:ACCENT);
  image(art_id(std::string("button-28-28-")+(down?"pressed":hover?"hover":"normal")),q.x+fill,q.y+4);return true;}
 return false;
}
/* A mode tile's picture on a dark well. A disabled tile is drawn in grey; a missing
   picture leaves the well with the mode's name, so a kit without the art still reads. */
void paint_icon(const Seat &q,bool hover,bool down){
 rect(q.x-4,q.y-4,q.w+8,q.h+8,0x080a09);rect(q.x-3,q.y-3,q.w+6,q.h+6,theme.ra?0x2a1c1a:0x1a1d19);
 auto it=arts.find(art_id(q.icon));if(it==arts.end())it=arts.find(q.icon);
 if(it!=arts.end()){const Art &a=it->second;const int ox=q.x+(q.w-a.w)/2,oy=q.y+(q.h-a.h)/2+(down?1:0);
  mix(6);mixart(a);mix(ox);mix(oy);mix(q.disabled);
  if(!recording&&++writes)for(int yy=0;yy<a.h;yy++)for(int xx=0;xx<a.w;xx++){const unsigned char *p=&a.pixels[(yy*a.w+xx)*4];if(!p[3])continue;
   unsigned r=p[0],g=p[1],b=p[2];if(q.disabled){unsigned l=(r*30+g*59+b*11)/100;r=g=b=l*3/5;}
   pixel_(ox+xx,oy+yy,(r<<16)|(g<<8)|b,p[3]);}}
 else text(fitted(q.sourceLabel,q.w-12),q.x+q.w/2,q.y+q.h/2-8,MUTED,true);
 if(hover||q.active){const unsigned c=q.disabled?MUTED:ACCENT;rect(q.x-4,q.y-4,q.w+8,2,c);rect(q.x-4,q.y+q.h+2,q.w+8,2,c);rect(q.x-4,q.y-4,2,q.h+8,c);rect(q.x+q.w+2,q.y-4,2,q.h+8,c);}
}
void paint(){
 for(int n=0;n<int(v.seats.size());n++){
  if(n==v.popupFirst)well("",v.popupX,v.popupY,v.popupW,v.popupH,true);
  const Seat &q=v.seats[n];bool hover=(v.popupFirst<0||n>=v.popupFirst||n==v.popupOwner)&&v.mousex>=q.x&&v.mousey>=q.y&&v.mousex<q.x+q.w&&v.mousey<q.y+q.h;
  bool down=v.held==n;unsigned col=q.disabled||q.muted?MUTED:TEXT;
  if(q.kind==ICON){paint_icon(q,hover&&!q.disabled,down);continue;}
  if(theme.ra&&paint_ra(q,hover,down,col))continue;
  if(q.kind==FIELD){rect(q.x,q.y,q.w,q.h,0x080a09);rect(q.x,q.y,q.w,1,q.active?0xb19a63:0x606255);rect(q.x,q.y,1,q.h,0x606255);rect(q.x,q.y+q.h-1,q.w,1,0x353b30);rect(q.x+q.w-1,q.y,1,q.h,0x353b30);text(fitted(q.label,q.w-24),q.x+12,q.y+12,col);if(q.active&&SDL_GetTicks()%800<400)rect(std::min(q.x+12+textwidth(q.label),q.x+q.w-14),q.y+10,1,19,0xb5cf91);continue;}
  if(q.kind==ROW){if(q.active||hover){rect(q.x,q.y,q.w,q.h,q.active?0x273521:0x30342b);rect(q.x,q.y,2,q.h,0x637450);}text(fitted(q.label,q.w-(q.badge<0?24:56)),q.x+(q.badge<0?12:44),q.y+(q.h-16)/2,col);continue;}
  if(q.kind==CHECK){if(hover&&!q.disabled)rect(q.x,q.y,q.w,q.h,0x353c30);int x=q.x+2,y=q.y+7;
   rect(x,y,22,22,0x787b6b);rect(x+1,y+1,20,20,0x10130f);rect(x+2,y+20,18,1,0x373b32);rect(x+20,y+2,1,18,0x373b32);
   if(q.value){int pts[][2]={{5,10},{7,12},{9,14},{11,12},{13,10},{15,8},{17,6}};for(auto &p:pts)rect(x+p[0]-1,y+p[1]-1,3,3,q.disabled?0x797f73:0x72df4c);}
   text(fitted(q.label,q.w-44),q.x+38,q.y+10,col);continue;}
  if(q.kind==RANGE){int fill=int((q.w-28)*double(q.value-q.min)/std::max(1,q.max-q.min));int x=q.x+14,y=q.y+14,w=q.w-28;
   rect(x-2,y-2,w+4,12,0x080a08);rect(x,y+9,w,1,0x55574c);rect(x+w+1,y,1,9,0x55574c);rect(x,y,w,8,0x1b2618);
   rect(x,y,fill,8,q.disabled?0x41483b:hover?0x7ddb45:0x527d35);rect(x,y,fill,1,0xa5c777);
   image(std::string("button-28-28-")+(down?"pressed":hover?"hover":"normal"),q.x+fill,q.y+4);continue;}
  std::string state=q.disabled?"disabled":down?"pressed":hover?(q.active?"activehover":"hover"):q.active?"active":"normal";
  std::string id=art_id("button-"+std::to_string(q.w)+"-"+std::to_string(q.h)+"-"+state);
  if(arts.count(id))image(id,q.x,q.y);else {rect(q.x,q.y,q.w,q.h,0x292d28);rect(q.x+2,q.y,q.w-4,2,0x858b7d);rect(q.x,q.y+2,2,q.h-4,0x666a60);rect(q.x+2,q.y+q.h-2,q.w-4,2,0x090b09);}
  if(q.kind==COLOUR){rect(q.x+7,q.y+7,14,14,q.value);continue;}
  if(!q.icon.empty()){auto it=arts.find(art_id(q.icon));if(it!=arts.end())blit(it->second,q.x+18,q.y+(q.h-it->second.h)/2);}
  text(fitted(q.label,q.w-(q.badge>=0?52:16)),q.x+q.w/2+(q.badge>=0?14:0)-(q.action==DROP?8:0),q.y+(q.h-16)/2+(down?1:0),col,true);
  if(q.action==DROP)for(int i=0;i<5;i++)rect(q.x+q.w-27+i,q.y+17+i,9-2*i,1,col);
 }
 if(!v.popupHint.empty())text(v.popupHint,v.popupX+16,v.popupY+v.popupH-26,MUTED);
}
int hit(int x,int y){
 for(int i=int(v.seats.size())-1;i>=0;i--){
  if(v.popupFirst>=0&&i<v.popupFirst&&i!=v.popupOwner)continue;
  const Seat &q=v.seats[i];if(x>=q.x&&y>=q.y&&x<q.x+q.w&&y<q.y+q.h)return q.disabled?-1:i;
 }
 return -1;
}
/* One screen builder, drawn only when its key has changed (see REDRAW ON CHANGE). The
   builders this wraps are safe to run twice in a row: dms_draw already ran each of them
   twice per pass, once to place the seats for the pointer and once to draw. */
template<class F> void memo(F &&build){
 const bool seatsOnly=layout_only;
 recording=true;rhash=0x5eedULL;build();recording=false;
 if(seatsOnly)return;
 if(v.rasterValid&&rhash==v.rasterKey&&v.rasterWrites==writes&&v.rgba.size()==size_t(v.w)*v.h*4)return;
 rhash=0x5eedULL;build();
 v.rasterKey=rhash;v.rasterWrites=writes;v.rasterValid=true;em_rasters_n++;
}
int legal_scale(int w,int h){return std::max(1,std::min(w/DM_LEGAL_W,h/480));}
void layout(int w,int h){v.fbw=w;v.fbh=h;v.scale=std::max(1,h/693);const int titleWidth=v.title?std::max(v.w,arts.at(theme.ra?"visuals-frame-red":"visuals-frame").w):v.w;while(v.scale>1&&(titleWidth*v.scale>w-40||v.h*v.scale>h-20))v.scale--;v.x=(w-v.w*v.scale)/2;v.y=(h-v.h*v.scale)/2+(v.title?52*v.scale:0);if(v.title)v.y=std::min(v.y,h-v.h*v.scale-DM_LEGAL_H*legal_scale(w,h));}
int virtualseat(int x){int n=-20000-x;return x<=-20000&&n>=0&&n<int(v.seats.size())?n:-1;}
void glquad(GLuint t,float x,float y,float w,float h,float u,float z){glBindTexture(GL_TEXTURE_2D,t);glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(u,0);glVertex2f(x+w,y);glTexCoord2f(u,z);glVertex2f(x+w,y+h);glTexCoord2f(0,z);glVertex2f(x,y+h);glEnd();}
void arttexture(GLuint &tex,const Art &a){if(tex)return;int w=1,h=1;while(w<a.w)w*=2;while(h<a.h)h*=2;std::vector<unsigned char> padded(w*h*4,0);for(int y=0;y<a.h;y++)memcpy(&padded[y*w*4],&a.pixels[y*a.w*4],a.w*4);glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,padded.data());}
/* Red Alert's title is drawn from its HIRES plate where that fits at least twice, from the
   LORES plate below that, so a small window does not show a 640 x 400 title mostly hidden. */
GLuint bgtexture_lo=0,logotexture_lo=0;
/* ONE SIZE FOR THE BACKDROP ON EVERY SCREEN. The title fitted the plate (the largest whole
   multiple inside the window) and every other screen covered the window with it, so opening
   the lobby, the Special Ops list, a confirm box or a notice made the picture jump larger
   (Red Alert at 1920x1080: 1280x800 to 1920x1200) and closing it shrank it again. Every
   screen fits it, as the title does, in both games. */
void backdrop(int w,int h){const bool lo=theme.ra&&std::min(w/640,h/400)<2;const Art &a=arts.at(lo?"title-backdrop-lo":"title-backdrop");arttexture(lo?bgtexture_lo:bgtexture,a);GLuint tex=lo?bgtexture_lo:bgtexture;int s=std::max(1,std::min(w/a.w,h/a.h)),pw=1,ph=1;while(pw<a.w)pw*=2;while(ph<a.h)ph*=2;glquad(tex,(w-a.w*s)/2,(h-a.h*s)/2,a.w*s,a.h*s,float(a.w)/pw,float(a.h)/ph);}
unsigned player_colour(int i);
/* RED ALERT'S MAP FRAME. Its previews are baked by tools/ra3d/bake-ra-map-previews.py into
   a 120 pixel square with the map's playable rectangle fitted inside, aspect kept and
   centred; start k is drawn on the frame pixel its cell's centre falls on. This is that
   fit, in the same arithmetic (a double scale, the size rounded half to even as Python
   rounds, the position truncated), so a start computed here lands on the pixel the baker
   marked. Answers false for a map whose size is not known. */
const int RA_PREVIEW_FRAME=120;
bool ra_preview_fit(const SK_Map &m,double &s,int &ox,int &oy,int &iw,int &ih){
 if(m.w<=0||m.h<=0)return false;
 s=std::min(double(RA_PREVIEW_FRAME)/m.w,double(RA_PREVIEW_FRAME)/m.h);
 iw=std::max(1,int(std::nearbyint(m.w*s)));ih=std::max(1,int(std::nearbyint(m.h*s)));
 ox=(RA_PREVIEW_FRAME-iw)/2;oy=(RA_PREVIEW_FRAME-ih)/2;return true;
}
/* WHETHER A RED ALERT START IS COLOURED BY THE SEAT THAT PICKED IT. On: the Red Alert
   engine places each seat on the start it picked (Read_Scenario_INI reads the map's start
   waypoints before GlyphX_Assign_Houses counts them; until it did, every pick was dropped
   and the starts were dealt as though every seat had asked for Random). A variable rather
   than a constant so the menu gate can draw the idle case on the same map. */
bool ra_start_holders=true;
/* RED ALERT'S START MARKERS, over the map picture, so a spot on the map can be matched to
   its start number. bx, by is the frame's top left on the canvas and each frame pixel is
   k x k there. A marker is the baker's own diamond (filled out to two steps from the
   centre, a black rim at three), so it covers the white one baked into the picture
   exactly. Its fill is the idle grey, or, with ra_start_holders on, the colour of the seat
   holding the start (the same colour as that seat's square in the roster); a start nobody
   holds, and every start of a map that is only a candidate in the picker, stays idle.
   Idle diamonds are drawn first and held ones over them, so a held start is never covered
   by an idle one on the same or a neighbouring cell (some maps put two starts on one cell).
   The number is one based, in the menu's own font at the same k, and sits beside its
   diamond: right, else left, else above, else below, the first place inside the frame
   that covers no diamond and no number already placed. Numbers are drawn after every
   diamond so none is covered. */
void ra_start_markers(const SK_State *st,int ix,int bx,int by,int k){
 const SK_Map &m=st->maps[ix];double s;int ox,oy,iw,ih;if(!ra_preview_fit(m,s,ox,oy,iw,ih))return;
 const int n=std::min<int>(std::min<int>(m.start_cells,8),m.starts>0?m.starts:8),F=RA_PREVIEW_FRAME;
 const unsigned idle=0x545454,rim=0x000000;
 auto put=[&](int fx,int fy,unsigned col,int alpha){if(fx>=0&&fy>=0&&fx<F&&fy<F)rect(bx+fx*k,by+fy*k,k,k,col,alpha);};
 int cx[8],cy[8],holder[8];
 for(int i=0;i<n;i++){
  cx[i]=ox+int((m.start_x[i]-m.x+0.5)*s);cy[i]=oy+int((m.start_y[i]-m.y+0.5)*s);
  holder[i]=ra_start_holders&&ix==st->sel?sk_start_holder(st,i):-1;
 }
 for(int pass=0;pass<2;pass++)for(int i=0;i<n;i++){
  if((holder[i]>=0)!=(pass==1))continue;
  const unsigned fill=holder[i]>=0?player_colour(sk_row_colour(st,holder[i])):idle;
  for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++){const int d=std::abs(dx)+std::abs(dy);if(d<=3)put(cx[i]+dx,cy[i]+dy,d==3?rim:fill,255);}
 }
 const Art &font=arts.at("font_menu");const int gh=8;int labx[8],laby[8],labw[8];
 for(int i=0;i<n;i++){
  const std::string num=std::to_string(i+1);int nw=1;for(unsigned char c:num)if(c<glyphs)nw+=std::max(0,advance[c]);
  int nx=0,ny=0;
  for(int t=0;t<4;t++){
   nx=t==0?cx[i]+4:t==1?cx[i]-4-nw:cx[i]-nw/2;ny=t==2?cy[i]-4-gh:t==3?cy[i]+4:cy[i]-gh/2;
   nx=std::max(0,std::min(nx,F-nw));ny=std::max(0,std::min(ny,F-gh));
   bool clash=false;
   for(int j=0;j<n&&!clash;j++)clash=nx<cx[j]+4&&cx[j]-3<nx+nw&&ny<cy[j]+4&&cy[j]-3<ny+gh;
   for(int j=0;j<i&&!clash;j++)clash=nx<labx[j]+labw[j]&&labx[j]<nx+nw&&ny<laby[j]+gh&&laby[j]<ny+gh;
   if(!clash)break;
  }
  labx[i]=nx;laby[i]=ny;labw[i]=nw;
  for(unsigned char c:num){if(c>=glyphs)continue;
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){const unsigned char *g=&font.pixels[(yy*font.w+c*8+xx)*4];if(!g[3])continue;
    const bool shadow=g[0]<64&&g[1]<64&&g[2]<64;put(nx+xx,ny+yy,shadow?0:TEXT,g[3]);}
   nx+=std::max(0,advance[c]);}
 }
}
/* A map with no baked picture in the fitted rectangle (ra_preview_fit) of a frame at bx,by
   scaled k: its own imported picture when it has one, else a plain plate. */
void preview_plate(const SK_Map &m,int bx,int by,int k,int ox,int oy,int iw,int ih){
 if(m.pic&&m.pic_w>0&&m.pic_h>0){Art a;a.w=iw;a.h=ih;a.pixels.resize((size_t)iw*ih*4);
  for(int yy=0;yy<ih;yy++)for(int xx=0;xx<iw;xx++){const int sx=std::min(m.pic_w-1,xx*m.pic_w/iw),sy=std::min(m.pic_h-1,yy*m.pic_h/ih);
   memcpy(&a.pixels[((size_t)yy*iw+xx)*4],m.pic+((size_t)sy*m.pic_w+sx)*4,4);}
  blit(a,bx+ox*k,by+oy*k,k);return;}
 rect(bx+ox*k,by+oy*k,iw*k,ih*k,0x5c5c5c);rect(bx+(ox+1)*k,by+(oy+1)*k,(iw-2)*k,(ih-2)*k,0x1c1c1c);
}
void preview(const SK_State *st,const DB_Pack *p,int ix,int x,int y,int size){
 /* Red Alert's map pictures are in its kit (preview-SCMnnEA, baked from ra3d/previews),
    drawn at whole multiples of their size, with the start markers over them. A map with no
    picture (an imported one) is its fitted rectangle on a plain plate under the same
    markers. Never Tiberian Dawn's preview pack: the two games share the names SCM01EA and
    up, so it would show the other game's map. */
 if(theme.ra&&ix>=0&&ix<st->count&&st->maps[ix].scen){
  const SK_Map &m=st->maps[ix];const int F=RA_PREVIEW_FRAME,k=std::max(1,size/F),bx=x+(size-F*k)/2,by=y+(size-F*k)/2;
  auto it=arts.find(std::string("preview-")+m.scen);
  if(it!=arts.end()&&(it->second.w!=F||it->second.h!=F)){const Art &a=it->second;const int ak=std::max(1,size/std::max(1,a.w));blit(a,x+(size-a.w*ak)/2,y+(size-a.h*ak)/2,ak);return;}
  if(it!=arts.end())blit(it->second,bx,by,k);
  else{double s;int ox,oy,iw,ih;if((!m.start_cells&&!m.pic)||!ra_preview_fit(m,s,ox,oy,iw,ih)){note("No preview available",x,y+size/2,size,2);return;}
   preview_plate(m,bx,by,k,ox,oy,iw,ih);}
  ra_start_markers(st,ix,bx,by,k);return;}
 Art a;a.w=a.h=size;a.pixels.resize(size*size*4);if(sk_preview_rgba(st,p,ix,a.pixels.data(),size,size)){blit(a,x,y);return;}
 /* A TIBERIAN DAWN MAP WITH NO BAKED PICTURE (a user map): its fitted rectangle on a plain
    plate, numbered at its starts, as Red Alert draws one, so a start can still be picked by
    where it is. With no starts known it says so. A race course is never numbered: its
    starts are the grid's own cells and a race deals them at random (sk_draw_preview_at). */
 if(ix>=0&&ix<st->count){const SK_Map &m=st->maps[ix];const int F=RA_PREVIEW_FRAME,k=std::max(1,size/F),bx=x+(size-F*k)/2,by=y+(size-F*k)/2;double s;int ox,oy,iw,ih;
  if((m.start_cells||m.pic)&&ra_preview_fit(m,s,ox,oy,iw,ih)){preview_plate(m,bx,by,k,ox,oy,iw,ih);if(m.course==SK_COURSE_NONE)ra_start_markers(st,ix,bx,by,k);return;}}
 note("No preview available",x,y+size/2,size,2);}
void footer(const char *primary,int item,int backitem,DOPT_State *st){opt(st,backitem,"Back",38,v.h-86,192);opt(st,item,primary,v.w-258,v.h-86,220);}
} // namespace

extern "C" void em_sound_event(const SDL_Event *e,SDL_Window *window){
 if(!e||!window||!v.w)return;
 if(e->type==SDL_WINDOWEVENT&&e->window.event==SDL_WINDOWEVENT_LEAVE){sound_hover.clear();return;}
 bool motion=e->type==SDL_MOUSEMOTION,press=e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT;
 if(!motion&&!press)return;
 int ww,wh;SDL_GetWindowSize(window,&ww,&wh);if(ww<=0||wh<=0)return;
 int fx=(motion?e->motion.x:e->button.x)*v.fbw/ww,fy=(motion?e->motion.y:e->button.y)*v.fbh/wh;
 int n=hit((fx-v.x)/v.scale,(fy-v.y)/v.scale);
 std::string key;
 if(fx>=v.x&&fy>=v.y&&n>=0&&v.seats[n].kind!=FIELD){
  const Seat &q=v.seats[n];
  key=std::to_string(v.kind)+":"+std::to_string((uintptr_t)v.owner)+":"+std::to_string(q.item)+":"+std::to_string(q.action)+":"+std::to_string(q.arg)+":"+q.label;
  if(press||key!=sound_hover)game_enhanced_menu_sound(press?1:0);
 }
 sound_hover=key;
}

extern "C" void em_menu_overlay(const DB_Pack *pack,int x,int y){menu_pack=pack;pointerx=x;pointery=y;}
extern "C" void em_title_mode(int enabled){option_title=enabled!=0;}
extern "C" void em_set_language_hooks(const EM_LangHooks *hooks){langhooks=hooks;}
extern "C" void em_set_key_hooks(const EM_KeyHooks *hooks){keyhooks=hooks;kbcap=-1;}
extern "C" int em_keys_capturing(void){return kbcap>=0&&keyhooks&&v.kind==OPT;}
/* The key a waiting box was given. Escape (SDL's keycode 27) leaves the binding alone. */
extern "C" void em_keys_capture(int keycode,int scancode){
 if(kbcap<0||!keyhooks)return;const int cmd=kbcap;kbcap=-1;
 if(keycode==27){kbmsg=std::string(FX_KEYS[cmd].label)+": unchanged";return;}
 char msg[160]="";keyhooks->assign(cmd,keycode,scancode,msg,sizeof msg);kbmsg=msg;
 game_enhanced_menu_sound(1);
}
extern "C" int em_density(void){return v.scale;}
extern "C" int em_available(void){return load();}
namespace {
const int main_order[]={DM_START,DM_SPECIAL,DM_USERMAPS,DM_LOAD,DM_MULTIPLAYER,DM_SKIRMISH,DM_VISUALS,DM_INTRO,DM_EXIT};
const int main_y[]={64,108,152,196,260,304,368,412,456};
}
extern "C" int em_ra_speaker_rect(int* x,int* y,int* w,int* h){
 if(!theme.ra||!load())return 0;
 const bool lo=std::min(v.fbw/640,v.fbh/400)<2;
 const Art& a=arts.at(lo?"title-backdrop-lo":"title-backdrop");
 const int k=std::max(1,std::min(v.fbw/a.w,v.fbh/a.h));
 *x=(v.fbw-a.w*k)/2+260*a.w*k/320;*y=(v.fbh-a.h*k)/2;
 *w=60*a.w*k/320;*h=50*a.h*k/200;return 1;
}
extern "C" int em_main_next(const DM_State *st,int item,int delta){
 int at=0;for(int i=0;i<DM_ITEM_COUNT;i++)if(main_order[i]==item)at=i;
 for(int i=0;i<DM_ITEM_COUNT;i++){at=(at+(delta<0?DM_ITEM_COUNT-1:1))%DM_ITEM_COUNT;if(!dm_state_item_disabled(st,main_order[at]))return main_order[at];}
 return item;
}
extern "C" void em_main(const DM_State *st){memo([&]{
 if(!load())return;begin(MAIN,st,"main-frame","MAIN MENU");
 for(int n=0;n<DM_ITEM_COUNT;n++){int i=main_order[n];const char *label=i==DM_SPECIAL?(theme.ra?"Mission Disks":"Special Ops / Covert Ops"):i==DM_VISUALS?"Settings":dm_item_label(i);Seat &q=add(i,label,36,main_y[n],500);q.disabled=dm_state_item_disabled(st,i);q.active=st->selected==i;int x,y,w,h;if(dm_item_rect(st,i,&x,&y,&w,&h))legacy(q,x,y,w,h);}
 if(theme.ra)for(int y:{246,354}){rect(38,y,496,2,0x060202);rect(38,y+2,496,1,theme.slot[S_WELLLIT]);rect(38,y+3,496,1,0x140402);}
 else for(int y:{246,354}){rect(38,y,496,2,0x070908);rect(38,y+2,496,1,0x43453d);rect(38,y+3,496,1,0x181b17);}
 /* Red Alert's own menu state carries the 1996 menu's "RA3D" line; Enhanced names the build. */
 text(std::string("ENHANCED / ")+(theme.ra?theme.version.c_str():st->version?st->version:""),v.w/2,516,ACCENT,true);paint();
});}
extern "C" void em_to_legacy(int fx,int fy,int *x,int *y){
 v.mousex=(fx-v.x)/v.scale;v.mousey=(fy-v.y)/v.scale;
 if(fx<v.x||fy<v.y){v.mousex=v.mousey=-1000;}
 int n=v.drag>=0?v.drag:hit(v.mousex,v.mousey);
 if(n<0){*x=*y=-1000;return;}const Seat &q=v.seats[n];
 if(q.action){*x=-20000-n;*y=std::max(0,std::min(1000,(v.mousex-q.x-14)*1000/std::max(1,q.w-28)));}
 else {*x=q.lx+q.lw/2;*y=q.ly+q.lh/2;}
}
extern "C" int em_item_rect(int item,int *x,int *y,int *w,int *h){
 for(const Seat &q:v.seats)if(q.item==item){*x=v.x+q.x*v.scale;*y=v.y+q.y*v.scale;*w=q.w*v.scale;*h=q.h*v.scale;return 1;}return 0;
}
extern "C" void em_viewport(int width,int height){layout(width,height);}
/* The framebuffer centre of the live seat with this label (letter case ignored), for a
   scripted click that takes the pointer's own path. */
extern "C" int em_seat_point(const char *label,int *x,int *y){
 auto same=[](const std::string &a,const char *b){size_t i=0;for(;i<a.size()&&b[i];i++)if(tolower((unsigned char)a[i])!=tolower((unsigned char)b[i]))return false;return i==a.size()&&!b[i];};
 for(const Seat &q:v.seats)if(!q.disabled&&label&&(same(q.label,label)||same(q.sourceLabel,label))){*x=v.x+(q.x+q.w/2)*v.scale;*y=v.y+(q.y+q.h/2)*v.scale;return 1;}
 return 0;
}
namespace {
/* Render the unchanged website model at native row density. Hidden captures use a
   fixed angle; normal menus turn once in about 31 seconds. UI geometry owns the seat. */
float faction_angle_override=-1.0f;
/* One emblem: triangles of 24 floats (three corners of position, normal and UV), a
   material per triangle, and that many 32 x 32 nearest-sampled textures. */
struct Emblem{const float *mesh;int tris;const unsigned char *mats;const unsigned char *tex[4];int ntex;};
const Emblem td_emblems[2]={
 {menu_logo_gdi,int(sizeof(menu_logo_gdi)/sizeof(float)/24),menu_logo_gdi_materials,{menu_logo_gdi_texture0,menu_logo_gdi_texture1,nullptr,nullptr},2},
 {menu_logo_nod,int(sizeof(menu_logo_nod)/sizeof(float)/24),menu_logo_nod_materials,{menu_logo_nod_texture0,menu_logo_nod_texture1,menu_logo_nod_texture2,nullptr},3}};
const Emblem ra_emblems[2]={
 {menu_ra_allied,int(sizeof(menu_ra_allied)/sizeof(float)/24),menu_ra_allied_materials,
  {menu_ra_allied_textures,menu_ra_allied_textures+1*32*32*4,menu_ra_allied_textures+2*32*32*4,menu_ra_allied_textures+3*32*32*4},menu_ra_allied_texture_count},
 {menu_ra_soviet,int(sizeof(menu_ra_soviet)/sizeof(float)/24),menu_ra_soviet_materials,
  {menu_ra_soviet_textures,menu_ra_soviet_textures+1*32*32*4,menu_ra_soviet_textures+2*32*32*4,menu_ra_soviet_textures+3*32*32*4},menu_ra_soviet_texture_count}};
void faction_badges(){
 float angle=faction_angle_override>=0?faction_angle_override:SDL_getenv("OPENCNC3D_BACKGROUND")?.25f:SDL_GetTicks()*.0002f;
 const Emblem *em=theme.ra?ra_emblems:td_emblems;
 static GLuint tex[2][2][4]={};GLuint (*t)[4]=tex[theme.ra?1:0];
 for(int f=0;f<2;f++)for(int m=0;m<em[f].ntex;m++)if(!t[f][m]){glGenTextures(1,&t[f][m]);glBindTexture(GL_TEXTURE_2D,t[f][m]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,em[f].tex[m]);}
 float c=cosf(angle),s=sinf(angle),a=cosf(.34f),b=sinf(.34f);
 auto rotate=[=](const float *p,float *out){float u=c*p[0]+s*p[2],w=-s*p[0]+c*p[2];out[0]=u;out[1]=a*p[1]-b*w;out[2]=b*p[1]+a*w;};
 /* Smooth shading is asked for, not assumed: the movie player and the 1995 menu both leave
    GL_FLAT behind, which drew the badges faceted after the intro. em_draw's attribute push
    restores whatever was set before. */
 GLint vp[4];glGetIntegerv(GL_VIEWPORT,vp);glShadeModel(GL_SMOOTH);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glEnable(GL_SCISSOR_TEST);glClearDepth(1);
 std::vector<Seat> titlebadges;
 if(v.kind==MAIN){
  int size=std::min(200,std::max(0,(v.fbw-v.w*v.scale)/2/v.scale-36));
  if(size>=80)for(int f=0;f<2;f++){Seat q;q.badge=f;q.w=size;
   q.x=f?v.w+(v.fbw-v.x-v.w*v.scale)/v.scale/2-size/2:-v.x/v.scale/2-size/2;
   q.y=(v.fbh-v.y-std::max(36*v.scale,DM_LEGAL_H*legal_scale(v.fbw,v.fbh)+8))/v.scale-size;titlebadges.push_back(q);}
 }
 const std::vector<Seat> &badges=v.kind==MAIN?titlebadges:v.seats;
 for(const Seat &q:badges)if(q.badge>=0){int f=q.badge;
  /* the seat's square: the main menu's outer pair, a details plate's well, or a 28 pixel badge
     at the left of a row or (Red Alert, centred) a button */
  int ox=v.kind==MAIN?0:6,oy=v.kind==MAIN?0:2,side=v.kind==MAIN?q.w:28;
  if(q.kind==PLATE){ox=(q.w-(q.h-14))/2;oy=7;side=q.h-14;}else if(theme.ra&&q.kind==BUTTON&&v.kind!=MAIN)oy=(q.h-28)/2;
  float x=v.x+(q.x+ox)*v.scale,y=v.y+(q.y+oy)*v.scale,size=side*v.scale;
  glScissor(int(x),vp[3]-int(ceilf(y+size)),int(ceilf(size)),int(ceilf(size)));glClear(GL_DEPTH_BUFFER_BIT);
  for(int m=0;m<em[f].ntex;m++){glBindTexture(GL_TEXTURE_2D,t[f][m]);glBegin(GL_TRIANGLES);
   for(int i=0;i<em[f].tris;i++)if(em[f].mats[i]==m)for(int j=0;j<3;j++){
    const float *src=em[f].mesh+i*24+j*8;float p[3],n[3];rotate(src,p);rotate(src+3,n);
    float light=.36f+.7f*std::max(0.f,n[0]*.25f+n[1]*.5f+n[2]*.83f),shine=.42f*powf(std::max(0.f,n[0]*.13f+n[1]*.25f+n[2]*.96f),30.f);
    glColor3f(light+shine,light+shine,light+shine);glTexCoord2f(src[6],1-src[7]);glVertex3f(x+size/2+p[0]*size*.43f,y+size/2-p[1]*size*.43f,p[2]*.25f);
   }glEnd();
  }
 }
 glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glColor4f(1,1,1,1);
}
}
extern "C" void em_draw(int width,int height,int clear){
 if(!load()||!v.w)return;layout(width,height);
 glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_SCISSOR_TEST);glViewport(0,0,width,height);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);
 if(clear){glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDepthMask(GL_FALSE);}
 glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,height,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor4f(1,1,1,1);
 if(clear)backdrop(width,height);else {glDisable(GL_TEXTURE_2D);glColor4f(0,0,0,.3f);glBegin(GL_QUADS);glVertex2i(0,0);glVertex2i(width,0);glVertex2i(width,height);glVertex2i(0,height);glEnd();glEnable(GL_TEXTURE_2D);glColor4f(1,1,1,1);}
 if(clear&&v.title&&!theme.ra){const Art &a=arts.at("wordmark");arttexture(logotexture,a);int pw=1,ph=1;while(pw<a.w)pw*=2;while(ph<a.h)ph*=2;glquad(logotexture,(width-a.w*v.scale*2)/2,std::max(0,v.y-a.h*v.scale*2-16*v.scale),a.w*v.scale*2,a.h*v.scale*2,float(a.w)/pw,float(a.h)/ph);}
 /* Red Alert's lettering, lifted off its title: the HIRES cut at half the panel's scale when
    that scale is even, the LORES cut at the panel's scale when it is odd, so it always lands
    on whole pixels. */
 if(clear&&v.title&&theme.ra){const bool even=v.scale%2==0;const Art &a=arts.at(even?"wordmark":"wordmark-lo");GLuint &t=even?logotexture:logotexture_lo;arttexture(t,a);
  const int k=even?v.scale/2:v.scale;int pw=1,ph=1;while(pw<a.w)pw*=2;while(ph<a.h)ph*=2;
  glquad(t,(width-a.w*k)/2,std::max(0,v.y-a.h*k-16*v.scale),a.w*k,a.h*k,float(a.w)/pw,float(a.h)/ph);}
 if(!texture){glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2048,1024,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);uploaded_writes=~0ULL;}
 /* The panel goes to the card only when something has been drawn into it since it last
    went; an idle menu sends nothing. A panel whose picture does not match its size (a layout
    pass changed screens and nothing has drawn the new one yet) is not shown at all. */
 if(v.rgba.size()==size_t(v.w)*v.h*4){
  glBindTexture(GL_TEXTURE_2D,texture);
  if(uploaded_writes!=writes){glTexSubImage2D(GL_TEXTURE_2D,0,0,0,v.w,v.h,GL_RGBA,GL_UNSIGNED_BYTE,v.rgba.data());uploaded_writes=writes;em_uploads_n++;}
  glquad(texture,v.x,v.y,v.w*v.scale,v.h*v.scale,float(v.w)/2048,float(v.h)/1024);
 }
 if(v.kind==DATABASE||v.kind==MAIN||(v.kind==OPS&&((const DO_State*)v.owner)->grouped))faction_badges();
 else if(theme.ra&&std::any_of(v.seats.begin(),v.seats.end(),[](const Seat &q){return q.badge>=0;}))faction_badges();
 /* The original live legal notice belongs below the title panel. Its original
    bitmap font is retained, with integer scaling and no baked-in wording. */
 if(clear&&v.kind==MAIN&&menu_pack){
  const DM_State *st=(const DM_State*)v.owner;
  /* Red Alert's menu state keeps an empty notice, because its 1996 title plate carries its
     own line; the Enhanced footer prints the theme's. */
  // RA3D title cleanup, 24 October 2026: no copyright below the main menu.
  const char *notice=theme.ra?nullptr:st->copyright;
  if(notice&&*notice){
   Art a;a.w=DM_LEGAL_W;a.h=DM_LEGAL_H;
   if(!legaltexture||legal_pack!=menu_pack||legal_text!=notice){
    if(theme.ra){
     /* Red Alert's footer in the menu's own GRAD6FNT at its native size, in capitals, each
        line centred in the same 640 x 28 strip, in the grey Tiberian Dawn's footer prints */
     a.pixels.assign(a.w*a.h*4,0);const Art &font=arts.at("font_menu");std::vector<std::string> lines;std::string line;
     for(const char *c=notice;;c++){if(!*c||*c=='\n'){lines.push_back(line);line.clear();if(!*c)break;}else line+=char(toupper((unsigned char)*c));}
     int y=(a.h-(8+(int(lines.size())-1)*10))/2;
     for(const std::string &l:lines){int x=(a.w-textwidth(l)/2)/2;
      for(unsigned char c:l){if(c>=glyphs)continue;for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){const unsigned char *p=&font.pixels[(yy*font.w+c*8+xx)*4];if(!p[3]||x+xx<0||x+xx>=a.w||y+yy<0||y+yy>=a.h)continue;
        unsigned char *d=&a.pixels[((y+yy)*a.w+x+xx)*4];bool shadow=p[0]<64;d[0]=shadow?0:175;d[1]=shadow?0:183;d[2]=shadow?0:183;d[3]=255;}
       x+=std::max(0,advance[c]);}
      y+=10;}
    }else{
    std::vector<unsigned char> page(DM_LEGAL_W*DM_SCREEN_H*2*4,0);
    dm_draw_legal_rgba(page.data(),DM_LEGAL_W,DM_SCREEN_H*2,DM_LEGAL_W,menu_pack,notice,256);
    a.pixels.assign(page.end()-a.w*a.h*4,page.end());}
    if(legaltexture)glDeleteTextures(1,&legaltexture);legaltexture=0;arttexture(legaltexture,a);
    legal_pack=menu_pack;legal_text=notice;
   }
   int s=legal_scale(width,height);
   int pw=1,ph=1;while(pw<a.w)pw*=2;while(ph<a.h)ph*=2;
   glquad(legaltexture,(width-a.w*s)/2,height-a.h*s,a.w*s,a.h*s,float(a.w)/pw,float(a.h)/ph);
  }
 }
 if(clear==1&&pointerx>=0&&pointery>=0&&pointerx<width&&pointery<height)
  emcursor::draw(pointerx,pointery,v.scale);
 glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
extern "C" int em_write_shot(const char *path,int width,int height){
 if(!load()||!v.w)return 0;em_draw(width,height,1);std::vector<unsigned char> data(width*height*4),top(data.size());glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,data.data());for(int y=0;y<height;y++)memcpy(&top[y*width*4],&data[(height-1-y)*width*4],width*4);return png_write_rgba(path,top.data(),width,height);
}
extern "C" void em_reset(void){v=View();option_title=false;sound_hover.clear();}
extern "C" void em_layout_pass(int on){layout_only=on!=0;}
extern "C" unsigned em_raster_count(void){return em_rasters_n;}
extern "C" unsigned em_upload_count(void){return em_uploads_n;}
/* Red Alert's kit for this process, before the first screen asks for the pack: the version
   line and the legal notice its screens print. */
extern "C" void em_set_red_alert(const char *version,const char *legal){
 if(!theme.ra){theme.ra=true;loadstate=0;arts.clear();}
 theme.version=version?version:"";theme.legal=legal?legal:"";
}
extern "C" int em_red_alert(void){return theme.ra;}
extern "C" void em_set_mission_side(int soviet){theme.soviet=soviet!=0;}
/* RED ALERT'S ROUTES THAT ARE NOT IN THE BUILD YET. Their buttons stay live and answer with
   a notice saying what is missing, until the engine side of each is connected and the host
   marks it ready. Tiberian Dawn asks none of this: every route is its own and ready. */
namespace {bool route_ready[EM_ROUTE_COUNT]={};}
extern "C" void em_set_route_ready(int route,int ready){if(route>=0&&route<EM_ROUTE_COUNT)route_ready[route]=ready!=0;}
extern "C" int em_route_ready(int route){return !theme.ra||(route>=0&&route<EM_ROUTE_COUNT&&route_ready[route]);}
extern "C" const char *em_route_notice(int route){
 switch(route){
 case EM_ROUTE_SAVES:return "Red Alert saves are kept apart from Tiberian Dawn's. That split is not in this build yet, so saving and loading wait for it.";
 case EM_ROUTE_SKIRMISH:return "Skirmish is not in this Red Alert build yet: it waits for Red Alert's countries, colours, tech levels and ore.";
 case EM_ROUTE_NETWORK:return "Multiplayer is not in this Red Alert build yet: its lobby room does not list Red Alert's maps and countries yet.";
 case EM_ROUTE_USERMAPS:return "Red Alert user maps cannot be started in this build yet.";
 default:return "";}
}
namespace {
/* The pause page's Load, Save and Delete open the slot list; for Red Alert, until its saves
   have their own index, the notice instead. */
bool saves_blocked(const DOPT_State *st,int item){return st->page==DOPT_PAGE_OPTIONS&&(item==DOPT_LOAD||item==DOPT_SAVE||item==DOPT_DELETE)&&!em_route_ready(EM_ROUTE_SAVES);}
}

namespace {
/* Both games use the same live world-detail switches. A checkbox reports the
   renderer's value; it must never substitute an unchecked, disabled seat. */
bool ra_unavailable(int item){(void)item;return false;}
const char *vis_label(int item){return theme.ra&&item==DOPT_VE_TIB3D?"3D Ore":dopt_vis_elem_label(item);}
/* Vertical sync has a third state, adaptive, ticked like on; the label is what tells them apart. */
void optcheck(DOPT_State *st,int item,int x,int y){Seat &q=opt(st,item,item==DOPT_VE_VSYNC&&st->vis.vsync==2?"Adaptive sync":vis_label(item),x,y,424,36,CHECK);q.action=OPTITEM;
 q.value=item==DOPT_VE_VSYNC?st->vis.vsync:item==DOPT_VE_FPS?st->vis.fps:st->vis.prof[st->advTab].elem[item];q.active=false;
 if(ra_unavailable(item)){q.disabled=true;q.value=0;}}
void optdrop(DOPT_State *st,int item,int x,int y){text(dopt_vis_elem_label(item),x,y-26);Seat &q=opt(st,item,dopt_drop_text(st,item,dopt_enh_drop_value(st,item)),x,y,424);q.action=DROP;q.arg=item;q.active=v.drop==item;}
void optrange(DOPT_State *st,int item,const char *label,int x,int y,int w,int value,int max,bool percent){
 text(label,x,y-28);std::string val=percent?std::to_string(value*100/max)+"%":(st->page==DOPT_PAGE_CONTROLS&&item==DOPT_C_SPEED)?dopt_speed_name(value):value==4?"Normal":std::to_string(value+1);
 text(val,x+w-textwidth(val),y-28,ACCENT);Seat &q=opt(st,item,label,x,y,w,36,RANGE);q.action=RANGESET;q.value=value;q.max=max;q.active=false;
}
void optback(DOPT_State *st,int x=550,int y=-1){Seat &q=add(-1,"Back",x,y<0?v.h-86:y,192);q.action=BACK;}
void optreset(){Seat &q=add(-1,"Reset to Defaults",38,v.h-86,292);q.action=RESET;}
/* A slider of the Settings screen's Gameplay and Audio tabs (dopt_enh_setting). Their seats
   are numbered EM_SETTING_SEAT - which and EM_GAMEPLAY_SEAT - toggle, so the gate can find
   them by number (em_item_rect) as it finds every other seat. */
/* The three scroll speeds read "Normal" at their default (index 3) and a step number
   elsewhere, 1 slowest to 10 fastest. */
bool scrollsetting(int which){return which==DOPT_ES_SCROLL||which==DOPT_ES_SCROLLPUSH||which==DOPT_ES_SCROLLGRAB;}
void setrange(DOPT_State *st,int which,const char *label,int x,int y,int w,bool percent){
 const int value=dopt_enh_setting_value(st,which),max=dopt_enh_setting_max(which);
 text(label,x,y-28);std::string val=percent?std::to_string(value*100/max)+"%":which==DOPT_ES_SPEED?dopt_speed_name(value):scrollsetting(which)?(value==3?"Normal":std::to_string(value+1)):value==4?"Normal":std::to_string(value+1);
 text(val,x+w-textwidth(val),y-28,ACCENT);Seat &q=add(EM_SETTING_SEAT-which,label,x,y,w,36,RANGE);q.action=SETRANGE;q.arg=which;q.value=value;q.max=max;q.active=false;
}
/* THE LANGUAGE ROW AND ITS BOX (EM_LangHooks, answered by the game). Picking a language
   whose pack is on the disk uses it at once; picking one that is not opens the box, which
   says what will be fetched and how much, fetches it with a bar on the screen when asked,
   and uses it the moment it is complete. Cancelling keeps what arrived, and the next try
   resumes it. */
std::string megabytes(long long b){char t[32];snprintf(t,sizeof t,"%.0f MB",double(b)/1e6+0.5);return t;}
void langrow(int x,int y){
 if(!langhooks||langhooks->count()<2)return;
 text("Language",x,y-28);const int cur=langhooks->current();
 Seat &q=add(EM_LANG_SEAT,langhooks->name(cur),x,y,424);q.action=LANGDROP;q.active=v.drop==EM_LANG_DROP;
 if(v.drop==EM_LANG_DROP){const int n=langhooks->count();int owner=int(v.seats.size())-1,top=y+44;
  if(top+n*42+8>v.h-106)top=std::max(78,y-n*42-12);popup(owner,x-4,top-4,432,n*42+8);
  for(int i=0;i<n;i++){std::string label=langhooks->name(i);if(!langhooks->installed(i))label+="  (download)";
   Seat &e=add(EM_LANG_SEAT-1-i,label,x,top+i*42,424);e.action=LANGPICK;e.arg=i;e.active=i==cur;}}
}
void langbox(){
 const int i=v.langBox;long long done=0,total=0;char msg[256]="";
 const int st=langhooks->progress(&done,&total,msg,sizeof msg);
 const std::string name=langhooks->name(i);
 well(("LANGUAGE: "+name).c_str(),38,160,940,358-(v.compact?72:0));
 if(!v.langStarted){
  note(name+": the announcer, the unit voices, the briefings, the names of what you build and the movies, "
       "from the 1995 discs. The movies that release cut or changed stay in English, and so do the soldiers.",56,218,900,4,TEXT);
  text("To download: "+megabytes(langhooks->missing(i)),56,v.compact?330:352,ACCENT);
  Seat &go=add(EM_LANG_SEAT-20,"Download",56,v.compact?370:400,220);go.action=LANGGO;go.arg=i;
  Seat &no=add(EM_LANG_SEAT-21,"Not now",292,v.compact?370:400,220);no.action=LANGCLOSE;return;}
 if(st==1){/* running: the bar */
  const int bx=56,by=v.compact?286:300,bw=904,bh=24;rect(bx,by,bw,bh,0x101310);
  const int fill=total>0?int(double(bw-4)*double(done)/double(total)):0;rect(bx+2,by+2,std::max(0,std::min(bw-4,fill)),bh-4,theme.ra?0xa01c14:0x3a8a2a);
  text("Downloading "+name+": "+megabytes(done)+" of "+megabytes(total),56,by-36,TEXT);
  Seat &c=add(EM_LANG_SEAT-22,"Cancel",56,v.compact?370:400,220);c.action=LANGCANCEL;return;}
 if(st==2){if(langhooks->current()!=i)langhooks->choose(i);
  note(name+" is ready and in use: the voices now, the names and briefings from the next mission.",56,218,900,3,TEXT);
  Seat &ok=add(EM_LANG_SEAT-21,"OK",56,v.compact?370:400,220);ok.action=LANGCLOSE;return;}
 note(msg[0]?msg:"The download stopped.",56,218,900,3,TEXT);
 Seat &again=add(EM_LANG_SEAT-20,"Try again",56,v.compact?370:400,220);again.action=LANGGO;again.arg=i;
 Seat &no=add(EM_LANG_SEAT-21,"Close",292,v.compact?370:400,220);no.action=LANGCLOSE;
}

/* THE MOUSE SCHEME, three buttons under their label: Classic (the 1995 scheme), Left-handed
   (its buttons exchanged) and Modern (left selects, right orders). The same row on the
   Settings screen's Gameplay tab and on the pause menu's Gameplay page. */
void schemerow(DOPT_State *st,int x,int y){
 /* "Left hand" rather than fx_input.h's "Left-handed": the longer word does not fit the
    140 wide button art both kits carry, and three of those are what the well holds */
 static const char *const names[FX_MOUSE_COUNT]={"Classic","Left hand","Modern"};
 text("Mouse scheme",x,y);
 for(int i=0;i<FX_MOUSE_COUNT;i++){Seat &q=add(EM_SCHEME_SEAT-i,names[i],x+i*144,y+22,140);q.action=GPSCHEME;q.arg=i;q.active=st->gp.on[DOPT_G_MOUSE]==i;}
}
/* THE KEY BINDINGS PAGE. Every command key in two columns, a box per key; a press on a box
   makes it wait for the next key, which the game checks (a key the game reserves is
   refused, a key another command holds swaps with it) and the header says what happened.
   Under it, the mouse button that drags the map. The rows are fx_input.h's commands, the
   two guard keys and the four camera bookmarks each sharing a row. Buttons are sizes both
   kits carry art for (154 x 40). */
struct KeyRow{const char *label;int cmd[4];int n;};
const KeyRow keyrows[]={
 {"Stop",{FXK_STOP},1},{"Scatter",{FXK_SCATTER},1},{"Guard",{FXK_GUARD,FXK_GUARD2},2},
 {"Attack-move",{FXK_ATTACKMOVE},1},{"Go to base",{FXK_BASE},1},{"Rebuild last",{FXK_REBUILD},1},
 {"Radar on or off",{FXK_RADAR},1},{"Camera view",{FXK_CAMERA},1},
 {"Next graphics mode",{FXK_NEXTMODE},1},{"Next unit",{FXK_NEXT},1},{"Previous unit",{FXK_PREV},1},
 {"Select all on screen",{FXK_SELECTVIEW},1},{"Repair mode",{FXK_REPAIR},1},{"Sell mode",{FXK_SELL},1},
 {"Centre on selection",{FXK_CENTRE},1},{"Bookmarks",{FXK_VIEW1,FXK_VIEW2,FXK_VIEW3,FXK_VIEW4},4}};
void keyspage(){
 const int nrows=int(sizeof keyrows/sizeof keyrows[0]),half=(nrows+1)/2;
 const int dragh=56,dragy=v.h-106-dragh,keyh=dragy-8-140;
 const int pitch=v.compact?25:32,boxh=pitch-4,top=140+44;
 well("COMMAND KEYS",38,140,940,keyh);
 std::string head=kbcap>=0?std::string("Press the new key for ")+FX_KEYS[kbcap].label+". Escape cancels."
                 :!kbmsg.empty()?kbmsg:"Click a box, then a key. Ctrl + bookmark key stores the view.";
 head=fitted(head,960-(56+textwidth("COMMAND KEYS")+24));text(head,960-textwidth(head),154,kbcap>=0?ACCENT:MUTED);
 for(int r=0;r<nrows;r++){
  const KeyRow &k=keyrows[r];const int x=r<half?56:516,y=top+(r%half)*pitch,colw=444;
  const int bw=k.n==1?150:k.n==2?100:66,gap=6,bx0=x+colw-(k.n*bw+(k.n-1)*gap);
  text(fitted(k.label,bx0-x-12),x,y+(boxh-16)/2);
  for(int j=0;j<k.n;j++){const int c=k.cmd[j],bx=bx0+j*(bw+gap);
   rect(bx,y,bw,boxh,theme.ra?0x060303:0x080a09);rect(bx,y,bw,1,theme.ra?0x807870:0x606255);rect(bx,y+boxh-1,bw,1,theme.ra?0x342826:0x353b30);
   const char *name=keyhooks->key(c);Seat &q=add(EM_KEYS_SEAT-c,kbcap==c?"?":(name&&*name?name:"-"),bx,y,bw,boxh,ROW);
   q.sourceLabel=std::string("Key: ")+FX_KEYS[c].label;q.action=KEYCAP;q.arg=c;q.active=kbcap==c;}
 }
 well("",38,dragy,940,dragh);
 text("Drag the map with",56,dragy+(dragh-16)/2);
 const int g=keyhooks->grab();
 for(int b=0;b<FX_GRAB_COUNT;b++){Seat &q=add(EM_KEYS_DRAG-b,FX_GRAB_NAME[b],326+b*160,dragy+(dragh-40)/2,154);q.action=KEYDRAG;q.arg=b;q.active=g==b;}
}

/* SETTINGS, ONE SCREEN FOR EVERYTHING. The title's Settings opened the visuals alone; game
   speed, scrolling, the mouse and every volume were only in the pause menu, so a player
   could not set them before a mission. The tab row now runs Display, World, Effects and
   Interface (the picture) and then Gameplay and Audio, on the same page: those two write
   the block the pause menu's Game Controls and Sound Controls write and are remembered
   with it, and OK, Back and Escape close the screen from any tab as before. More is the
   picture's own overflow and is reached from its button. */
void visuals(DOPT_State *st){
 static const char *cats[]={"Display","World","Effects","Interface","More","Gameplay","Audio"};
 static const int tabs[]={0,1,2,3,5,6};
 const int shrink=v.compact?72:0;
 // More retains source settings absent from the review fixture, without dropping them.
 for(int i=0;i<6;i++){Seat &q=add(-1,cats[tabs[i]],38+i*158,86,148);q.action=TAB;q.arg=tabs[i];q.active=v.section==tabs[i]||(v.section==7&&tabs[i]==5);}
 /* a box left waiting for a key stops waiting when the page it is on goes */
 if(v.section!=7)kbcap=-1;
 const char *modes[]={"Classic","N64","Enhanced"};
 if(v.section==0){
  well("DISPLAY",38,160,460,358-shrink);well("GRAPHICS MODE",518,160,460,180);well("OUTPUT",518,360,460,158-shrink);
  int dispitems[]={DOPT_VE_FULLSCREEN,DOPT_VE_WINDOWED,DOPT_VE_BORDERLESS};const char *disps[]={"Fullscreen","Windowed","Borderless"};int dispvalues[]={2,0,1};
  for(int i=0;i<3;i++){Seat &q=opt(st,dispitems[i],disps[i],54+i*144,208,140);q.action=OPTITEM;q.active=st->vis.dispmode==dispvalues[i];}
  optdrop(st,DOPT_VE_RESOLUTION,56,v.compact?272:294);optcheck(st,DOPT_VE_VSYNC,56,v.compact?330:356);optcheck(st,DOPT_VE_FPS,56,v.compact?382:408);
  if(st->vis.drawable_w>0&&st->vis.drawable_h>0)text("Current: "+std::to_string(st->vis.drawable_w)+" X "+std::to_string(st->vis.drawable_h),56,v.compact?426:476,MUTED);
  for(int i=0;i<3;i++){Seat &q=add(-1,modes[i],534+i*144,208,140);q.action=MODE;q.arg=i;q.active=st->vis.mode==i;q.disabled=i==1&&st->n64_unavailable;}
  text(std::string(modes[st->vis.mode])+" presentation",536,278,MUTED);
  optcheck(st,DOPT_VE_GAMMA,536,406);
 }else if(v.section==1){
  well("SURFACES",38,160,460,358-shrink);well("OBJECTS",518,160,460,358-shrink);
  optdrop(st,DOPT_VE_TEXSET,56,244);optcheck(st,DOPT_VE_WATER,56,v.compact?300:314);optcheck(st,DOPT_VE_BILINEAR,56,v.compact?344:366);
  optdrop(st,DOPT_VE_INFSET,536,244);optcheck(st,DOPT_VE_TREES,536,v.compact?300:314);optcheck(st,DOPT_VE_GRASS,536,v.compact?344:366);optcheck(st,DOPT_VE_TIB3D,536,v.compact?388:418);

 }else if(v.section==2){
  well("LIGHTING",38,160,460,358-shrink);well("POST PROCESSING",518,160,460,358-shrink);
  int left[]={DOPT_VE_SHADOWS,DOPT_VE_OCCLUSION,DOPT_VE_LIGHT},right[]={DOPT_VE_BLOOM,DOPT_VE_GRADE,DOPT_VE_CRT};
  for(int i=0;i<3;i++){optcheck(st,left[i],56,210+i*52);optcheck(st,right[i],536,210+i*52);}
 }else if(v.section==3){
  well("INTERFACE",38,160,460,358-shrink);well("CAMERA",518,160,460,358-shrink);
  optcheck(st,DOPT_VE_NEWHUD,56,210);optcheck(st,DOPT_VE_SMOOTH,56,262);optdrop(st,DOPT_VE_UISCALE,56,362);optdrop(st,DOPT_VE_PERSPECTIVE,536,244);
  text("Classic: top-down view",536,326,MUTED);text("Isometric: angled view",536,358,MUTED);

 }else if(v.section==4){
  well("WORLD DETAIL",38,160,460,358-shrink);well("TERRAIN DETAIL",518,160,460,358-shrink);
  int a[]={DOPT_VE_FOLIAGE,DOPT_VE_ROCKS};
  for(int detailIndex=0;detailIndex<2;detailIndex++)optcheck(st,a[detailIndex],56,210+detailIndex*52);
  optcheck(st,DOPT_VE_PATCHES,536,210);
 }else if(v.section==5&&v.langBox>=0&&langhooks){
  langbox();
 }else if(v.section==5){
  well("GAME",38,160,460,358-shrink);well("INPUT",518,160,460,358-shrink);
  /* the game speed and the three scroll speeds, one per way of moving the view */
  /* (four rows below the well's title: at the title screen's shorter frame they sit
     closer, and neither size has room left for the Slower and Faster captions) */
  const int y0=v.compact?234:252,dy=v.compact?58:74;
  setrange(st,DOPT_ES_SPEED,"Game speed",56,y0,424,false);
  setrange(st,DOPT_ES_SCROLL,"Screen edge and arrow keys",56,y0+dy,424,false);
  setrange(st,DOPT_ES_SCROLLPUSH,"Right-button drag",56,y0+2*dy,424,false);
  setrange(st,DOPT_ES_SCROLLGRAB,"Middle-button drag",56,y0+3*dy,424,false);
  schemerow(st,536,v.compact?210:214);
  for(int i=1;i<DOPT_G_TOGGLES;i++){Seat &q=add(EM_GAMEPLAY_SEAT-i,dopt_gp_label(i),536,v.compact?238+i*46:244+i*52,424,36,CHECK);q.action=GPCHECK;q.arg=i;q.value=st->gp.on[i];q.active=false;}
  langrow(536,v.compact?400:430);
 }else if(v.section==7&&keyhooks){
  keyspage();
 }else{
  well("VOLUME",38,160,460,358-shrink);well("VOICES AND WORLD",518,160,460,358-shrink);
  setrange(st,DOPT_ES_MUSIC,"Music",56,v.compact?238:252,424,true);
  setrange(st,DOPT_ES_SOUND,"Sound effects",56,v.compact?326:352,424,true);
  setrange(st,DOPT_ES_VOICES,"Voices",536,v.compact?238:252,424,true);
  text("The announcer's reports",536,v.compact?286:300,MUTED);
  setrange(st,DOPT_ES_AMBIENCE,"Ambience",536,v.compact?362:390,424,true);
 }
 if(v.section<4){Seat &q=add(-1,"More Settings",342,v.h-86,192);q.action=TAB;q.arg=4;}
 if(v.section==5&&v.langBox<0&&keyhooks){Seat &q=add(EM_KEYS_OPEN,"Key Bindings",342,v.h-86,192);q.action=TAB;q.arg=7;}
 optreset();optback(st);opt(st,DOPT_A_OK,"OK",758,v.h-86,220);
 if(v.drop>=0){
  int owner=-1;for(int i=0;i<int(v.seats.size());i++)if(v.seats[i].action==DROP&&v.seats[i].arg==v.drop)owner=i;
  if(owner>=0){Seat q=v.seats[owner];int count=dopt_drop_count(st,v.drop),start=v.drop==DOPT_VE_RESOLUTION?st->restop:0;
   int available=0;for(int i=start;i<count;i++)if(dopt_drop_listed(v.drop,i))available++;
   const int shown=std::min(available,7);
   const int hintHeight=count>7?30:0;
   int y=q.y+44;if(y+shown*42+8+hintHeight>v.h-106)y=std::max(78,q.y-shown*42-12-hintHeight);
   popup(owner,q.x-4,y-4,q.w+8,shown*42+8+hintHeight);
   for(int val=start,row=0;val<count&&row<shown;val++){if(!dopt_drop_listed(v.drop,val))continue;Seat &e=add(v.drop,dopt_drop_text(st,v.drop,val),q.x,y+row*42,q.w);row++;e.action=DROPCHOICE;e.arg=val;e.active=val==(v.popupChoice>=0?v.popupChoice:dopt_enh_drop_value(st,v.drop));e.disabled=!dopt_enh_drop_allowed(st,v.drop,val);}
   if(count>7)v.popupHint="Mouse wheel: more resolutions";
  }
 }
}
}
extern "C" void em_options(DOPT_State *st){
 if(!load())return;int oldpage=st->page;
 if(st->page==DOPT_PAGE_VISUALS){st->page=DOPT_PAGE_ADVANCED;st->advTab=st->vis.mode;st->advTop=0;}
 const char *frame="visuals-frame",*title="OPTIONS";
 switch(st->page){case DOPT_PAGE_OPTIONS:frame="pause-frame";break;case DOPT_PAGE_CONTROLS:title="GAME CONTROLS";break;
 case DOPT_PAGE_GAMEPLAY:title="GAMEPLAY";break;case DOPT_PAGE_ADVANCED:title="SETTINGS";break;
 case DOPT_PAGE_SLOTS:title=st->sl.mode==DOPT_SL_SAVE?"SAVE MISSION":st->sl.mode==DOPT_SL_DELETE?"DELETE MISSION":"LOAD MISSION";break;
 case DOPT_PAGE_CONFIRM:frame="confirm-frame";title=st->cfkind==2?"EXIT GAME":st->cfkind==1?"DELETE MISSION":st->match?"LEAVE MATCH":"ABORT MISSION";break;
 case DOPT_PAGE_RESTATE:frame="settings-frame";title="MISSION BRIEFING";break;
 case DOPT_PAGE_NOTICE:frame="confirm-frame";title=st->ncaption;break;
 case DOPT_PAGE_SOUND:title="AUDIO";break;case DOPT_PAGE_CHEATS:title="CHEATS";break;}
 begin(OPT,st,frame,title);if(oldpage==DOPT_PAGE_VISUALS)v.section=0;
 if(v.page!=st->page){v.held=v.drag=-1;v.drop=v.popupChoice=-1;v.page=st->page;}
  if(st->page==DOPT_PAGE_OPTIONS){
  text("MISSION PAUSED",354,64,ACCENT,true);text("MISSION",36,110,MUTED);text("SETTINGS",362,110,MUTED);
  int a[]={DOPT_LOAD,DOPT_SAVE,DOPT_DELETE,DOPT_RESTATE},b[]={DOPT_GAME,DOPT_VISUALS,DOPT_GAMEPLAY,DOPT_AUDIO};
  for(int i=0;i<4;i++)opt(st,a[i],dopt_item_label(st,a[i]),36,136+i*48,310);
  for(int i=0;i<4;i++)opt(st,b[i],dopt_item_label(st,b[i]),362,136+i*48,310);
  opt(st,DOPT_RESUME,"Resume Mission",36,410,204).active=true;
  opt(st,DOPT_ABORT,dopt_abort_label(st),252,410,204);opt(st,DOPT_EXIT,"Exit Game",468,410,204);
 }else if(st->page==DOPT_PAGE_CONTROLS){
  /* (the volumes and the jukebox are the pause menu's Audio page) */
  well("GAMEPLAY",38,110,940,v.h-228);
  /* THE THREE SCROLL SPEEDS are the Settings screen's own sliders (setrange): one block
     of settings, two pages that show it. The game speed keeps the 1995 slider. */
  const int y0=v.compact?168:192,dy=v.compact?64:76;
  optrange(st,DOPT_C_SPEED,"Game speed",56,y0,904,st->set.speed,6,false);
  setrange(st,DOPT_ES_SCROLL,"Screen edge and arrow keys",56,y0+dy,904,false);
  setrange(st,DOPT_ES_SCROLLPUSH,"Right-button drag",56,y0+2*dy,904,false);
  setrange(st,DOPT_ES_SCROLLGRAB,"Middle-button drag",56,y0+3*dy,904,false);
  if(!v.compact){text("Slower",56,y0+3*dy+44,MUTED);text("Faster",894,y0+3*dy+44,MUTED);}
  optreset();optback(st);opt(st,DOPT_C_OK,"OK",758,v.h-86,220);
 }else if(st->page==DOPT_PAGE_GAMEPLAY){
  well("INPUT",38,110,460,v.h-228);well("FEEDBACK",518,110,460,v.h-228);
  schemerow(st,56,170);
  for(int i=1;i<DOPT_G_TOGGLES;i++){Seat &q=opt(st,i,dopt_gp_label(i),i==2?536:56,i==1?266:202,424,36,CHECK);q.value=st->gp.on[i];q.active=false;}
  optback(st,38);opt(st,DOPT_G_OK,"OK",758,v.h-86,220);
 }else if(st->page==DOPT_PAGE_ADVANCED){visuals(st);
 }else if(st->page==DOPT_PAGE_SLOTS){
  well("SAVED MISSIONS",38,110,600,v.h-228);well("MISSION",658,110,320,v.h-228);
  int rows=st->sl.mode==DOPT_SL_SAVE?9:10;
  int start=std::max(0,std::min(st->sl.top,std::max(0,st->sl.nrows-rows)));
  /* An empty list has no selection (-1), which is never a row index. */
  if(st->sl.sel>=0&&st->sl.sel<start)start=st->sl.sel;if(st->sl.sel>=start+rows)start=st->sl.sel-rows+1;
  for(int i=0;i<rows&&start+i<st->sl.nrows;i++){Seat &q=add(DOPT_SL_LIST,dopt_slot_row_text(st,start+i),56,164+i*(v.compact?24:32),564,v.compact?24:32,ROW,OPTROW,start+i);q.active=st->sl.sel==start+i;q.muted=st->sl.mode==DOPT_SL_LOAD&&st->sl.rows[start+i].disabled;}
  if(!st->sl.nrows)text("No saved missions",56,178,MUTED);
  /* Every slot taken: Save names no row until the player picks the one to replace. */
  if(st->sl.mode==DOPT_SL_SAVE&&st->sl.sel<0)note("Every save slot is in use. Choose a saved mission to replace.",676,186,284,6,MUTED);
  else note(st->sl.descr,676,186,284,6,TEXT);
  if(st->sl.mode==DOPT_SL_SAVE){text("Mission name",676,374);Seat &q=opt(st,DOPT_SL_EDIT,st->sl.descr,676,398,284,40,FIELD);q.active=st->selected==DOPT_SL_EDIT;}
  footer(st->sl.mode==DOPT_SL_SAVE?"Save":st->sl.mode==DOPT_SL_DELETE?"Delete":"Load",DOPT_SL_OK,DOPT_SL_CANCEL,st);
 }else if(st->page==DOPT_PAGE_CONFIRM){
  text(dopt_confirm_msg(st),338,136,TEXT,true);const char *msg2=dopt_confirm_msg2(st);if(msg2&&*msg2)text(msg2,338,162,TEXT,true);
  if(!st->match&&st->cfkind==0){opt(st,DOPT_CF_CANCEL,"Cancel",38,238,192);opt(st,DOPT_CF_RESTART,"Restart",242,238,192);opt(st,DOPT_CF_ABORT,dopt_confirm_yes(st),446,238,192);}
  else{opt(st,DOPT_CF_CANCEL,dopt_confirm_no(st),98,238,220);opt(st,DOPT_CF_ABORT,dopt_confirm_yes(st),358,238,220);}
 }else if(st->page==DOPT_PAGE_RESTATE){
  well("MISSION OBJECTIVE",38,110,600,v.h-234);
  /* Red Alert puts its side's briefing paper behind this box (ALIPAPER or SOVPAPER, the
     side of the mission being played, as the 1996 Restate_Mission does), at 62% under the
     text. */
  if(theme.ra){auto it=arts.find(theme.soviet?"paper-soviet":"paper-allied");
   if(it!=arts.end()&&!recording&&++writes){const Art &a=it->second;for(int yy=44;yy<std::min(a.h,v.h-234)-4;yy++)memcpy(&v.rgba[((110+yy)*v.w+38+4)*4],&a.pixels[(yy*a.w+4)*4],(std::min(a.w,600)-8)*4);}}
  note(st->brief,56,178,564,10,TEXT);
  if(st->rvideo)opt(st,DOPT_R_LEFT,"Video",38,v.h-86,192);opt(st,st->rvideo?DOPT_R_RIGHT:DOPT_R_LEFT,"Back",446,v.h-86,192);
 }else if(st->page==DOPT_PAGE_NOTICE){
  /* Four lines at the usual pitch; a longer text (a file's path to send) closes the
     pitch to fit six above the button rather than losing its end. */
  if(wrapped(st->brief,480).size()<=4)note(st->brief,98,112,480,4,TEXT);else note(st->brief,98,108,480,6,TEXT,20);
  opt(st,0,"OK",228,238,220);
 }else if(st->page==DOPT_PAGE_SOUND){
  well("JUKEBOX",38,110,600,v.h-228);well("VOLUME",658,110,320,v.h-228);
  for(int i=0;i<9&&st->trkTop+i<st->ntracks;i++){int ix=st->trkTop+i;Seat &q=add(DOPT_S_LIST,st->tracks[ix].fullname?st->tracks[ix].fullname:st->tracks[ix].base,56,164+i*(v.compact?24:32),564,v.compact?24:32,ROW,OPTROW,ix);q.active=st->trkSel==ix;}
  optrange(st,DOPT_S_MUSIC,"Music",676,v.compact?188:210,284,st->set.music,239,true);optrange(st,DOPT_S_SOUND,"Sound effects",676,v.compact?256:290,284,st->set.sound,239,true);
  optrange(st,DOPT_S_SPEECH,"Voices",676,v.compact?324:370,284,st->set.speech,239,true);optrange(st,DOPT_S_AMBIENCE,"Ambience",676,v.compact?392:450,284,st->set.ambience,239,true);
  opt(st,DOPT_S_PLAY,"Play",38,v.h-86,192).icon=theme.ra?"btn-pl":"";opt(st,DOPT_S_STOP,"Stop",242,v.h-86,192).icon=theme.ra?"btn-st":"";opt(st,DOPT_S_OK,"Back",758,v.h-86,220);
  Seat &a=opt(st,DOPT_S_SHUFFLE,"Shuffle",56,v.compact?396:468,268,36,CHECK);a.value=st->shuffle;a.active=false;Seat &b=opt(st,DOPT_S_REPEAT,"Repeat",352,v.compact?396:468,268,36,CHECK);b.value=st->repeat;b.active=false;
 }else if(st->page==DOPT_PAGE_CHEATS){
  well("SANDBOX",38,110,940,v.h-228);for(int i=0;i<DOPT_CH_TOGGLES;i++){Seat &q=opt(st,i,dopt_cheat_label(i),i<4?56:536,178+(i%4)*60,424,36,CHECK);q.value=st->cheat.on[i];q.active=false;}
  opt(st,DOPT_CH_WIN,"Win",38,v.h-86,192);opt(st,DOPT_CH_LOSE,"Lose",242,v.h-86,192);opt(st,DOPT_CH_RESET,"Reset",550,v.h-86,192);opt(st,DOPT_CH_OK,"OK",758,v.h-86,220);
 }
 // Every regular native button invokes the source controller by item, not old geometry.
 for(Seat &q:v.seats)if(!q.action){q.action=OPTITEM;q.arg=q.item;}
 paint();
}

extern "C" int em_opt_press(DOPT_State *st,int x,int y){
 int n=virtualseat(x);
 if(v.popupFirst>=0&&(n<0||v.seats[n].disabled)){
  v.held=v.drag=-1;st->pressed=-1;
  if(!popup_contains(v.mousex,v.mousey)){v.drop=-1;st->texdrop=-1;}
  return 0;
 }
 if(n<0)return dopt_press(st,x,y);Seat q=v.seats[n];v.held=n;
 if(q.kind==FIELD){st->selected=q.item;st->pressed=-1;return 0;}
 if(q.action==RANGESET){v.drag=n;int val=q.min+(q.max-q.min)*std::max(0,std::min(1000,y))/1000;dopt_enh_control(st,q.item,val);st->selected=q.item;}
 if(q.action==SETRANGE){v.drag=n;dopt_enh_setting(st,q.arg,q.min+(q.max-q.min)*std::max(0,std::min(1000,y))/1000);}
 return 0;
}
extern "C" int em_opt_motion(DOPT_State *st,int x,int y){
 int n=virtualseat(x);if(n>=0){if(v.drag==n){const Seat &q=v.seats[n];const int val=q.min+(q.max-q.min)*std::max(0,std::min(1000,y))/1000;if(q.action==SETRANGE)dopt_enh_setting(st,q.arg,val);else dopt_enh_control(st,q.item,val);}return 0;}return v.popupFirst>=0?0:dopt_motion(st,x,y);
}
extern "C" int em_opt_release(DOPT_State *st,int x,int y){
 int n=virtualseat(x),held=v.held;v.held=v.drag=-1;
 /* any other press ends a box's wait for a key */
 if(kbcap>=0&&(n<0||v.seats[n].action!=KEYCAP)){kbmsg=std::string(FX_KEYS[kbcap].label)+": unchanged";kbcap=-1;}
 if(n<0)return v.popupFirst>=0?0:dopt_release(st,x,y);if(n!=held)return 0;Seat q=v.seats[n];
 switch(q.action){
 case KEYCAP:kbcap=kbcap==q.arg?-1:q.arg;kbmsg.clear();return 0;
 case KEYDRAG:if(keyhooks)keyhooks->set_grab(q.arg);return 0;
 case GPSCHEME:dopt_enh_gameplay_set(st,DOPT_G_MOUSE,q.arg);return 0;
 case RANGESET:case SETRANGE:return 0;
 case GPCHECK:dopt_enh_gameplay_toggle(st,q.arg);return 0;
 case LANGDROP:v.drop=v.drop==EM_LANG_DROP?-1:EM_LANG_DROP;return 0;
 case LANGPICK:v.drop=-1;if(langhooks){if(langhooks->installed(q.arg))langhooks->choose(q.arg);else{v.langBox=q.arg;v.langStarted=false;}}return 0;
 case LANGGO:if(langhooks)langhooks->download(q.arg);v.langStarted=true;return 0;
 case LANGCANCEL:if(langhooks)langhooks->cancel();return 0;
 case LANGCLOSE:v.langBox=-1;v.langStarted=false;return 0;
 case OPTITEM:if(st->page==DOPT_PAGE_OPTIONS&&q.item==DOPT_EXIT){dopt_enh_confirm_exit(st);return 0;}
  if(saves_blocked(st,q.item)){st->selected=q.item;dopt_open_page_notice(st,"NOTICE",em_route_notice(EM_ROUTE_SAVES));return 0;}
  return dopt_enh_action(st,q.item);
 case TAB:v.section=q.arg;v.drop=-1;if(q.arg!=7)kbcap=-1;kbmsg.clear();return 0;
 case BACK:st->texdrop=-1;v.drop=-1;return em_opt_key(st,DOPT_KEY_ESC);
 case RESET:if(st->page==DOPT_PAGE_CONTROLS){DOPT_Settings d;dopt_settings_init(&d);st->set.speed=d.speed;st->set.scrollrate=d.scrollrate;st->set.scrollpush=d.scrollpush;st->set.scrollgrab=d.scrollgrab;if(st->bind.apply)st->bind.apply(st->bind.user,&st->set);}
  /* (the Gameplay and Audio tabs reset their own settings, never the picture's) */
  else if(st->page==DOPT_PAGE_ADVANCED&&v.section==7){if(keyhooks){keyhooks->reset();kbcap=-1;kbmsg="Every key and the drag button are back to their defaults";}}
  else if(st->page==DOPT_PAGE_ADVANCED&&v.section>=5){DOPT_State d=*st;dopt_settings_init(&d.set);
   const int game[]={DOPT_ES_SPEED,DOPT_ES_SCROLL,DOPT_ES_SCROLLPUSH,DOPT_ES_SCROLLGRAB},sound[]={DOPT_ES_MUSIC,DOPT_ES_SOUND,DOPT_ES_VOICES,DOPT_ES_AMBIENCE};
   const int *w=v.section==5?game:sound,n=4;for(int k=0;k<n;k++)dopt_enh_setting(st,w[k],dopt_enh_setting_value(&d,w[k]));}
  else dopt_enh_action(st,DOPT_A_RESET);return 0;
 case MODE:dopt_enh_mode(st,q.arg);v.drop=-1;return 0;
 case DROP:v.drop=v.drop==q.arg?-1:q.arg;v.popupChoice=-1;st->texdrop=v.drop;st->restop=0;return 0;
 case DROPCHOICE:dopt_enh_choose(st,q.item,q.arg);v.drop=-1;return 0;
 case OPTROW:dopt_enh_row(st,q.arg);return 0;
 default:return 0;
 }
}

namespace {
Seat &skbutton(const SK_State *st,int item,const std::string &label,int x,int y,int w,int h=40,int kind=BUTTON){Seat &q=sk(st,item,label,x,y,w,h,kind);q.action=SKITEM;q.arg=item;return q;}
/* Red Alert's sides, its rule that ore grows back, its ten tech levels and its 1996 remap
   colours (PALETTE.CPS rows 0 to 7, from the pack). */
const char *side_name(int house){return theme.ra?(house?"Soviet":"Allies"):(house?"Nod":"GDI");}
/* Red Alert's countries by the engine's number (HousesType), and the order its list shows
   them in: the Allies, then the Soviets. */
const char *const ra_country[8]={"Spain","Greece","USSR","England","Ukraine","Germany","France","Turkey"};
const int ra_country_order[8]={3,5,6,0,1,7,2,4};
/* What a seat's side reads: its country where the seats are countries. */
std::string seat_side(const SK_State *st,int seat){int c=sk_row_country(st,seat);return c>=0&&c<8?ra_country[c]:side_name(sk_row_house(st,seat));}
unsigned player_colour(int i);
/* RED ALERT'S OWN RULES (sk_enh_ra_rule), which the 1995 controller has no item for: a
   seat of their own, read only on a joiner's screen. The difficulty is a three step slider
   as on the 1996 skirmish dialog, its value named rather than numbered. */
Seat &rarule(const SK_State *st,int rule,const char *label,int x,int y,int w,int h,int kind){Seat &q=add(-50-rule,label,x,y,w,h,kind);q.action=kind==RANGE?RARANGE:RACHECK;q.arg=rule;q.value=sk_ra_rule(st,rule);q.disabled=sk_ra_rule_disabled(st)!=0;q.active=false;return q;}
void radifficulty(const SK_State *st,int x,int y){const int d=std::max(0,std::min(3,sk_ra_rule(st,SK_RA_DIFFICULTY)));text("New AI default",x,y-28);text(sk_level_name(d),x+424-textwidth(sk_level_name(d)),y-28,ACCENT);Seat &q=rarule(st,SK_RA_DIFFICULTY,"New AI default",x,y,424,36,RANGE);q.min=0;q.max=3;q.step=1;}
void skrange(const SK_State *st,int item,const char *label,int x,int y,int value,int lo,int hi,int step){text(label,x,y-28);std::string val=item==SK_I_SPEED?dopt_speed_name(value):std::to_string(value);text(val,x+424-textwidth(val),y-28,ACCENT);Seat &q=skbutton(st,item,label,x,y,424,36,RANGE);q.action=SKRANGE;q.value=value;q.min=lo;q.max=hi;q.step=step;q.active=false;}
/* Tiberian Dawn's eight player colours by PlayerColorType, the number a square hands the
   engine: gold, light blue, red, green, orange, blue, grey, brown. Each square is the colour
   that player's army is painted: step 2 of the colour's remap ramp (TEMPERAT.PAL entry
   Remap<colour>[178], widened << 2), the renderer's LIVERY_BAND[colour][2], which is the
   body tone of every graphics mode's units and buildings and the entry the 1995-style
   lobby fills its own square from. The game's light blue is a pale blue-grey and its blue
   a blue-green, and the squares say so. */
const unsigned int colours[]={0xc4a85c,0xc0c0d0,0xc42814,0xa0e01c,0xd47810,0x648088,0xa0a0a0,0xa8704c};
unsigned player_colour(int i){i&=7;if(theme.ra){auto it=arts.find("remap-colours");if(it!=arts.end()&&it->second.w>i){const unsigned char *p=&it->second.pixels[i*4];return (p[0]<<16)|(p[1]<<8)|p[2];}}return colours[i];}
/* THE ROSTER'S SIDE COLUMN on Players & Map: its heading and each seat's line. Red Alert's
   network column keeps side and team; its start lives in Player Setup, since "Random"
   would be cut short in the 176 pixel column. A race has no teams and deals its grid at
   random (Player Setup shows both as not offered), so a Rally's roster prints the side
   alone rather than a team and a start the race never reads. */
static bool roster_race(const SK_State *st){return st->rally&&!st->countries;}
std::string roster_heading(const SK_State *st){return roster_race(st)?"Side":st->net?"Side / Team":"Side / Team / Start";}
std::string roster_summary(const SK_State *st,int i){
 const int mode=sk_row_mode(st,i),start=sk_row_start(st,i);
 if(mode==SK_SEAT_OPEN||mode==SK_SEAT_BLOCK)return "";
 if(roster_race(st))return seat_side(st,i);
 return seat_side(st,i)+" / "+std::to_string(sk_row_team(st,i)+1)+(st->net?std::string():" / "+(start<0?std::string("Random"):std::to_string(start+1)));
}
void skseat(const SK_State *st){
 int seat=st->popup;begin(LOBBY,st,"settings-frame","PLAYER SETUP");well(("PLAYER "+std::to_string(seat+1)).c_str(),38,102,600,356);
 auto dropdown=[&](int id,const char *label,const std::string &value,int x,int y,int w){text(label,x,y-24);Seat &q=add(id,value,x,y,w);q.action=DROP;q.arg=id;q.active=v.drop==id;};
 int mode=sk_row_mode(st,seat),side=sk_row_house(st,seat),team=sk_row_team(st,seat),start=sk_row_start(st,seat),colour=sk_row_colour(st,seat);
 dropdown(-10,"Seat type",mode==SK_SEAT_BOT?"Computer":mode==SK_SEAT_OPEN?"Empty":mode==SK_SEAT_BLOCK?"Blocked":"Player",56,178,268);
 dropdown(-14,"Difficulty",mode==SK_SEAT_BOT?sk_level_name(sk_row_level(st,seat)):"-",352,178,268);v.seats.back().disabled=mode!=SK_SEAT_BOT||st->net==2;
 dropdown(-11,sk_row_country(st,seat)>=0?"Country":theme.ra?"Side":"Faction",seat_side(st,seat),56,270,268);text("Player colour",352,246);
 for(int i=0;i<8;i++){Seat &q=skbutton(st,SK_I_POP_C1+i,"",352+i*32,270,28,28,COLOUR);q.active=colour==i;q.value=player_colour(i);}
 /* a race has no teams and draws its grid at random: both are shown as the race has them and
    are not offered */
 const bool race=st->rally&&!st->countries;
 dropdown(-12,"Team",race?"-":std::to_string(team+1),56,364,268);v.seats.back().disabled=v.seats.back().disabled||race;
 dropdown(-13,"Start position",race||start<0?"Random":std::to_string(start+1),352,364,268);v.seats.back().disabled=v.seats.back().disabled||race;
 Seat &done=add(EM_SK_DONE,"Done",446,498,192);done.action=SEATDONE;done.arg=1;Seat &back=add(EM_SK_BACK,"Back",38,498,192);back.action=SEATDONE;back.arg=0;
 int drop=v.drop;if(drop<=-10&&drop>=-14){
  std::vector<std::pair<std::string,int>> options;
  if(drop==-10){if(mode==SK_SEAT_HUMAN)options.push_back({"Player",-1});else{options.push_back({"Computer",SK_I_POP_BOT});options.push_back({"Empty",SK_I_POP_OPEN});options.push_back({"Blocked",SK_I_POP_BLOCK});}}
  if(drop==-11&&sk_row_country(st,seat)>=0)for(int k:ra_country_order)options.push_back({ra_country[k],-100-k});
  else if(drop==-11){options.push_back({side_name(0),SK_I_POP_GDI});options.push_back({side_name(1),SK_I_POP_NOD});}
  if(drop==-12)for(int i=0;i<8;i++)options.push_back({std::to_string(i+1),SK_I_POP_T1+i});
  if(drop==-14)for(int i=0;i<4;i++)options.push_back({sk_level_name(i),SK_I_POP_EASY+i});
  if(drop==-13){options.push_back({"Random",-2});int count=st->sel>=0?std::min(8,int(st->maps[st->sel].starts)):0;for(int i=0;i<count;i++)options.push_back({std::to_string(i+1),SK_I_POP_S1+i});}
  int x=drop==-13||drop==-14?352:56,w=268,ownerY=drop==-10||drop==-14?178:drop==-11?270:364,h=int(options.size())*42+8,y=ownerY+44;
  if(y+h>486)y=std::max(78,ownerY-h-4);
  int owner=-1;for(int index=0;index<int(v.seats.size());index++)if(v.seats[index].action==DROP&&v.seats[index].arg==drop)owner=index;
  popup(owner,x-4,y-4,w+8,h);
  for(int i=0;i<int(options.size());i++){Seat &q=add(options[i].second,options[i].first,x,y+i*42,w);q.action=options[i].second==-2?SKRANDOM:SKITEM;q.arg=q.item;q.disabled=q.item==-1||(q.item>=0&&sk_item_disabled(st,q.item));
   if(q.item<=-100){q.action=SKCOUNTRY;q.arg=-100-q.item;q.active=sk_row_country(st,seat)==q.arg;}}
 }
 paint();
 if(st->status&&v.drop==-1)note(st->status,56,432,564,1,ACCENT);
}
}
/* THE GAME MODE DROPDOWN, at the right end of the lobby's tab row in every host lobby (a
   skirmish and a network room) and shown to a joiner as the host has it, greyed. A lobby
   opens in Free for All; a change refilters the map list and switches Match Rules at once
   (sk_set_mode). Its button is added with the tab row, its list last so it draws on top. */
const char *const em_mode_names[SK_MODE_COUNT]={"Free for All","Gun Game","Rally"};
void modebutton(const SK_State *st){
 text("Game Mode",v.w-258,62);
 Seat &q=add(EM_SK_MODE_DROP,em_mode_names[sk_mode(st)],v.w-258,86,220);q.action=DROP;q.arg=EM_SK_MODE_DROP;
 q.active=v.drop==EM_SK_MODE_DROP;q.disabled=sk_mode_locked(st);
 if(q.disabled&&v.drop==EM_SK_MODE_DROP)v.drop=-1;
}
void modelist(const SK_State *st){
 if(v.drop!=EM_SK_MODE_DROP)return;
 int owner=-1;for(int i=0;i<int(v.seats.size());i++)if(v.seats[i].action==DROP&&v.seats[i].arg==EM_SK_MODE_DROP)owner=i;
 if(owner<0){v.drop=-1;return;}
 const std::string hint=st->countries?"Dawn3D only for now"
  :!sk_mode_offered(st,SK_MODE_GUN_GAME)?"Gun Game: LAN only"
  :!sk_mode_offered(st,SK_MODE_RALLY)?"Rally: no course":"";
 /* the list hangs under the button, its right edge on the button's, and is the seat
    dropdowns' 268 wide when its foot line needs the room (buttons come in the baked widths) */
 const int w=hint.empty()?220:268,x=v.w-38-w,y=130,h=SK_MODE_COUNT*42+8+(hint.empty()?0:30);
 popup(owner,x-4,y-4,w+8,h);
 for(int m=0;m<SK_MODE_COUNT;m++){Seat &e=add(EM_SK_MODE_BASE+m,em_mode_names[m],x,y+m*42,w);e.action=SKMODE;e.arg=m;
  e.active=sk_mode(st)==m;e.disabled=!sk_mode_offered(st,m);}
 if(!hint.empty())v.popupHint=hint;
}
extern "C" void em_lobby(const SK_State *st,const DB_Pack *p){memo([&]{
 if(!load())return;
 if(st->popup>=0){skseat(st);return;}
 bool net=st->net!=0,rules=v.kind==LOBBY&&v.owner==st&&v.sksection==1;
 bool picker=st->picker!=0;
 begin(LOBBY,st,!picker&&!rules&&net?"lobby-frame":"visuals-frame",picker?"CHANGE MAP":net?"MULTIPLAYER LOBBY":"SKIRMISH");
 if(picker){
  skbutton(st,SK_I_PICK_TAB0,"Official",38,86,230).active=st->tab==0;skbutton(st,SK_I_PICK_TAB1,"User Maps",276,86,230).active=st->tab==1;
  well("MAPS",38,150,600,368);well("DETAILS",658,150,320,368);
  /* the tab's maps that this lobby's mode lists (a Rally's courses, or every other map) */
  std::vector<int> maps;for(int i=0;i<st->count;i++)if(st->maps[i].user==st->tab&&sk_map_offered(st,i))maps.push_back(i);
  v.opstop=std::max(0,std::min(v.opstop,std::max(0,int(maps.size())-8)));
  for(int i=0;i<8&&v.opstop+i<int(maps.size());i++){int ix=maps[v.opstop+i];Seat &q=add(SK_I_PICK_LIST,st->maps[ix].name?st->maps[ix].name:st->maps[ix].scen,56,198+i*34,564,32,ROW,MAPROW,ix);q.active=st->pick_sel==ix;}
  text(std::to_string(maps.size())+" maps",56,490,MUTED);
  if(maps.size()>8){Seat &a=add(-1,"Previous",346,474,132,32);a.action=OPSTOP;a.arg=-8;a.disabled=v.opstop==0;Seat &b=add(-1,"Next",488,474,132,32);b.action=OPSTOP;b.arg=8;b.disabled=v.opstop+8>=int(maps.size());}
  int ix=st->pick_sel;if(ix>=0&&ix<st->count){preview(st,p,ix,698,204,240);const SK_Map &m=st->maps[ix];text(std::to_string(m.starts)+" players / "+std::to_string(m.w)+" X "+std::to_string(m.h),676,460,MUTED);text(m.theater?m.theater:"",676,486,MUTED);}
  skbutton(st,SK_I_PICK_CANCEL,"Back",38,550,192);skbutton(st,SK_I_PICK_OK,"Use Map",758,550,220);paint();return;
 }
 for(int i=0;i<2;i++){Seat &q=add(-1,i?"Match Rules":"Players & Map",38+i*238,86,230);q.action=TAB;q.arg=i;q.active=v.sksection==i;}
 if(net)skbutton(st,SK_I_CHANGEMAP,"Change Map",514,86,230);
 modebutton(st);
 if(rules){
  const bool rally=st->rally&&!st->countries;
  well(rally?"THE RACE":"STARTING CONDITIONS",38,150,460,368);well("MATCH RULES",518,150,460,368);
  if(st->countries){
   /* RED ALERT'S RULES, as its 1996 dialog and the approved design have them: the three
      sliders and the computers' difficulty, then Bases, Ore Regrowth Rate, Crates, Shroud
      Regrows, Capture The Flag and AI Takeover. Superweapons and Short Game keep the
      room's defaults (on and off). */
   skrange(st,SK_I_BUILD,"Tech Level",56,net?225:232,st->build,1,10,1);
   skrange(st,SK_I_CREDITS,"Credits",56,net?305:314,st->credits,0,10000,500);
   skrange(st,SK_I_UNITS,"Unit Count",56,net?385:396,st->units,0,10,1);
   radifficulty(st,net?536:56,net?476:478);
   const int first=net?198:202,pitch=net?42:48;
   Seat &bases=skbutton(st,SK_I_BASES,"Bases",536,first,424,36,CHECK);bases.value=1;bases.active=false;
   skbutton(st,SK_I_TIBERIUM,std::string("Ore Regrowth Rate   ")+sk_regrowth_rate_name(st->tiberium),536,first+pitch-2,424,40);
   Seat &crates=skbutton(st,SK_I_CRATES,"Crates",536,first+2*pitch,424,36,CHECK);crates.value=st->crates;crates.active=false;
   rarule(st,SK_RA_SHROUD,"Shroud Regrows",536,first+3*pitch,424,36,CHECK);rarule(st,SK_RA_CTF,"Capture The Flag",536,first+4*pitch,424,36,CHECK);
   Seat &ai=skbutton(st,SK_I_AITAKE,"AI Takeover",536,first+5*pitch,424,36,CHECK);ai.value=st->aitake;ai.active=false;
  }else if(rally){
   /* A RALLY: what a race is, and only the rules a race reads. The laps (a point-to-point
      course is one run whatever they say), the computer taking a seat that leaves (a
      network room's), and the speed below; the economy, the bases, the crates and the
      tech level mean nothing to a race and are not offered. */
   const char* lines[] = {"Rally: a race on this course.", "Choose a vehicle, then race.",
                          "W A S D drive, Shift drifts.", "Mouse aims, left button fires.",
                          "Space or right button: item.", "R puts you back on the course.",
                          "First across the line wins."};
   /* a little closer than Gun Game's lines, so a room's Game Speed below has its gap */
   for (size_t i = 0; i < sizeof lines / sizeof lines[0]; ++i)
       text(lines[i],56,206+i*30,TEXT);
   const bool run=st->sel>=0&&st->sel<st->count&&st->maps[st->sel].course==SK_COURSE_RUN;
   skrange(st,SK_I_LAPS,"Laps",536,232,run?1:st->laps,SK_LAPS_MIN,SK_LAPS_MAX,1);
   note(run?"This course is one run from the start to the finish.":"The start line is the finish line.",536,284,424,2,MUTED);
   if(net){Seat &ai=skbutton(st,SK_I_AITAKE,"AI Takeover",536,330,424,36,CHECK);ai.value=st->aitake;ai.active=false;
    note("A computer driver takes the seat of a player who leaves.",536,372,424,2,MUTED);}
  }else{
   if (st->gun_game) {
    const char* lines[] = {"Gun Game: every player alone.", "Start as a Minigunner.",
                           "Hold to choose your next unit.", "Respawn as your current unit.",
                           "Press Z or C to roll or jump.",
                           "Win with a Mammoth Tank kill.", "Enhanced graphics only."};
    for (size_t i = 0; i < sizeof lines / sizeof lines[0]; ++i)
        text(lines[i],56,208+i*34,TEXT);
   } else {
    skrange(st,SK_I_BUILD,"Tech Level",56,net?225:242,st->build,1,7,1);
    skrange(st,SK_I_CREDITS,"Credits",56,net?305:344,st->credits,0,10000,500);
    skrange(st,SK_I_UNITS,"Unit Count",56,net?385:446,st->units,0,10,1);
   }
   const int ids[] = {SK_I_BASES,SK_I_TIBERIUM,SK_I_SUPER,SK_I_CRATES,SK_I_AITAKE,SK_I_SHORTGAME};
   const char* names[] = {"Bases","Tiberium Regrowth Rate","Superweapons","Crates","AI Takeover","Short Game"};
   const int vals[] = {1,1,st->super,st->crates,st->aitake,st->shortgame};
   for (int i = 0; i < 6; ++i) {
    const bool regrowth = ids[i] == SK_I_TIBERIUM;
    const std::string label = regrowth
        ? std::string(names[i]) + "   " + sk_regrowth_rate_name(st->gun_game ? 0 : st->tiberium)
        : names[i];
    Seat& q = skbutton(st,ids[i],label,536,202+i*48-(regrowth?2:0),424,regrowth?40:36,regrowth?BUTTON:CHECK);
    q.value = st->gun_game ? ids[i] == SK_I_SUPER || ids[i] == SK_I_CRATES : vals[i];
    q.active = false;
   }
  }
  if(net)skrange(st,SK_I_SPEED,"Game Speed",56,465,st->speed,0,DOPT_MAX_SPEED-1,1);
  Seat &a=add(-1,"Back",38,550,192);a.action=TAB;a.arg=-1;Seat &b=add(-1,"Done",758,550,220);b.action=TAB;b.arg=0;
 }else{
  well("PLAYERS",38,150,net?508:600,368);well("MAP",net?566:658,150,net?188:320,368);
  text("Player",56,194,MUTED);text(roster_heading(st),net?246:238,194,MUTED);text("Difficulty",net?408:500,194,MUTED);
  int seats=sk_players(st);std::vector<std::string> summaries;std::vector<int> rowcol;
  for(int i=0;i<std::min(8,seats);i++){
   char name[40];sk_seat_name(st,i,name,sizeof name);int mode=sk_row_mode(st,i);
   Seat &q=skbutton(st,SK_I_ROW0+i,fitted(name,net?132:186),56,222+i*32,net?348:440,30,ROW);q.label="  "+q.label;q.active=false;
   Seat &level=skbutton(st,SK_I_LEVEL0+i,mode==SK_SEAT_BOT?sk_level_name(sk_row_level(st,i)):"-",net?408:500,222+i*32,120,30,ROW);level.active=false;
   summaries.push_back(roster_summary(st,i));
   rowcol.push_back(sk_row_colour(st,i));
  }
  int ix=st->sel;if(ix>=0&&ix<st->count){const SK_Map &m=st->maps[ix];
   preview(st,p,ix,net?600:698,net?204:202,net?120:240);
   if(net){note(m.name?m.name:m.scen,584,348,152,2);text(std::to_string(m.starts)+" players",584,412,MUTED);text("Starts 1 - "+std::to_string(m.starts),584,444,MUTED);text(std::to_string(m.w)+" X "+std::to_string(m.h),584,476,MUTED);}
   else {text(fitted(m.name?m.name:m.scen,284),676,460,MUTED);text(std::to_string(m.starts)+" players / "+std::to_string(m.w)+" X "+std::to_string(m.h),676,486,MUTED);}
  }
  if(net){
   well("CHAT",774,150,404,368);std::string messages;
   for(int i=0;i<st->chat_count&&i<SK_CHAT_LOG;i++){
    /* The network controller already files each line with its speaker prefix. */
    if(!messages.empty())messages+='\n';messages+=st->chat_line[i];
   }
   note(messages,792,194,368,11,TEXT,24,true);
   Seat &q=skbutton(st,SK_I_CHAT,st->chat_in,792,466,276,40,FIELD);q.active=st->chat_focus!=0;
   Seat &send=add(-1,"Send",1080,466,80);send.action=SEND;send.disabled=sk_item_disabled(st,SK_I_CHAT);
  }else skbutton(st,SK_I_CHANGEMAP,"Change Map",550,550,192);
  skbutton(st,SK_I_CANCEL,net?"Leave":"Back",38,550,192);
  skbutton(st,SK_I_PLAY,net?sk_item_label(st,SK_I_PLAY):"Play",net?958:758,550,220);
  modelist(st);paint();
  for(int i=0;i<int(summaries.size());i++){rect(64,231+i*32,12,12,player_colour(rowcol[i]));text(fitted(summaries[i],net?154:264),net?246:230,229+i*32,MUTED);}
  if(st->status)note(st->status,net?774:242,528,net?404:496,1,ACCENT);
  /* how others join this room, on the left of the title row: the right end is Game Mode's */
  if(net&&st->net_join[0])text(fitted(st->net_join,414),38,62,ACCENT);
  return;
 }
 modelist(st);paint();if(st->status)note(st->status,242,528,496,1,ACCENT);
});}
extern "C" int em_sk_drop_open(void){return v.kind==LOBBY&&v.drop!=-1;}
extern "C" void em_sk_drop_close(void){v.drop=-1;}
extern "C" int em_sk_press(SK_State *st,int x,int y){
 int n=virtualseat(x);
 if(v.popupFirst>=0&&(n<0||v.seats[n].disabled)){v.held=v.drag=-1;if(!popup_contains(v.mousex,v.mousey))v.drop=-1;return 0;}
 if(n<0)return sk_press(st,x,y);const Seat &q=v.seats[n];v.held=n;
 st->chat_focus=q.kind==FIELD&&q.item==SK_I_CHAT;
 if(q.action==SKRANGE){v.drag=n;int val=q.min+(q.max-q.min)*std::max(0,std::min(1000,y))/1000;val=q.min+((val-q.min+q.step/2)/q.step)*q.step;sk_enh_control(st,q.item,val);}
 if(q.action==RARANGE){v.drag=n;int val=q.min+((q.max-q.min)*std::max(0,std::min(1000,y))+500)/1000;sk_enh_ra_rule(st,q.arg,val);}
 return 0;
}
extern "C" int em_sk_motion(SK_State *st,int x,int y){
 int n=virtualseat(x);if(n>=0){if(v.drag==n){const Seat &q=v.seats[n];
  if(q.action==RARANGE){sk_enh_ra_rule(st,q.arg,q.min+((q.max-q.min)*std::max(0,std::min(1000,y))+500)/1000);return 0;}
  int val=q.min+(q.max-q.min)*std::max(0,std::min(1000,y))/1000;val=q.min+((val-q.min+q.step/2)/q.step)*q.step;sk_enh_control(st,q.item,val);}return 0;}return v.popupFirst>=0?0:sk_motion(st,x,y);
}
/* MATCH RULES' BACK UNDOES WHAT WAS CHANGED THERE, as Back does on a player's own screen
   (sk_seat_edit_finish): the rules are copied when the tab opens and Back puts them back;
   Done, and the tab strip, keep them. Offline only, as the seat editor's snapshot is: a
   network room's edits are live on every peer as they are made. */
namespace {
struct SkRulesSnap{bool valid=false;SK_State *owner=nullptr;int build,credits,units,tiberium,super,crates,aitake,shortgame,laps,speed,shroud,ctf,ai_difficulty;};
SkRulesSnap g_skRules;
void sk_rules_open(SK_State *st){
 g_skRules.valid=st&&!st->net;if(!g_skRules.valid)return;g_skRules.owner=st;
 g_skRules.build=st->build;g_skRules.credits=st->credits;g_skRules.units=st->units;g_skRules.tiberium=st->tiberium;
 g_skRules.super=st->super;g_skRules.crates=st->crates;g_skRules.aitake=st->aitake;g_skRules.shortgame=st->shortgame;
 g_skRules.laps=st->laps;g_skRules.speed=st->speed;g_skRules.shroud=st->shroud;g_skRules.ctf=st->ctf;g_skRules.ai_difficulty=st->ai_difficulty;
}
void sk_rules_back(SK_State *st){
 if(!g_skRules.valid||g_skRules.owner!=st||st->net){g_skRules.valid=false;return;}
 st->build=g_skRules.build;st->credits=g_skRules.credits;st->units=g_skRules.units;st->tiberium=g_skRules.tiberium;
 st->super=g_skRules.super;st->crates=g_skRules.crates;st->aitake=g_skRules.aitake;st->shortgame=g_skRules.shortgame;
 st->laps=g_skRules.laps;st->speed=g_skRules.speed;st->shroud=g_skRules.shroud;st->ctf=g_skRules.ctf;st->ai_difficulty=g_skRules.ai_difficulty;
 g_skRules.valid=false;
}
}
extern "C" int em_sk_release(SK_State *st,int x,int y){
 int n=virtualseat(x),held=v.held;v.held=v.drag=-1;if(n<0)return v.popupFirst>=0?0:sk_release(st,x,y);if(n!=held)return 0;const Seat &q=v.seats[n];
 switch(q.action){case SKRANGE:return 0;
 case TAB:if(q.arg<0){sk_rules_back(st);v.sksection=0;}else{if(q.arg==1&&v.sksection!=1)sk_rules_open(st);if(q.arg!=1)g_skRules.valid=false;v.sksection=q.arg;}v.drop=-1;return 0;
 case SKITEM:{int act=sk_enh_action(st,q.item);if(SK_IS_PICK(q.item)||SK_IS_POP(q.item))v.drop=-1;return act;}
 case MAPROW:sk_enh_pick_row(st,q.arg);return 0;
 case OPSTOP:v.opstop=std::max(0,v.opstop+q.arg);return 0;
 case SEATDONE:sk_seat_edit_finish(st,q.arg);v.drop=-1;return 0;
 case DROP:v.drop=v.drop==q.arg?-1:q.arg;return 0;
 case SKRANDOM:sk_enh_random_start(st);v.drop=-1;return 0;
 case SKCOUNTRY:sk_enh_set_country(st,st->popup,q.arg);v.drop=-1;return 0;
 case SKMODE:sk_set_mode(st,q.arg);v.drop=-1;return 0;
 case RACHECK:sk_enh_ra_rule(st,q.arg,!sk_ra_rule(st,q.arg));return 0;
 case RARANGE:return 0;
 case SEND:st->chat_focus=1;return sk_key(st,SK_KEY_ENTER);
 default:return 0;}
}

namespace {
int ops_category(const DO_State *st,int row){
 if(!st->grouped)return 0;int cat=-1;
 for(int i=0;i<=row&&i<st->count;i++)if(st->list[i].header){std::string h=st->list[i].name?st->list[i].name:"";
  if(theme.ra)cat=h.find("COUNTERSTRIKE")!=std::string::npos?0:h.find("AFTERMATH")!=std::string::npos?1:-1;
  else cat=h.find("COVERT")!=std::string::npos?0:h.find("SPECIAL")!=std::string::npos?1:h.find("FUNPARK")!=std::string::npos?2:-1;}
 return cat;
}
/* THE TABS THIS LIST HAS: Covert Ops and Special Ops (Counterstrike and Aftermath in Red
   Alert) always, and a third, Funpark, only where the list carries the Funpark section,
   so a folder without the dinosaur missions keeps two tabs and Red Alert never has three. */
int ops_tabs(const DO_State *st){
 if(!st->grouped||theme.ra)return 2;
 for(int i=0;i<st->count;i++)if(st->list[i].header&&st->list[i].name&&std::string(st->list[i].name).find("FUNPARK")!=std::string::npos)return 3;
 return 2;
}
/* A Funpark row is played as either side, picked on the side plate when it starts. */
bool ops_funpark(const DO_Mission &m){return m.scen&&strlen(m.scen)>2&&m.scen[2]=='J';}
std::vector<int> ops_rows(const DO_State *st,int cat){std::vector<int> rows;
 for(int i=0;i<st->count;i++)if(do_row_playable(st,i)&&(!st->grouped||ops_category(st,i)==cat))rows.push_back(i);
 if(st->grouped)std::stable_sort(rows.begin(),rows.end(),[st](int a,int b){return st->list[a].nod<st->list[b].nod;});return rows;
}
}
extern "C" void em_ops_move(DO_State *st,int delta){
 if(!st->grouped){if(delta<=-10000)do_move_home(st);else if(delta>=10000)do_move_end(st);else do_move(st,delta);return;}
 const auto rows=ops_rows(st,std::max(0,v.opsfilter));if(rows.empty())return;
 auto it=std::find(rows.begin(),rows.end(),st->selected);int ix=it==rows.end()?0:int(it-rows.begin());
 ix=std::max(0,std::min(int(rows.size())-1,ix+delta));st->selected=rows[ix];
}
extern "C" void em_ops_filter(DO_State *st,int category){
 if(!st->grouped)return;v.opsfilter=std::max(0,std::min(ops_tabs(st)-1,category));v.opstop=0;
 auto rows=ops_rows(st,v.opsfilter);st->selected=rows.empty()?-1:rows.front();
}
/* LEFT and RIGHT: one tab along, stopping at either end. */
extern "C" void em_ops_step(DO_State *st,int delta){
 if(!st->grouped)return;em_ops_filter(st,std::max(0,v.opsfilter)+delta);
}
extern "C" void em_ops(DO_State *st){memo([&]{
 if(!load())return;begin(OPS,st,"visuals-frame",st->ants?"ANT MISSIONS":st->grouped?(theme.ra?"MISSION DISKS":"SPECIAL OPS"):"USER MAPS");
 if(v.opsfilter<0)v.opsfilter=std::max(0,ops_category(st,st->selected));
 if(st->grouped){const char *tabs[]={theme.ra?"Counterstrike":"Covert Ops",theme.ra?"Aftermath":"Special Ops","Funpark"};for(int i=0;i<ops_tabs(st);i++){Seat &q=add(-1,tabs[i],38+i*238,86,230);q.action=OPSFILTER;q.arg=i;q.active=v.opsfilter==i;}}
 auto rows=ops_rows(st,v.opsfilter);
 auto it=std::find(rows.begin(),rows.end(),st->selected);
 if(it==rows.end()&&!rows.empty()){st->selected=rows.front();it=rows.begin();}
 int index=it==rows.end()?-1:int(it-rows.begin());
 int y=st->grouped?150:110,h=st->grouped?368:408;well("MISSIONS",38,y,600,h);well("DETAILS",658,y,320,h);
 if(st->selected!=v.lastselected){if(index>=0&&index<v.opstop)v.opstop=index;if(index>=v.opstop+8)v.opstop=index-7;v.lastselected=st->selected;}
 v.opstop=std::max(0,std::min(v.opstop,std::max(0,int(rows.size())-8)));
 for(int i=0;i<8&&v.opstop+i<int(rows.size());i++){int ix=rows[v.opstop+i];const DO_Mission &m=st->list[ix];Seat &q=add(ix,m.name?m.name:m.scen,56,y+48+i*34,564,32,ROW,OPTROW,ix);q.active=ix==st->selected;if(st->grouped&&!ops_funpark(m))q.badge=m.nod?1:0;}
 text(std::to_string(rows.size())+" missions",56,490,MUTED);
 if(rows.size()>8){Seat &a=add(-1,"Previous",346,474,132,32);a.action=OPSTOP;a.arg=-8;a.disabled=v.opstop==0;Seat &b=add(-1,"Next",488,474,132,32);b.action=OPSTOP;b.arg=8;b.disabled=v.opstop+8>=int(rows.size());}
 if(index>=0&&theme.ra){const DO_Mission &m=st->list[st->selected];Seat &p=add(-1,"",742,212,136,120,PLATE);p.badge=m.nod?1:0;p.disabled=true;text(m.nod?"Soviet":"Allied",676,370,MUTED);text(m.scen?m.scen:"",676,404,MUTED);note(m.name?m.name:"",676,438,284,2);}
 else if(index>=0&&ops_funpark(st->list[st->selected])){const DO_Mission &m=st->list[st->selected];text("Funpark",676,370,MUTED);text(m.scen?m.scen:"",676,404,MUTED);note("Played as GDI or Nod, picked when it starts.",676,438,284,2);}
 else if(index>=0){const DO_Mission &m=st->list[st->selected];image(m.nod?"faction-Nod":"faction-GDI",742,212);text(m.nod?"Nod":"GDI",676,370,MUTED);text(m.scen?m.scen:"",676,404,MUTED);note(m.name?m.name:"",676,438,284,2);}
 /* Red Alert ships with no expansion missions and no user maps: the empty screen says why,
    and where user maps go. */
 if(index<0&&(theme.ra||st->user_maps)){
  note(st->grouped?"No Counterstrike or Aftermath missions are installed. The 1996 freeware discs carry the base game only; the main menu greys this button until expansion missions are found.":"No singleplayer maps installed. Choose Import Map to browse your computer. Multiplayer maps appear in Skirmish and Multiplayer.",56,y+64,564,6);
  note("Nothing to play yet",676,y+64,284,2);}
 Seat &a=add(DO_HIT_CANCEL,"Back",38,550,192);legacy(a,st->user_maps?24:DO_CANCEL_X,DO_BTN_Y,DO_CANCEL_W,DO_BTN_H);
 if(st->user_maps){Seat &import=add(DO_HIT_IMPORT,"Import Map",398,550,220);legacy(import,DO_IMPORT_X,DO_BTN_Y,DO_IMPORT_W,DO_BTN_H);}
 Seat &b=add(DO_HIT_PLAY,"Play",758,550,220);legacy(b,st->user_maps?220:DO_PLAY_X,DO_BTN_Y,DO_PLAY_W,DO_BTN_H);b.disabled=index<0;paint();
});}
extern "C" int em_ops_hit(const DO_State *st,int x,int y){int n=virtualseat(x);if(n<0)return do_hit_test(st,x,y);const Seat &q=v.seats[n];return q.action==OPTROW?q.arg:-1000-n;}
extern "C" void em_ops_action(DO_State *st,int code){
 int n=-1000-code;if(n<0||n>=int(v.seats.size()))return;Seat q=v.seats[n];
 if(q.action==OPSFILTER){em_ops_filter(st,q.arg);}
 if(q.action==OPSTOP){em_ops_move(st,q.arg);v.opstop=std::max(0,v.opstop+q.arg);}
}

extern "C" void em_import_map(const DMS_MapImportState *st){memo([&]{
 if(!st||!load())return;
 begin(IMPORTMAP,st,"visuals-frame",st->completed?"MAP IMPORTED":"IMPORT MAP");
 auto control=[&](int id,const std::string &label,int x,int y,int w,int h=40,int kind=BUTTON)->Seat&{
  Seat &q=add(id,label,x,y,w,h,kind);q.disabled=st->busy||(st->completed&&id!=MI_BACK);
  if(st->pressed==id)v.held=int(v.seats.size())-1;
  return q;
 };
 v.held=-1;
 control(MI_HOME,"Home",38,86,132);control(MI_PARENT,"Up",182,86,132);
 note(st->message,338,87,640,2,MUTED,20);
 Seat &location=control(MI_LOCATION,st->location,38,138,788,40,FIELD);location.active=st->focus!=0;
 control(MI_GO,"Go",838,138,140);
 well("FOLDERS AND MAPS",38,194,600,304);well("MAP PREVIEW",658,194,320,304);
 for(int i=0;i<st->row_count;i++){
  Seat &row=control(MI_ROW_BASE+i,std::string(st->row_directory[i]?"> ":"  ")+st->rows[i],56,242+i*26,564,24,ROW);
  row.active=i==st->selected_row;
 }
 if(!st->row_count)note("No supported maps in this folder. Choose another folder or enter a path above.",56,250,564,3);
 const int pw=st->info.preview_w,ph=st->info.preview_h;
 if(pw>0&&ph>0&&pw<=128&&ph<=128){
  const int zoom=std::max(1,std::min(276/pw,132/ph));
  Art a;a.w=pw;a.h=ph;a.pixels.assign(st->info.preview,st->info.preview+pw*ph*4);
  blit(a,680+(276-pw*zoom)/2,244+(132-ph*zoom)/2,zoom);
 }else note(st->busy?"Reading map...":"Select a map to inspect its terrain and game type.",680,270,276,3);
 if(st->info.name[0])text(fitted(st->info.name,276),680,382,TEXT);
 if(st->info.game[0])text(fitted(st->info.game,276),680,406,ACCENT);
 if(st->info.kind[0])text(fitted(st->info.kind,276),680,430,ACCENT);
 if(st->info.width>0){text(std::to_string(st->info.width)+" x "+std::to_string(st->info.height)+" / "+std::to_string(st->info.starts)+" starts",680,454,MUTED);text(fitted(st->info.theater,276),680,478,MUTED);}
 Seat &previous=control(MI_PREVIOUS,"Previous",346,506,132,32);previous.disabled|=st->top<=0;
 Seat &next=control(MI_NEXT,"Next",488,506,132,32);next.disabled|=st->top+st->row_count>=st->total;
 text(std::to_string(st->total)+" items",56,514,MUTED);
 control(MI_BACK,st->completed?"Done":"Back",38,550,192);
 Seat &import=control(MI_IMPORT,"Import",758,550,220);import.disabled|=!st->ready||st->completed;
 if(st->importing||st->completed){
  const int amount=std::max(0,std::min(100,st->progress));
  rect(250,550,488,18,0x080a09);rect(252,552,484*amount/100,14,theme.ra?theme.slot[S_RAMP0+4]:0x527d35);
  rect(252,552,484*amount/100,1,ACCENT);text(std::to_string(amount)+"%",494,578,ACCENT,true);
 }else text("Auto Heightmap is included",494,566,MUTED,true);
 paint();
});}

extern "C" void em_browser(const MP_State *st){memo([&]{
 if(!load())return;
 if(st->prompt){begin(BROWSER,st,"settings-frame","PLAYER NAME");well("YOUR NAME",38,110,600,350);text("Player name",56,202);Seat &q=mp(st,MP_I_HANDLE,st->handle,56,238,564,40,FIELD);q.active=true;mp(st,MP_I_HANDLE_OK,"OK",446,498,192);paint();return;}
 if(st->page==MP_PAGE_WAIT){begin(BROWSER,st,"settings-frame","MULTIPLAYER LOBBY");well("ROOM",38,110,600,350);note(st->name,56,178,564,2,TEXT);note(st->myaddr,56,246,564,2,ACCENT);note(st->status,56,314,564,3);mp(st,MP_I_LEAVE,"Leave",38,498,192);mp(st,st->wait_ishost?MP_I_START:MP_I_READY,st->wait_ishost?"Start Game":"Ready",446,498,192);paint();return;}
 begin(BROWSER,st,"visuals-frame","MULTIPLAYER");bool host=st->tab==MP_TAB_HOST;
 if(!host){if(st->rowsel!=v.lastselected){if(st->rowsel>=0&&st->rowsel<v.opstop)v.opstop=st->rowsel;if(st->rowsel>=v.opstop+3)v.opstop=st->rowsel-2;v.lastselected=st->rowsel;}v.opstop=std::max(0,std::min(v.opstop,std::max(0,st->rowcount-3)));}
 mp(st,MP_I_TAB_HOST,"Host Game",38,86,230).active=host;mp(st,MP_I_TAB_JOIN,"Join Game",276,86,230).active=!host;
 if(host){
  well("GAME",38,150,460,368);well("ACCESS",518,150,460,368);
  text("Game name",56,208);Seat &name=mp(st,MP_I_NAME,st->name,56,232,424,40,FIELD);name.active=st->focus==MP_I_NAME;
  text("Player name",56,306);Seat &handle=add(MP_I_HANDLE,st->handle,56,330,424,40,FIELD,MPHANDLE);handle.active=st->focus==MP_I_HANDLE;
  Seat &setup=add(-1,"Set Up Match",56,422,424);setup.action=MPSETUP;text(fitted(st->mapname,424),56,478,MUTED);
  Seat &a=mp(st,MP_I_PRIVATE,"Private Game",536,204,424,36,CHECK);a.value=st->private_game;a.active=false;
  text("Passcode",536,270);Seat &pass=mp(st,MP_I_PASS,std::string(strlen(st->pass),'*'),536,294,424,40,FIELD);pass.active=st->focus==MP_I_PASS;
  Seat &b=mp(st,MP_I_RELAY,"Internet Game",536,356,424,36,CHECK);b.value=st->relay_game;b.active=false;
  Seat &c=mp(st,MP_I_PUBLIC,"List Publicly",536,414,424,36,CHECK);c.value=st->list_game;c.active=false;
  note(st->relay_game?"Join using a room code":"Public listing shares your address",536,474,424,2);
  mp(st,MP_I_CANCEL,"Back",38,550,192);mp(st,MP_I_HOST,"Host",758,550,220);
 }else{
  mp(st,MP_I_NET_LAN,"LAN",514,86,140).active=st->net==MP_NET_LAN;mp(st,MP_I_NET_NET,"Internet",662,86,140).active=st->net==MP_NET_INTERNET;
  well("AVAILABLE GAMES",38,150,600,368);well("JOIN DETAILS",658,150,320,368);
  text("Game",56,200,MUTED);text("Map",302,200,MUTED);text("Players",534,200,MUTED);
  for(int i=0;i<3&&v.opstop+i<st->rowcount;i++){int ix=v.opstop+i;const MP_Row &r=st->rows[ix];Seat &q=add(1000+ix,fitted(r.name,220),56,240+i*48,564,44,ROW,MAPROW,ix);q.active=st->rowsel==ix;}
  if(!st->rowcount)text("No games found",68,254,MUTED);
  Seat &a=mp(st,MP_I_HIDE_LOCKED,"Hide Locked",56,408,424,36,CHECK);a.value=st->hide_locked;a.active=false;Seat &b=mp(st,MP_I_HIDE_GREY,"Hide Greyed",56,454,424,36,CHECK);b.value=st->hide_grey;b.active=false;
  text("Player name",676,214);Seat &handle=add(MP_I_HANDLE,st->handle,676,238,284,40,FIELD,MPHANDLE);handle.active=st->focus==MP_I_HANDLE;
  text(st->net==MP_NET_INTERNET?"Room code / Address":"Host address",676,312);Seat &addr=mp(st,MP_I_ADDR,st->addr,676,336,284,40,FIELD);addr.active=st->focus==MP_I_ADDR;
  text("Join passcode",676,410);Seat &pass=mp(st,MP_I_PASS,std::string(strlen(st->pass),'*'),676,434,284,40,FIELD);pass.active=st->focus==MP_I_PASS;
  mp(st,MP_I_REFRESH,"Refresh",38,550,192);mp(st,MP_I_CANCEL,"Back",550,550,192);mp(st,MP_I_JOIN,"Join",758,550,220);
 }
 if(st->status[0])note(st->status,242,528,496,1,ACCENT);paint();
 if(!host)for(int i=0;i<3&&v.opstop+i<st->rowcount;i++){const MP_Row &r=st->rows[v.opstop+i];text(fitted(r.map,208),302,254+i*48,MUTED);text(std::to_string(r.players_now)+" / "+std::to_string(r.players_max),534,254+i*48,MUTED);}
});}
extern "C" int em_mp_press(MP_State *st,int x,int y){
 int n=virtualseat(x);if(n<0)return mp_press(st,x,y);v.held=n;const Seat &q=v.seats[n];
 if(q.action==MPSETUP)return -3;
 if(q.action==MPHANDLE){st->focus=MP_I_HANDLE;return 0;}
 if(q.action==MAPROW){int ix=q.arg;if(ix<0||ix>=st->rowcount)return 0;mp_enh_select(st,ix);return 0;}return 0;
}
extern "C" void em_mp_motion(MP_State *st,int x,int y){if(virtualseat(x)<0)mp_motion(st,x,y);}

extern "C" void em_choice(int which,int selected,int pressed){memo([&]{
 if(!load())return;begin(CHOICE,nullptr,which==0?"confirm-frame":"settings-frame",which==0?"EXIT GAME":which==1?"INTRO & SNEAK PEEK":"CHOOSE YOUR SIDE");
 if(which==0){text(theme.ra?"Exit Red Alert 3D?":"Exit OpenCNC3D?",338,136,TEXT,true);add(0,"Cancel",98,238,220).active=selected==0;add(1,"Exit Game",358,238,220).active=selected==1;}
 else if(which==1){well("MOVIES",38,110,600,350);image("intro-frame",178,174);add(1,"Play Intro",56,382,564).active=selected==1;add(0,"Back",38,498,192).active=selected==0;}
 else {well("CAMPAIGN",38,110,600,350);image("faction-GDI",120,196);image("faction-Nod",420,196);add(1,"GDI",56,356,268).active=selected==1;add(2,"Nod",352,356,268).active=selected==2;add(0,"Back",38,498,192).active=selected==0;}
 v.held=-1;for(int i=0;i<int(v.seats.size());i++)if(v.seats[i].item==pressed)v.held=i;paint();
});}
/* Red Alert's side choice, which the 1996 game asks as a plain message box after a
   difficulty slider: both emblems on their plates, the two sides, the difficulty, Back and
   Start, on one screen. side 0 Allies / 1 Soviet, difficulty 0..2. */
extern "C" void em_side_choice(int side,int difficulty,int pressed){memo([&]{
 if(!load())return;begin(SIDE,nullptr,"settings-frame","CHOOSE YOUR SIDE");
 well("CAMPAIGN",38,110,600,350);
 for(int f=0;f<2;f++){Seat &p=add(-1,"",f?434:106,166,136,120,PLATE);p.badge=f;p.disabled=true;}
 add(EM_SIDE_ALLIES,"Allies",56,312,268).active=side==0;add(EM_SIDE_SOVIET,"Soviet",352,312,268).active=side==1;
 text("Difficulty",56,374);
 const char *names[]={"Easy","Normal","Hard"};
 for(int k=0;k<3;k++)add(EM_SIDE_EASY+k,names[k],56+k*192,398,180).active=difficulty==k;
 add(EM_SIDE_BACK,"Back",38,498,192);add(EM_SIDE_START,"Start",446,498,192).active=true;
 v.held=-1;for(int i=0;i<int(v.seats.size());i++)if(v.seats[i].item==pressed&&!v.seats[i].disabled)v.held=i;paint();
});}
extern "C" int em_match_debrief_result(const EM_Debrief *d,char *buf,int buflen){
 if(!buf||buflen<=0)return 2;buf[0]=0;if(!d)return 2;
 if(d->ended[0]){snprintf(buf,size_t(buflen),"%s",d->ended);return 0;}
 if(d->place<=0){snprintf(buf,size_t(buflen),"%s","Final standings");return 2;}
 const int p=d->place,t=p%100,o=p%10;const char *suf=(t>=11&&t<=13)?"th":o==1?"st":o==2?"nd":o==3?"rd":"th";
 snprintf(buf,size_t(buflen),"Your score: %d - %d%s of %d",d->score,p,suf,d->places);return 1;
}
/* THE MATCH DEBRIEF, ONE LAYOUT FOR BOTH GAMES. Tiberian Dawn's 1995 multiplayer score and
   Red Alert's 1996 one are the same screen (score.cpp, Multi_Score_Presentation: MLTIPLYR.WSA
   behind each commander's name in their colour, battles won this session and kills in each
   of the last games), so one builder draws it, in whichever game's kit is loaded: that kit's
   frame art, inks, player colours, buttons and selected-row fill. What differs is only what
   the words name: a seat played a country in Red Alert and a side in Tiberian Dawn, and the
   harvest is ore in one and tiberium in the other.

   It has pages, a row of tabs under the title like the Options screen's: SUMMARY is that
   1995 and 1996 table; SCORE gives every commander's score as a bar against the leader with
   the single-player screen's two ratings, leadership and efficiency; UNITS and STRUCTURES
   what each built, destroyed and lost; ECONOMY the money; GRAPH the match drawn over time.
   The result lines and Continue stay under every page. */
namespace {
const char *const debrief_tabs[EM_PAGES]={"Summary","Score","Units","Structures","Economy","Graph"};
const char *const debrief_series_td[EM_SERIES]={"Score","Army","Structures","Tiberium","Credits","Kills"};
const char *const debrief_series_ra[EM_SERIES]={"Score","Army","Structures","Ore","Credits","Kills"};
const char *debrief_series_name(int k){k=std::max(0,std::min(EM_SERIES-1,k));return theme.ra?debrief_series_ra[k]:debrief_series_td[k];}
/* A right-aligned figure, the way every column of numbers on these pages lines up. */
void figure(long long n,int right,int y,unsigned col){const std::string t=std::to_string(n);text(t,right-textwidth(t),y,col);}
/* One commander's name and what they played, at the left of a page's row, as on SUMMARY. */
void debrief_who(const EM_DebriefRow &row,int y){
 const unsigned own=player_colour(row.colour&7);
 rect(56,y+2,12,12,own);text(fitted(row.name,140),76,y,own);
 const std::string country=theme.ra?ra_country[row.country&7]:side_name(row.country&1);text(country,320-textwidth(country),y,MUTED);
}
/* A line of `t` pixels from (x0,y0) to (x1,y1), Bresenham's, each step a t x t square. */
void line(int x0,int y0,int x1,int y1,unsigned col,int t){
 const int dx=std::abs(x1-x0),dy=-std::abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1;int err=dx+dy;
 for(;;){rect(x0-t/2,y0-t/2,t,t,col);if(x0==x1&&y0==y1)break;const int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}
/* The graph's top value: the smallest of 1, 2 or 5 times a power of ten that holds the
   largest sample, so the four grid lines fall on round numbers. */
long long nice_top(long long m){if(m<4)return 4;long long p=1;while(p*10<=m)p*=10;for(long long f:{1LL,2LL,5LL,10LL})if(f*p>=m&&(f*p)%4==0)return f*p;return 10*p;}
std::string clock_text(long long seconds){char b[32];snprintf(b,sizeof b,"%lld:%02lld",seconds/60,seconds%60);return b;}
}
/* Which colour the graph draws a row's line in, for a gate to find it: its player colour. */
extern "C" unsigned em_debrief_row_colour(int colour){return player_colour(colour&7);}
extern "C" void em_match_debrief(const EM_Debrief *d,int pressed){memo([&]{
 if(!load()||!d)return;begin(DEBRIEF,d,"visuals-frame","TOP SCORES");
 const int page=std::max(0,std::min(EM_PAGES-1,d->page));
 /* the result and the match, between the title and the tabs */
 const char *result=d->win?"Victory":d->decided?"Defeat":"Match Ended";
 text(result,56,60,ACCENT);
 char facts[160];snprintf(facts,sizeof facts,"%s / %s / %d:%02d",d->mapname[0]?d->mapname:"",d->net?"Network":"Skirmish",d->seconds/60,d->seconds%60);
 const std::string f=fitted(facts,600);text(f,960-textwidth(f),60,MUTED);
 /* the pages, as the Options screen lays its tabs out */
 for(int i=0;i<EM_PAGES;i++)add(EM_DEBRIEF_TAB+i,debrief_tabs[i],38+i*158,86,148).active=i==page;
 /* this machine's row is filled as a selected list row is in this kit: Red Alert's pack
    names the colour, Tiberian Dawn's rows use their own dark green */
 const unsigned on=theme.ra?theme.slot[S_ROWON]:0x273521u;
 const int rows=std::max(0,std::min(8,d->nrows));
 const int games=std::max(1,std::min(4,d->games));
 if(page==EM_PAGE_SUMMARY){
  /* the 1995 and 1996 screen's three panels: commander, battles won, kills one column a game */
  well("COMMANDER",38,130,300,388);well("BATTLES WON",358,130,300,388);well("KILLS",678,130,300,388);
  text("Game",696,182,MUTED);for(int k=0;k<games;k++){const std::string g=std::to_string(k+1);text(g,800+k*53-textwidth(g),182,k==games-1?ACCENT:MUTED);}
  for(int r=0;r<rows;r++){const EM_DebriefRow &row=d->row[r];const int y=210+r*36;
   if(row.is_local){rect(46,y-6,284,30,on);rect(366,y-6,284,30,on);rect(686,y-6,284,30,on);}
   const unsigned ink=row.defeated?MUTED:TEXT;
   debrief_who(row,y);
   const std::string won=std::to_string(row.wins);text(won,508-textwidth(won)/2,y,ink);
   for(int k=0;k<games;k++){const std::string n=row.kills[k]<0?"-":std::to_string(row.kills[k]);text(n,800+k*53-textwidth(n),y,k==games-1?ink:MUTED);}
  }
 }else if(page!=EM_PAGE_GRAPH){
  /* a table page: one wide panel, the commander at the left as on SUMMARY, then four
     columns of figures right-aligned under their headings */
  struct Col{const char *head;int right;};
  const char *harvest=theme.ra?"Ore":"Tiberium";
  Col cols[4];int ncol=4;const char *title="";
  switch(page){
   case EM_PAGE_SCORE:title="SCORE";cols[0]={"Score",700};cols[1]={"Leadership",830};cols[2]={"Efficiency",960};ncol=3;break;
   case EM_PAGE_UNITS:title="UNITS";cols[0]={"Built",560};cols[1]={"Destroyed",690};cols[2]={"Lost",800};cols[3]={"Peak army",960};break;
   case EM_PAGE_STRUCTURES:title="STRUCTURES";cols[0]={"Built",560};cols[1]={"Destroyed",690};cols[2]={"Lost",800};cols[3]={"Standing",960};break;
   default:title="ECONOMY";cols[0]={"Start",560};cols[1]={harvest,690};cols[2]={"Spent",800};cols[3]={"Held",960};break;
  }
  well(title,38,130,940,388);
  text("Commander",56,182,MUTED);
  for(int c=0;c<ncol;c++)text(cols[c].head,cols[c].right-textwidth(cols[c].head),182,MUTED);
  int best=1;for(int r=0;r<rows;r++)best=std::max(best,int(d->row[r].score));
  for(int r=0;r<rows;r++){const EM_DebriefRow &row=d->row[r];const int y=210+r*36;
   if(row.is_local)rect(46,y-6,924,30,on);
   const unsigned ink=row.defeated?MUTED:TEXT;
   debrief_who(row,y);
   long long v[4]={0,0,0,0};
   switch(page){
    case EM_PAGE_SCORE:{
     /* the score as a bar against whoever leads, the old match plate's idiom */
     const int len=int(280LL*std::max(0,int(row.score))/best);
     if(len>0)rect(340,y+4,len,10,player_colour(row.colour&7));
     v[0]=row.score;
     const std::string l=std::to_string(row.leadership)+"%",e=std::to_string(row.efficiency)+"%";
     text(std::to_string(row.score),700-textwidth(std::to_string(row.score)),y,ink);
     text(l,830-textwidth(l),y,ink);text(e,960-textwidth(e),y,ink);
     continue;}
    case EM_PAGE_UNITS:v[0]=row.units_built;v[1]=row.killed;v[2]=row.units_lost;v[3]=row.army_peak;break;
    case EM_PAGE_STRUCTURES:v[0]=row.structures_built;v[1]=row.razed;v[2]=row.structures_lost;v[3]=row.structures_end;break;
    default:v[0]=row.credits_start;v[1]=row.harvested;v[2]=row.spent;v[3]=row.credits_end;break;
   }
   for(int c=0;c<ncol;c++)figure(v[c],cols[c].right,y,ink);
  }
 }else{
  /* THE GRAPH: one line per commander over the match, in their colour, the commander at
     this machine drawn last and thickest. The series is picked with the buttons above
     the plot, or the up and down keys. Time runs along the bottom in minutes and
     seconds, the series' value up the side on four round grid lines. */
  const int series=std::max(0,std::min(EM_SERIES-1,d->series));
  char head[64];snprintf(head,sizeof head,"%s OVER TIME",debrief_series_name(series));
  std::string upper=head;for(char &c:upper)c=char(toupper((unsigned char)c));
  well(upper.c_str(),38,130,940,388);
  for(int k=0;k<EM_SERIES;k++)add(EM_DEBRIEF_SERIES+k,debrief_series_name(k),56+k*150,176,148).active=k==series;
  const int px0=128,px1=952,py0=240,py1=462;
  const int n=std::max(0,std::min(EM_SAMPLES,d->samples));
  if(n<2){
   const std::string none="Not enough of the match was recorded to draw";
   text(none,(px0+px1)/2-textwidth(none)/2,(py0+py1)/2-8,MUTED);
  }else{
   long long top=0;for(int r=0;r<rows;r++)for(int k=0;k<n;k++)top=std::max<long long>(top,d->timeline[r][series][k]);
   top=nice_top(top);
   const unsigned grid=theme.ra?theme.slot[S_ROWHOVER]:0x30342bu;
   for(int g=0;g<=4;g++){const int y=py1-(py1-py0)*g/4;rect(px0,y,px1-px0+1,1,g?grid:MUTED);
    const std::string lab=std::to_string(top*g/4);text(lab,px0-10-textwidth(lab),y-8,MUTED);}
   rect(px0,py0,1,py1-py0+1,MUTED);
   const long long span=(long long)(n-1)*std::max(1,d->sample_ticks)/15;
   for(int g=0;g<=4;g++){const int x=px0+(px1-px0)*g/4;rect(x,py1+1,1,5,MUTED);
    const std::string lab=clock_text(span*g/4);text(lab,g==4?x-textwidth(lab):g?x-textwidth(lab)/2:x,py1+10,MUTED);}
   auto at=[&](int r,int k,int &x,int &y){x=px0+(px1-px0)*k/(n-1);
    y=py1-int((long long)(py1-py0)*std::max<long long>(0,d->timeline[r][series][k])/top);};
   for(int pass=0;pass<2;pass++)for(int r=0;r<rows;r++){
    if((d->row[r].is_local!=0)!=(pass==1))continue;
    const unsigned col=player_colour(d->row[r].colour&7);const int t=pass?4:2;
    int x0,y0;at(r,0,x0,y0);for(int k=1;k<n;k++){int x1,y1;at(r,k,x1,y1);line(x0,y0,x1,y1,col,t);x0=x1;y0=y1;}
   }
  }
 }
 /* this machine's own result first: its score and where that put it, the number a player
    looks for on a screen titled TOP SCORES. Why the match ended takes the line when it
    broke. Under it what this machine's commander did, and under that how the score is
    added up, when there is a score to explain: inside the frame, in the muted ink, as one
    more line of the result rather than a note beside the button. */
 const char *harvest=theme.ra?"Ore":"Tiberium";
 char own[160];
 const int kind=em_match_debrief_result(d,own,sizeof own);text(fitted(own,700),38,536,kind==0?ACCENT:TEXT);
 snprintf(own,sizeof own,"Destroyed: %d units, %d structures   Lost: %d   %s: %d",d->killed,d->razed,d->lost,harvest,d->harvested);
 text(fitted(own,700),38,562,MUTED);
 if(!d->ended[0]&&d->place>0){const std::string how=std::string("Score = units x2 + structures x5 + ")+(theme.ra?"ore":"tiberium")+"/10";if(textwidth(how)<=700)text(how,38,588,MUTED);}
 add(EM_DEBRIEF_CONTINUE,"Continue",758,550,220).active=true;
 v.held=-1;for(int i=0;i<int(v.seats.size());i++)if(v.seats[i].item==pressed)v.held=i;paint();
});}
extern "C" int em_pick(int fx,int fy){int x,y;em_to_legacy(fx,fy,&x,&y);int n=hit(v.mousex,v.mousey);return n<0?-1:v.seats[n].item;}

extern "C" int em_mp_row_at(const MP_State *st,int x,int y){int n=virtualseat(x);return n>=0&&v.seats[n].action==MAPROW?v.seats[n].arg:mp_row_at(st,x,y);}
extern "C" void em_scroll(int delta){if(v.kind==OPS||v.kind==BROWSER||(v.kind==LOBBY&&((const SK_State*)v.owner)->picker))v.opstop=std::max(0,v.opstop+delta);}
extern "C" void em_opt_scroll(DOPT_State *st,int delta){
 if(v.kind==OPT&&v.owner==st&&v.drop>=0&&v.drop!=DOPT_VE_RESOLUTION)return;
 dopt_scroll(st,delta);
}
extern "C" int em_opt_key(DOPT_State *st,int key){
 int before=st->selected;bool enhanced=v.kind==OPT&&v.owner==st;
 if(enhanced&&st->page==DOPT_PAGE_ADVANCED&&v.drop>=0){
  if(key==DOPT_KEY_ESC){v.drop=v.popupChoice=-1;st->texdrop=-1;return DOPT_ACT_NONE;}
  const int count=dopt_drop_count(st,v.drop);
  int choice=v.popupChoice>=0?v.popupChoice:dopt_enh_drop_value(st,v.drop);
  if(key==DOPT_KEY_UP||key==DOPT_KEY_DOWN){
   for(int checked=0;checked<count;checked++){
    choice=(choice+count+(key==DOPT_KEY_UP?-1:1))%count;
    if(dopt_drop_listed(v.drop,choice)&&dopt_enh_drop_allowed(st,v.drop,choice)){
     v.popupChoice=choice;if(v.drop==DOPT_VE_RESOLUTION){if(choice<st->restop)st->restop=choice;if(choice>=st->restop+7)st->restop=choice-6;}
     game_enhanced_menu_sound(0);break;
    }
   }
  }else if(key==DOPT_KEY_ENTER&&dopt_enh_drop_allowed(st,v.drop,choice)){
   dopt_enh_choose(st,v.drop,choice);v.drop=v.popupChoice=-1;game_enhanced_menu_sound(1);
  }
  return DOPT_ACT_NONE;
 }
 if(enhanced&&key==DOPT_KEY_ENTER)for(const Seat &q:v.seats)if(q.item==before&&!q.disabled&&q.kind!=FIELD){game_enhanced_menu_sound(1);break;}
 if(enhanced&&key==DOPT_KEY_ENTER&&saves_blocked(st,st->selected)){dopt_open_page_notice(st,"NOTICE",em_route_notice(EM_ROUTE_SAVES));return DOPT_ACT_NONE;}

 if(v.kind==OPT&&v.owner==st&&st->page==DOPT_PAGE_ADVANCED&&
    ((key==DOPT_KEY_ESC&&st->texdrop<0)||(key==DOPT_KEY_ENTER&&st->selected==DOPT_A_OK)))
  return dopt_enh_action(st,DOPT_A_OK);
 int ret=dopt_key(st,key);
 /* the arrows step over rows Red Alert greys, as they step over any disabled row */
 for(int guard=0;guard<8&&enhanced&&st->page==DOPT_PAGE_ADVANCED&&ra_unavailable(st->selected)&&(key==DOPT_KEY_UP||key==DOPT_KEY_DOWN);guard++)ret=dopt_key(st,key);
 if(enhanced&&st->selected!=before&&(key==DOPT_KEY_UP||key==DOPT_KEY_DOWN||key==DOPT_KEY_LEFT||key==DOPT_KEY_RIGHT))game_enhanced_menu_sound(0);
 if(v.kind==OPT&&v.owner==st&&st->page==DOPT_PAGE_ADVANCED&&(key==DOPT_KEY_UP||key==DOPT_KEY_DOWN)){
  int id=st->selected;
  if(id<=DOPT_VE_FPS||id==DOPT_VE_GAMMA)v.section=0;
  else if(id==DOPT_VE_TEXSET||id==DOPT_VE_INFSET||id==DOPT_VE_WATER||id==DOPT_VE_BILINEAR||id==DOPT_VE_TREES||id==DOPT_VE_TIB3D||id==DOPT_VE_GRASS)v.section=1;
  else if(id>=DOPT_VE_SHADOWS&&id<=DOPT_VE_CRT)v.section=2;
  else if(id==DOPT_VE_UISCALE||id==DOPT_VE_PERSPECTIVE||id==DOPT_VE_NEWHUD||id==DOPT_VE_SMOOTH)v.section=3;
  else if(id>=DOPT_VE_NEWAUDIO&&id<DOPT_VE_COUNT)v.section=4;
 }
 if(st->texdrop>=0)v.drop=st->texdrop;else v.drop=-1;return ret;
}

/* Database shares the accepted housing, native button fittings and original font.
   Scene data comes from the engine table; model pixels are the runtime renderer's. */
extern "C" void em_database(const EM_Database *st){
 if(!load())return;begin(DATABASE,nullptr,"lobby-frame","DATABASE");
 /* Red Alert has a fifth category, its ships, so its tabs are narrower. */
 if(theme.ra){const char *cats[]={"Structures","Infantry","Vehicles","Ships","Aircraft"};
  for(int i=0;i<5;i++)add(1000+i,cats[i],38+i*168,86,160).active=st->category==i;}
 else{const char *cats[]={"Structures","Infantry","Vehicles","Aircraft"};
  for(int i=0;i<4;i++)add(1000+i,cats[i],38+i*200,86,192).active=st->category==i;}
 {Seat &q=add(4000,theme.ra?"Allied":"GDI",890,86,132);q.active=st->house==1;q.badge=0;}
 {Seat &q=add(4001,theme.ra?"Soviet":"Nod",1046,86,132);q.active=st->house==2;q.badge=1;}
 well("ROSTER",38,150,312,368);well("MODEL PREVIEW",370,150,368,368);well("FIELD DATA",758,150,420,368);
 for(int i=0;i<8&&i+st->top<st->count;i++)add(2000+i+st->top,st->names[i+st->top],56,200+i*32,276,30,ROW).active=i+st->top==st->selected;
 add(3000,"Previous",56,474,132,32).disabled=st->top==0;
 add(3001,"Next",200,474,132,32).disabled=st->top+8>=st->count;
 text(std::to_string(st->selected+1)+" / "+std::to_string(st->count),194,458,MUTED,true);
 const char *tabs[]={"Info","Stats","Matchups","Tech"};
 const int tabx[]={770,858,946,1086},tabw[]={80,80,132,80};
 for(int i=0;i<4;i++)add(5000+i,tabs[i],tabx[i],200,tabw[i],40).active=st->detail==i;
 auto names=wrapped(st->name?st->name:"",384);int y=250;
 for(auto &line:names){text(line,776,y,TEXT);y+=24;}
 text(std::string(st->code?st->code:"")+" / $"+std::to_string(st->cost),776,y+4,ACCENT);y+=40;
 if(st->detail==0){
  note(st->brief?st->brief:"",776,y,384,5,MUTED,24);
  text("Armour",776,454,MUTED);text(st->armour?st->armour:"--",952,454,TEXT);
  text("Hit points",776,486,MUTED);text(std::to_string(st->hp),952,486,ACCENT);
 }else if(st->detail==1){
  const char *labels[]={"Damage","Speed","Attack interval","Range"};
  char range[48];snprintf(range,sizeof range,"%.1f cells",st->range256/256.f);
  std::string vals[]={st->damage<0?"Unarmed":std::to_string(st->damage),st->speed<0?"Stationary":std::to_string(st->speed)+" mph",st->rof<0?"--":std::to_string(st->rof)+" ticks",st->range256<0?"--":range};
  for(int i=0;i<4;i++){text(labels[i],776,y+i*28,MUTED);text(vals[i],1000,y+i*28,TEXT);}
 }else if(st->detail==2){
  text("Strong against",776,y,ACCENT);auto strong=wrapped(st->strong&&*st->strong?st->strong:"None",384);
  for(auto &line:strong){text(line,776,y+26,MUTED);y+=22;}y+=36;
  text("Weak against",776,y,ACCENT);note(st->weak&&*st->weak?st->weak:"None",776,y+26,384,7,MUTED,22);
 }else{
  text("Unlocks",776,y,ACCENT);note(st->unlocks&&*st->unlocks?st->unlocks:"None",776,y+28,384,7,MUTED,22);
 }
 /* A Red Alert type without an Enhanced model shows its 1996 cameo, at twice its size. */
 if(!st->model_available&&theme.ra&&st->cameo&&st->cameo_w>0&&st->cameo_h>0){Art a;a.w=st->cameo_w;a.h=st->cameo_h;a.pixels.assign(st->cameo,st->cameo+a.w*a.h*4);blit(a,388+(332-a.w*2)/2,202+(296-a.h*2)/2,2);}
 else if(!st->model_available)note("Enhanced model not installed",390,324,328,3,MUTED,26);
 text("Arrows: browse / Esc: close",38,564,MUTED);add(6000,"Close",958,550,220);paint();
}
extern "C" int em_database_view(int *x,int *y,int *w,int *h){
 if(v.kind!=DATABASE)return 0;*x=v.x+388*v.scale;*y=v.y+202*v.scale;*w=332*v.scale;*h=296*v.scale;return 1;
}
extern "C" int em_panel_rect(int *x,int *y,int *w,int *h){
 if(!v.w||!v.h||!v.fbw)return 0;*x=v.x;*y=v.y;*w=v.w*v.scale;*h=v.h*v.scale;return 1;
}

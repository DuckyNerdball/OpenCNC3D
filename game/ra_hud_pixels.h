/* Exact approved pixels, native units. Shared by live HUD and selection-card adapter. */
#ifndef RA_HUD_PIXELS_H
#define RA_HUD_PIXELS_H
#include "ra_hud_layout.h"
static H6_Pack *g_raHudPack=0;
static bool rah_enabled(void);
static bool rah_pack_ok(const H6_Pack *p){if(!p)return false;for(size_t k=0;k<sizeof(rah_asset_specs)/sizeof(rah_asset_specs[0]);k++){const RAH_AssetSpec& spec=rah_asset_specs[k];const H6_Asset *a=hud640_asset(p,spec.name);if(!a||a->w!=spec.w||a->h!=spec.h||a->frames!=spec.frames)return false;}return true;}
struct RAH_Surface {unsigned char *p;int w,h;};
static void rah_pixel(RAH_Surface s,int x,int y,const unsigned char *q){
 if(x<0||y<0||x>=s.w||y>=s.h||!q[3])return;
 unsigned char *d=s.p+((size_t)y*s.w+x)*4;
 if(q[3]==255){memcpy(d,q,4);return;}
 unsigned a=q[3],oa=a+d[3]*(255-a)/255;
 for(int k=0;k<3;k++)d[k]=oa?(q[k]*a+d[k]*d[3]*(255-a)/255)/oa:0;
 d[3]=(unsigned char)oa;
}
static void rah_rect(RAH_Surface s,int x,int y,int w,int h,int r,int g,int b,int a=255){unsigned char q[4]={(unsigned char)r,(unsigned char)g,(unsigned char)b,(unsigned char)a};for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)rah_pixel(s,xx,yy,q);}
/* rah_raw and rah_art take the source pixel (yy*sh/h, xx*sw/w); at 1:1 that is (yy, xx)
   exactly, so the 1:1 case skips the two divisions and reads the same pixels. */
static void rah_raw(RAH_Surface s,const unsigned char *p,int sw,int sh,int x,int y,int w,int h){if(!p)return;
 if(sw==w&&sh==h){for(int yy=0;yy<h;yy++){const unsigned char *row=p+(size_t)yy*sw*4;for(int xx=0;xx<w;xx++)rah_pixel(s,x+xx,y+yy,row+(size_t)xx*4);}return;}
 for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)rah_pixel(s,x+xx,y+yy,p+((size_t)(yy*sh/h)*sw+xx*sw/w)*4);}
static void rah_art(RAH_Surface s,const char *name,int x,int y,int frame=0,int w=0,int h=0){const H6_Asset *a=hud640_asset(g_raHudPack,name);if(!a)return;frame=std::max(0,std::min(a->frames-1,frame));if(!w)w=a->w;if(!h)h=a->h;
 if(w==a->w&&h==a->h){for(int yy=0;yy<h;yy++){const unsigned char *row=a->rgba+((size_t)yy*a->w*a->frames+(size_t)frame*a->w)*4;for(int xx=0;xx<w;xx++)rah_pixel(s,x+xx,y+yy,row+(size_t)xx*4);}return;}
 for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){const unsigned char *q=a->rgba+((size_t)(yy*a->h/h)*a->w*a->frames+frame*a->w+xx*a->w/w)*4;rah_pixel(s,x+xx,y+yy,q);}}
static int rah_text_w(const char *t){int w=0;for(;t&&*t;t++){unsigned c=(unsigned char)*t;if(c>='a'&&c<='z')c-=32;if(c<sizeof rah_adv)w+=rah_adv[c];}return w;}
static void rah_text(RAH_Surface s,const char *t,int x,int y,const char *font="font",bool center=false,int maxw=10000){
 const H6_Asset *a=hud640_asset(g_raHudPack,font);if(!a)return;if(center)x-=rah_text_w(t)/2;int left=x;
 for(;t&&*t;t++){unsigned c=(unsigned char)*t;if(c>='a'&&c<='z')c-=32;if(c>=sizeof rah_adv)continue;if(x+rah_adv[c]>left+maxw)break;for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)rah_pixel(s,x+xx,y+yy,a->rgba+((size_t)yy*a->w+c*8+xx)*4);x+=rah_adv[c];}}
/* A SCROLL BUTTON'S ARROW, a filled triangle five rows tall and nine wide at its base, in the
   ink of the given font: the up and down buttons are one shape mirrored. The two used to be
   the characters ^ and V, and in this font the caret is a short mark at the top of the line
   while V fills it, so the up arrow read half the size of the down one. */
static void rah_scroll_arrow(RAH_Surface s,int cx,int top,bool down,const char *font){
 const H6_Asset *a=hud640_asset(g_raHudPack,font);if(!a)return;
 unsigned char ink[4]={0,0,0,0};const unsigned c='V';
 for(int yy=0;yy<8&&!ink[3];yy++)for(int xx=0;xx<8&&!ink[3];xx++){const unsigned char *q=a->rgba+((size_t)yy*a->w+c*8+xx)*4;if(q[3]==255)memcpy(ink,q,4);}
 if(!ink[3])return;
 for(int r=0;r<5;r++){const int half=down?4-r:r;for(int xx=cx-half;xx<=cx+half;xx++)rah_pixel(s,xx,top+r,ink);}
}
static void rah_well(RAH_Surface s,int x,int y,int w,int h){rah_rect(s,x,y,w,h,16,19,17);rah_rect(s,x,y,w,1,9,11,9);rah_rect(s,x,y,1,h,9,11,9);rah_rect(s,x,y+h-1,w,1,103,109,103);rah_rect(s,x+w-1,y,1,h,103,109,103);}
static void rah_outline(RAH_Surface s,int x,int y,int w,int h){rah_rect(s,x,y,w,1,231,167,0);rah_rect(s,x,y+h-1,w,1,231,167,0);rah_rect(s,x,y,1,h,231,167,0);rah_rect(s,x+w-1,y,1,h,231,167,0);}
static int rah_height(int rows){return std::max(450,309+rows*64);}
static int rah_power_height(int n,int limit=405){int v=0;n=std::max(0,std::min(n,1000000));for(int k=0;k<n/100&&v<limit-5;k++)v+=(limit-v)/6;return v+((limit-v)/6)*(n%100)/100;}
#endif

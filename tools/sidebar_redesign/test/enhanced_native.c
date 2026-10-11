/* Compare native runtime output to snapshots of the approved Lab compositor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hud640.h"
#include "hud_enhanced.h"
#include "hud640.c"
static void output(const char *dir,const char *name,unsigned char *px,int size){char path[1024];FILE *f;snprintf(path,sizeof path,"%s/%s.rgba",dir,name);f=fopen(path,"wb");if(!f)exit(3);fwrite(px,1,size,f);fclose(f);}
int main(int argc,char **argv){H6_Pack *p;EH_State s;char e[256],name[32];unsigned char *d;int i,j,n;float progress[3]={0,.38f,1};
 if(argc!=3)return 1;p=hud640_load(argv[1],e,256);if(!p||!hud_enhanced_ok(p))return 2;
 d=malloc(EH_WIDTH*EH_MAX_H*4);memset(&s,0,sizeof s);s.rows=8;s.base.rows=8;s.base.power_level=41;s.base.power_drain_level=30;
 for(i=0;i<3;i++)s.tool_frame[i]=-1;
 hud_enhanced_bar(d,p,&s);output(argv[2],"body",d,EH_WIDTH*EH_MAX_H*4);
 for(n=0;n<2;n++)for(j=0;j<3;j++){
  for(i=0;i<64*48*4;i++)d[i]=(unsigned char)(i%4==3?255:i%251);
  hud_enhanced_clock(d,progress[j],0,n,1.25f);snprintf(name,sizeof name,"%s-%s",n?"nod":"gdi",j==0?"0":j==1?"38":"100");output(argv[2],name,d,64*48*4);
 }
 {
  static const char *icons[]={"eh_repair","eh_sell","eh_power","eh_structures","eh_infantry","eh_vehicles","eh_aircraft"};
  for(n=0;n<7;n++)for(j=0;j<2;j++)eh_button(d+(n*2+j)*35*35*4,p,icons[n],j?3:0,1);
  output(argv[2],"buttons",d,7*2*35*35*4);
 }
 free(d);hud640_free(p);return 0;
}

#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../game/fx_state.h"
#include "../../game/fx_panel_policy.h"
static const FxParam* param(const char* key){for(const FxParam*p=FX_PARAMS;p->kind!=FXP_END;p++)if(p->kind!=FXP_GROUP&&!strcmp(p->key,key))return p;assert(false);return 0;}
int main(int argc,char**argv){
 assert(argc==3);
 const char* enabled[]={"st_second_sun_on","vh_second_sun_on","inf_second_sun_on"};
 const char* powers[]={"st_second_sun_power","vh_second_sun_power","inf_second_sun_power"};
 for(int ra=0;ra<2;ra++){
  fx_red_alert_dials(ra);FxState s;fx_defaults(&s);s.enabled=1;s.dos_classic=0;
  int matched=0;FILE*f=fopen(argv[1],"r");assert(f);char line[512],key[160];float value;
  while(fgets(line,sizeof line,f))if(sscanf(line,"%159s %f",key,&value)==2)
   for(int t=0;t<4;t++){char prefix[64];snprintf(prefix,sizeof prefix,"lighting.%s.",FX_LIGHT_KEY[t]);if(strncmp(key,prefix,strlen(prefix)))continue;fx_lighting_select(&s,t);assert(fabsf(fx_lighting_get(&s,param(key+strlen(prefix)))-value)<0.00001f);matched++;}
  fclose(f);assert(matched==884);
  for(int t=0;t<4;t++){
   fx_lighting_select(&s,t);
   s.st_second_sun_on=s.vh_second_sun_on=s.inf_second_sun_on=0;
   s.st_second_sun_power=s.vh_second_sun_power=s.inf_second_sun_power=1.0f;
   for(int c=0;c<3;c++){
    const FxParam* on=param(enabled[c]);const FxParam* power=param(powers[c]);
    assert(!*fx_iptr(&s,on)&&*fx_fptr(&s,power)==1.0f);assert(!fxp_row_enabled(power,&s));
    *fx_fptr(&s,power)=2.5f;assert(fx_second_sun_power(&s,c)==1.0f);
    *fx_iptr(&s,on)=1;assert(fxp_row_enabled(power,&s)&&fx_second_sun_power(&s,c)==2.5f);
    *fx_iptr(&s,on)=0;*fx_fptr(&s,power)=1.0f;
   }
  }
  fx_lighting_select(&s,0);s.st_second_sun_on=1;s.st_second_sun_power=0.0f;s.vh_second_sun_on=1;s.vh_second_sun_power=2.5f;s.inf_second_sun_on=1;s.inf_second_sun_power=0.5f;
  fx_lighting_select(&s,1);assert(!s.st_second_sun_on&&!s.vh_second_sun_on&&!s.inf_second_sun_on);
  assert(fx_save(&s,argv[2]));FxState loaded;fx_defaults(&loaded);fx_load(&loaded,argv[2]);fx_lighting_select(&loaded,0);
  assert(loaded.st_second_sun_on&&loaded.st_second_sun_power==0.0f&&loaded.vh_second_sun_on&&loaded.vh_second_sun_power==2.5f&&loaded.inf_second_sun_on&&loaded.inf_second_sun_power==0.5f);
  fx_defaults(&loaded);fx_load(&loaded,argv[1]);fx_lighting_select(&loaded,0);assert(loaded.st_second_sun_on&&fabsf(loaded.st_second_sun_power-1.232143f)<0.00001f);
  const FxParam* group=0;int rows=0;for(const FxParam*p=FX_PARAMS;p->kind!=FXP_END;p++){if(p->kind==FXP_GROUP)group=p;else for(int c=0;c<3;c++)if(p==param(enabled[c])||p==param(powers[c])){assert(fx_lighting_param(p,group));assert(fxp_param_tab(p,group)==2&&strlen(p->label)*6<=160);rows++;}}
  assert(rows==6);printf("SECOND-SUN-POWER|%s|884-authority-values|four-profiles|off-neutral|checkbox-enables-slider|independent-roundtrip|Lighting-tab|PASS\n",ra?"RA":"TD");
 }
}

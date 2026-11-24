#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../game/fx_state.h"
#include "../../game/fx_panel_policy.h"
int main(int argc,char**argv){
 assert(argc==3);
 for(int ra=0;ra<2;ra++){
  fx_red_alert_dials(ra);FxState state;fx_defaults(&state);state.enabled=1;state.dos_classic=0;
  int authority=0;FILE*f=fopen(argv[1],"r");assert(f);char line[512],key[160];float value;
  while(fgets(line,sizeof line,f))if(sscanf(line,"%159s %f",key,&value)==2){
   for(int t=0;t<4;t++){
    char prefix[64];snprintf(prefix,sizeof prefix,"lighting.%s.",FX_LIGHT_KEY[t]);
    if(strncmp(key,prefix,strlen(prefix)))continue;
    fx_lighting_select(&state,t);const FxParam*group=0;
    for(const FxParam*p=FX_PARAMS;p->kind!=FXP_END;p++){
     if(p->kind==FXP_GROUP)group=p;
     else if(!strcmp(p->key,key+strlen(prefix))){assert(fx_lighting_param(p,group));assert(fabsf(fx_lighting_get(&state,p)-value)<0.00001f);authority++;}
    }
   }
  }
  fclose(f);assert(authority==884);
  fx_lighting_select(&state,0);state.second_sun_on=1;state.second_sun_az=302.5f;state.second_sun_energy=2.2f;
  fx_lighting_select(&state,1);assert(!state.second_sun_on&&state.second_sun_az==225.0f&&state.second_sun_energy==1.0f);
  fx_lighting_select(&state,0);assert(state.second_sun_on&&state.second_sun_az==302.5f);
  assert(fx_save(&state,argv[2]));FxState loaded;fx_defaults(&loaded);fx_load(&loaded,argv[2]);
  fx_lighting_select(&loaded,0);assert(loaded.second_sun_on&&loaded.second_sun_az==302.5f&&fabsf(loaded.second_sun_energy-2.2f)<0.00001f);
  fx_lighting_select(&loaded,1);assert(!loaded.second_sun_on&&loaded.second_sun_az==225.0f&&loaded.second_sun_energy==1.0f);
  fx_defaults(&loaded);fx_load(&loaded,argv[1]);
  const FxParam*group=0;int rows=0;
  for(const FxParam*p=FX_PARAMS;p->kind!=FXP_END;p++){
   if(p->kind==FXP_GROUP)group=p;
   else if(!strncmp(p->key,"second_sun_",11)){assert(fx_lighting_param(p,group));assert(fxp_param_tab(p,group)==2);assert(strlen(p->label)*6<=160);rows++;}
  }
  assert(rows==3);printf("SECOND-SUN|%s|884-authority-values|four-profiles|toggle-bearing-energy-roundtrip|saved-authority|Lighting-tab|label-fit|PASS\n",ra?"RA":"TD");
 }
}

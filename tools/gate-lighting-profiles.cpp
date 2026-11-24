#include <cassert>
#include <string>
#include "../game/fx_state.h"
#include "../game/fx_panel_policy.h"
static std::string read_cfg(const char* path){FILE*f=fopen(path,"r");assert(f);std::string s;char b[1024];while(fgets(b,sizeof b,f))s+=b;fclose(f);return s;}
int main(int argc,char** argv){
 for(int ra=0;ra<2;ra++) {
 fx_red_alert_dials(ra);
 for(int t=0;t<4;t++) {
  FxState a,b;fx_defaults(&a);fx_lighting_select(&a,t);
  fx_save(&a,"/tmp/cnc3d-v091-context-a.cfg");fx_defaults(&b);
  fx_load(&b,"/tmp/cnc3d-v091-context-a.cfg");assert(b.lighting_active==t);
  fx_save(&b,"/tmp/cnc3d-v091-context-b.cfg");
  assert(read_cfg("/tmp/cnc3d-v091-context-a.cfg")==read_cfg("/tmp/cnc3d-v091-context-b.cfg"));
  const std::string saved=read_cfg("/tmp/cnc3d-v091-context-a.cfg");
  assert(saved.find("# was")==std::string::npos && saved.find("# moved")==std::string::npos);
 }
 printf("LIGHTING|%s|four-theater-context|default-annotations|exact-roundtrip|PASS\n",ra?"RA":"TD");
 FxState s;fx_defaults(&s);s.enabled=1;s.dos_classic=0;
 if(argc>1) {
  FILE* f=fopen(argv[1],"r");assert(f);char line[512],key[128];float value;int matched=0;
  while(fgets(line,sizeof line,f))if(sscanf(line,"%127s %f",key,&value)==2) {
   int t=!strncmp(key,"lighting.temperate.",19)?0:!strncmp(key,"lighting.desert.",16)?3:-1;
   if(t<0)continue;const char* k=key+(t==0?19:16);fx_lighting_select(&s,t);
   for(const FxParam*p=FX_PARAMS;p->kind!=FXP_END;p++)if(p->kind!=FXP_GROUP&&!strcmp(p->key,k)) {
    assert(fabsf(fx_lighting_get(&s,p)-value)<0.000001f);matched++;
   }
  }
  fclose(f);assert(matched==424);
  printf("LIGHTING|%s|promoted-424-authority-values|PASS\n",ra?"RA":"TD");
  fx_defaults(&s);s.enabled=1;s.dos_classic=0;
 }
 s.fow_smooth=0;s.fow_falloff=1.7f;s.fow_darkness=.81f;s.fow_final=.63f;s.fow_noise=.35f;
 for(int t=0;t<4;t++){fx_lighting_select(&s,t);assert(s.fow_smooth==0&&s.fow_falloff==1.7f&&s.fow_darkness==.81f&&s.fow_final==.63f&&s.fow_noise==.35f);}
 fx_lighting_select(&s,1);const float baseline=s.sun_energy;fx_lighting_select(&s,0);s.sun_energy=.31f;s.etree_shadow_fill=.23f;
 fx_lighting_select(&s,1);assert(s.sun_energy==baseline);s.sun_energy=.51f;
 fx_lighting_select(&s,2);s.sun_energy=.72f;fx_lighting_select(&s,3);s.sun_energy=.92f;
 fx_lighting_select(&s,0);assert(s.sun_energy==.31f);assert(s.etree_shadow_fill==.23f);
 s.mouse_scheme=1;s.water_flow=.4f;fx_save(&s,"/tmp/cnc3d-v091-lighting-roundtrip.cfg");
 FxState loaded;fx_defaults(&loaded);fx_load(&loaded,"/tmp/cnc3d-v091-lighting-roundtrip.cfg");
 assert(loaded.fow_smooth==0&&loaded.fow_falloff==1.7f&&loaded.fow_darkness==.81f&&loaded.fow_final==.63f&&loaded.fow_noise==.35f);
 const FxParam*fg=0;int fogRows=0;
 for(const FxParam*p=FX_PARAMS;p->kind!=FXP_END;p++){if(p->kind==FXP_GROUP)fg=p;else if(!strncmp(p->key,"fow_",4)){assert(!fx_lighting_param(p,fg));assert(fxp_param_tab(p,fg)==2);fogRows++;}}
 assert(fogRows==5);
 printf("FOG|%s|global-across-four-theaters|preset-roundtrip|Lighting-tab|PASS\n",ra?"RA":"TD");
 float expected[]={.31f,.51f,.72f,.92f};for(int t=0;t<4;t++){fx_lighting_select(&loaded,t);assert(fabsf(loaded.sun_energy-expected[t])<.0001f);assert(loaded.mouse_scheme==1);assert(loaded.water_flow==.4f);}
 FILE*f=fopen("/tmp/cnc3d-v091-lighting-legacy.cfg","w");fprintf(f,"sun_energy .63\netree_shadow_fill .41\n");fclose(f);
 fx_defaults(&loaded);fx_load(&loaded,"/tmp/cnc3d-v091-lighting-legacy.cfg");for(int t=0;t<4;t++){fx_lighting_select(&loaded,t);assert(fabsf(loaded.sun_energy-.63f)<.0001f);}
 // Classic switches must never overwrite another theater's saved toggles.
 fx_defaults(&loaded);loaded.enabled=1;fx_lighting_select(&loaded,0);loaded.shadow_on=0;
 fx_lighting_select(&loaded,1);loaded.shadow_on=1;fx_lighting_select(&loaded,0);
 loaded.dos_classic=1;loaded.enabled=0;loaded.shadow_on=0;
 fx_lighting_select(&loaded,1);fx_lighting_capture(&loaded,1);
 loaded.dos_classic=0;loaded.enabled=1;fx_lighting_apply(&loaded,1);assert(loaded.shadow_on==1);
 // Public Visuals edits must survive the theater apply, including edits from Classic.
 const int editRows[]={FXR_SHADOWS,FXR_AO,FXR_LIGHT,FXR_BLOOM,FXR_GRADE};
 for(int n=0;n<5;n++) {
  FxState ui;fx_defaults(&ui);ui.enabled=1;ui.dos_classic=0;
  fx_lighting_select(&ui,0);fx_mode_set_live(&ui,editRows[n],1);fx_lighting_capture(&ui,0);
  fx_lighting_select(&ui,1);fx_mode_set_live(&ui,editRows[n],1);fx_lighting_capture(&ui,1);
  fx_lighting_select(&ui,0);float before[FXR_COUNT];memcpy(before,ui.mode_row[FX_MODE_ENHANCED],sizeof before);
  before[editRows[n]]=1;ui.mode_row[FX_MODE_ENHANCED][editRows[n]]=0;
  fx_lighting_mode_edits(&ui,before);fx_lighting_apply(&ui,0);assert(fx_mode_live(&ui,editRows[n])==0);
  fx_lighting_select(&ui,1);assert(fx_mode_live(&ui,editRows[n])==1);fx_lighting_select(&ui,0);
  ui.enabled=0;ui.dos_classic=1;fx_mode_set_live(&ui,editRows[n],0);
  memcpy(before,ui.mode_row[FX_MODE_ENHANCED],sizeof before);ui.mode_row[FX_MODE_ENHANCED][editRows[n]]=1;
  fx_lighting_mode_edits(&ui,before);assert(fx_mode_live(&ui,editRows[n])==0);
  ui.enabled=1;ui.dos_classic=0;fx_lighting_apply(&ui,0);assert(fx_mode_live(&ui,editRows[n])==1);
 }
 printf("LIGHTING|%s|public-Visuals-five-switches|theater-isolation|inactive-Enhanced-edits|PASS\n",ra?"RA":"TD");
 int count=0;const FxParam*g=0;for(const FxParam*p=FX_PARAMS;p->kind!=FXP_END;p++){if(p->kind==FXP_GROUP)g=p;else if(fx_lighting_param(p,g)){count++;assert(fxp_param_tab(p,g)==2);}}
 assert(count<FX_LIGHT_SLOTS);printf("LIGHTING|%s|four-independent-profiles|roundtrip|legacy|input-and-motion-preserved|slots=%d|PASS\n",ra?"RA":"TD",count);
 }
}

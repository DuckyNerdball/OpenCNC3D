/* Start every installed original expansion mission through the real campaign API. */
#define main ra_lockstep_gate_main
#include "ra_twobrain.c"
#undef main
int main(int argc,char**argv) {
 if(argc!=3)return 2;Brain b={0};
 if(!load_brain(&b,argv[1],RB_COPY_A,"expansions"))return 2;
 char missions[2048],data[2048],catalog[2048];
 snprintf(missions,sizeof missions,"%s/missions/",argv[2]);snprintf(data,sizeof data,"%s/data/",argv[2]);snprintf(catalog,sizeof catalog,"%s/expansions.txt",argv[2]);
 b.AddModPath(data);b.AddModPath(missions);b.Init("-HEADLESS",noop_callback);
 typedef unsigned char(CNC_CALL* Start)(int,int,int,int,const char*,const char*,const char*,int,const char*);
 Start start=(Start)cnc_lib_sym(b.lib,"CNC_Start_Instance_Variation");if(!start)return 3;
 FILE*f=fopen(catalog,"r");if(!f)return 4;char line[512],family[24],code[16];int count=0;
 for(int catalog_index=0;catalog_index<2;catalog_index++) {
 if(catalog_index){snprintf(catalog,sizeof catalog,"%s/ants.txt",argv[2]);f=fopen(catalog,"r");if(!f)return 4;}
 while(fgets(line,sizeof line,f)) {
  if(sscanf(line,"%23s %15s",family,code)!=2)continue;
  char ini[32];snprintf(ini,sizeof ini,"%s.INI",code);
  if(!start(atoi(code+3),0,0,10,code[2]=='U'?"USSR":code[2]=='A'?"SPAIN":"GREECE","GAME_NORMAL",data,0,ini))return 5;
  for(int t=0;t<60;t++)b.Advance(0ULL);
  size_t bytes;char*dump=capture_dump(&b,&bytes);if(!dump||!strstr(dump,"OBJDUMP-END"))return 6;free(dump);
  printf("RAEXPANSIONSTART|%s|%s|60-ticks|PASS\n",family,code);count++;
 }
 fclose(f);}
 printf("RAEXPANSIONSTART|total=%d|%s\n",count,count==38?"PASS":"FAIL");return count==38?0:1;
}

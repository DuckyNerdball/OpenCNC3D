#define main td_lockstep_gate_main
#include "cnc_twobrain.c"
#undef main
static int owned(const char* data,const char* house) {
 int n=0;const char* p=data;
 while(*p){const char* e=strchr(p,'\n');if(!e)e=p+strlen(p);const char* h=strstr(p,house);if(!strncmp(p,"OBJ|",4)&&h&&h<e)n++;p=*e?e+1:e;}return n;
}
int main(int argc,char**argv) {
 if(argc!=4)return 2;Brain a={0},b={0};size_t na,nb;
 if(!load_brain(&a,argv[1],TB_COPY_A,"warm")||!load_brain(&b,argv[1],TB_COPY_B,"fresh"))return 2;
 CNCRulesDataStruct rules;fill_rules(&rules);a.Init("",noop_callback);b.Init("",noop_callback);a.Config(&rules);b.Config(&rules);
 typedef unsigned char(CNC_CALL* Blowup)(unsigned long long);
 Blowup blow=(Blowup)cnc_lib_sym(a.lib,"OpenCNC3D_Player_Blowup");if(!blow)return 3;
 CNCMultiplayerOptionsStruct opt={0};CNCPlayerInfoStruct seats[2]={{0}};
 opt.MPlayerCount=2;opt.MPlayerBases=1;opt.MPlayerCredits=10000;opt.MPlayerTiberium=1;opt.MPlayerSolo=1;opt.DestroyStructures=1;opt.ShortGame=1;
 for(int i=0;i<2;i++){snprintf(seats[i].Name,sizeof seats[i].Name,"seat%d",i);seats[i].House=i;seats[i].ColorIndex=i;seats[i].GlyphxPlayerID=i+1;seats[i].Team=i;seats[i].StartLocationIndex=i;}
 if(!a.SetMP(0,&opt,2,seats,2)||!a.Start(argv[2],argv[3],"SCM01EA",7,1))return 4;
 for(int t=0;t<30;t++)a.Advance(1ULL);if(!blow(1ULL))return 5;for(int t=0;t<300;t++)a.Advance(1ULL);
 if(!a.Start(argv[2],argv[3],"SCG01EA",1,0)||!b.Start(argv[2],argv[3],"SCG01EA",1,0))return 6;
 for(int t=0;t<300;t++){a.Advance(0ULL);b.Advance(0ULL);}
 char*da=capture_dump(&a,&na),*db=capture_dump(&b,&nb);if(!da||!db)return 7;
 int after=owned(da,"|GoodGuy|"),control=owned(db,"|GoodGuy|");free(da);free(db);
 printf("TD-SESSION|campaign=SCG01EA|ticks=300|after-surrender=%d|fresh=%d|%s\n",after,control,after>0&&after==control?"PASS":"FAIL");return after>0&&after==control?0:1;
}

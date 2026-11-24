/* The Python driver creates real archive/JSON edge fixtures. */
#include "../app/map_import.h"
#include <cstdio>
static int last_progress=-1,steps=0;
static void progress(void*,int n,const char*){if(n<last_progress||n<0||n>100){fprintf(stderr,"non-monotonic progress\n");std::abort();}last_progress=n;++steps;}
int main(int argc,char**argv){
    if(argc<4)return 2;
    std::string action=argv[1];mapimport::Info info;
    if(action=="reject") {bool ok=mapimport::inspect(argv[2],info);printf("%s\n",info.message.c_str());return !ok&&info.message.find(argv[3])!=std::string::npos?0:1;}
    if(action=="inspect") {bool ok=mapimport::inspect(argv[2],info);printf("%s\n",info.message.c_str());return ok&&info.bundle_count==atoi(argv[3])?0:1;}
    if(argc<5)return 2;
    if(!mapimport::run(argv[2],argv[3],argv[4],info,progress)){fprintf(stderr,"%s\n",info.message.c_str());return 1;}
    printf("DONE|%d|steps=%d|last=%d\n",info.bundle_count,steps,last_progress);
    for(const auto& m:info.imported_maps)printf("MAP\t%s\t%s\t%s\t%s\n",m.red_alert?"ra":"td",m.multiplayer?"mp":"sp",m.scenario.c_str(),m.name.c_str());
    if(info.imported_maps.empty())printf("MAP\t%s\t%s\t%s\t%s\n",info.red_alert?"ra":"td",info.multiplayer?"mp":"sp",info.scenario.c_str(),info.name.c_str());
    return last_progress==100?0:1;
}

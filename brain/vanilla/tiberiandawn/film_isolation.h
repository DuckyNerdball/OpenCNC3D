#ifndef CNC3D_FILM_ISOLATION_H
#define CNC3D_FILM_ISOLATION_H
#include <cstdlib>
#include <cstdio>
#include <cstring>
// Explicit private background-capture controls. Normal launches have no override.
static bool Film_Isolation_Enabled()
{
    static const bool on = []() {
        const char* a=std::getenv("OPENCNC3D_FILM_ISOLATE");
        const char* b=std::getenv("OPENCNC3D_BACKGROUND");
        const char* c=std::getenv("OPENCNC3D_FILM_PRIVATE_MISSION");
        const bool yes=a&&b&&c&&!std::strcmp(a,"1")&&!std::strcmp(b,"1")&&!std::strcmp(c,"1");
        if(yes) std::fprintf(stderr,"FILM_ISOLATION|private background capture|director=off|free-actors=off|power-decay=off|defenses=whitelist\n");
        return yes;
    }();
    return on;
}
static bool Film_Defense_Allowed(int x,int y)
{
    if(!Film_Isolation_Enabled())return true;
    const char* p=std::getenv("OPENCNC3D_FILM_DEFENSES");
    while(p&&*p){int a=-1,b=-1;if(std::sscanf(p,"%d,%d",&a,&b)==2&&a==x&&b==y)return true;p=std::strchr(p,';');if(p)++p;}
    return false;
}
#endif

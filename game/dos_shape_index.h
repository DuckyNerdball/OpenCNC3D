#ifndef OPENCNC3D_DOS_SHAPE_INDEX_H
#define OPENCNC3D_DOS_SHAPE_INDEX_H
#include <cstddef>
#include <cstring>
/* The pack admits at most 1024 immutable, NUL-terminated 15-byte names.
   Store source indices in a half-full fixed table. Duplicate names retain the
   first source record, just like the original linear scan. No allocation or
   texture/pose/palette data is kept here. Clear before every pack load/free. */
struct DosShapeIndex {
    enum { Capacity=2048,MaxShapes=1024 };
    int slot[Capacity];const void* base=NULL;size_t count=0;bool ready=false;
    void clear(){ready=false;base=NULL;count=0;}
    static unsigned hash(const char* name){unsigned h=2166136261u;for(;*name;name++)h=(h^(unsigned char)*name)*16777619u;return h;}
    template<class Shapes>bool matches(const Shapes& shapes)const{return ready&&base==(shapes.empty()?NULL:shapes.data())&&count==shapes.size();}
    template<class Shapes>bool build(const Shapes& shapes){
        clear();if(shapes.size()>MaxShapes)return false;
        for(size_t i=0;i<Capacity;i++)slot[i]=-1;
        for(size_t i=0;i<shapes.size();i++){
            const char* name=shapes[i].name;
            if(!memchr(name,0,16)){clear();return false;}
            size_t p=hash(name)&(Capacity-1);
            while(slot[p]>=0 && strcmp(shapes[(size_t)slot[p]].name,name))p=(p+1)&(Capacity-1);
            if(slot[p]<0)slot[p]=(int)i;
        }
        base=shapes.empty()?NULL:shapes.data();count=shapes.size();ready=true;return true;
    }
    template<class Shapes>int find(const Shapes& shapes,const char* name)const{
        if(!matches(shapes))return -2;
        /* No admitted name can match a longer query. */
        size_t n=0;while(n<16&&name[n])n++;if(n==16)return -1;
        size_t p=hash(name)&(Capacity-1);
        while(slot[p]>=0){const int i=slot[p];if(!strcmp(shapes[(size_t)i].name,name))return i;p=(p+1)&(Capacity-1);}
        return -1;
    }
};
static_assert(sizeof(DosShapeIndex)<9u*1024u,"Fixed sprite index budget");
#endif

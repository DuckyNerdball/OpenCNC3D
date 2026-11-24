#include <vector>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "../../../game/dos_shape_index.h"
struct Shape{char name[16];};
static int reference(const std::vector<Shape>& shapes,const char* name){for(size_t i=0;i<shapes.size();i++)if(!strcmp(shapes[i].name,name))return (int)i;return -1;}
int main(){DosShapeIndex index;std::vector<Shape> shapes;unsigned long checks=0;
 for(unsigned generation=0;generation<12;generation++){
  index.clear();shapes.clear();size_t n=generation==11?1024:generation*81;
  for(size_t i=0;i<n;i++){Shape s={};snprintf(s.name,sizeof s.name,"%c/%04zu",'A'+generation,i?i%((n/2)+1):0);shapes.push_back(s);}
  if(!index.build(shapes)||!index.matches(shapes))return 1;
  for(unsigned repeat=0;repeat<8;repeat++)for(unsigned i=0;i<1400;i++){char name[48];snprintf(name,sizeof name,"%c/%04u",'A'+generation,i);if(reference(shapes,name)!=index.find(shapes,name))return 2;checks++;}
  for(const char* query:{"","A","a/0000","name-longer-than-15-characters","AAAAAAAAAAAAAAA"}){if(reference(shapes,query)!=index.find(shapes,query))return 3;checks++;}
  shapes.reserve(shapes.capacity()+1000);if(!shapes.empty()&&index.matches(shapes))return 4;if(!index.build(shapes))return 5;
 }
 Shape invalid={};memset(invalid.name,'x',sizeof invalid.name);shapes[0]=invalid;index.clear();if(index.build(shapes))return 6;
 shapes.assign(1025,Shape{});if(index.build(shapes))return 7;
 shapes.resize(392);for(size_t i=0;i<shapes.size();i++)snprintf(shapes[i].name,16,"N%04zu",i);if(!index.build(shapes))return 8;
 volatile unsigned result=0;clock_t t=clock();for(unsigned i=0;i<100000;i++)result+=(unsigned)reference(shapes,shapes[(i*13)%shapes.size()].name);double oldsec=(double)(clock()-t)/CLOCKS_PER_SEC;
 t=clock();for(unsigned i=0;i<100000;i++)result+=(unsigned)index.find(shapes,shapes[(i*13)%shapes.size()].name);double newsec=(double)(clock()-t)/CLOCKS_PER_SEC;
 printf("DOSSHAPEINDEX|checks=%lu|bad=0|fixed_bytes=%zu|linear_seconds=%.6f|index_seconds=%.6f|sink=%u\n",checks,sizeof index,oldsec,newsec,result);return 0;
}

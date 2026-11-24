#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdint>
#include <ctime>
#include "../../../game/stable_vector_step.h"
struct Record{uint32_t word[16];};static_assert(sizeof(Record)==64,"No padding in independent records");
struct State{uint32_t counter=0;std::vector<uint32_t> emitted;};
static unsigned rng=4242;static unsigned random_value(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static bool update(Record& r,State& s,unsigned pass,unsigned pattern){
 r.word[2]+=++s.counter;float p,v;memcpy(&p,r.word+4,4);memcpy(&v,r.word+8,4);p+=v*(1.0f/60.0f);memcpy(r.word+4,&p,4);
 if((r.word[0]+pass)%3==0)s.emitted.push_back(r.word[0]^s.counter);
 switch(pattern){case 0:return true;case 1:return false;case 2:return (r.word[0]&1)!=0;case 3:return r.word[0]<7;case 4:return r.word[0]>11;default:return ((r.word[0]^s.counter^pass)%7)!=0;}
}
static void original(std::vector<Record>& v,State& s,unsigned pass,unsigned pattern){for(size_t i=0;i<v.size();)if(update(v[i],s,pass,pattern))i++;else v.erase(v.begin()+i);}
static void candidate(std::vector<Record>& v,State& s,unsigned pass,unsigned pattern,bool on){StableVectorStep<Record> step(v,on);for(size_t i=0;i<v.size();)step.visit(i,update(v[i],s,pass,pattern));step.finish();}
int main(){unsigned long long checks=0;double oldTime=0,newTime=0;
 for(unsigned test=0;test<512;test++){
  std::vector<Record> a,b,c;State sa,sb,sc;size_t n=test<508?random_value()%1500:16000;
  for(size_t i=0;i<n;i++){Record r={};for(unsigned& w:r.word)w=random_value();r.word[0]=(unsigned)i;float p=(float)(int)(i%50)-25.0f,v=(float)(int)(i%17)*0.25f;memcpy(r.word+4,&p,4);memcpy(r.word+8,&v,4);a.push_back(r);}b=c=a;
  for(unsigned pass=0;pass<6;pass++){
   const unsigned pattern=(test+pass)%7;clock_t t=clock();original(a,sa,pass,pattern);oldTime+=(double)(clock()-t)/CLOCKS_PER_SEC;
   t=clock();candidate(b,sb,pass,pattern,true);newTime+=(double)(clock()-t)/CLOCKS_PER_SEC;candidate(c,sc,pass,pattern,false);
   if(a.size()!=b.size()||a.size()!=c.size()||(!a.empty()&&(memcmp(a.data(),b.data(),a.size()*sizeof(Record))||memcmp(a.data(),c.data(),a.size()*sizeof(Record))))||sa.counter!=sb.counter||sa.counter!=sc.counter||sa.emitted!=sb.emitted||sa.emitted!=sc.emitted){fprintf(stderr,"FAIL|case=%u|pass=%u|pattern=%u\n",test,pass,pattern);return 1;}checks++;
  }
 }
 printf("STABLESTEP|cases=512|ordered_state_checks=%llu|bad=0|reference_seconds=%.6f|compact_seconds=%.6f\n",checks,oldTime,newTime);return 0;
}

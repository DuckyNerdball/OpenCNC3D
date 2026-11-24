#ifndef OPENCNC3D_STABLE_VECTOR_STEP_H
#define OPENCNC3D_STABLE_VECTOR_STEP_H
#include <vector>
#include <cstddef>
/* Preserve visit order and the exact surviving records without repeatedly
   shifting the tail. The visit body may update other pools, but must neither
   resize this vector nor observe its intermediate size/order. No allocation.
   Disabled mode is the original erase-and-revisit loop. Call finish once. */
template<class T>struct StableVectorStep {
    std::vector<T>& values;const bool compact;std::size_t write=0;
    unsigned long long visited=0,moved=0,removed=0;
    StableVectorStep(std::vector<T>& v,bool on):values(v),compact(on){}
    void visit(std::size_t& read,bool keep){
        ++visited;if(!keep)++removed;
        if(compact){
            if(keep){if(write!=read){values[write]=values[read];++moved;}++write;}
            ++read;
        }else if(keep)++read;else values.erase(values.begin()+read);
    }
    void finish(){if(compact)values.resize(write);}
};
#endif

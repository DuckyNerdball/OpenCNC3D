#ifndef CNC3D_MESH_DEPTH_DIRECT_H
#define CNC3D_MESH_DEPTH_DIRECT_H
/* Opaque sun depth needs positions alone. Fill its shared position store directly,
   keeping the original CPU equations and the CPU pose for later colour/normal passes.
   Unsupported articulation, wind, animation and geometry retain draw_mesh's loop. */
static int mesh_depth_direct_mode() {
    static const int mode=[](){const char* p=getenv("OPENCNC3D_MESH_DEPTH_DIRECT");
        return !p?1:!strcmp(p,"0")?0:!strcmp(p,"verify")?2:1;}();return mode;
}
#include "mesh_transform_four.h"
static bool mesh_depth_four_enabled() {
    static const bool on=[](){const char* p=getenv("OPENCNC3D_MESH_DEPTH_FOUR");
return !p||strcmp(p,"0"); // Native Mac bundle and Windows strict-corner proof.
}();return on;
}
static unsigned long long g_meshDepthFourBlocks=0;
static unsigned long long g_meshDepthLoops=0,g_meshDepthDraws=0,g_meshDepthVertices=0;
static unsigned long long g_meshDepthVerified=0,g_meshDepthBad=0,g_meshDepthFallbacks=0;
static void mesh_depth_direct_dump() {
    printf("MESHDEPTHFOUR|enabled=%d|blocks=%llu|vertices=%llu\n",mesh_depth_four_enabled()?1:0,g_meshDepthFourBlocks,g_meshDepthFourBlocks*4);
    printf("MESHDEPTHDIRECT|loops=%llu|draws=%llu|vertices=%llu|verified=%llu|bad=%llu|fallbacks=%llu\n",
        g_meshDepthLoops,g_meshDepthDraws,g_meshDepthVertices,g_meshDepthVerified,g_meshDepthBad,g_meshDepthFallbacks);
}
template<bool Extra,bool Wobble>
static void mesh_depth_direct_fill(const PackMesh& mesh,const MeshPreparedEntry& source,
        MeshGeometryPose& geometry,size_t first,const float* extra,float cx,float cz,
        float ground,float lift,float rc,float rs,float wsx,float wcx,float wsz,float wcz,
        bool verify,float* lo,float* hi) {
    size_t cursor=0;const bool four=mesh_depth_four_enabled();
    while(cursor<source.corners.size()) {
        if(four && source.corners.size()-cursor>=4) {
            bool fresh=true;
            for(int k=0;k<4;++k)if(geometry.valid[source.corners[cursor+k].source]&1)fresh=false;
            if(fresh) {
                float input[4][3],posed[4][3];
                for(int k=0;k<4;++k) {
                    const auto index=source.corners[cursor+k].source;
                    const PackVert& v=mesh.tris[first+index/3].v[index%3];
                    input[k][0]=v.x;input[k][1]=v.y;input[k][2]=v.z;
                }
                mesh_transform_four<Extra,Wobble>(input,posed,extra,MODEL_SCALE,cx,cz,ground,lift,rc,rs,wsx,wcx,wsz,wcz);
                for(int k=0;k<4;++k) {
                    const auto index=source.corners[cursor+k].source;
                    memcpy(&g_meshGeometryUpload[(cursor+k)*3],posed[k],3*sizeof(float));
                    if(!verify){memcpy(geometry.corners[index].p,posed[k],3*sizeof(float));geometry.valid[index]|=1;g_meshGeometryBuilds++;}
                    for(int a=0;a<3;++a) {
                        if(!cursor&&!k)lo[a]=hi[a]=posed[k][a];
                        else{lo[a]=std::min(lo[a],posed[k][a]);hi[a]=std::max(hi[a],posed[k][a]);}
                    }
                }
                cursor+=4;++g_meshDepthFourBlocks;continue;
            }
        }
        const auto& corner=source.corners[cursor];
        float* out=&g_meshGeometryUpload[cursor*3];
        if(geometry.valid[corner.source]&1)memcpy(out,geometry.corners[corner.source].p,3*sizeof(float));
        else {
            const PackVert& v=mesh.tris[first+corner.source/3].v[corner.source%3];
            float vx=v.x,vy=v.y,vz=v.z;
            if(Extra) {
                const float bx=extra[0]*vx+extra[1]*vy+extra[2]*vz+extra[3];
                const float by=extra[4]*vx+extra[5]*vy+extra[6]*vz+extra[7];
                const float bz=extra[8]*vx+extra[9]*vy+extra[10]*vz+extra[11];vx=bx;vy=by;vz=bz;
            }
            const float mx=vx*rc+vz*rs,mz=-vx*rs+vz*rc;
            if(Wobble) {
                const float lx=mx*MODEL_SCALE,ly=vy*MODEL_SCALE+0.0f+lift,lz=mz*MODEL_SCALE;
                const float y1=ly*wcx-lz*wsx,z1=ly*wsx+lz*wcx;
                const float x2=lx*wcz-y1*wsz,y2=lx*wsz+y1*wcz;
                out[0]=cx+x2+0.0f;out[1]=y2+ground;out[2]=cz+z1+0.0f;
            } else {out[0]=cx+mx*MODEL_SCALE+0.0f;out[1]=vy*MODEL_SCALE+0.0f+lift+ground;out[2]=cz+mz*MODEL_SCALE+0.0f;}
            if(!verify){memcpy(geometry.corners[corner.source].p,out,3*sizeof(float));geometry.valid[corner.source]|=1;g_meshGeometryBuilds++;}
        }
        for(int axis=0;axis<3;axis++) {
            if(!cursor)lo[axis]=hi[axis]=out[axis];
            else{lo[axis]=std::min(lo[axis],out[axis]);hi[axis]=std::max(hi[axis],out[axis]);}
        }
        cursor++;
    }
}
static bool mesh_depth_direct_prepare(const Pack& src,const PackMesh& mesh,MeshGeometryPose& geometry,
        size_t first,size_t last,const float* extra,float cx,float cz,float ground,float lift,
        float rc,float rs,bool wob,float wsx,float wcx,float wsz,float wcz,bool verify,float* lo,float* hi) {
    const auto* source=mesh_prepared_source(src,mesh,first,last,MODE_OPAQUE,true,false,false,true);
    if(!source||!source->compatible||source->corners.empty()||source->corners.size()>16u*1024u*1024u/(3*sizeof(float)))return false;
    try {
        if(!mesh_perf_switch("OPENCNC3D_MESH_DEPTH_DIRECT_ALLOC_OK"))throw std::bad_alloc();
        g_meshGeometryUpload.resize(source->corners.size()*3);
#define DEPTH_FILL(EXTRA,WOB) mesh_depth_direct_fill<EXTRA,WOB>(mesh,*source,geometry,first,extra,cx,cz,ground,lift,rc,rs,wsx,wcx,wsz,wcz,verify,lo,hi)
        if(extra){if(wob)DEPTH_FILL(true,true);else DEPTH_FILL(true,false);}
        else{if(wob)DEPTH_FILL(false,true);else DEPTH_FILL(false,false);}
#undef DEPTH_FILL
        g_meshDepthLoops++;g_meshDepthVertices+=source->corners.size();return true;
    } catch(const std::bad_alloc&){return false;}
}
static void mesh_depth_direct_compare(const MeshBatch& output,const float* lo,const float* hi) {
    g_meshDepthVerified++;bool same=output.vertices.size()*3==g_meshGeometryUpload.size();
    for(size_t i=0;same&&i<output.vertices.size();i++)same=!memcmp(output.vertices[i].p,&g_meshGeometryUpload[i*3],3*sizeof(float));
    same=same&&!memcmp(output.entry.lo,lo,sizeof output.entry.lo)&&!memcmp(output.entry.hi,hi,sizeof output.entry.hi);
    if(!same)g_meshDepthBad++;
}
#endif

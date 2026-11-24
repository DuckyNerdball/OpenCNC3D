#ifndef CNC3D_MESH_PREPARED_DRAW_H
#define CNC3D_MESH_PREPARED_DRAW_H
/* Immutable material/UV runs are prepared once. Positions and articulated normals
   retain the original CPU equations, including their operation order. Unsupported
   vertex effects continue through draw_mesh's complete loop. */
static unsigned long long g_meshPreparedDraws=0,g_meshPreparedDrawVertices=0;
static unsigned long long g_meshPreparedDrawVerified=0,g_meshPreparedDrawBad=0;
static int mesh_prepared_draw_mode() {
    static const int mode=[](){const char* p=getenv("OPENCNC3D_MESH_PREPARED_DRAW");
        return !p?1:!strcmp(p,"0")?0:!strcmp(p,"verify")?2:1;}();
    return mode;
}
static void mesh_prepared_draw_dump() {
    printf("MESHPREPAREDDRAW|draws=%llu|vertices=%llu|verified=%llu|bad=%llu\n",
        g_meshPreparedDraws,g_meshPreparedDrawVertices,g_meshPreparedDrawVerified,g_meshPreparedDrawBad);
}
template<bool Depth,bool Normals,bool Extra,bool Wobble>
static bool mesh_prepared_draw_loop(MeshBatch& output,const Pack& src,const PackMesh& mesh,
        MeshGeometryPose* geometry,const MeshPreparedEntry& prepared,size_t first,
        const float* extra,float cx,float cz,float ground,float lift,float rc,float rs,
        float wsx,float wcx,float wsz,float wcz,float turyaw,float rotor,bool verify,int pass) {
    output.prepare(prepared.corners.size());
    output.entry.runs=prepared.runs;output.entry.uv=prepared.uv;
    const size_t nparts=mesh.parts.size();size_t pi=0,previous=~size_t(0);
    float psn=0,pcs=1,treadV=0;bool spin=false;const PackPart* part=NULL;
    for(const auto& c:prepared.corners) {
        const size_t i=first+c.source/3,k=c.source%3;
        const PackTri& t=mesh.tris[i];const PackVert& v=t.v[k];
        if(i!=previous) {
            while(pi+1<nparts && (int)i>=mesh.parts[pi].tri0+mesh.parts[pi].ntris)pi++;
            part=nparts?&mesh.parts[pi]:NULL;spin=false;
            if(part && part->role==ROLE_TURRET && turyaw!=0) {
                const float a=-turyaw*(2.0f*(float)M_PI/256.0f);
                psn=sinf(a);pcs=cosf(a);spin=true;
            } else if(part && part->role==ROLE_ROTOR && rotor!=0) {
                psn=sinf(-rotor);pcs=cosf(-rotor);spin=true;
            }
            if(!Depth) {
                const int frames=(int)(t.wrap>>4);treadV=0;
                if(frames==1)treadV=g_treadNow;
                else if(frames>1) {
                    const float ph=g_treadNow-floorf(g_treadNow);
                    treadV=floorf(ph*(float)frames)/(float)frames;
                }
            }
            previous=i;
        }
        if(!Depth) {
            if(c.textured && (!g_meshBakePass||g_meshBakeCutout) && (t.wrap>>4)) {
                const PackTex& tx=src.tex[t.tex];float vv=v.v;vv+=treadV;
                output.uv(v.u*(float)tx.uw/(float)tx.w,vv*(float)tx.uh/(float)tx.h);
            } else if(c.textured) {output.current.uv[0]=c.uv[0];output.current.uv[1]=c.uv[1];}
            if(Normals) {
                const float* nn=&mesh.nrm[i*9+k*3];float nx=nn[0],ny=nn[1],nz=nn[2];
                if(Extra) {
                    const float ax=extra[0]*nx+extra[1]*ny+extra[2]*nz;
                    const float ay=extra[4]*nx+extra[5]*ny+extra[6]*nz;
                    const float az=extra[8]*nx+extra[9]*ny+extra[10]*nz;
                    nx=ax;ny=ay;nz=az;
                }
                if(spin){const float dx=nx,dz=nz;nx=dx*pcs+dz*psn;nz=-dx*psn+dz*pcs;}
                float n[3]={nx*rc+nz*rs,ny,-nx*rs+nz*rc};
                if(Wobble) {
                    const float y1=n[1]*wcx-n[2]*wsx,z1=n[1]*wsx+n[2]*wcx;
                    const float x2=n[0]*wcz-y1*wsz,y2=n[0]*wsz+y1*wcz;
                    n[0]=x2;n[1]=y2;n[2]=z1;
                }
                if(geometry && (geometry->valid[c.source]&2))memcpy(n,geometry->corners[c.source].n,sizeof n);
                else if(geometry&&!verify){memcpy(geometry->corners[c.source].n,n,sizeof n);geometry->valid[c.source]|=2;}
                output.normal(n[0],n[1],n[2]);output.color(255,255,255,c.selflit);
            } else {
                unsigned char alpha;
                if(g_meshBakePass)alpha=255;
                else if(g_meshShadowPass)alpha=g_meshShadowAlpha;
                else if(g_meshFlash&&c.textured&&pass!=MODE_XLU)alpha=g_meshAlphaScale;
                else alpha=(unsigned char)(((int)c.c[3]*(int)g_meshAlphaScale)/255);
                if(g_meshShadowPass)output.color(0,0,0,alpha);
                else output.color(c.c[0],c.c[1],c.c[2],alpha);
            }
        }
        if(geometry && (geometry->valid[c.source]&1)) {
            const float* p=geometry->corners[c.source].p;output.vertex(p[0],p[1],p[2]);
        } else {
            float vx=v.x,vy=v.y,vz=v.z;
            if(Extra) {
                const float bx=extra[0]*vx+extra[1]*vy+extra[2]*vz+extra[3];
                const float by=extra[4]*vx+extra[5]*vy+extra[6]*vz+extra[7];
                const float bz=extra[8]*vx+extra[9]*vy+extra[10]*vz+extra[11];vx=bx;vy=by;vz=bz;
            }
            if(spin) {const float dx=vx-part->px,dz=vz-part->pz;
                vx=part->px+dx*pcs+dz*psn;vz=part->pz-dx*psn+dz*pcs;}
            const float mx=vx*rc+vz*rs,mz=-vx*rs+vz*rc;
            if(Wobble) {
                float lx=mx*MODEL_SCALE,ly=vy*MODEL_SCALE+0.0f+lift,lz=mz*MODEL_SCALE;
                const float y1=ly*wcx-lz*wsx,z1=ly*wsx+lz*wcx;
                const float x2=lx*wcz-y1*wsz,y2=lx*wsz+y1*wcz;
                output.vertex(cx+x2+0.0f,y2+ground,cz+z1+0.0f);
            } else {
                const float wx=cx+mx*MODEL_SCALE+0.0f,wz=cz+mz*MODEL_SCALE+0.0f;
                output.vertex(wx,vy*MODEL_SCALE+0.0f+lift+ground,wz);
            }
            if(geometry&&!verify){memcpy(geometry->corners[c.source].p,output.current.p,3*sizeof(float));
                geometry->valid[c.source]|=1;g_meshGeometryBuilds++;}
        }
    }
    return true;
}
#include "mesh_prepared_bulk.h"
template<bool Depth,bool Normals>
static bool mesh_prepared_draw_dispatch(MeshBatch& output,const Pack& src,const PackMesh& mesh,
        MeshGeometryPose* geometry,const MeshPreparedEntry& prepared,size_t first,
        const float* extra,float cx,float cz,float ground,float lift,float rc,float rs,bool wob,
        float wsx,float wcx,float wsz,float wcz,float turyaw,float rotor,bool verify,int pass) {
#define MESH_PREPARED_DRAW(EXTRA,WOB) mesh_prepared_selected_loop<Depth,Normals,EXTRA,WOB>(output,src,mesh,geometry,prepared,first,extra,cx,cz,ground,lift,rc,rs,wsx,wcx,wsz,wcz,turyaw,rotor,verify,pass)
    if(extra){if(wob)return MESH_PREPARED_DRAW(true,true);return MESH_PREPARED_DRAW(true,false);}
    if(wob)return MESH_PREPARED_DRAW(false,true);return MESH_PREPARED_DRAW(false,false);
#undef MESH_PREPARED_DRAW
}
static bool mesh_prepared_draw(MeshBatch& output,const Pack& src,const PackMesh& mesh,
        MeshGeometryPose* geometry,size_t first,size_t last,int pass,bool depth,bool normals,bool hasX2,
        const float* extra,float cx,float cz,float ground,float lift,float rc,float rs,bool wob,
        float wsx,float wcx,float wsz,float wcz,float turyaw,float rotor,bool verify) {
    const auto* prepared=mesh_prepared_source(src,mesh,first,last,pass,depth,normals,hasX2,true,mesh_prepared_bulk_mode());
    if(!prepared||!prepared->compatible)return false;
    try {
#define MESH_PREPARED_DISPATCH(DEPTH,NORMALS) mesh_prepared_draw_dispatch<DEPTH,NORMALS>(output,src,mesh,geometry,*prepared,first,extra,cx,cz,ground,lift,rc,rs,wob,wsx,wcx,wsz,wcz,turyaw,rotor,verify,pass)
        if(depth)MESH_PREPARED_DISPATCH(true,false);
        else if(normals)MESH_PREPARED_DISPATCH(false,true);
        else MESH_PREPARED_DISPATCH(false,false);
#undef MESH_PREPARED_DISPATCH
        g_meshPreparedDraws++;g_meshPreparedDrawVertices+=output.vertices.size();return true;
    } catch(const std::bad_alloc&){return false;}
}
static void mesh_prepared_draw_compare(const MeshBatch& original,const MeshBatch& fast,const char* name,int pass) {
    g_meshPreparedDrawVerified++;
    bool same=original.vertices.size()==fast.vertices.size()&&original.entry.runs.size()==fast.entry.runs.size()
        &&original.entry.uv==fast.entry.uv;
    if(same&&!original.vertices.empty())same=!memcmp(original.vertices.data(),fast.vertices.data(),original.vertices.size()*sizeof(MeshBatchVertex))
        &&!memcmp(original.entry.lo,fast.entry.lo,sizeof fast.entry.lo)&&!memcmp(original.entry.hi,fast.entry.hi,sizeof fast.entry.hi);
    for(size_t i=0;same&&i<original.entry.runs.size();i++) {
        const auto& a=original.entry.runs[i];const auto& b=fast.entry.runs[i];
        same=a.tex==b.tex&&a.wrap==b.wrap&&a.x2==b.x2&&a.first==b.first&&a.count==b.count;
    }
    if(!same){if(g_meshPreparedDrawBad<8)fprintf(stderr,"MESHPREPAREDDRAW|mismatch|mesh=%s|pass=%d|original=%zu|fast=%zu\n",name,pass,original.vertices.size(),fast.vertices.size());g_meshPreparedDrawBad++;}
}
#endif

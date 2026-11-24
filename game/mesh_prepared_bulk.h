#ifndef CNC3D_MESH_PREPARED_BULK_H
#define CNC3D_MESH_PREPARED_BULK_H
/* Copy the immutable corner records once, then update their varying fields in place.
   The arithmetic is the original draw loop's, not a composed GPU transform. */
static bool mesh_prepared_bulk_mode() {
    static const bool enabled=[](){const char* p=getenv("OPENCNC3D_MESH_PREPARED_BULK");return !p||strcmp(p,"0");}();
    return enabled;
}
static unsigned long long g_meshPreparedBulkDraws=0,g_meshPreparedBulkVertices=0;
static void mesh_prepared_bulk_dump() {
    printf("MESHPREPAREDBULK|draws=%llu|vertices=%llu\n",g_meshPreparedBulkDraws,g_meshPreparedBulkVertices);
}
template<bool Depth,bool Normals,bool Extra,bool Wobble>
static bool mesh_prepared_bulk_loop(MeshBatch& output,const Pack& src,const PackMesh& mesh,
        MeshGeometryPose* geometry,const MeshPreparedEntry& prepared,size_t first,
        const float* extra,float cx,float cz,float ground,float lift,float rc,float rs,
        float wsx,float wcx,float wsz,float wcz,float turyaw,float rotor,bool verify,int pass) {
    if(prepared.flat.size()!=prepared.corners.size())return false;
    output.prepare(prepared.flat.size());
    output.vertices=prepared.flat;output.entry.runs=prepared.runs;output.entry.uv=prepared.uv;
    const size_t nparts=mesh.parts.size();size_t pi=0,previous=~size_t(0),cursor=0;
    float psn=0,pcs=1,treadV=0,lastUV[2]={0,0};bool spin=false;const PackPart* part=NULL;
    for(const auto& c:prepared.corners) {
        MeshBatchVertex& out=output.vertices[cursor++];
        const size_t i=first+c.source/3,k=c.source%3;const PackTri& t=mesh.tris[i];
        if(i!=previous) {
            while(pi+1<nparts && (int)i>=mesh.parts[pi].tri0+mesh.parts[pi].ntris)pi++;
            part=nparts?&mesh.parts[pi]:NULL;spin=false;
            if(part && part->role==ROLE_TURRET && turyaw!=0) {
                const float a=-turyaw*(2.0f*(float)M_PI/256.0f);psn=sinf(a);pcs=cosf(a);spin=true;
            } else if(part && part->role==ROLE_ROTOR && rotor!=0) {psn=sinf(-rotor);pcs=cosf(-rotor);spin=true;}
            if(!Depth) {
                const int frames=(int)(t.wrap>>4);treadV=0;
                if(frames==1)treadV=g_treadNow;
                else if(frames>1){const float ph=g_treadNow-floorf(g_treadNow);treadV=floorf(ph*(float)frames)/(float)frames;}
            }
            previous=i;
        }
        if(!Depth) {
            if(c.textured && (!g_meshBakePass||g_meshBakeCutout)) {
                if(t.wrap>>4){const PackVert& v=t.v[k];const PackTex& tx=src.tex[t.tex];float vv=v.v;vv+=treadV;
                    out.uv[0]=v.u*(float)tx.uw/(float)tx.w;out.uv[1]=vv*(float)tx.uh/(float)tx.h;}
                lastUV[0]=out.uv[0];lastUV[1]=out.uv[1];
            } else {out.uv[0]=lastUV[0];out.uv[1]=lastUV[1];}
            if(Normals) {
                if(geometry && (geometry->valid[c.source]&2))memcpy(out.n,geometry->corners[c.source].n,sizeof out.n);
                else {
                    const float* nn=&mesh.nrm[i*9+k*3];float nx=nn[0],ny=nn[1],nz=nn[2];
                    if(Extra) {
                        const float ax=extra[0]*nx+extra[1]*ny+extra[2]*nz;
                        const float ay=extra[4]*nx+extra[5]*ny+extra[6]*nz;
                        const float az=extra[8]*nx+extra[9]*ny+extra[10]*nz;nx=ax;ny=ay;nz=az;
                    }
                    if(spin){const float dx=nx,dz=nz;nx=dx*pcs+dz*psn;nz=-dx*psn+dz*pcs;}
                    float n[3]={nx*rc+nz*rs,ny,-nx*rs+nz*rc};
                    if(Wobble){const float y1=n[1]*wcx-n[2]*wsx,z1=n[1]*wsx+n[2]*wcx;
                        const float x2=n[0]*wcz-y1*wsz,y2=n[0]*wsz+y1*wcz;n[0]=x2;n[1]=y2;n[2]=z1;}
                    memcpy(out.n,n,sizeof n);
                    if(geometry&&!verify){memcpy(geometry->corners[c.source].n,n,sizeof n);geometry->valid[c.source]|=2;}
                }
                out.c[0]=out.c[1]=out.c[2]=255;out.c[3]=c.selflit;
            } else {
                if(g_meshBakePass)out.c[3]=255;
                else if(g_meshShadowPass){out.c[0]=out.c[1]=out.c[2]=0;out.c[3]=g_meshShadowAlpha;}
                else if(g_meshFlash&&c.textured&&pass!=MODE_XLU)out.c[3]=g_meshAlphaScale;
                else out.c[3]=(unsigned char)(((int)c.c[3]*(int)g_meshAlphaScale)/255);
            }
        }
        if(geometry && (geometry->valid[c.source]&1))memcpy(out.p,geometry->corners[c.source].p,sizeof out.p);
        else {
            float vx=out.p[0],vy=out.p[1],vz=out.p[2];
            if(Extra) {
                const float bx=extra[0]*vx+extra[1]*vy+extra[2]*vz+extra[3];
                const float by=extra[4]*vx+extra[5]*vy+extra[6]*vz+extra[7];
                const float bz=extra[8]*vx+extra[9]*vy+extra[10]*vz+extra[11];vx=bx;vy=by;vz=bz;
            }
            if(spin){const float dx=vx-part->px,dz=vz-part->pz;vx=part->px+dx*pcs+dz*psn;vz=part->pz-dx*psn+dz*pcs;}
            const float mx=vx*rc+vz*rs,mz=-vx*rs+vz*rc;
            if(Wobble) {
                float lx=mx*MODEL_SCALE,ly=vy*MODEL_SCALE+0.0f+lift,lz=mz*MODEL_SCALE;
                const float y1=ly*wcx-lz*wsx,z1=ly*wsx+lz*wcx;
                const float x2=lx*wcz-y1*wsz,y2=lx*wsz+y1*wcz;out.p[0]=cx+x2+0.0f;out.p[1]=y2+ground;out.p[2]=cz+z1+0.0f;
            } else {out.p[0]=cx+mx*MODEL_SCALE+0.0f;out.p[2]=cz+mz*MODEL_SCALE+0.0f;out.p[1]=vy*MODEL_SCALE+0.0f+lift+ground;}
            if(geometry&&!verify){memcpy(geometry->corners[c.source].p,out.p,sizeof out.p);geometry->valid[c.source]|=1;g_meshGeometryBuilds++;}
        }
    }
    if(!output.vertices.empty()) {
        memcpy(output.entry.lo,output.vertices[0].p,sizeof output.entry.lo);memcpy(output.entry.hi,output.vertices[0].p,sizeof output.entry.hi);
        for(size_t i=1;i<output.vertices.size();i++)for(int k=0;k<3;k++) {
            output.entry.lo[k]=std::min(output.entry.lo[k],output.vertices[i].p[k]);
            output.entry.hi[k]=std::max(output.entry.hi[k],output.vertices[i].p[k]);
        }
        output.current=output.vertices.back();
    }
    g_meshPreparedBulkDraws++;g_meshPreparedBulkVertices+=output.vertices.size();return true;
}
template<bool Depth,bool Normals,bool Extra,bool Wobble>
static bool mesh_prepared_selected_loop(MeshBatch& output,const Pack& src,const PackMesh& mesh,
        MeshGeometryPose* geometry,const MeshPreparedEntry& prepared,size_t first,
        const float* extra,float cx,float cz,float ground,float lift,float rc,float rs,
        float wsx,float wcx,float wsz,float wcz,float turyaw,float rotor,bool verify,int pass) {
#define MESH_BULK_ARGS output,src,mesh,geometry,prepared,first,extra,cx,cz,ground,lift,rc,rs,wsx,wcx,wsz,wcz,turyaw,rotor,verify,pass
    if(mesh_prepared_bulk_mode())return mesh_prepared_bulk_loop<Depth,Normals,Extra,Wobble>(MESH_BULK_ARGS);
    return mesh_prepared_draw_loop<Depth,Normals,Extra,Wobble>(MESH_BULK_ARGS);
#undef MESH_BULK_ARGS
}
#endif

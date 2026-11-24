#ifndef CNC3D_MESH_TRANSFORM_FOUR_H
#define CNC3D_MESH_TRANSFORM_FOUR_H
/* Evaluate four independent corners with the original expression tree. The
   compiler uses the platform's existing 128-bit vector instruction set; no new
   ISA requirement, fast-math flag, matrix composition or precision change. */
typedef float MeshFloat4 __attribute__((vector_size(16)));
template<bool Extra,bool Wobble>
static void mesh_transform_four(const float input[4][3],float output[4][3],
        const float* extra,float scale,float cx,float cz,float ground,float lift,
        float rc,float rs,float wsx,float wcx,float wsz,float wcz) {
    MeshFloat4 vx={input[0][0],input[1][0],input[2][0],input[3][0]};
    MeshFloat4 vy={input[0][1],input[1][1],input[2][1],input[3][1]};
    MeshFloat4 vz={input[0][2],input[1][2],input[2][2],input[3][2]};
    if(Extra) {
        const MeshFloat4 bx=extra[0]*vx+extra[1]*vy+extra[2]*vz+extra[3];
        const MeshFloat4 by=extra[4]*vx+extra[5]*vy+extra[6]*vz+extra[7];
        const MeshFloat4 bz=extra[8]*vx+extra[9]*vy+extra[10]*vz+extra[11];vx=bx;vy=by;vz=bz;
    }
    const MeshFloat4 mx=vx*rc+vz*rs,mz=-vx*rs+vz*rc;
    MeshFloat4 x,y,z;
    if(Wobble) {
        const MeshFloat4 lx=mx*scale,ly=vy*scale+0.0f+lift,lz=mz*scale;
        const MeshFloat4 y1=ly*wcx-lz*wsx,z1=ly*wsx+lz*wcx;
        const MeshFloat4 x2=lx*wcz-y1*wsz,y2=lx*wsz+y1*wcz;
        x=cx+x2+0.0f;y=y2+ground;z=cz+z1+0.0f;
    } else {x=cx+mx*scale+0.0f;y=vy*scale+0.0f+lift+ground;z=cz+mz*scale+0.0f;}
    for(int i=0;i<4;++i){output[i][0]=x[i];output[i][1]=y[i];output[i][2]=z[i];}
}
#endif

#ifndef CNC3D_EVHC_PICK_FRAME_H
#define CNC3D_EVHC_PICK_FRAME_H
/* Picking calls evhc_lamp_world four times per part to build its affine frame.
   Facing, lean, altitude and ground are identical for every one of those points.
   Resolve those pure inputs once per object, preserving the original operation
   order for each point. No cross-frame cache or pose/selection policy changes. */
struct EvhcPickFrame {
    float x,z,rs,rc,scale,lift,ground,wsx,wcx,wsz,wcz;
};
static EvhcPickFrame evhc_pick_frame(const SimObject& o) {
    EvhcPickFrame F;F.x=o.wx;F.z=o.wz;F.scale=MODEL_SCALE;
    facing_rot(evhc_draw_face(o),&F.rs,&F.rc);
    F.lift=alt_lift(o)+o.ylift;
    float wax=0,waz=0;mesh_wobble(wobble_of(o),o.wx,o.wz,&wax,&waz);
    F.wsx=sinf(wax);F.wcx=cosf(wax);F.wsz=sinf(waz);F.wcz=cosf(waz);
    F.ground=terrain_y(o.wx,o.wz);
    /* a racer's lift and anchor height, as draw_mesh takes them (rally_mod.h) */
    if(g_rlyDraw&&wobble_of(o)!=WOBBLE_NONE)rly_draw_at(o.wx,o.wz,&F.lift,&F.ground);
    return F;
}
static void evhc_pick_frame_point(const EvhcPickFrame& F,const float* M,const float* v,float* out) {
    float l[3]={v[0],v[1],v[2]};
    if(M) {
        l[0]=M[0]*v[0]+M[1]*v[1]+M[2]*v[2]+M[3];
        l[1]=M[4]*v[0]+M[5]*v[1]+M[6]*v[2]+M[7];
        l[2]=M[8]*v[0]+M[9]*v[1]+M[10]*v[2]+M[11];
    }
    const float mx=l[0]*F.rc+l[2]*F.rs,mz=-l[0]*F.rs+l[2]*F.rc;
    const float lx=mx*F.scale,lz=mz*F.scale;
    const float ly=l[1]*F.scale+F.lift;
    const float y1=ly*F.wcx-lz*F.wsx,z1=ly*F.wsx+lz*F.wcx;
    const float x2=lx*F.wcz-y1*F.wsz,y2=lx*F.wsz+y1*F.wcz;
    out[0]=F.x+x2;out[1]=F.ground+y2;out[2]=F.z+z1;
}
static int evhc_pick_frame_mode() {
    static const int mode=[](){const char* p=getenv("OPENCNC3D_EVHC_PICK_FRAME");
        if(!p)return 1; // Qualified native bundle; camera/pose arithmetic unchanged.
        return p&&!strcmp(p,"verify")?2:p&&strcmp(p,"0")?1:0;}();return mode;
}
static unsigned long long g_evhcPickFrameObjects=0,g_evhcPickFramePoints=0,
    g_evhcPickFrameChecks=0,g_evhcPickFrameBad=0;
static bool evhc_pick_poly_equal(const ScreenPoly& a,const ScreenPoly& b) {
    if(a.ok!=b.ok||memcmp(a.d,b.d,sizeof a.d))return false;
    if(!a.ok)return true; // The original leaves the unused rectangle unwritten.
    return !memcmp(&a.c0,&b.c0,sizeof(float))&&!memcmp(&a.r0,&b.r0,sizeof(float))
        &&!memcmp(&a.c1,&b.c1,sizeof(float))&&!memcmp(&a.r1,&b.r1,sizeof(float));
}
static void evhc_pick_frame_dump() {
    printf("EVHCPICKFRAME|mode=%d|objects=%llu|points=%llu|verified=%llu|bad=%llu\n",
        evhc_pick_frame_mode(),g_evhcPickFrameObjects,g_evhcPickFramePoints,g_evhcPickFrameChecks,g_evhcPickFrameBad);
}
#endif

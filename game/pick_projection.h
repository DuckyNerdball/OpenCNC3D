#ifndef CNC3D_PICK_PROJECTION_H
#define CNC3D_PICK_PROJECTION_H
/* A picking traversal has one camera. Reuse its original resolved constants,
   not a different matrix or approximation. The scope never survives a query;
   nested object bounds reuse the same camera. Classic/ortho keep their path. */
struct PickProjectionFrame {
    int width,height;double x,z,y,sp,cp,sy,cy,distance,aspect,f;
};
static const PickProjectionFrame* g_pickProjection=NULL;
static unsigned long long g_pickProjectionScopes=0,g_pickProjectionReused=0,
    g_pickProjectionPoints=0,g_pickProjectionChecks=0,g_pickProjectionBad=0;
static int pick_projection_mode() {
    static const int mode=[](){const char* p=getenv("OPENCNC3D_PICK_PROJECTION");
        if(!p)return 1; // Qualified native bundle; camera/pose arithmetic unchanged.
        return p&&!strcmp(p,"verify")?2:p&&strcmp(p,"0")?1:0;}();return mode;
}
static PickProjectionFrame pick_projection_frame(int width,int height) {
    PickProjectionFrame F;F.width=width;F.height=height;F.x=g_camX;F.z=g_camZ;
    const double p=n64_pitch();F.sp=sin(p);F.cp=cos(p);
    F.sy=sin((double)g_camYaw);F.cy=cos((double)g_camYaw);
    F.y=cam_at_y();F.distance=n64_dist_cells();F.aspect=cam_aspect(width,height);F.f=cam_f();return F;
}
struct PickProjectionScope {
    const PickProjectionFrame* previous;PickProjectionFrame frame;
    PickProjectionScope(int width,int height):previous(g_pickProjection) {
        if(!pick_projection_mode()||g_fx.dos_classic||g_camMode!=CAM_N64)return;
        if(previous&&previous->width==width&&previous->height==height){++g_pickProjectionReused;return;}
        frame=pick_projection_frame(width,height);g_pickProjection=&frame;++g_pickProjectionScopes;
    }
    ~PickProjectionScope(){g_pickProjection=previous;}
};
static bool pick_projection_point(const PickProjectionFrame& F,float wx,float wy,float wz,float* col,float* row) {
    const double ax0=(double)wx-F.x,ay=(double)wy-F.y,az0=(double)wz-F.z;
    const double xe=ax0*F.cy+az0*F.sy,az=-ax0*F.sy+az0*F.cy;
    const double ye=ay*F.cp-az*F.sp;
    double depth=F.distance-ay*F.sp-az*F.cp;
    const bool front=depth>1.0e-3;if(!front)depth=1.0e-3;
    *col=(float)((double)F.width*0.5*(1.0+xe*F.f/(F.aspect*depth)));
    *row=(float)((double)F.height*0.5*(1.0-ye*F.f/depth));return front;
}
static bool project_point(float wx,float wy,float wz,int fbw,int fbh,float* col,float* row) {
    const PickProjectionFrame* F=g_pickProjection;
    if(!F||g_fx.dos_classic||g_camMode!=CAM_N64||F->width!=fbw||F->height!=fbh)
        return project_point_original(wx,wy,wz,fbw,fbh,col,row);
    ++g_pickProjectionPoints;
    if(pick_projection_mode()==1)return pick_projection_point(*F,wx,wy,wz,col,row);
    float c,r,oldc,oldr;const bool now=pick_projection_point(*F,wx,wy,wz,&c,&r);
    const bool old=project_point_original(wx,wy,wz,fbw,fbh,&oldc,&oldr);
    ++g_pickProjectionChecks;
    if(old!=now||memcmp(&c,&oldc,sizeof c)||memcmp(&r,&oldr,sizeof r)) {
        ++g_pickProjectionBad;
        if(g_pickProjectionBad<8)fprintf(stderr,"PICKPROJECTION|mismatch|world=%.9g,%.9g,%.9g|old=%.9g,%.9g|new=%.9g,%.9g\n",wx,wy,wz,oldc,oldr,c,r);
    }
    *col=oldc;*row=oldr;return old;
}
static void pick_projection_dump() {
    printf("PICKPROJECTION|mode=%d|scopes=%llu|reused=%llu|points=%llu|verified=%llu|bad=%llu\n",
        pick_projection_mode(),g_pickProjectionScopes,g_pickProjectionReused,g_pickProjectionPoints,g_pickProjectionChecks,g_pickProjectionBad);
}
#endif

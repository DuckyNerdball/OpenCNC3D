/* Approved RA Ant, 956 body + 2 transient fluid triangles. Source and packing:
   game/ant_ra/material.json. No gameplay changes; native ANTDIE owns retirement. */
#ifndef CNC_ANT3D_MOD_H
#define CNC_ANT3D_MOD_H
namespace ant3d {
struct V {
    float x,y,z;
    V(float X=0,float Y=0,float Z=0):x(X),y(Y),z(Z){}
    V operator+(V b)const{return V(x+b.x,y+b.y,z+b.z);}
    V operator-(V b)const{return V(x-b.x,y-b.y,z-b.z);}
    V operator*(float s)const{return V(x*s,y*s,z*s);}
};
static float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V cross(V a,V b){return V(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static float length(V a){return sqrtf(dot(a,a));}
static V norm(V a){return a*(1.0f/std::max(length(a),1e-8f));}
struct Q {float x,y,z,w;Q(float X=0,float Y=0,float Z=0,float W=1):x(X),y(Y),z(Z),w(W){} };
static Q unit(Q q){float s=1/sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return Q(q.x*s,q.y*s,q.z*s,q.w*s);}
static Q inv(Q q){return Q(-q.x,-q.y,-q.z,q.w);}
static Q mul(Q a,Q b){return Q(a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z);}
static V rot(Q q,V p){V t=cross(V(q.x,q.y,q.z),p)*2;return p+t*q.w+cross(V(q.x,q.y,q.z),t);}
static Q between(V a,V b){a=norm(a);b=norm(b);float d=dot(a,b);if(d<-.9999f){V v=norm(cross(a,fabsf(a.y)<.9f?V(0,1,0):V(1,0,0)));return Q(v.x,v.y,v.z,0);}V v=cross(a,b);return unit(Q(v.x,v.y,v.z,1+d));}
static Q blend(Q a,Q b,float t){float s=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0?-1:1;return unit(Q(a.x+(s*b.x-a.x)*t,a.y+(s*b.y-a.y)*t,a.z+(s*b.z-a.z)*t,a.w+(s*b.w-a.w)*t));}
static Q quat(const float* p){return unit(Q(p[0],p[1],p[2],p[3]));}
static V pos(const float* p){return V(p[4],p[5],p[6]);}
static void put(float* p,Q q,V v){q=unit(q);p[0]=q.x;p[1]=q.y;p[2]=q.z;p[3]=q.w;p[4]=v.x;p[5]=v.y;p[6]=v.z;p[7]=1;}
static V yaw(V p,float a){float c=cosf(a),s=sinf(a);return V(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
struct Leg {char name[8];unsigned bone[4];float point[15],offset,l1,l2;};
static_assert(sizeof(Leg)==96,"Ant rig record layout");
static Leg legs[6];static int parents[32];static float rest[256];
static int model=-1,fluid=-1,lastTick=-1;static float cycle=.2f,stance=.6f,lift=.055f,turnRate=1.9098593f;
struct Foot {V anchor,from,target;float phase;bool swing;Foot():phase(0),swing(false){} };
struct Actor {SimObject o;float yaw,phase;int tick,combat;float pose[256],previous[256];Foot feet[6],previousFeet[6];Actor():yaw(0),phase(0),tick(-1),combat(0){memset(pose,0,sizeof pose);} };
struct Dead {Actor actor;int anim,stage,stages,stageTick,delay;float phase;};
static std::map<int,Actor> live;static std::map<int,Dead> dead;
static bool type(const char* s){return !strcmp(s,"ANT1")||!strcmp(s,"ANT2")||!strcmp(s,"ANT3");}
static bool enabled(){return g_raModule && evhc_rule_now() && !g_fx.dos_classic && model>=0 && fluid>=0 && fx_gl_ready && m3_prog()!=0;}
static int clip(const char* n){for(size_t i=0;i<g_m3[model].clip.size();i++)if(g_m3[model].clip[i].name==n)return (int)i;return 0;}
static V point(const Leg& l,int i){return V(l.point[i*3],l.point[i*3+1],l.point[i*3+2]);}
static float angle(const SimObject& o){float s,c;facing_rot(draw_heading(o),&s,&c);return atan2f(s,c);}
/* Physics samples engine facing only; render interpolation must not feed the gait. */
static float tickAngle(const SimObject& o){float s,c;facing_rot((float)o.face,&s,&c);return atan2f(s,c);}
static float delta(float a,float b){float d=a-b;while(d>M_PI)d-=2*M_PI;while(d<-M_PI)d+=2*M_PI;return d;}
static V world(V p,const SimObject& o,float a){p=yaw(p*(1.0f/1024),a);return p+V(o.wx,terrain_y(o.wx,o.wz),o.wz);}
static V local(V p,const SimObject& o,float a){return yaw(p-V(o.wx,terrain_y(o.wx,o.wz),o.wz),-a)*1024;}
static V ground(V p){p.y=terrain_y(p.x,p.z);return p;}
/* Fit the exact source rig's two links to a world-space foot anchor. The hip,
   receiver, knee pole and lengths all come from the approved native mesh. */
static void solve(float* pose,const SimObject& o,float a,const Foot* feet){
    Q body=quat(pose+8),rb=quat(rest+8),D=mul(body,inv(rb));V bp=pos(pose+8),rp=pos(rest+8);
    for(int j=0;j<6;j++){
        const Leg& l=legs[j];V h=bp+rot(D,point(l,0)-rp),b=bp+rot(D,point(l,1)-rp),pole=bp+rot(D,point(l,2)-rp);
        V toe=local(feet[j].anchor,o,a),ankle=toe+(point(l,3)-point(l,4));V diff=ankle-b;float dist=length(diff);
        V axis=norm(diff);float reach=std::max(.001f,std::min(dist,l.l1+l.l2-.001f));float along=(l.l1*l.l1-l.l2*l.l2+reach*reach)/(2*reach);
        V bend=norm(pole-b-axis*dot(pole-b,axis));V knee=b+axis*along+bend*sqrtf(std::max(0.f,l.l1*l.l1-along*along));
        V starts[4]={h,b,knee,ankle},ends[4]={b,knee,ankle,toe};
        for(int k=0;k<4;k++){
            unsigned bi=l.bone[k];V old=rot(D,point(l,k+1)-point(l,k));Q q=mul(between(old,ends[k]-starts[k]),mul(D,quat(rest+bi*8)));
            put(pose+bi*8,q,starts[k]);
        }
    }
}
static void sample(Actor& a,const char* name,float frame){m3_sample(g_m3[model],clip(name),frame,a.pose);}
static void init(Actor& a,const SimObject& o,int tick){a.o=o;a.yaw=tickAngle(o);a.tick=tick;a.phase=0;sample(a,"rest",0);for(int j=0;j<6;j++){a.feet[j].anchor=ground(world(point(legs[j],4),o,a.yaw));a.feet[j].from=a.feet[j].target=a.feet[j].anchor;a.feet[j].phase=legs[j].offset;}memcpy(a.previous,a.pose,sizeof a.pose);for(int j=0;j<6;j++)a.previousFeet[j]=a.feet[j];}
static void tick(int frame){
    if(frame==lastTick)return;
    if(frame<lastTick){live.clear();dead.clear();}lastTick=frame;
    if(!enabled()){live.clear();dead.clear();return;}
    /* Read native Ant body frames only. Fire/Tesla subobjects remain native. */
    std::map<int,int> firing;
    static std::vector<unsigned char> bytes(4*1024*1024);
    memset(&bytes[0],0,16);
    BrainGetState(GAME_STATE_LAYERS,0,&bytes[0],(unsigned)bytes.size());
    CNCObjectListStruct* list=(CNCObjectListStruct*)&bytes[0];
    if(list->Count>=0 && list->Count<50000)for(int i=0;i<list->Count;i++){const CNCObjectStruct& s=list->Objects[i];if(type(s.AssetName)&&!s.SubObject&&s.ShapeIndex>=72&&s.ShapeIndex<104)firing[s.ID]=1;}
    std::set<int> seen;
    for(size_t i=0;i<g_objects.size();i++){
        const SimObject& o=g_objects[i];if(o.kind!=K_UNIT||!type(o.type)||o.limbo||o.str<=0)continue;seen.insert(o.id);
        Actor& a=live[o.id];if(a.tick<0||strcmp(a.o.type,o.type)||hypotf(a.o.wx-o.wx,a.o.wz-o.wz)>2){init(a,o,frame);continue;}
        memcpy(a.previous,a.pose,sizeof a.pose);for(int j=0;j<6;j++)a.previousFeet[j]=a.feet[j];
        float ny=tickAngle(o),dx=o.wx-a.o.wx,dz=o.wz-a.o.wz,dist=hypotf(dx,dz),dy=delta(ny,a.yaw);float advance=dist/cycle+fabsf(dy)*turnRate;
        bool moving=dist>1e-6f||fabsf(dy)>1e-5f;
        bool unsettled=false;for(int j=0;j<6;j++){V target=ground(world(point(legs[j],4),o,ny));if(length(a.feet[j].anchor-target)>.005f)unsettled=true;}
        /* Finish planted-to-swing settling with the same approved tripod sequence. */
        if(!moving&&unsettled)advance=1.f/16.f;
        a.combat=firing.count(o.id)!=0;
        /* Subdivide an actual engine move/turn, never the gameplay tick. A native
           pivot can traverse an entire swing interval in one tick; testing only its
           endpoint skipped that step and left the old anchor beyond the leg's reach. */
        const int steps=std::max(1,(int)ceilf(advance/.04f));
        const float da=advance/steps;
        for(int step=1;step<=steps;step++){
            float u=step/(float)steps;SimObject at=o;at.wx=a.o.wx+dx*u;at.wz=a.o.wz+dz*u;float ay=a.yaw+dy*u;
            a.phase+=da;
            for(int j=0;j<6;j++){
                Foot& f=a.feet[j];float old=f.phase;f.phase=fmodf(a.phase+legs[j].offset,1.f);bool sw=f.phase>=stance;
                if(sw&&!f.swing){
                    f.from=f.anchor;float remain=(1-f.phase)/std::max(advance,1e-5f);remain=std::min(remain,10.f);
                    SimObject predicted=at;predicted.wx+=dx*remain;predicted.wz+=dz*remain;
                    V toe=point(legs[j],4);if(dist>1e-6f)toe.z+=.06f*1024;
                    f.target=ground(world(toe,predicted,ay+dy*remain));
                }
                if(sw){float t=(f.phase-stance)/(1-stance);float v=t*t*(3-2*t);f.anchor=f.from*(1-v)+f.target*v;f.anchor.y+=sinf(t*M_PI)*lift;}
                else if(f.swing||f.phase<old)f.anchor=f.target;
                f.swing=sw;
            }
        }
        sample(a,a.combat?"combat":moving?"walk":"idle",a.combat?(frame%16)*4.f:moving?a.phase*64.f:(frame%32)*4.f);
        a.o=o;a.yaw=ny;a.tick=frame;solve(a.pose,o,ny,a.feet);
    }
    std::set<int> anims;
    for(size_t i=0;i<g_efxAnims.size();i++){
        const EfxAnim& e=g_efxAnims[i];if(strncmp(e.name,"ANTDIE",6))continue;anims.insert(e.id);
        if(!dead.count(e.id)){
            std::map<int,Actor>::iterator best=live.end();float nearest=.36f;
            for(std::map<int,Actor>::iterator p=live.begin();p!=live.end();++p)if(!seen.count(p->first)){float d=hypotf(p->second.o.wx-e.wx,p->second.o.wz-e.wz);if(d<nearest){nearest=d;best=p;}}
            if(best!=live.end()){Dead d;d.actor=best->second;d.actor.o.blush=0;d.anim=e.id;d.stage=e.stage;d.stages=std::max(1,e.stages);d.stageTick=frame;d.delay=4;d.phase=0;dead[e.id]=d;live.erase(best);}
        }
        if(dead.count(e.id)){Dead& d=dead[e.id];if(d.stage!=e.stage){int step=std::max(1,e.stage-d.stage);d.delay=std::max(1,(frame-d.stageTick)/step);d.stage=e.stage;d.stageTick=frame;}d.phase=std::min(1.f,(e.stage+std::min(.999f,(frame-d.stageTick)/(float)d.delay))/std::max(1,e.stages));}
    }
    for(std::map<int,Actor>::iterator i=live.begin();i!=live.end();)if(!seen.count(i->first))live.erase(i++);else ++i;
    for(std::map<int,Dead>::iterator i=dead.begin();i!=dead.end();)if(!anims.count(i->first))dead.erase(i++);else ++i;
}
/* Parent-local interpolation preserves fitted receivers during a captured-pose death. */
static void deathPose(const Dead& d,float* out){
    float target[256];m3_sample(g_m3[model],clip("death"),d.phase*128,target);float t=std::min(1.f,d.phase*8);t=t*t*(3-2*t);
    for(int b=0;b<32;b++){
        int p=parents[b];Q qa=quat(d.actor.pose+b*8),qb=quat(target+b*8);V va=pos(d.actor.pose+b*8),vb=pos(target+b*8);
        if(p>=0){Q ia=inv(quat(d.actor.pose+p*8)),ib=inv(quat(target+p*8));va=rot(ia,va-pos(d.actor.pose+p*8));vb=rot(ib,vb-pos(target+p*8));qa=mul(ia,qa);qb=mul(ib,qb);}
        Q q=blend(qa,qb,t);V v=va*(1-t)+vb*t;if(p>=0){v=pos(out+p*8)+rot(quat(out+p*8),v);q=mul(quat(out+p*8),q);}put(out+b*8,q,v);
    }
}
static void draw(const Actor& a,const SimObject& o,const float* pose){
    M3Model& m=g_m3[model];if(!m3_skin_gpu(m,pose))m3_skin(m,pose);
    M3Draw d;memset(&d,0,sizeof d);d.cx=o.wx;d.cz=o.wz;d.ground=terrain_y(o.wx,o.wz);d.yaw=angle(o)*180/M_PI;d.vehicle=1;
    house_colour(o,&d.pr,&d.pg,&d.pb);
    if(o.remap>=0&&o.remap<LIVERY_COUNT){int c=o.remap==5?6:o.remap==6?5:o.remap;d.pr=LIVERY_BAND[c][2][0]/255.f;d.pg=LIVERY_BAND[c][2][1]/255.f;d.pb=LIVERY_BAND[c][2][2]/255.f;}
    // The original gold remap uses the approved sprite-derived Ant tint.
    d.cr=d.pr; d.cg=d.pg; d.cb=d.pb;
    if (o.remap == 0) {
        d.cr = 0.75037006f; d.cg = 0.61170012f; d.cb = 0.14931935f;
    }
    d.camt=1; d.normalizeCloth=1;
    d.tintR=g_meshTintR/255.f;d.tintG=g_meshTintG/255.f;d.tintB=g_meshTintB/255.f;d.alpha=g_meshAlphaScale/255.f;d.flash=g_flashOn&&o.blush;
    d.shroud=(g_objShroud&&!g_meshNoShroud)?shroud_env_at(o.wx,o.wz):0;m3_draw(model,d);
}
static bool object(const SimObject& o,int pass){if(!enabled()||o.kind!=K_UNIT||!type(o.type))return false;if(pass!=MODE_OPAQUE||!g_vShowUnits)return true;
    std::map<int,Actor>::iterator it=live.find(o.id);if(it==live.end())return false;Actor& a=it->second;float pose[256];Foot feet[6];float t=g_smoothMove?g_tickAlpha:1.f;
    for(int b=0;b<32;b++)put(pose+b*8,blend(quat(a.previous+b*8),quat(a.pose+b*8),t),pos(a.previous+b*8)*(1-t)+pos(a.pose+b*8)*t);
    for(int j=0;j<6;j++){feet[j]=a.feet[j];feet[j].anchor=a.previousFeet[j].anchor*(1-t)+a.feet[j].anchor*t;}
    solve(pose,o,angle(o),feet);draw(a,o,pose);return true;}
static void corpses(int pass){if(!enabled()||pass!=MODE_OPAQUE||!g_vShowUnits)return;for(std::map<int,Dead>::const_iterator i=dead.begin();i!=dead.end();++i){float pose[256];deathPose(i->second,pose);if(visible(i->second.actor.o))draw(i->second.actor,i->second.actor.o,pose);}}
static void fluids(){if(!enabled()||dead.empty()||!g_vShowUnits)return;GLboolean blendOn=glIsEnabled(GL_BLEND),alphaOn=glIsEnabled(GL_ALPHA_TEST),depth;GLint src,dst;glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_ALPHA_TEST);glDepthMask(GL_FALSE);
    for(std::map<int,Dead>::const_iterator i=dead.begin();i!=dead.end();++i){const Dead& d=i->second;if(!visible(d.actor.o))continue;float p[32];M3Model& m=g_m3[fluid];m3_sample(m,0,0,p);
        SimObject o=d.actor.o;V off=yaw(V(.08f,0,-.08f),d.actor.yaw);o.wx+=off.x;o.wz+=off.z;
        for(int b=0;b<4;b++){V corner=pos(p+b*8);V w=world(corner,o,d.actor.yaw);corner.y+=(terrain_y(w.x,w.z)-terrain_y(o.wx,o.wz))*1024;put(p+b*8,quat(p+b*8),corner);}
        if(!m3_skin_gpu(m,p))m3_skin(m,p);M3Draw v;memset(&v,0,sizeof v);v.cx=o.wx;v.cz=o.wz;v.ground=terrain_y(o.wx,o.wz);v.yaw=d.actor.yaw*180/M_PI;v.tintR=v.tintG=v.tintB=v.alpha=1;v.fluidGrowth=std::min(1.f,.25f+d.phase*2);v.shroud=(g_objShroud&&!g_meshNoShroud)?shroud_env_at(o.wx,o.wz):0;m3_draw(fluid,v);
    }
    glDepthMask(depth);glBlendFunc(src,dst);if(!blendOn)glDisable(GL_BLEND);if(alphaOn)glEnable(GL_ALPHA_TEST);
}
}
static void ant3d_step(int frame){ant3d::tick(frame);}
static bool ant3d_ready(const SimObject& o){return ant3d::enabled()&&o.kind==K_UNIT&&ant3d::type(o.type)&&ant3d::live.count(o.id);}
static bool ant3d_draw_object(const SimObject& o,int pass){return ant3d::object(o,pass);}
static bool ant3d_card(const CNCObjectStruct& o){if(!ant3d::enabled())return false;if(ant3d::type(o.AssetName)){int id=o.SubObject?o.BaseObjectID:o.ID;return ant3d::live.count(id)!=0;}return !strncmp(o.AssetName,"ANTDIE",6)&&ant3d::dead.count(o.ID);}
static void ant3d_load(const char*){
    if(!g_raModule||ant3d::model>=0)return;
    // Module packages use a path accepted by existing launchers.
    const char* prefix = "ant";
    if (dir_exists("ra3d/3d/ant")) prefix = "ra3d/3d/ant";
    else if (dir_exists("3d/ant")) prefix = "3d/ant";
    else if (dir_exists("ra3d/ant")) prefix = "ra3d/ant";
    char path[256];snprintf(path,sizeof path,"%s/ANT.rig",prefix);
    FILE* f=fopen(path,"rb");if(!f)return;char magic[8];unsigned h[6];float cfg[4];bool ok=fread(magic,1,8,f)==8&&!memcmp(magic,"ANTRIG1\0",8)&&fread(h,4,6,f)==6&&h[0]==1&&h[1]==32&&h[2]==6&&fread(cfg,4,4,f)==4&&fread(ant3d::parents,4,32,f)==32&&fread(ant3d::rest,4,256,f)==256&&fread(ant3d::legs,sizeof(ant3d::Leg),6,f)==6;fclose(f);if(!ok)return;
    char bodyPath[256],fluidPath[256];snprintf(bodyPath,sizeof bodyPath,"%s/ANT.c3m",prefix);snprintf(fluidPath,sizeof fluidPath,"%s/ANTFX.c3m",prefix);
    M3Model a,b;if(!m3_read_one(bodyPath,a)||!m3_read_one(fluidPath,b)||a.bone.size()!=32||b.bone.size()!=4||a.idx.size()!=956*3||b.idx.size()!=6)return;
    if(a.clip.size()!=10 || b.clip.size()!=1 || cfg[0]<=0 || cfg[1]<=0 || cfg[1]>=1)return;
    for(int j=0;j<32;j++)if(ant3d::parents[j]>=j||ant3d::parents[j]<-1)return;
    for(int j=0;j<6;j++)for(int k=0;k<4;k++)if(ant3d::legs[j].bone[k]>=32)return;
    a.enh=b.enh=true;for(size_t i=0;i<a.mat.size();i++)if(a.mat[i].flags&M3_MAT_CLOTH)a.mat[i].clothRef=.4f;
    ant3d::cycle=cfg[0];ant3d::stance=cfg[1];ant3d::lift=cfg[2];ant3d::turnRate=cfg[3];ant3d::model=g_m3.size();g_m3.push_back(a);ant3d::fluid=g_m3.size();g_m3.push_back(b);
    printf("ANT3D|loaded|body=956|fluid=2|bones=32|variants=ANT1,ANT2,ANT3\n");
}
static void ant3d_normals(){if(!ant3d::enabled())return;int save=g_m3NormalPass;g_m3NormalPass=1;for(size_t i=0;i<g_objects.size();i++)if(visible(g_objects[i]))ant3d_draw_object(g_objects[i],MODE_OPAQUE);ant3d::corpses(MODE_OPAQUE);g_m3NormalPass=save;}
static void ant3d_probe(){printf("ANT3D|probe|enabled=%d|model=%d|fluid=%d|live=%zu|dead=%zu|tris=958\n",ant3d::enabled(),ant3d::model,ant3d::fluid,ant3d::live.size(),ant3d::dead.size());for(std::map<int,ant3d::Actor>::const_iterator i=ant3d::live.begin();i!=ant3d::live.end();++i){const ant3d::Actor& a=i->second;float error=0,footError=0,linkError=0;
        for(int j=0;j<6;j++){
            const ant3d::Leg& l=ant3d::legs[j];unsigned b=l.bone[3];
            ant3d::V toe=ant3d::pos(a.pose+b*8)+ant3d::rot(ant3d::mul(ant3d::quat(a.pose+b*8),ant3d::inv(ant3d::quat(ant3d::rest+b*8))),ant3d::point(l,4)-ant3d::pos(ant3d::rest+b*8));
            toe=ant3d::world(toe,a.o,a.yaw);footError=std::max(footError,ant3d::length(toe-a.feet[j].anchor));
            if(!a.feet[j].swing)error=std::max(error,fabsf(toe.y-terrain_y(toe.x,toe.z)));
            for(int k=1;k<3;k++){float actual=ant3d::length(ant3d::pos(a.pose+l.bone[k+1]*8)-ant3d::pos(a.pose+l.bone[k]*8))/1024;float expected=(k==1?l.l1:l.l2)/1024;linkError=std::max(linkError,fabsf(actual-expected));}
        }
        printf("ANT3D|rigcheck|id=%d|foot_error=%.8f|link_error=%.8f\n",i->first,footError,linkError);
        printf("ANT3D|actor|id=%d|type=%s|phase=%.6f|combat=%d|contact_error=%.8f\n",i->first,a.o.type,a.phase,a.combat,error);}for(std::map<int,ant3d::Dead>::const_iterator i=ant3d::dead.begin();i!=ant3d::dead.end();++i)printf("ANT3D|corpse|anim=%d|stage=%d|stages=%d|phase=%.5f\n",i->first,i->second.stage,i->second.stages,i->second.phase);fflush(stdout);}
#endif

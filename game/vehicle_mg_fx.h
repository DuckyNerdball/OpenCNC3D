/* Enhanced machine guns use the existing infantry flash/casing effects, placed on
   the actual posed enhanced vehicle gun. Native firing events are the only trigger. */
static std::vector<EfxFire> g_vehicleMgPending;
static unsigned g_vehicleMgShots = 0;
static void vehicle_mg_fx_queue(const EfxFire& f)
{
    if (!g_fx.enabled || f.which != 0) return;
    if (strcmp(f.type,"JEEP") && strcmp(f.type,"BGGY") && strcmp(f.type,"APC")) return;
    if (g_vehicleMgPending.size()<128) g_vehicleMgPending.push_back(f);
}
static void vehicle_mg_fx_draw_step()
{
    for (const EfxFire& f:g_vehicleMgPending) {
        const SimObject* by=NULL;
        for (const SimObject& o:g_objects)
            if(o.kind==K_UNIT && o.id==f.id && !strcmp(o.type,f.type) && !o.limbo && !o.dying) {by=&o;break;}
        if(!by || !visible(*by)) continue;
        const SimObject& o=*by;const int model=evhc_for(o);
        if(model<0 || !evhc_resident(g_evhc[model],false))continue;
        const EvhcModel& V=g_evhc[model];const EbldSet& S=V.m.set[EBS_INTACT];
        int gun=-1;float maxz=-1e9f,minz=1e9f;
        for(size_t p=0;p<V.part.size();++p) if(V.part[p].kind==EVP_TURRET) {
            float z=-1e9f;
            for(size_t k=0;k<S.piece.size();++k)if(V.partOf[k]==(int)p)
                for(int t=S.piece[k].tri0;t<S.piece[k].tri0+S.piece[k].ntri;++t)
                    for(const EbldVert& v:S.tri[t].v)z=std::max(z,v.z);
            if(z>maxz){maxz=z;gun=(int)p;}
        }
        if(gun<0)continue;
        float muzzle[3]={0,0,0};int count=0;
        for(size_t k=0;k<S.piece.size();++k)if(V.partOf[k]==gun)
            for(int t=S.piece[k].tri0;t<S.piece[k].tri0+S.piece[k].ntri;++t)
                for(const EbldVert& v:S.tri[t].v){minz=std::min(minz,v.z);if(v.z>=maxz-3){muzzle[0]+=v.x;muzzle[1]+=v.y;muzzle[2]+=v.z;++count;}}
        if(!count)continue;for(float& v:muzzle)v/=count;
        auto it=g_evhcUnit.find(smooth_key(o));const EvhcUnit* U=it!=g_evhcUnit.end()&&it->second.model==model?&it->second:NULL;
        EvhcPose P;evhc_pose_of(V,o,U,P);std::vector<float> mats;evhc_pose_all(V,P,U,mats);
        const float* transform=&mats[gun*12];float mw[3],we[3],right[3];
        evhc_lamp_world(o,transform,muzzle,true,mw);
        float port[3]={muzzle[0]+30,muzzle[1],(minz+maxz)*.5f};evhc_lamp_world(o,transform,port,true,we);
        const float axis[3]={1,0,0};evhc_lamp_world(o,transform,axis,false,right);ir_norm3(right);
        const unsigned seed=ir_hash((unsigned)o.id*31u+(unsigned)f.bullet,(unsigned)g_engineFrame);
        if(g_efxEnhOn && g_fx.einf_flash){efx_emit(EFX_ENH_EXTRA[0].src,mw[0],mw[1],mw[2],1,seed,g_engineFrame,0x72,0);++g_irFlashes;}
        if(g_fx.einf_casings){
            IrCasing c;memcpy(c.p,we,sizeof we);
            for(int k=0;k<3;++k){c.v[k]=right[k]*(.9f+.4f*ir_r01(seed));c.a[k]=ir_r01(seed>>(k*5+1))*6.28f;c.w[k]=(ir_r01(seed>>(k*3+16))-.5f)*40;}
            c.v[1]+=.65f;c.born=g_engineFrame;c.rest=0;
            if(g_irCasings.size()>=IR_CASING_MAX)g_irCasings.erase(g_irCasings.begin());g_irCasings.push_back(c);++g_irCasingsMade;
        }
        ++g_vehicleMgShots;printf("VEHMGFX|type=%s|id=%d|frame=%d|muzzle=%.5f,%.5f,%.5f|eject=%.5f,%.5f,%.5f|casings=%d|shots=%u\n",o.type,o.id,g_engineFrame,mw[0],mw[1],mw[2],we[0],we[1],we[2],g_irCasingsMade,g_vehicleMgShots);
    }
    g_vehicleMgPending.clear();
}

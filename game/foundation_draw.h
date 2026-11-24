#ifndef OPENCNC3D_FOUNDATION_DRAW_H
#define OPENCNC3D_FOUNDATION_DRAW_H

static bool ra3d_foundation_material(int x,int z,GLuint* texture,float uv[4]);
static size_t g_foundationDrawn[3]={0,0,0};

static const std::vector<FoundationStep>& foundation_steps_now()
{
    static std::vector<FoundationStep> steps;
    static unsigned gen=0,epoch=0;
    if(gen!=g_groundGen||epoch!=g_padEpoch) {
        steps.clear();
        if(g_foundationGen==g_groundGen&&!g_pack.corner.empty())steps=g_foundations.steps(g_pack.corner.data());
        gen=g_groundGen;epoch=g_padEpoch;
    }
    return steps;
}

/* Closed vertical terrain faces between the independent cell planes. They reuse
   the higher cell's existing ground artwork: no authored assets or palettes are
   introduced. The same triangles enter colour, normal, depth and reflection
   passes. Native water holes remain open; submerged dock cells are not earth. */
static void foundation_steps_draw(int mode) // 0 colour, 1 encoded normals, 2 depth
{
    g_foundationDrawn[mode]=0;
    const auto& steps=foundation_steps_now();
    if(steps.empty()||ra3d_flat_ground())return;
    static unsigned mapGen=0;static std::vector<int> of;
    if(mapGen!=g_groundGen||of.size()!=(size_t)g_gridW*g_gridH) {
        of.assign((size_t)g_gridW*g_gridH,-1);
        for(size_t i=0;i<g_pack.cell.size();++i) {
            const PackCell& c=g_pack.cell[i];
            if(c.x>=0&&c.y>=0&&c.x<g_gridW&&c.y<g_gridH)of[(size_t)c.y*g_gridW+c.x]=(int)i;
        }
        mapGen=g_groundGen;
    }
    static std::vector<ImmVert> verts;verts.clear();GLuint texture=0;
    for(const FoundationStep& s:steps) {
        const int nx=s.x+(s.axis==0),nz=s.z+(s.axis==1);
        if(!cell_shown(s.x,s.z)&&!cell_shown(nx,nz))continue;
        const bool a=(int)s.a0+s.a1>=(int)s.b0+s.b1;
        const int sx=a?s.x:nx,sz=a?s.z:nz;
        const int ci=of[(size_t)sz*g_gridW+sx];
        if(ci<0||g_pack.cell[ci].holes)continue;
        float uv[4]={0,0,1,1};
        if(mode==0) {
            if(g_raModule) {if(!ra3d_foundation_material(sx,sz,&texture,uv))continue;}
            else {
                const PackTex& t=g_pack.tex[terrain_atlas_index()];const PackCell& c=g_pack.cell[ci];
                texture=t.gl;
                uv[0]=c.u0*(float)t.uw/t.w;uv[1]=c.v0*(float)t.uh/t.h;
                uv[2]=c.u1*(float)t.uw/t.w;uv[3]=c.v1*(float)t.uh/t.h;
            }
        }
        float P[4][3];s.vertices(g_terrainBase,P);
        const int order[6]={0,1,2,2,1,3};
        const float sign=a?1.0f:-1.0f;
        for(int j:order) {
            const bool endVertex=(j&1)!=0,upperVertex=j>=2;
            float r=1,g=1,b=1;
            if(mode==1) {r=s.axis==0?0.5f+0.5f*sign:0.5f;g=0.5f;b=s.axis==1?0.5f+0.5f*sign:0.5f;}
            else if(mode==0) {
                const int cx=(int)P[j][0],cz=(int)P[j][2];
                r=g=b=(fx_unified_lighting()?160.0f/255.0f:terrain_shade(cx,cz))*shroud_corner_vis(cx,cz);
            }
            imm_put(verts,P[j][0],P[j][1],P[j][2],endVertex?uv[2]:uv[0],upperVertex?uv[1]:uv[3],r,g,b,1);
        }
    }
    if(verts.empty())return;
    g_foundationDrawn[mode]=verts.size()/6;
    glActiveTexture(GL_TEXTURE0);
    glPushAttrib(GL_ENABLE_BIT|GL_TEXTURE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE1);glDisable(GL_TEXTURE_2D);glActiveTexture(GL_TEXTURE0);
    glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);
    if(mode==0) {glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);}
    else glDisable(GL_TEXTURE_2D);
    imm_draw_triangles(verts,false);
    glPopAttrib();
}

#endif

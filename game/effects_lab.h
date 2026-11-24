/* Private user-map effect gallery. Uses production animation recipes and the same
   pools, materials and post chain as battle effects; never mutates engine state. */
#pragma once
struct EffectsLabCell { int type; const char* name; float x,z; };
static std::vector<EffectsLabCell> g_effectsLabCells;
static int g_effectsLabOn=0,g_effectsLabStart=-1;
static void effects_lab_read(const char* path) {
    g_effectsLabOn=0;g_effectsLabStart=-1;g_effectsLabCells.clear();
    bool privateInstall=false;
    const char* records[]={"opencnc3d-install.txt","../opencnc3d-install.txt",0};
    char line[512];
    for(int i=0;records[i];i++)if(FILE* f=fopen(records[i],"r")) {
        while(fgets(line,sizeof line,f))if(!strncmp(line,"private_developer 1",19))privateInstall=true;
        fclose(f);
    }
    FILE* f=fopen(path,"r");if(!f)return;bool section=false,enabled=false;
    while(fgets(line,sizeof line,f)) {
        if(line[0]=='[')section=!strncasecmp(line,"[EffectsLab]",12);
        else if(section && !strncasecmp(line,"Enabled=1",9))enabled=true;
    }
    fclose(f);if(!privateInstall||!enabled)return;
    bool used[66]={false};
    for(int type=0;type<EFX_ANIM2RECIPE_N;type++) {
        const int rec=efx_recipe_index(type);if(rec<0||rec>=66||used[rec])continue;
        const EfxRecipe& recipe=EFX_RECIPE[rec];
        if(!recipe.nslot && strcmp(recipe.name,"MUZZLE_FLASH") && strcmp(recipe.name,"ATOM_BLAST") && strcmp(recipe.name,"ION_CANNON"))continue;
        used[rec]=true;int n=(int)g_effectsLabCells.size();
        EffectsLabCell cell={type,recipe.name,11.0f+(n%7)*6.0f,11.0f+(n/7)*6.0f};
        g_effectsLabCells.push_back(cell);
    }
    g_effectsLabOn=1;
    printf("EFFECTSLAB|private=1|cells=%d|production-recipes|loop=180-ticks\n",(int)g_effectsLabCells.size());
}
static void effects_lab_step(int frame) {
    if(!g_effectsLabOn)return;
    if(g_effectsLabStart<0||frame<g_effectsLabStart)g_effectsLabStart=frame;
    /* The dump refreshes the list, but repeated steps without a dump must be bounded. */
    for(size_t i=0;i<g_efxAnims.size();) {
        if(g_efxAnims[i].id<=-20000 && g_efxAnims[i].id>-21000)g_efxAnims.erase(g_efxAnims.begin()+i);else i++;
    }
    for(size_t i=0;i<g_effectsLabCells.size();i++) {
        const EffectsLabCell& c=g_effectsLabCells[i];
        const int age=(frame-g_effectsLabStart+(int)i*3)%180;
        if(age>=60)continue;
        EfxAnim a={};a.id=-20000-(int)i;a.type=a.etype=c.type;a.stage=age;a.stages=60;
        a.attref=a.attkind=a.owner=-1;a.chunkHouse=-1;a.ground=1;a.size=24;
        snprintf(a.name,sizeof a.name,"%s",c.name);a.wx=c.x;a.wz=c.z;
        g_efxAnims.push_back(a);
    }
}
static void effects_lab_draw(int fbw,int fbh,int labelsOnly) {
    if(!g_effectsLabOn)return;
    /* Grid follows each station's ground; all effects render through the battle path. */
    if(!labelsOnly) {
    glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glColor3f(.36f,.48f,.39f);
    glBegin(GL_LINES);
    for(size_t i=0;i<g_effectsLabCells.size();i++) {
        const EffectsLabCell& c=g_effectsLabCells[i];
        for(int edge=0;edge<4;edge++) {
            static const float q[4][2]={{-2.8f,-2.8f},{2.8f,-2.8f},{2.8f,2.8f},{-2.8f,2.8f}};
            for(int end=0;end<2;end++) {int k=(edge+end)%4;float x=c.x+q[k][0],z=c.z+q[k][1];glVertex3f(x,terrain_y(x,z)+.025f,z);}
        }
    }
    glEnd();glColor3f(1,1,1);return;
    }
    fxp_font_init();
    struct Label {float x,y;const char* name;};std::vector<Label> labels;
    for(size_t i=0;i<g_effectsLabCells.size();i++) {
        const EffectsLabCell& c=g_effectsLabCells[i];Label l;
        world_to_screen(c.x,terrain_y(c.x,c.z)+.05f,c.z+2.5f,fbw,fbh,&l.x,&l.y);
        l.name=c.name;if(l.x>0&&l.x<fbw&&l.y>0&&l.y<fbh)labels.push_back(l);
    }
    begin_overlay(fbw,fbh);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,g_fxpFont);
    for(size_t i=0;i<labels.size();i++) {
        const Label& l=labels[i];int x=(int)l.x-(int)strlen(l.name)*3;
        fxp_text(x+1,(int)l.y+1,l.name,1,0,0,0,1);fxp_text(x,(int)l.y,l.name,1,.9f,1,.9f,1);
    }
    glDisable(GL_TEXTURE_2D);end_overlay();
}

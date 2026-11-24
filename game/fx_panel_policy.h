/* Curated Enhanced tuning rows. Hidden legacy rows remain in the preset schema.
   Both layout and scripted navigation use this policy, so clicks and labels agree. */
#ifndef OPENCNC3D_FX_PANEL_POLICY_H
#define OPENCNC3D_FX_PANEL_POLICY_H


enum { FXP_TAB_COUNT=4 };
static const int FXP_TAB_X[FXP_TAB_COUNT]={8,92,176,260};
static const int FXP_TAB_W=80;
static const char* const FXP_TAB_LABEL[FXP_TAB_COUNT]={"ENHANCED","WEATHER","LIGHTING","EXPERIMENTAL"};
static int fxp_tab_at(int x) {
    for(int i=0;i<FXP_TAB_COUNT;i++)if(x>=FXP_TAB_X[i]&&x<FXP_TAB_X[i]+FXP_TAB_W)return i;
    return -1;
}

static int fxp_group_tab(const FxParam* group)
{
    if (!group) return 0;
    const char* label = group->label;
    if (!strcmp(label,"EXPERIMENTAL")) return 3;
    return strstr(label,"CLOUD SHADOWS") || strstr(label,"MIST / FOG") ||
           strstr(label,"SAND DUST") || strstr(label,"DISTANCE FOG") ||
           strstr(label,"TIBERIUM FOG") || strstr(label,"WATER") ||
           strstr(label,"SNOWFALL") ? 1 : 0;
}

static int fxp_row_enabled(const FxParam* p,const FxState* state)
{
    if(!strcmp(p->key,"st_second_sun_power"))return state->st_second_sun_on;
    if(!strcmp(p->key,"vh_second_sun_power"))return state->vh_second_sun_on;
    if(!strcmp(p->key,"inf_second_sun_power"))return state->inf_second_sun_on;
    return 1;
}

static int fxp_row_visible(const FxParam* p, const FxParam* group, const FxState* state)
{
    if (!group) return 0;
    if(!strcmp(group->label,"STRUCTURE READABILITY"))return 0;
    if(group&&!strcmp(group->label,"UNIT READABILITY"))return 0;
    const char* modeOwned[] = { "einf_on", "m3inf_on", "m3_on", "uvfx_on", "curgreen_on",
        "efx_enh", "ecur_on", "grass", "new_audio", 0 };
    for (int index = 0; modeOwned[index]; index++) if (!strcmp(p->key,modeOwned[index])) return 0;
    const char* label = group->label;
    if (strstr(label,"ENHANCED TREES") && !strncmp(p->key,"tree3d_",7)) {
        const char* live[]={"tree3d_wind","tree3d_wind_speed","tree3d_wind_dir","tree3d_flutter",0};
        bool supported=false;for(int i=0;live[i];i++)if(!strcmp(p->key,live[i]))supported=true;
        if(!supported)return 0;
    }
    if (strstr(label,"VI OUTPUT GAMMA") || strstr(label,"CRT") ||
        strstr(label," RAIN ") || strstr(label,"RELIGHT THE CARTRIDGE") ||
        strstr(label,"DEBUG") || !strcmp(label,"NEW AUDIO")) return 0;
    if (strstr(label,"ISOMETRIC") && state->perspective != FX_PERSP_ISO) return 0;
    /* AMBIENT OCCLUSION shows the dials of the method in use (ao_mode) and hides the
       other method's, which would do nothing. Intensity and the class weights serve both. */
    if (strstr(label,"AMBIENT OCCLUSION")) {
        const bool gtao = state->ao_mode >= 0.5f;
        if (!strncmp(p->key,"gtao_",5)) return gtao;
        const char* hemisphere[] = { "ssao_radius", "ssao_bias", "ssao_power", "ssao_samples", 0 };
        for (int i = 0; hemisphere[i]; i++) if (!strcmp(p->key,hemisphere[i])) return !gtao;
    }
    if (!strcmp(label,"3D GRASS") && !state->grass && !state->experimental_textures) return 0;
    if (strstr(label,"AUTHORED MODELS") && !state->experimental_textures) return 0;
    if (!strcmp(label,"EXPERIMENTAL") && !state->experimental_textures) {
        return !strcmp(p->key,"new_sprites") || !strncmp(p->key,"ns_",3) || !strncmp(p->key,"uvfx_",5) || !strcmp(p->key,"green_pointer") ||
               !strcmp(p->key,"pointer_scale");
    }
    /* Input, audio and display choices live in Options. These rows were saved by
       F5 but most did not apply live there, and do not tune the Enhanced picture. */
    if (!strcmp(label,"MASTER")) {
        const char* hidden[] = { "new_hud", "texset", "infset", "ui_scale", "display_mode",
            "res_w", "res_h", "mouse_scheme", "grab_button", "right_drag_scroll", "cash_tick", 0 };
        for (int i = 0; hidden[i]; i++) if (!strcmp(p->key,hidden[i])) return 0;
    }
    return 1;
}

static int fxp_param_tab(const FxParam* p,const FxParam* group) {
    /* The occlusion method is one setting for all theaters (it is not in a theater's
       lighting) but stays beside the dials it chooses between. */
    if (p->kind != FXP_GROUP && !strcmp(p->key,"ao_mode")) return 2;
    if(group && strstr(group->label,"GROUND MATERIALS")) return 2;
    return group && !strcmp(group->label,"FOG OF WAR (GLOBAL)") ? 2 : fx_lighting_param(p,group) ? 2 :
        group && strstr(group->label,"LINEAR LIGHT") ? 2 : fxp_group_tab(group);
}
static int fxp_on_tab(const FxParam* p,const FxParam* group,const FxState* state,int tab) {
    if(p->kind!=FXP_GROUP)return fxp_param_tab(p,group)==tab && fxp_row_visible(p,group,state);
    for(const FxParam* q=p+1;q->kind!=FXP_END&&q->kind!=FXP_GROUP;q++)
        if(fxp_param_tab(q,p)==tab && fxp_row_visible(q,p,state))return 1;
    return 0;
}
static int fxp_group_visible(const FxParam* group, const FxState* state)
{
    for (const FxParam* p = group+1; p->kind != FXP_END && p->kind != FXP_GROUP; p++)
        if (fxp_row_visible(p,group,state)) return 1;
    return 0;
}
#endif

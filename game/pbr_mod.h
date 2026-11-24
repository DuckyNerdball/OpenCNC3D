/* ====================================================================================
 *  pbr_mod.h -- the host side of the buildings' and vehicles' material model.
 *
 *  The shader text and the reasoning behind it are pbr_glsl.h; this file hands the light
 *  pass what that text reads: the switch, the five dials, and the same sky the 3D
 *  soldiers are lit by (sky3d_mod.h), its nine irradiance coefficients as uniforms and
 *  its prefiltered cube on a texture unit of its own. Included by the host after
 *  sky3d_mod.h, whose globals it reads; fx_post.h declares the three hooks.
 *
 *  UNIT 15, and the sampler is pointed at it on every frame whether or not anything
 *  samples it. A sampler uniform nobody sets reads unit 0, where the scene colour is a
 *  2D texture, and a cube sampler and a 2D sampler on one unit make the whole draw
 *  invalid on a validating driver: the light pass would vanish rather than lose a term.
 *
 *  No new GL entry point: a cube map is OpenGL 1.3 tokens on 1.1 calls, and
 *  glActiveTexture is the one the chain already loads.
 * ==================================================================================== */
#ifndef OPENCNC3D_PBR_MOD_H
#define OPENCNC3D_PBR_MOD_H

#define PBR_ENV_UNIT 15

static int g_pbrEnvBound = 0;

static bool ra3d_interior_swapping(void);   /* ra3d_interior_mod.h, later in the host */

static void pbr_light_uniforms(GLuint prog, int bakeOn)
{
    /* The model replaces the unified relight's arithmetic, so it needs the bake target
       that relight divides by and the unified light it is written against. */
    const int on = (g_fx.pbr_on && bakeOn && fx_unified_lighting()) ? 1 : 0;
    fx_set1i(prog, "uPbrOn", on);
    fx_set1i(prog, "uPbrEnv", PBR_ENV_UNIT);
    const float dial[4] = { g_fx.pbr_rough_scale, g_fx.pbr_metal_scale,
                            g_fx.pbr_ibl_strength, g_fx.pbr_sky_tint };
    fx_set4fv(prog, "uPbrDial", 1, dial);
    fx_set1f(prog, "uPbrSun", g_fx.pbr_sun_strength);
    /* The sky's irradiance on an upward surface, evaluated as the shader does it with
       n = (0, 1, 0), which leaves four of the nine terms (m3_env_scale does the same). */
    /* INDOORS THERE IS NO SKY: a Red Alert mission set inside a building keeps its walls'
       own colour in the shade and mirrors the room's white fill, not a canyon's blue. */
    float lumaUp = 0.0f;
    if (on && g_sky3dHave && g_sky3dTex && !ra3d_interior_swapping()) {
        float up[3];
        for (int c = 0; c < 3; c++)
            up[c] = g_sky3dSH[0][c] * 0.282095f + g_sky3dSH[1][c] * 0.488603f
                  - g_sky3dSH[6][c] * 0.315392f - g_sky3dSH[8][c] * 0.546274f;
        lumaUp = 0.2126f * up[0] + 0.7152f * up[1] + 0.0722f * up[2];
    }
    const int env = lumaUp > 1e-4f ? 1 : 0;
    fx_set1i(prog, "uPbrEnvOn", env);
    g_pbrEnvBound = env;
    if (!env)
        return;
    float sh[36];
    for (int i = 0; i < 9; i++) {
        sh[i * 4 + 0] = g_sky3dSH[i][0];
        sh[i * 4 + 1] = g_sky3dSH[i][1];
        sh[i * 4 + 2] = g_sky3dSH[i][2];
        sh[i * 4 + 3] = 0.0f;
    }
    fx_set4fv(prog, "uPbrSH", 9, sh);
    fx_set1f(prog, "uPbrEnvMaxLod", sky3d_max_lod());
    fx_set1f(prog, "uPbrEnvNorm", 3.14159265f / lumaUp);
}

/* GL_TEXTURE_2D is not enabled on this unit: a cube sampler reads the cube binding,
   and enabling the 2D target beside it is what makes some drivers hand back white. */
static void pbr_light_bind(void)
{
    glActiveTexture(GL_TEXTURE0 + PBR_ENV_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, g_pbrEnvBound ? g_sky3dTex : 0);
    glActiveTexture(GL_TEXTURE0);
}

static void pbr_light_unbind(void)
{
    glActiveTexture(GL_TEXTURE0 + PBR_ENV_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
}

#endif /* OPENCNC3D_PBR_MOD_H */

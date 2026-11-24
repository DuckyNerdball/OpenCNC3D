/* ====================================================================================
 *  glint_glsl.h -- FLASH GLINTS AND BLAST SHADOWS: what the 24 point lights (fx_post.h,
 *                  gathered by fx_gather_lights in cnc_eyes.cpp) do beyond brightening.
 *
 *  Before this, an explosion, a muzzle flash or a fire lit a surface with a wrapped
 *  Lambert and nothing else: the ground beside a blast went orange and a tank hull beside
 *  it went exactly as orange, steel and dirt alike, and the light passed through walls.
 *  Two things are added, both inside the light pass, both behind the light_spec dial:
 *
 *  1. A HIGHLIGHT FROM EVERY LIGHT. The same lobe the sun gets on that class of surface:
 *     where a remastered vehicle or building carries a specular map, the map's own
 *     tightness and weight, as the sun's highlight takes them there (its Blinn-Phong
 *     exponent carried over to the GGX roughness that matches it); on a relit cartridge
 *     mesh the relight's own roughness; on generated rock and gravel their own; elsewhere
 *     a fixed material per class, broad and faint on ground and foliage. Every lobe is
 *     energy normalised, so a tight lobe on glossy steel is a small hot glint and a broad
 *     one on dirt spreads the same light thin rather than washing the ground out. A flame
 *     is treated as a small sphere, not a point, so it shows in a hull as a blob of its
 *     own size. The sea is left to the water's own shader and a self-lit pixel makes its
 *     own light.
 *
 *  2. SHADOWS FROM THE BIGGEST BLASTS. The blast_shadows brightest explosions in the light
 *     list (it is ranked by power, so they are the first explosions in it) are marched
 *     toward through the depth buffer from each surface they light, as a headlight is: a
 *     step that lands just behind something nearer the camera is in its shadow, and that
 *     surface loses blast_shadow_strength of that blast's light, diffuse and highlight
 *     together. The march runs toward the heart of the fireball, raised over the blast,
 *     and stops blast_shadow_near short of it, so the wreck or wall the blast sits on
 *     does not shadow its own fireball while a tank or a building beside it does. Only a
 *     surface turned toward the blast marches (one turned away keeps the wrap's last
 *     light, which marched would find its own back and come out as speckle). Muzzle
 *     flashes and fires never cast: a flash is two ticks long and a fire sits inside the
 *     wreck that is burning.
 *
 *     A DEPTH BUFFER HOLDS A SURFACE, NOT A SOLID, so what is behind a visible face has
 *     to be assumed, and the assumption is what sizes the shadow. A hull, a soldier or a
 *     piece of wreck is taken as blast_shadow_thick deep, a building, a wall or the
 *     ground as blast_shadow_wall (read from the class the normal target carries at the
 *     step). The fireball is a sphere, so the light a blocker stops is the share of the
 *     cone from the surface to that sphere it fills: a step counts by how far inside the
 *     blocker it is, from either face, against the cone's width there. A thin piece of
 *     wreck flying beside a fireball the size of a tank fills little of a cone that wide
 *     and casts a faint shadow; a building beside the surface fills all of a narrow one.
 *     Taken as half a cell deep, as everything once was, the flying pieces of a destroyed
 *     tank threw long, dark fans across two cells of grass.
 *
 *  LIGHT SPACE. The highlight is computed as linear light. With the light pass in display
 *  colour (linear_on 0) it is added to the frame's colour decoded with glint_decode and
 *  encoded again with glint_encode, and the light's colour is used as it is, which is how
 *  it was tuned. With the pass in linear light it is added to the radiance as it is, the
 *  light's colour is decoded first (it is a display value, the flame's colour on screen),
 *  and GLINT_LIN_GAIN keeps the strength the display-colour version has. The shadow takes
 *  back exactly what the point-light loop added (glint_point_diffuse mirrors it), in the
 *  encoding that loop adds in: the point lights are summed onto display colour in both
 *  modes (fx_lin_add), so the lost share is taken off display colour and decoded again.
 *
 *  DETERMINISM. The march is offset per pixel by interleaved gradient noise on the
 *  pixel's own position, never a clock, so two runs of one script are one picture. The
 *  offset is the same every frame, so its fine grain on a shadow's edge stays put on the
 *  screen; a temporal resolve can only average it away once the offset turns from frame
 *  to frame on that resolve's own frame index.
 *
 *  WHICH LIGHTS. Every light carries the radius of its flame (FxLight.flame), and a light
 *  whose flame is 0 puts no highlight: a standing building's own working lights, whose
 *  look was approved without one, unless light_spec_works is on.
 *
 *  OFF (light_spec 0) the block is a uniform branch not taken and the frame is the one
 *  the light pass drew without it, to the byte.
 * ==================================================================================== */
#ifndef OPENCNC3D_GLINT_GLSL_H
#define OPENCNC3D_GLINT_GLSL_H

/* Appended to the light program after FX_COMMON, so fx_world, uVP, uDepth, uCamPos and
   the point lights' own uniforms are already declared. */
static const char* FX_GLSL_GLINT =
    "uniform int   uGlintOn;\n"
    "uniform float uGlintSpec, uGlintPaint, uGlintShadow, uGlintThick, uGlintWall, uGlintNear;\n"
    "uniform int   uGlintSteps;\n"
    /* per light: where its highlight and shadow come from (the light raised to the heart
       of its flame, FxLight.lift), and in w the radius of its flame, negative when it
       casts a shadow this frame and 0 when it puts no highlight at all */
    "uniform vec4  uGlintPos[24];\n"
    "vec3 glint_decode(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }\n"
    "vec3 glint_encode(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2)); }\n"
    /* What the point-light loop in the light pass added for light i: kept in step with
       that loop, so a shadow takes back exactly the light that was given. */
    "vec3 glint_point_diffuse(int i, vec3 n, vec3 L, float att) {\n"
    "    return uLightCol[i].rgb * uLightCol[i].w * att * (0.35 + 0.65 * max(0.0, dot(n, L)));\n"
    "}\n"
    /* One light's highlight, before its colour and N.L included: a GGX lobe with Smith
       visibility and a Schlick Fresnel from f0, the relight's own form (fx_post.h).
       A FLASH IS NOT A POINT. A fireball half a cell across reflects in a hull as a blob of
       that size, not as a pinprick, so the light is a sphere of radius r: the lobe is
       taken toward the point of the sphere nearest the reflected view ray, and widened by
       the angle the sphere subtends, with its peak lowered by the same ratio so the energy
       stays what a point would give (the representative-point sphere light). Lv runs
       from the surface to the sphere's centre. */
    "float glint_lobe(vec3 n, vec3 V, vec3 Lv, float r, float rough, float f0) {\n"
    "    float d   = max(length(Lv), 1e-4);\n"
    "    vec3  R   = reflect(-V, n);\n"
    "    vec3  c2r = dot(Lv, R) * R - Lv;\n"
    /* (both directions are divided by a floored length rather than normalised: where
       the reflected ray runs through the sphere's centre, or the light sits exactly
       behind the eye, a zero vector must give a dark pixel, not a NaN a float target
       would carry into the bloom) */
    "    vec3  Lr  = Lv + c2r * clamp(r / max(length(c2r), 1e-4), 0.0, 1.0);\n"
    "    vec3  L   = Lr / max(length(Lr), 1e-5);\n"
    "    vec3  Hh  = L + V;\n"
    "    vec3  H   = Hh / max(length(Hh), 1e-5);\n"
    "    float NoL = max(dot(n, L), 0.0);\n"
    "    float NoH = max(dot(n, H), 0.0);\n"
    "    float NoV = max(dot(n, V), 1e-4);\n"
    "    float VoH = max(dot(V, H), 0.0);\n"
    "    float a   = max(rough * rough, 1e-3);\n"
    "    float aw  = min(a + r / (2.0 * d), 1.0);\n"
    "    float a2  = aw * aw;\n"
    "    float dd  = NoH * NoH * (a2 - 1.0) + 1.0;\n"
    "    float D   = a2 / max(3.14159265 * dd * dd, 1e-7) * (a / aw) * (a / aw);\n"
    "    float gv  = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);\n"
    "    float gl  = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);\n"
    "    float Vis = 0.5 / max(gv + gl, 1e-6);\n"
    "    float F   = f0 + (1.0 - f0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);\n"
    "    return F * D * Vis * NoL;\n"
    "}\n"
    /* THE BLAST'S SHADOW: 1 where the way to the blast is clear, 0 where something stands
       in it. From a little way off the surface toward the blast, stopping uGlintNear short
       of it, through the depth buffer. A step that lands e behind the surface the camera
       sees there, with that thing taken as T deep (uGlintThick for a hull, a soldier or
       wreckage, uGlintWall for the rest), is inside it, and blocks the cone to the
       fireball by min(e, T - e) against the cone's half width w at that step: a step at
       either face blocks nothing, one a cone's width inside blocks it all, and a blocker
       thinner than the cone is wide never blocks it all. The class is read at the texel's
       centre, as the contact march reads it: the target is linearly filtered and a blend
       between two classes would pass through a third. The taps are inside a branch that
       is not uniform across a quad, which is legal here for the reason the headlights'
       march gives: the depth and normal targets have no mipmaps, so level zero is read
       whatever the implicit derivative says. */
    "float glint_blast_vis(vec3 P, vec3 n, vec3 L, float d, float r, float jit) {\n"
    "    vec3  sp    = P + n * 0.04 + L * 0.08;\n"
    "    float reach = d - uGlintNear - 0.08;\n"
    "    if (reach <= 0.0) return 1.0;\n"
    "    float occ = 0.0;\n"
    "    for (int s = 0; s < 16; s++) {\n"
    "        if (s >= uGlintSteps) break;\n"
    "        float t  = (float(s) + jit) / float(uGlintSteps);\n"
    "        vec3  wp = sp + L * (t * reach);\n"
    "        vec4  cp = uVP * vec4(wp, 1.0);\n"
    "        if (cp.w <= 0.0) continue;\n"
    "        vec2  su = (cp.xy / cp.w) * 0.5 + 0.5;\n"
    "        if (su.x < 0.0 || su.x > 1.0 || su.y < 0.0 || su.y > 1.0) continue;\n"
    "        vec3  sw = fx_world(su, min(texture2D(uDepth, su).r, 0.999989));\n"
    "        float e  = length(wp - uCamPos) - length(sw - uCamPos);\n"
    "        if (e <= 0.0005 || e >= max(uGlintWall, uGlintThick)) continue;\n"
    "        float T = uGlintWall;\n"
    "        if (uGNormOn == 1) {\n"
    "            float gc = texture2D(uGNorm, (floor(su * uGNormSize) + 0.5) / uGNormSize).a;\n"
    "            if ((gc > 0.08 && gc < 0.10) || (gc > 0.20 && gc < 0.24) || (gc > 0.26 && gc < 0.36))\n"
    "                T = uGlintThick;\n"
    "        }\n"
    "        if (e >= T) continue;\n"
    "        float w = max(r * (0.08 + t * reach) / d, 0.02);\n"
    "        occ = max(occ, clamp(min(e, T - e) / w, 0.0, 1.0));\n"
    "        if (occ >= 1.0) break;\n"
    "    }\n"
    "    return 1.0 - occ;\n"
    "}\n";

/* WHAT A GLOSSY SURFACE MIRRORS IS THE FLAME, and the flame is far brighter than its
   light's power says. A point light's power is set for the warm spill it adds to the
   colour of whatever it falls on, which is a small fraction of white; the flame itself is
   drawn near full white. The highlight is the flame seen in the surface, so it is raised
   by this factor toward the brightness the flame is drawn at, and light_spec_strength 1
   is that. Measured on a burning refinery and a destroyed tank: at a quarter of this a
   glint never cleared the grain of the hull's own paint. */
#define GLINT_GAIN 16.0f

/* THE SAME GLINT IN LINEAR LIGHT. With the light pass in linear (linear_on), the flame's
   colour is decoded before it is reflected, so the highlight shows the flame's own hue,
   and the film curve rolls off what passes white where the eight bit target clipped it.
   Both take brightness out of the highlight: at GLINT_GAIN alone the glints on a
   destroyed tank, the Barracks and the silos beside it summed to about four fifths of
   the light they add with the pass in display colour. This factor, measured over those
   blast moments, brings the sum back to within two percent of it, so light_spec_strength
   means the same in both. The blast shadows take back the point lights' own light,
   which is unchanged, and need nothing. */
#define GLINT_LIN_GAIN 1.3f

/* The light pass's uniforms for this block, set beside the point lights' own. nl is the
   number of point lights the pass was handed this frame, linLive whether the pass works
   in linear light this frame. */
static void glint_light_uniforms(GLuint prog, int nl, int linLive)
{
    const int on = (g_fx.light_spec && nl > 0) ? 1 : 0;
    fx_set1i(prog, "uGlintOn", on);
    if (!on) return;
    fx_set1f(prog, "uGlintSpec", g_fx.light_spec_strength * GLINT_GAIN
                                 * (linLive ? GLINT_LIN_GAIN : 1.0f));
    fx_set1f(prog, "uGlintPaint", g_fx.light_spec_paint);
    /* The casters: the first explosions in the list, which is ranked brightest first. */
    int want = (int)(g_fx.blast_shadows + 0.5f);
    if (want < 0) want = 0;
    if (want > 4) want = 4;
    float gp[FX_MAX_LIGHTS * 4];
    int nc = 0;
    for (int i = 0; i < nl; i++) {
        const FxLight& L = g_fxLight[i];
        const int casts = L.blast && nc < want && g_fx.blast_shadow_strength > 0.0f;
        nc += casts;
        /* the flame's radius, kept off zero for a caster so its sign survives */
        float fr = L.flame > 0.0f ? L.flame : 0.0f;
        if (casts && fr < 0.01f) fr = 0.01f;
        gp[i*4+0] = L.x; gp[i*4+1] = L.y + L.lift; gp[i*4+2] = L.z;
        gp[i*4+3] = casts ? -fr : fr;
    }
    fx_set4fv(prog, "uGlintPos", nl, gp);
    fx_set1f(prog, "uGlintShadow", g_fx.blast_shadow_strength);
    /* TWELVE STEPS by default over a blast's reach (the light radius, about three cells)
       is a step a quarter of a cell long, finer than a hull. The march stops a cell short
       of the fireball's heart by default, which sits three quarters of a cell over the
       blast (FxLight.lift): the wreck or the wall a shell lands on is inside the fireball
       and does not shadow it, while a tank or a building a cell away does. Measured on a
       destroyed Medium Tank: stopping half a cell short, the wreck's own pieces put the
       ground round it almost wholly in shadow; a cell short, the tank beside it throws a
       clear shadow and the pieces short ones. A building is taken as half a cell deep
       behind its visible face by default, deeper than a headlight's 0.35 allows for, so a
       step inside it still finds it; a hull or a piece of wreck as a fifth of a cell,
       about a tank's height. The march is offset per pixel by up to a whole step, turning
       the banding of a dozen fixed steps into a fine grain. */
    int steps = (int)(g_fx.blast_shadow_steps + 0.5f);
    if (steps < 1) steps = 1;
    if (steps > 16) steps = 16;
    fx_set1i(prog, "uGlintSteps", steps);
    fx_set1f(prog, "uGlintNear", g_fx.blast_shadow_near);
    fx_set1f(prog, "uGlintThick", g_fx.blast_shadow_thick);
    fx_set1f(prog, "uGlintWall", g_fx.blast_shadow_wall);
}

#endif

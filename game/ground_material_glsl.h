/* ====================================================================================
 *  ground_material_glsl.h -- GROUND MATERIALS: what open ground is made of, in the light
 *  pass. Snow, wet shore, dry sand and grass stop answering the sun as one flat matte
 *  surface and each answers it the way that material does.
 *
 *  WHERE IT SITS. One function, called from the light pass's terrain branch (fx_post.h,
 *  FX_FS_LIGHT_BODY) in place of the plain "colour times the ground's light" step, under
 *  unified daylight, on every ground-class pixel. The generated rock and gravel and the
 *  stone materials arrive with a STONE weight: they keep their own dielectric response
 *  (added after this step by the caller) and take no sand, grass, wet or glint term,
 *  but on a snow map they take the snow's sky exactly as the ground beside them does.
 *  Snow is painted across both classes, and a shade that stopped at the class edge
 *  would draw the class map onto the snow as a stepped seam. The approved terrain
 *  TEXTURES are not touched: every term here is a shading term on top of them. With the
 *  master dial off the call is not made and the pass draws exactly what it drew before
 *  this file existed.
 *
 *  HOW A PIXEL KNOWS WHAT IT IS. The normal target says "ground" and nothing finer, and
 *  the authored grass masks are per tile and live in the grass bake, not here. So the
 *  material is a WEIGHT, built from two things the pass does know:
 *    - the theater (uGmTheater): snow only exists on snow maps, sand on desert maps,
 *      grass on temperate and winter maps. Red Alert's interior has none of them.
 *    - the ground's own colour, read before any light reaches it: snow is bright and
 *      nearly colourless, sand is warm, grass is green. Soft thresholds, so a road or a
 *      concrete pad on a snow map fades out of the snow weight rather than flipping.
 *  A slope limit keeps cliff faces out of sand and grass, and a softer one out of snow.
 *  The wet shore is not a colour at all: it is a band of the sea's own shore field.
 *
 *  THE MATERIALS, each with its own strength dial (fx_state.h, GROUND MATERIALS):
 *    SNOW   wrapped diffuse, normalised so level ground in full sun keeps exactly its
 *           brightness: the terminator on a hillside softens the way it does on a
 *           scattering surface, sunward slopes blow out less and slopes turned away keep
 *           some light. The sky lights it COOL and the sun is warmed by exactly the
 *           amount that cancels that on level sunlit ground, so snow in the sun stays
 *           white and snow in shade goes blue, at the same luminance it had. A sparse
 *           sparkle of facet glints, fixed to the ground, only in direct sun.
 *    WET    a band inland of the shore wash on soil: the ground darkens the way a wet
 *           porous surface does, and takes a low-roughness sun highlight and the sky's
 *           Fresnel reflection, both held back where the painting is dark (a crevice
 *           sees no sky), fading out inland along a noisy edge.
 *    SAND   a rough-diffuse (Oren-Nayar) response, again normalised to level ground, so
 *           dune faces light more evenly and back-scatter toward the sun; and rare grain
 *           glints in the sand's own colour, a brighter grain rather than a white speck.
 *    GRASS  a soft cloth-like sheen (the Charlie distribution) on the ground's smooth
 *           normal, and a little light through the blades when the view faces into the
 *           sun, in the blades' own colour.
 *
 *  COLOUR SPACE. Every term is worked out in linear light: a material's change is a
 *  ratio against the plain light, everything ADDED (glints, highlights, sheen, light
 *  through the blades) is radiance, and the wet darkening acts on the decoded albedo.
 *  The colour MASKS that say what a pixel is made of were tuned on gamma-encoded
 *  colour, so they always read it that way. What differs is only the pass around it:
 *    - linear light on (uLinOn 1, docs/modern-lighting/linear.md): the pass hands in
 *      linear radiance and takes linear radiance back. The caller's own light keeps its
 *      conversion from the tuned value (fx_lin_gain), the material ratio multiplies it
 *      as it is, and the added terms are added as they are, with no clip: a glint that
 *      passes white is rolled off by the film curve at the end.
 *    - linear light off: the pass lights the gamma-encoded colour by multiplying it, so
 *      the ratio is applied as its 1/2.2 power and the added terms go through a decode
 *      and an encode (gm_dec, gm_enc). That is the same change seen in linear, clipped
 *      at white by the eight bit target as everything else in that chain is.
 *
 *  DETERMINISM. No clock. The only noise is a hash of world position, so two runs of
 *  one script draw one picture.
 * ==================================================================================== */
#ifndef OPENCNC3D_GROUND_MATERIAL_GLSL_H
#define OPENCNC3D_GROUND_MATERIAL_GLSL_H

static const char* GM_GLSL =
    /* uGmTheater: x snow, y desert sand, z grass; 1 where this theater's open ground
       can be that material. uGmStrength: x snow, y wet shore, z dry sand, w grass.
       uGmSparkle: the snow's glints. uGmWet: x the wet band's width in shore-field
       units, y 1 when the shore field is bound this frame and the ground meets the water
       itself, z the field's value at the swash's inland reach, w half the fade across
       that edge. uGmSnowSky: the cool tint the sky lends snow, luminance 1. */
    "uniform int   uGmOn;\n"
    "uniform vec3  uGmTheater;\n"
    "uniform vec4  uGmStrength;\n"
    "uniform float uGmSparkle;\n"
    "uniform vec4  uGmWet;\n"
    "uniform vec3  uGmSnowSky;\n"
    "uniform int   uGmView;\n"
    "vec3 gm_dec(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }\n"
    "vec3 gm_enc(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2)); }\n"
    /* Sine-free hashes: a sine of a large argument differs between drivers, and these
       are fed whole-number cell coordinates that grow with the map. */
    "float gm_hash(vec2 p) {\n"
    "    vec3 q = fract(vec3(p.xyx) * 0.1031);\n"
    "    q += dot(q, q.yzx + 33.33);\n"
    "    return fract((q.x + q.y) * q.z);\n"
    "}\n"
    "vec2 gm_hash2(vec2 p) {\n"
    "    vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));\n"
    "    q += dot(q, q.yzx + 33.33);\n"
    "    return fract((q.xx + q.yz) * q.zy);\n"
    "}\n"
    "float gm_vnoise(vec2 p) {\n"
    "    vec2 i = floor(p), f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    return mix(mix(gm_hash(i), gm_hash(i + vec2(1.0, 0.0)), f.x),\n"
    "               mix(gm_hash(i + vec2(0.0, 1.0)), gm_hash(i + vec2(1.0, 1.0)), f.x), f.y);\n"
    "}\n"
    /* THE GLINTS' FRAME: the camera's own axis (toward the eye) in xyz and, in w, the
       size of the world-space cell a glint lives in. Both are the same for every pixel
       of a frame and neither changes while the view scrolls, because this camera only
       translates: the axis is the ray through the middle of the screen, and the cell is
       four times the footprint of one pixel at the ground plane on that ray, rounded up
       to a power of two, so it changes only when the zoom crosses a step. Taking the
       glint's eye direction from each pixel's own ray instead would be the exact answer,
       and it re-deals almost every glint after a one-cell scroll: the sparkle would
       twinkle like noise whenever the view moves, which is shimmer and not snow. The
       cell is four pixels and not two because the position this pass rebuilds from
       depth wanders by a pixel or so between two views of the same ground; with dots
       spaced that far apart a wander moves a dot rather than dealing a new one.
       Four pixels at the 900 lines the glints were tuned at (gm_res): a taller picture
       of the same view, a Retina window or a supersampled one, keeps the same cell on
       the ground and so the same number of glints, where counting in its own pixels it
       dealt about four times as many at twice the height and turned the snow to specks. */
    "float gm_res() { return max(uDepthSize.y / 900.0, 1.0); }\n"
    "vec4 gm_frame() {\n"
    "    vec3 c0 = fx_world(vec2(0.5), 0.0), c1 = fx_world(vec2(0.5), 0.5);\n"
    "    vec3 cx = fx_world(vec2(0.5 + 1.0 / max(uDepthSize.x, 1.0), 0.5), 0.5);\n"
    "    vec3 ax = normalize(c0 - c1);\n"
    "    float ang = length(cx - c1) / max(length(c1 - uCamPos), 1e-4);\n"
    "    float t = max(uCamPos.y, 1e-3) / max(ax.y, 0.2);\n"
    "    float reach0 = t * ang / max(ax.y, 0.2);\n"
    "    return vec4(ax, exp2(ceil(log2(max(reach0 * 4.0 * gm_res(), 1e-5)))));\n"
    "}\n"
    /* ONE FACET GLINT. The ground is cut into world-space cells of the frame's cell size.
       Each cell holds one small mirror at a hashed spot, turned at a hashed angle
       anywhere up to about eighty degrees off the surface, as loose crystals are; it
       flashes when its tilt sends the sun along the camera's axis, so which cells glint
       depends on where the sun is and on the camera's angle, as it does on real snow,
       and about as many glint with the sun behind the eye as with it in front. The spot
       is a soft dot about a pixel wide, measured in this pixel's own footprint (reach,
       taken at the tuned 900 lines like the cell, so a taller picture draws the same dot
       over more of its pixels), so a dot that falls between two pixel centres lights
       both a little instead of vanishing. cone is the cosine the facet's normal must reach toward the half
       vector: closer to 1 is rarer. */
    "float gm_glint(vec2 xz, vec3 n, vec3 H, float cell, float reach, float cone) {\n"
    "    float r = max(reach, 1e-5);\n"
    "    vec2 g = xz / cell;\n"
    "    vec2 id = floor(g);\n"
    "    vec2 c = id + 0.2 + 0.6 * gm_hash2(id);\n"
    "    float px = length(g - c) * cell / r;\n"
    "    float spot = 1.0 - smoothstep(0.35, 1.10, px);\n"
    "    vec2 t = gm_hash2(id + vec2(17.0, 59.0)) * 2.0 - 1.0;\n"
    "    float h = 0.15 + 0.85 * gm_hash(id + vec2(41.0, 7.0));\n"
    "    vec3 T = normalize(cross(n, vec3(0.0, 0.0, 1.0)) + vec3(1e-4, 0.0, 0.0));\n"
    "    vec3 B = cross(T, n);\n"
    "    vec3 m = normalize(n * h + t.x * T + t.y * B);\n"
    "    return spot * smoothstep(cone, mix(cone, 1.0, 0.35), dot(m, H));\n"
    "}\n"
    /* Oren-Nayar's rough diffuse, the qualitative form: s2 is the facet slope variance. */
    "float gm_oren(vec3 n, vec3 L, vec3 V, float s2) {\n"
    "    float NoL = max(dot(n, L), 0.0), NoV = max(dot(n, V), 0.0);\n"
    "    float A = 1.0 - 0.5 * s2 / (s2 + 0.33), B = 0.45 * s2 / (s2 + 0.09);\n"
    "    vec3 lp = L - n * dot(n, L), vp = V - n * dot(n, V);\n"
    "    float cphi = max(dot(lp, vp) / max(length(lp) * length(vp), 1e-4), 0.0);\n"
    "    float cA = min(NoL, NoV), cB = max(NoL, NoV);\n"
    "    float sinA = sqrt(max(1.0 - cA * cA, 0.0));\n"
    "    float tanB = sqrt(max(1.0 - cB * cB, 0.0)) / max(cB, 1e-3);\n"
    "    return NoL * (A + B * cphi * sinA * tanB);\n"
    "}\n"
    /* THE MATERIAL STEP. albIn is the ground's colour before any light (the pass's own
       albedo: gamma encoded, or linear radiance under linear light); slopeLight is the
       light the caller would have multiplied it by, after its pale-ground rolloff; geo
       is how far the normal rebuilt from depth agrees with the surface's own (1 on clean
       ground, low on a depth seam, where the pixel footprint and the art's relief are
       both meaningless); stone is 1 on the generated rock and gravel and the stone
       materials. Returns the lit colour, in the space it was handed in.

       The caller's light is rebuilt here term by term (sky, sun, second sun) so each
       material can change the term it changes. A material's change is a RATIO against
       that plain light, worked out in linear light; the caller's light, rolloff
       included, stays as it was underneath. Without linear light the pass multiplies
       gamma-encoded colour, so a linear ratio r is applied to it as r^(1/2.2), which is
       the same change seen in linear. alb is always the gamma-encoded colour, for the
       masks, and albLin always the linear one, for the light. */
    "vec3 gm_ground(vec3 albIn, float slopeLight, vec3 P, vec3 n, vec3 relG,\n"
    "               float sh, float cl, float reach, float geo, float stone) {\n"
    "    vec3 alb = fx_lin_encode(albIn);\n"
    "    vec3 lit = albIn * fx_lin_gain(slopeLight);\n"
    "    float lum = dot(alb, vec3(0.2126, 0.7152, 0.0722));\n"
    "    float mx = max(alb.r, max(alb.g, alb.b)), mn = min(alb.r, min(alb.g, alb.b));\n"
    "    float sat = (mx - mn) / max(mx, 1e-3);\n"
    "    float up = n.y;\n"
    "    float soft = 1.0 - clamp(stone, 0.0, 1.0);\n"
    "    float rl = length(relG);\n"
    /* the masks, 0 to 1, before any strength dial */
    "    float mSnow = uGmTheater.x * smoothstep(0.28, 0.48, lum)\n"
    "                * (1.0 - smoothstep(0.12, 0.30, sat)) * smoothstep(0.25, 0.50, up);\n"
    "    float warm = smoothstep(0.03, 0.12, (alb.r - alb.b) / max(mx, 1e-3));\n"
    "    float mSand = soft * uGmTheater.y * warm * smoothstep(0.15, 0.35, lum) * smoothstep(0.55, 0.80, up);\n"
    "    float green = (alb.g - max(alb.r, alb.b)) / max(mx, 1e-3);\n"
    "    float mGrass = soft * uGmTheater.z * smoothstep(0.02, 0.10, green) * smoothstep(0.55, 0.80, up);\n"
    "    float mWet = 0.0;\n"
    "    if (uGmWet.y > 0.5 && uTerrainWaterOn == 1 && uGmStrength.y > 0.0) {\n"
    "        float shore = texture2D(uTerrainWater, P.xz * uTerrainWaterScale).r;\n"
    /* Inland of the swash's reach (uGmWet.z), in shore-field units; a slow world-space
       noise moves the band's inland edge so it is a tide line and not a contour. Faded
       out under the swash itself, which draws its own water and must not be darkened
       twice. Only on SOIL: warm, level, mid to dark. Snow by a river would go to grey
       slush and read as a fault, grass does not darken like a porous bank, and pale
       ground is snow, a road or the lit face of a stone. The shaded faces of the boulders
       painted along a bank cannot be told from soil by colour or by the painting's
       relief (grass carries as much relief as a boulder at this scale), so they take the
       darkening, as stones at a waterline are wet too; the reflections are held back on
       dark paint below, which is what had turned them grey-blue. */
    "        float inland = uGmWet.z - shore;\n"
    "        float wobble = (gm_vnoise(P.xz * 1.7) - 0.5) * 0.7 * uGmWet.x;\n"
    "        float band = 1.0 - smoothstep(0.0, uGmWet.x, inland + wobble);\n"
    "        float wash = smoothstep(-uGmWet.w, uGmWet.w, -inland);\n"
    "        float soil = 1.0 - smoothstep(0.34, 0.46, lum);\n"
    "        mWet = soft * band * (1.0 - wash) * (1.0 - mSnow) * (1.0 - mGrass) * warm * soil\n"
    "             * smoothstep(0.70, 0.90, up);\n"
    "    }\n"
    /* The tuning views paint display colours, decoded under linear light so they read
       the same on screen either way. */
    "    if (uGmView == 2 && uTerrainWaterOn == 1) {\n"
    "        float r = texture2D(uTerrainWater, P.xz * uTerrainWaterScale).r;\n"
    "        return fx_lin_decode(vec3(clamp((r - 0.4) * 5.0, 0.0, 1.0), r > uTerrainDryEdge ? 1.0 : 0.0, r > 0.5 ? 1.0 : 0.0));\n"
    "    }\n"
    "    if (uGmView == 3) return fx_lin_decode(vec3(clamp(rl * geo / 3.0, 0.0, 1.0), lum, stone));\n"
    "    if (uGmView == 1)\n"
    "        return fx_lin_decode(mix(vec3(lum * 0.35), vec3(1.0, 0.0, 1.0), mWet) + vec3(0.0, 0.0, 1.0) * mSnow\n"
    "             + vec3(1.0, 0.5, 0.0) * mSand + vec3(0.0, 1.0, 0.0) * mGrass);\n"
    "    if (uGmTheater.x * uGmStrength.x + mSand * uGmStrength.z + mGrass * uGmStrength.w\n"
    "        + mWet * uGmStrength.y < 1e-3) return lit;\n"
    "\n"
    "    vec3 L = -uSunDir, V = normalize(uCamPos - P);\n"
    "    float NoL = dot(n, L), NoV = max(dot(n, V), 0.05);\n"
    "    float vis = 1.0 - max(sh * uSStrength, cl * uCStrength);\n"
    /* What a glint or a highlight needs is the sun's disc itself, so its gate is the
       shadow and the cloud as they are, not the strength dials: those set how dark a
       shadow PAINTS (the fill the sky and the bounce leave in it), and a shadow painted
       at 85 percent still has the whole sun blocked. */
    "    float sunDisc = smoothstep(0.55, 0.90, 1.0 - max(sh, cl)) * smoothstep(0.0, 0.15, NoL);\n"
    "    float sky = uTerrainAmbient * uSkyEnergy * (0.65 + 0.35 * max(up, 0.0));\n"
    "    float sun = uSunEnergy * max(NoL, 0.0) * vis;\n"
    "    float second = uSecondSunOn > 0.5 ? uSecondSunEnergy * max(dot(n, -uSecondSunDir), 0.0) : 0.0;\n"
    "    float base = max(sky + sun + second, 1e-4);\n"
    "    float flatL = max(-uSunDir.y, 0.05);\n"
    "    vec3 light = vec3(base);\n"
    "    vec3 add = vec3(0.0);\n"
    "    vec3 albLin = uLinOn == 1 ? albIn : gm_dec(alb);\n"
    "    float gReach = reach * gm_res();\n"
    "    float glintOk = soft * geo * (1.0 - smoothstep(0.08, 0.16, gReach)) * sunDisc;\n"
    "    vec4 fr = vec4(0.0);\n"
    "    vec3 H0 = vec3(0.0, 1.0, 0.0);\n"
    "    if (glintOk > 0.0 && (mSnow * uGmSparkle > 0.0 || mSand * uGmStrength.z > 0.0)) {\n"
    "        fr = gm_frame();\n"
    "        H0 = normalize(L + fr.xyz);\n"
    "    }\n"
    /* SNOW. The sky's colour is the sky's, so on a snow map it is cool on all the open
       ground: the dirt, the roads and the rock as well as the snow, since a mask edge
       between snow that went blue in shade and a patch beside it that did not would draw
       a seam. sky0 and sun0 are the two terms on level ground in full sun, and the sun is
       warmed by (1 - cool) x sky0 / sun0, which makes their sum there exactly neutral:
       white in the sun, blue where only the sky reaches, at the luminance it had.
       The wrap is the snow's own, normalised against level ground, so it bends the
       terminator across a hillside without moving the brightness of a flat field. */
    "    if (uGmTheater.x > 0.0 && uGmStrength.x > 0.0) {\n"
    "        float s = uGmStrength.x;\n"
    "        float wrap = 0.25 * s * mSnow;\n"
    "        float ws = max((NoL + wrap) / (1.0 + wrap), 0.0) * flatL / ((flatL + wrap) / (1.0 + wrap));\n"
    "        vec3 cool = max(mix(vec3(1.0), uGmSnowSky, s * uGmTheater.x), vec3(0.0));\n"
    "        float sky0 = uTerrainAmbient * uSkyEnergy, sun0 = uSunEnergy * flatL;\n"
    "        vec3 warmSun = max(vec3(1.0) + (vec3(1.0) - cool) * sky0 / max(sun0, 0.05), vec3(0.0));\n"
    "        light = sky * cool + uSunEnergy * ws * vis * warmSun + second;\n"
    "        if (fr.w > 0.0 && mSnow * uGmSparkle > 0.0)\n"
    "            add += vec3(0.9, 0.95, 1.0) * (2.0 * uGmSparkle * mSnow * uSunEnergy * glintOk)\n"
    "                 * gm_glint(P.xz, n, H0, fr.w, gReach, 0.978);\n"
    "    }\n"
    /* DRY SAND, the rough diffuse in place of the plain one, normalised to level ground
       under the same eye so a flat pan keeps its brightness. Its glints are a quartz
       grain catching the sun at the size of a pixel: added in the sand's own colour, so
       a glinting grain reads as a brighter grain of the same sand and never as a white
       speck, and rarer than the snow's. */
    "    if (mSand > 0.0 && uGmStrength.z > 0.0) {\n"
    "        float s = uGmStrength.z;\n"
    "        float s2 = 0.30 * s * s;\n"
    "        float on = gm_oren(n, L, V, s2) * flatL / max(gm_oren(vec3(0.0, 1.0, 0.0), L, V, s2), 1e-3);\n"
    "        light = mix(light, vec3(sky + uSunEnergy * on * vis + second), mSand);\n"
    "        if (fr.w > 0.0)\n"
    "            add += albLin * (1.0 * s * mSand * uSunEnergy * glintOk)\n"
    "                 * gm_glint(P.xz, n, H0, fr.w, gReach, 0.996);\n"
    "    }\n"
    "    vec3 albMod = albLin;\n"
    /* WET SHORE. Water filling the pores darkens a porous surface and deepens its colour:
       a power on the decoded albedo and a gain, as the rain does to the whole map. The
       film on top is smooth water, F0 0.02, on the art's own relief (bounded: at a
       painted edge the gradient is a cliff, and an unbounded tilt fires every grazing
       term at once) so the highlight breaks up the way wet grains do rather than lying
       on the ground as one disc. It mirrors the sky at its Fresnel weight, F times the
       same sky the diffuse is lit by, which from this camera's height is a few percent.
       Both reflections are held back where the painting is dark: the art has its
       crevices and the shaded sides of its stones painted in, and those see neither the
       sky nor the sun, so lifting them to the sky's blue-grey is exactly wrong. */
    "    if (mWet > 0.0 && uGmStrength.y > 0.0) {\n"
    "        float k = min(mWet * uGmStrength.y, 1.5);\n"
    "        albMod = mix(albMod, pow(albMod, vec3(1.3)) * 0.82, clamp(0.75 * k, 0.0, 1.0));\n"
    "        vec3 rel = (rl > 0.5 ? relG * (0.5 / rl) : relG) * geo;\n"
    "        vec3 nw = normalize(n - rel * 0.35);\n"
    "        float wNoL = max(dot(nw, L), 0.0), wNoV = max(dot(nw, V), 0.05), NoH = max(dot(nw, normalize(L + V)), 0.0);\n"
    "        float a = 0.22 * 0.22, a2 = a * a;\n"
    "        float den = NoH * NoH * (a2 - 1.0) + 1.0;\n"
    "        float D = a2 / max(3.14159265 * den * den, 1e-5);\n"
    "        float kg = 0.5 * a;\n"
    "        float G = (wNoL / (wNoL * (1.0 - kg) + kg)) * (wNoV / (wNoV * (1.0 - kg) + kg));\n"
    "        float F = 0.02 + 0.98 * pow(clamp(1.0 - dot(V, normalize(L + V)), 0.0, 1.0), 5.0);\n"
    "        float occ = smoothstep(0.015, 0.08, dot(albLin, vec3(0.2126, 0.7152, 0.0722)));\n"
    "        float sunDiscW = smoothstep(0.55, 0.90, 1.0 - max(sh, cl));\n"
    "        float spec = min(F * D * G / max(4.0 * wNoV, 0.05), 2.0) * uSunEnergy * sunDiscW;\n"
    "        float Fv = 0.02 + 0.98 * pow(clamp(1.0 - wNoV, 0.0, 1.0), 5.0);\n"
    "        vec3 skyRefl = vec3(0.80, 0.90, 1.0) * (Fv * sky);\n"
    "        add += (vec3(spec) + skyRefl) * (k * occ);\n"
    "    }\n"
    /* GRASS. The sheen is the cloth distribution, broad, on the ground's smooth normal:
       a field is a velvet of blades, and what it throws back at a grazing sun is the
       waxy surface of the blades, in their own colour lightened. On the smooth normal
       and not on the painting's relief, because relief-bent sheen is per-pixel grain on
       a field the painting already textures. It is normalised the way the snow and the
       sand are: the same lobe on level ground is taken off, because the painting already
       holds a level field's brightness, so a flat meadow keeps exactly the brightness it
       had and the sheen shows only as hillsides turn their blades toward or away from
       the light. The light through the blades is there only as the view turns into the
       sun, and is the blades' colour deepened toward yellow-green, which is what light
       that has passed through a leaf is. From this camera, with the sun behind it, both
       are slight: grass seen from above with the sun at one's back is close to matte,
       and this does not pretend otherwise. */
    "    if (mGrass > 0.0 && uGmStrength.w > 0.0) {\n"
    "        float s = uGmStrength.w * mGrass;\n"
    "        float inv = 1.0 / 0.45;\n"
    "        vec3 H = normalize(L + V);\n"
    "        float fNoL = max(NoL, 0.0), fNoH = clamp(dot(n, H), 0.0, 1.0);\n"
    "        float sheen = (2.0 + inv) * pow(max(1.0 - fNoH * fNoH, 0.0), 0.5 * inv) / 6.2831853\n"
    "                    / max(4.0 * (fNoL + NoV - fNoL * NoV), 0.2) * fNoL;\n"
    "        float lNoL = max(L.y, 0.0), lNoV = max(V.y, 0.05), lNoH = clamp(H.y, 0.0, 1.0);\n"
    "        sheen -= (2.0 + inv) * pow(max(1.0 - lNoH * lNoH, 0.0), 0.5 * inv) / 6.2831853\n"
    "               / max(4.0 * (lNoL + lNoV - lNoL * lNoV), 0.2) * lNoL;\n"
    "        float gl = dot(albLin, vec3(0.2126, 0.7152, 0.0722));\n"
    "        vec3 sheenCol = mix(albLin, vec3(gl), 0.3) * 4.0;\n"
    "        float back = pow(clamp(-dot(V, L), 0.0, 1.0), 2.0);\n"
    "        vec3 through = albLin * vec3(0.9, 1.25, 0.45) * (back * 0.30);\n"
    "        add += (sheenCol * sheen + through * smoothstep(0.0, 0.2, NoL))\n"
    "               * (s * uSunEnergy * vis);\n"
    "    }\n"
    /* Under linear light the step is the plain product and sum, in radiance; the caller's
       light keeps the conversion it has everywhere else in the pass. */
    "    if (uLinOn == 1)\n"
    "        return max(albMod * fx_lin_gain(slopeLight) * (light / base) + add, vec3(0.0));\n"
    "    vec3 outc = gm_enc(albMod) * slopeLight * pow(light / base, vec3(1.0 / 2.2));\n"
    "    if (add != vec3(0.0)) outc = gm_enc(max(gm_dec(outc) + add, vec3(0.0)));\n"
    "    return outc;\n"
    "}\n";

#endif

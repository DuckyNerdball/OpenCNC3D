/* ====================================================================================
 *  pbr_glsl.h -- one material model for the buildings and vehicles the light pass relights.
 *
 *  WHAT THIS IS. The shader text the light pass (fx_post.h) uses to light a remastered
 *  building or vehicle the way the 3D soldiers are lit (model3d_mod.h, M3_FS): a Cook-
 *  Torrance sun lobe (GGX distribution, Smith height-correlated visibility, Schlick
 *  Fresnel), sky light from the nine spherical-harmonic coefficients of the sky pack for
 *  the diffuse half, and the sky pack's prefiltered cube with the split-sum environment
 *  BRDF for the reflection. The host side (uniforms, the cube's texture unit) is
 *  pbr_mod.h. Before this, a relit hull was one fixed roughness, Lambert, a white
 *  two-term hemisphere and the specular map's lobes added on top: every surface read as
 *  the same painted plastic, and a metal had nothing to reflect.
 *
 *  THE MATERIAL IS DERIVED, NOT AUTHORED, and that is the one compromise here. No hull
 *  carries a roughness or a metalness map. What exists per texel is the specular target
 *  the remasters' .spec maps draw (evhc_draw_spec): red, how strongly the part reflects
 *  (metals 0.4 to 1, glass and lenses 1, paint 0.3 to 0.42, masonry and rubber under
 *  0.2), and green, how tight its highlight is (rubber 0.15, concrete 0.2, paint 0.45,
 *  steel 0.65, polished metal and glass above 0.9). pbr_material turns those into:
 *
 *    roughness  = mix(0.95, 0.15, green)                      times pbr_rough_scale
 *    paint's f0 = 0.04 x clamp(red / 0.3, 0.2, 1.5)          paint 0.04, concrete less
 *    metalness  = smoothstep(0.45, 0.85, red)                 a reflective part
 *               x smoothstep(0.555, 0.60, green)              of a metal's class
 *               x (1 - smoothstep(0.25, 0.55, saturation))    is grey, not house paint
 *               x smoothstep(0.015, 0.06, linear luminance)   and not black glass
 *                                                             times pbr_metal_scale
 *
 *  so steel and pipe become metal, house paint stays a dielectric whatever its
 *  strength (and on a steel part takes paint's tightness), plate, trim and galvanised
 *  sheet are lit as the painted surfaces they are drawn as, glass and lenses (dark)
 *  stay a smooth dielectric, and rubber and concrete go rough. A pixel no map covers
 *  (a model without a .spec, a cartridge mesh, a wall) keeps today's relight whole and
 *  takes only the sky's colour in its fill. When authored maps arrive they replace
 *  pbr_material and nothing else.
 *
 *  WHY THE BRIGHTNESS STAYS WHERE IT WAS. With linear light off (linear_on 0) the light
 *  pass is gamma encoded: the world pass multiplies a gamma texture by a gamma vertex
 *  shade and nothing decodes it. The relight's diffuse is therefore today's figure, the
 *  ratio of the old light to the baked shade applied to the encoded colour, and it is
 *  DECODED here (pbr_to_linear) so that the specular terms can be added in linear light,
 *  the way they are physically summed, and the sum encoded back (pbr_to_display). For a
 *  rough dielectric texel that round trip returns today's colour, so what moves is the
 *  material: metal loses its diffuse and takes a coloured reflection instead, a smooth
 *  surface mirrors the sky, a sun highlight follows the surface's roughness, and a
 *  little of the sky's colour replaces the white of the fill. A rough dielectric adds no
 *  sky reflection of its own (pbr_sheen_weight): its painted colour already holds it.
 *  The sun's and the sky's linear energies come from the same gamma-domain sums
 *  (pbr_irradiance) so they stay in proportion with that diffuse. pow(x, 2.2) and not
 *  the piecewise sRGB curve, because it is a product rule: decode(texture x shade) =
 *  decode(texture) x decode(shade), which is what lets the division by the shade stay
 *  exact on either side of it.
 *
 *  WITH LINEAR LIGHT ON (linear_on 1: the scene colour decoded before the relight, a
 *  float lit target, a film curve in the resolve) the same material runs in the light
 *  pass's own radiance, and four things change and nothing else. The diffuse is the
 *  decoded colour times the converted light over the decoded shade, the conversion the
 *  plain relight uses (fx_lin_gain), so no decode is needed; pbr_irradiance converts its
 *  energies with that same fx_lin_gain; the lobes are added to the radiance with no
 *  decode and no encode around them; and the display-encoded albedo the material, the
 *  cavity and a metal's reflectance are read from is the encoded colour over the shade,
 *  the value they were tuned on. At linear_light_k 2.2 every one of those is the gamma
 *  figure decoded, so the material keeps its character and strength and only the film
 *  curve, which rolls off what the eight-bit target used to clip, tells the two apart.
 * ==================================================================================== */
#ifndef OPENCNC3D_PBR_GLSL_H
#define OPENCNC3D_PBR_GLSL_H

/* The explicit-level cube fetch the roughness chain needs, as M3_FS takes it. Has to sit
   directly after the #version line of the light program (an #extension is not allowed
   after the first declaration on every compiler), so fx_post.h puts this string there.
   The #else keeps a driver without it drawing: the bias form lands near the right
   level on a surface that is not edge-on. */
static const char* PBR_GLSL_EXT =
    "#extension GL_ARB_shader_texture_lod : enable\n"
    "#ifdef GL_ARB_shader_texture_lod\n"
    "#define PBR_ENVLOD(s, d, l) textureCubeLod(s, d, l)\n"
    "#else\n"
    "#define PBR_ENVLOD(s, d, l) textureCube(s, d, l)\n"
    "#endif\n";

/* The uniforms and the helpers, after the light program's own uniforms (uSpecMap,
   uGNormSize, uReRough are read from there). */
static const char* PBR_GLSL =
    /* uPbrOn is 0 with the dial off or no bake target, and the whole model is then a
       uniform branch not taken. uPbrEnvOn is 0 without a sky pack: the fill keeps its
       white hemisphere and the reflection sees the same hemisphere. uPbrDial is the
       roughness scale, the metalness scale, the reflection strength and how much of
       the sky's hue the fill takes. uPbrEnvNorm is
       pi over the luminance of the sky's irradiance on an upward surface, which turns
       the cube's radiance into a fraction of what an upward surface receives, so the
       reflection is measured in the same units as the fill and the sky photograph's
       own exposure drops out (m3_env_scale makes the same choice for the soldiers). */
    "uniform int   uPbrOn, uPbrEnvOn;\n"
    "uniform samplerCube uPbrEnv;\n"
    "uniform float uPbrEnvMaxLod, uPbrEnvNorm;\n"
    "uniform vec4  uPbrSH[9];\n"
    "uniform vec4  uPbrDial;\n"
    /* uPbrSun scales the sun's lobe on every material (pbr_sun_strength). */
    "uniform float uPbrSun;\n"
    "vec3 pbr_to_linear(vec3 c)  { return pow(max(c, vec3(0.0)), vec3(2.2)); }\n"
    "vec3 pbr_to_display(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2)); }\n"
    "float pbr_luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }\n"
    "float pbr_ggx(float NoH, float a) {\n"
    "    float a2 = a * a;\n"
    "    float d = NoH * NoH * (a2 - 1.0) + 1.0;\n"
    "    return a2 / max(3.14159265 * d * d, 1e-7);\n"
    "}\n"
    /* Smith, height correlated, with the 4 NoL NoV of the microfacet denominator
       already cancelled: the lobe is D x V x F and nothing else. */
    "float pbr_vis(float NoV, float NoL, float a) {\n"
    "    float a2 = a * a;\n"
    "    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);\n"
    "    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);\n"
    "    return 0.5 / max(gv + gl, 1e-6);\n"
    "}\n"
    /* f90, the reflectance at grazing, follows f0 down below the 0.04 of ordinary paint
       (Filament's clamp(50 f0)): a surface whose map says it hardly reflects at all,
       concrete or rubber, must not then mirror the whole sky at its silhouette. */
    "float pbr_f90(vec3 f0) { return clamp(50.0 * dot(f0, vec3(0.3333)), 0.0, 1.0); }\n"
    "vec3 pbr_fresnel(vec3 f0, float u) {\n"
    "    return f0 + (vec3(pbr_f90(f0)) - f0) * pow(clamp(1.0 - u, 0.0, 1.0), 5.0);\n"
    "}\n"
    /* The sky's irradiance on a surface facing n, the cosine convolution already folded
       into the coefficients by the baker (the same nine terms as m3_sh). */
    "vec3 pbr_sh(vec3 n) {\n"
    "    return uPbrSH[0].rgb * 0.282095\n"
    "         + uPbrSH[1].rgb * (0.488603 * n.y)\n"
    "         + uPbrSH[2].rgb * (0.488603 * n.z)\n"
    "         + uPbrSH[3].rgb * (0.488603 * n.x)\n"
    "         + uPbrSH[4].rgb * (1.092548 * n.x * n.y)\n"
    "         + uPbrSH[5].rgb * (1.092548 * n.y * n.z)\n"
    "         + uPbrSH[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0))\n"
    "         + uPbrSH[7].rgb * (1.092548 * n.x * n.z)\n"
    "         + uPbrSH[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));\n"
    "}\n"
    /* THE SKY'S COLOUR on a surface facing n, at a luminance of one: the hue of the
       light it receives from the sky, with the brightness left to the fill's own
       hemisphere term. Taken this way so a sky photograph can colour the shade (blue on
       a roof, the ground's colour underneath) without being able to change how bright
       the shade is, which is the fill dial's job. */
    "vec3 pbr_sky_hue(vec3 n) {\n"
    "    if (uPbrEnvOn != 1) return vec3(1.0);\n"
    "    vec3 e = max(pbr_sh(n), vec3(0.0));\n"
    "    float l = pbr_luma(e);\n"
    "    return mix(vec3(1.0), l > 1e-4 ? clamp(e / l, vec3(0.0), vec3(4.0)) : vec3(1.0), uPbrDial.w);\n"
    "}\n"
    /* The world as a mirror of this roughness sees it in direction r, as a fraction of
       the irradiance an upward surface receives (divided by pi: a uniform sky reads 1).
       The photograph is a canyon rim, sky above and sunlit rock below, and its colour
       is taken at the same share as the fill's (pbr_sky_hue), so a desert base does not
       turn blue in its steel. Without a sky pack, the white hemisphere the fill uses. */
    "vec3 pbr_sky_radiance(vec3 r, float rough) {\n"
    "    if (uPbrEnvOn != 1) return vec3(0.6 + 0.4 * r.y);\n"
    "    vec3 e = pbr_to_linear(PBR_ENVLOD(uPbrEnv, r, rough * uPbrEnvMaxLod).rgb) * uPbrEnvNorm;\n"
    "    return mix(vec3(pbr_luma(e)), e, uPbrDial.w);\n"
    "}\n"
    /* The other half of the split sum, Karis's analytic fit (as m3_env_brdf). */
    "vec2 pbr_env_brdf(float rough, float NoV) {\n"
    "    vec4 c0 = vec4(-1.0, -0.0275, -0.572,  0.022);\n"
    "    vec4 c1 = vec4( 1.0,  0.0425,  1.040, -0.040);\n"
    "    vec4 r  = rough * c0 + c1;\n"
    "    float a = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;\n"
    "    return vec2(-1.04, 1.04) * a + r.zw;\n"
    "}\n"
    "vec3 pbr_env_spec(vec3 f0, float rough, float NoV) {\n"
    "    vec2 ab = pbr_env_brdf(rough, NoV);\n"
    "    return f0 * ab.x + vec3(pbr_f90(f0) * ab.y);\n"
    "}\n"
    /* SPECULAR OCCLUSION from the ambient occlusion (Lagarde's fit): a crease that the
       occlusion darkens must not then mirror the open sky, or every panel seam glows. */
    "float pbr_spec_occlusion(float NoV, float ao, float rough) {\n"
    "    return clamp(pow(NoV + ao, exp2(-16.0 * rough - 1.0)) - 1.0 + ao, 0.0, 1.0);\n"
    "}\n"
    /* THE MATERIAL, as the header describes: x roughness, y metalness, z the
       reflectance of its dielectric part at normal incidence, 0.04 for paint, less where
       the map's strength is lower (masonry and rubber), up to 0.06 for glass; w is 1
       where a specular map covers the texel and 0 where none does, and sheen is the
       map's upright sheen (its blue). albG is the texel's own colour, display encoded
       (the colour over the baked shade).

       METAL IS A MATERIAL CLASS, READ FROM THE MAP'S GREEN. The baker gives every part
       one tightness by its class and leaves it alone, but scales the strength (red) by
       how light each texel is against its part, so a light patch of paint on painted
       plate reaches the strength of bare steel. The barracks' camouflaged roof is that
       case: plate, 0.55 tight, its light grey patches as strong as steel, and as metal
       they mirrored the sky in a speckle of light blue. Steel, pipe, bronze, polished
       metal and glass are 0.60 and tighter; plate, trim, galvanised sheet, towers and
       paint are 0.55 and looser, and are lit as the painted surfaces they are drawn as.

       vspec_gloss (uSpecGloss) tightens every highlight as it always did: the old lobe
       multiplied a Blinn exponent by it, and a GGX width goes as one over the square
       root of that exponent, so roughness goes as its fourth root. */
    "vec4 pbr_material(vec2 tc, vec3 albG, out float sheen) {\n"
    "    float rough = uReRough;\n"
    "    float metal = 0.0, f0 = 0.04, mapped = 0.0;\n"
    "    sheen = 0.0;\n"
    "    if (uSpecOn == 1) {\n"
    "        vec4 sp = texture2D(uSpecMap, (floor(tc * uGNormSize) + 0.5) / uGNormSize);\n"
    "        if (sp.a > 0.5) {\n"
    "            mapped = 1.0;\n"
    "            sheen = sp.b;\n"
    "            float mx = max(albG.r, max(albG.g, albG.b));\n"
    "            float sat = (mx - min(albG.r, min(albG.g, albG.b))) / max(mx, 1e-4);\n"
    "            float metalClass = smoothstep(0.555, 0.60, sp.g);\n"
    "            metal = smoothstep(0.45, 0.85, sp.r) * metalClass\n"
    "                  * (1.0 - smoothstep(0.25, 0.55, sat))\n"
    "                  * smoothstep(0.015, 0.06, pbr_luma(pbr_to_linear(albG)));\n"
    /* HOUSE PAINT ON A STEEL PART IS PAINT. The part's tightness is the steel's, and
       a red band or a yellow panel on it took steel's gloss and a reflection with it,
       and went pink. Below glass's tightness (0.85 and up, which stays smooth), a
       texel of a metal-class part that is not itself metal is lit as paint: paint's
       tightness, paint's reflectance. */
    "            float onSteel = metalClass * (1.0 - smoothstep(0.78, 0.84, sp.g)) * (1.0 - metal);\n"
    "            float g = mix(sp.g, min(sp.g, 0.45), onSteel);\n"
    "            rough = mix(0.95, 0.15, g) * inversesqrt(sqrt(max(uSpecGloss, 0.01)));\n"
    "            f0 = mix(0.04 * clamp(sp.r / 0.3, 0.2, 1.5), 0.04, onSteel);\n"
    "        }\n"
    "    }\n"
    "    return vec4(clamp(rough * uPbrDial.x, 0.06, 1.0), clamp(metal * uPbrDial.y, 0.0, 1.0), f0, mapped);\n"
    "}\n"
    /* HOW MUCH OF A DIELECTRIC'S SKY REFLECTION IS NEW. A rough painted surface mirrors
       the sky as an even veil that hardly changes with the view, and the painted colour
       already holds it: the remasters were painted to look right as lit surfaces, and
       their value is the paint and its sheen together. Added again, that veil laid a
       milky film over every dark texel and took a fifth of the saturation out of the
       house colours. What the paint cannot hold is the reflection of a smooth surface,
       which moves with the view: glass, lenses, polished parts. So the reflection is
       added in full at glass's roughness (0.2), fades out by bronze's (0.4), and is
       gone at paint's and everything rougher. A metal has no painted diffuse to hold
       it and always reflects in full. */
    "float pbr_sheen_weight(float rough) { return 1.0 - smoothstep(0.20, 0.45, rough); }\n"
    /* PAINTED CAVITY: a texel painted near black is soot, grime, a seam or a painted
       crease far more often than black gloss, and the occlusion target is too coarse to
       know it. Its reflections fade out with its darkness, to nothing. A smooth texel
       is exempt: dark glass is the one dark thing that does mirror the sky. */
    "float pbr_cavity(vec3 albG, float rough) {\n"
    "    return max(smoothstep(0.03, 0.30, pbr_luma(albG)), 1.0 - smoothstep(0.20, 0.35, rough));\n"
    "}\n"
    /* THE LINEAR ENERGIES from the relight's gamma-domain sums. lg is today's diffuse
       light on this pixel (fill plus suns), amb the fill's share of it, sun the main
       sun's share (NoL and shadow included). Decoding the SUM keeps the diffuse exactly
       where it was; the shares then split that linear total in the same proportion, so
       the sun lobe and the fill's colour stay in step with it. Returns x the fill's
       linear energy, y the main sun's, and z the level the sky reflection is measured
       against.

       THAT LEVEL IS THE SKY'S SHARE OF THE OPEN-AIR LIGHT, KEPT AS A RATIO. What the
       dials say about the sky is how strong the fill is next to the sun: ambUp, the
       fill on an upward surface, against openUp, fill and sun together there. That
       ratio (about one to three) is the art direction, and it is a ratio, so it is
       carried over unchanged onto the linear light of open ground. Decoding the fill
       on its own instead squares that ratio away (one to twenty) and every metal
       mirrored a world as dark as its own shadow and read as black; measuring against
       the sunlit level instead put a grey film of sky over every dark wall. The
       photograph cannot settle it: it is eight bit, so its sky and its sunlit rock
       arrive at about the same level.

       WITH LINEAR LIGHT ON the light pass converts every tuned light the same way,
       fx_lin_gain (the tuned factor raised to linear_light_k), and the relight's diffuse
       is that converted light, so the energies here are converted the same way and stay
       in step with it when the dial moves. At its default of 2.2 the figures are the
       ones above. The ratio carry stays: the sky and sun are not yet summed in linear
       with energies of their own, and until they are the dials' one to three is still
       the art direction. */
    "vec3 pbr_irradiance(float lg, float amb, float sun, float ambUp, float openUp) {\n"
    "    float total = uLinOn == 1 ? fx_lin_gain(lg) : pow(max(lg, 0.0), 2.2);\n"
    "    float inv = 1.0 / max(lg, 1e-4);\n"
    "    float sky = (uLinOn == 1 ? fx_lin_gain(openUp) : pow(max(openUp, 0.0), 2.2))\n"
    "              * clamp(ambUp / max(openUp, 1e-4), 0.0, 1.0);\n"
    "    return vec3(total * amb * inv, total * sun * inv, sky);\n"
    "}\n";

#endif /* OPENCNC3D_PBR_GLSL_H */

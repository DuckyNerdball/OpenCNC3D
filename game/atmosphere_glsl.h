/* Moving air volumes. Analytical 3D noise avoids implicit texture derivatives inside
   a depth-dependent ray march. Scattering is integrated in front of opaque surfaces,
   with the same local light positions, colours and fade used by surface lighting. */
#ifndef OPENCNC3D_ATMOSPHERE_GLSL_H
#define OPENCNC3D_ATMOSPHERE_GLSL_H

static const char* ATMOSPHERE_FS_HEADER =
    "#version 120\n"
    "varying vec2 uv;\n"
    "uniform sampler2D uDepth, uVisibility;\n"
    "uniform vec2 uMapSize, uWind, uDrift;\n"
    "uniform float uDensity, uHeight, uScale, uScatter, uFalloff;\n"
    "uniform vec3 uAirColour;\n"
    "uniform int uNLights;\n"
    "uniform vec4 uLightPos[24], uLightCol[24];\n"
    /* The current field light is independent of the transient 24-light budget. */
    "uniform sampler2D uTibField;\n"
    "uniform float uTibInv, uTibLight, uTibLightH, uTibPulse, uTibTime;\n"
    "uniform vec3 uTibCol; uniform vec2 uTibH;\n";

/* ---- LIGHT SHAFTS ----------------------------------------------------------------
   Compiled in only for the shafts programs (atmosphere_mod.h), which put this block
   between FX_COMMON and the bodies below; without it the bodies preprocess to exactly
   the programs they were, so the master dial off draws the same pixels.

   THE PICTURE ALREADY SHOWS SUNLIT AIR. The ground's colours, the grade and the
   weather were all tuned with the air between camera and ground taken as lit, so in
   the open this feature changes nothing at all. What it adds is the other case: air
   the sun does not reach. At every march step shaftShade() asks whether the sun gets
   there, through the same sun shadow map, terrain shadow map and cloud deck the light
   pass reads for surfaces, and the sun's share of that step's light goes away, so a
   shadow cast on the ground continues up through the air to whatever cast it.

   Two media take it. The weather (mist, dust) keeps its own light and loses the sun's
   share of it in shadow. A thin lower haze, which has no light of its own to draw,
   only absorbs there: its sunlit glow is taken to match what lies behind it, so where
   it is lit it leaves the pixel alone, and where it is shadowed the glow is gone and
   only its extinction is left. That darkens by a fraction, which keeps hue and
   saturation and never pulls the view toward a grey; the haze cannot lift a shadow or
   flatten the contrast of lit ground.

   One nearest tap per step, not the surface's twelve-tap disc: the march's own jitter
   and the wider resolve below already average far more positions than a disc would,
   and air has no texel it stands in, so no normal offset either.

   Under linear light (uLinOn) the composite below encodes the scene, lays the air over
   it as tuned and decodes the result, so the haze's extinction and the weather's
   shadowed glow act on display values exactly as in the gamma chain and the same
   densities give the same depth of shadow. The volume is a float target then
   (fx_hdr_want), so a faint haze's transmittance is no longer rounded to 1/255. */
static const char* ATMOSPHERE_SHAFTS_FS =
    "#define AIR_SHAFTS 1\n"
    "uniform sampler2D uShaftShadow, uShaftTerrainShadow, uShaftCloud;\n"
    "uniform mat4 uShaftSunVP;\n"
    "uniform vec3 uShaftSunDir;\n"
    "uniform int uShaftShadowOn, uShaftTerrainOn, uShaftCloudOn;\n"
    "uniform float uShaftBias, uShaftTerrainStrength, uShaftStrength, uShaftG, uShaftPhase0;\n"
    "uniform float uShaftCStrength, uShaftCEdge, uShaftCWidth, uShaftCInvScale, uShaftCDrift, uShaftCHeight;\n"
    "uniform vec2 uShaftCDir;\n"
    "uniform float uShaftHaze, uShaftHazeHeight;\n"
    /* How much of the sun reaches a point in the air: 1 in the open, 0 behind a caster
       or a hill, and less under a cloud. Outside the shadow map's box the air is lit,
       the same answer the light pass gives a surface there. */
    "float shaftSun(vec3 p) {\n"
    "    float lit = 1.0;\n"
    "    if (uShaftShadowOn == 1) {\n"
    "        vec4 sc = uShaftSunVP * vec4(p, 1.0);\n"
    "        vec3 s = (sc.xyz / sc.w) * 0.5 + 0.5;\n"
    "        if (s.x > 0.001 && s.x < 0.999 && s.y > 0.001 && s.y < 0.999 && s.z < 1.0) {\n"
    "            float blocked = (s.z - uShaftBias > texture2D(uShaftShadow, s.xy).r) ? 1.0 : 0.0;\n"
    "            if (uShaftTerrainOn == 1)\n"
    "                blocked = max(blocked, (s.z - uShaftBias > texture2D(uShaftTerrainShadow, s.xy).r)\n"
    "                                       ? uShaftTerrainStrength : 0.0);\n"
    "            lit = 1.0 - blocked;\n"
    "        }\n"
    "    }\n"
    /* The cloud deck, found the way the light pass finds it: walk back along the sun
       to the deck's height and read the mask there. This read sits in a loop after
       per-pixel early returns, where an implicit derivative is undefined, so the CPU
       side switches the mask to its base level for this pass and no mip is chosen. */
    "    if (uShaftCloudOn == 1) {\n"
    "        float dy = max(-uShaftSunDir.y, 0.25);\n"
    "        float t = max(uShaftCHeight - p.y, 0.0) / dy;\n"
    "        vec2 g = p.xz - uShaftSunDir.xz * t;\n"
    "        vec2 q = vec2(dot(g, uShaftCDir), dot(g, vec2(-uShaftCDir.y, uShaftCDir.x))) * uShaftCInvScale;\n"
    "        q.x -= uShaftCDrift;\n"
    "        float m = texture2D(uShaftCloud, q).r;\n"
    "        lit *= 1.0 - uShaftCStrength * smoothstep(uShaftCEdge - uShaftCWidth, uShaftCEdge + uShaftCWidth, m);\n"
    "    }\n"
    "    return lit;\n"
    "}\n"
    /* A Henyey-Greenstein lobe along this pixel's ray, over the lobe's mean across the
       view (uShaftPhase0, found on the CPU). It scales only how deep the air's shadows
       go, never lit air: the camera's yaw does not turn in play, so a lobe on all of
       the air's light would be one fixed left-to-right ramp over every screen.
       uShaftSunDir is the way the light travels, -rd the way it leaves toward the eye. */
    "float shaftPhase(vec3 rd) {\n"
    "    float k = 1.0 + uShaftG*uShaftG - 2.0*uShaftG*dot(uShaftSunDir, -rd);\n"
    "    return clamp(pow(uShaftPhase0 / max(k, 1e-4), 1.5), 0.0, 4.0);\n"
    "}\n"
    /* The share of the sun's light missing at p: 0 in the open, up to 1 in full shadow. */
    "float shaftShade(vec3 p, float phase) {\n"
    "    return min(uShaftStrength * (1.0 - shaftSun(p)) * phase, 1.0);\n"
    "}\n"
    /* Interleaved gradient noise: a fixed function of the pixel whose neighbours fill
       the interval evenly, so the 4x4 resolve averages the march's start offsets into
       a smooth result where per-pixel white noise left grain, and twelve fixed starts
       left bands. No time in it, so a still frame is the same frame twice. */
    "float shaftJitter() {\n"
    "    return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n"
    "}\n";

static const char* ATMOSPHERE_FS_BODY =
    /* A fixed hash of lattice corners, interpolated in all three axes. Reducing
       coordinates before hashing makes the lattice tile exactly at 64, including
       its negative side. The two octave drifts are folded separately on the CPU. */
    "float airHash(vec3 p) {\n"
    "    p = mod(p, 64.0);\n"
    "    p = fract(p * vec3(0.1031, 0.1030, 0.0973));\n"
    "    p += dot(p, p.yxz + 33.33);\n"
    "    return fract((p.x + p.y) * p.z);\n"
    "}\n"
    "float airNoise(vec3 p) {\n"
    "    vec3 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);\n"
    "    float a = mix(airHash(i), airHash(i+vec3(1,0,0)), f.x);\n"
    "    float b = mix(airHash(i+vec3(0,1,0)), airHash(i+vec3(1,1,0)), f.x);\n"
    "    float c = mix(airHash(i+vec3(0,0,1)), airHash(i+vec3(1,0,1)), f.x);\n"
    "    float d = mix(airHash(i+vec3(0,1,1)), airHash(i+vec3(1,1,1)), f.x);\n"
    "    return mix(mix(a,b,f.y), mix(c,d,f.y), f.z);\n"
    "}\n"
    "void main() {\n"
    "    float depth = texture2D(uDepth, uv).r;\n"
    "    vec3 P = fx_world(uv, min(depth, 0.999989));\n"
    "    float visible = texture2D(uVisibility, (P.xz+0.5)/(uMapSize+1.0)).r;\n"
    "    gl_FragColor = vec4(0,0,0,1);\n"
    "    if (depth >= 0.99999 || visible < 0.001) return;\n"
    /* Rays start at each pixel's near plane, so orthographic and perspective views
       both march their actual view ray rather than fanning out from one eye point. */
    "    vec3 start = fx_world(uv, 0.0);\n"
    "    vec3 delta = P - start; float len = length(delta);\n"
    "    vec3 rd = delta / max(len, 1e-5);\n"
    "#ifdef AIR_SHAFTS\n"
    /* LIGHT SHAFTS: the haze lives inside the weather's slab when there is weather,
       so the weather is marched over the same span in the same twelve steps as
       without shafts, and over its own span when there is none. */
    "    float top = 4.0 * (uDensity > 0.0 ? uHeight : uShaftHazeHeight), bottom = -0.5;\n"
    "#else\n"
    "    float top = 4.0 * uHeight, bottom = -0.5;\n"
    "#endif\n"
    "    float enter = 0.0, leave = len;\n"
    "    if (abs(rd.y) < 1e-5) {\n"
    "        if (start.y < bottom || start.y > top) return;\n"
    "    } else {\n"
    "        float a = (bottom - start.y)/rd.y, b = (top - start.y)/rd.y;\n"
    "        enter = max(0.0, min(a,b)); leave = min(len, max(a,b));\n"
    "    }\n"
    "    if (leave <= enter) return;\n"
    "    float dt = (leave - enter)/12.0;\n"
    "#ifdef AIR_SHAFTS\n"
    "    float jitter = shaftJitter();\n"
    "    float shaftPh = shaftPhase(rd);\n"
    "#else\n"
    "    float jitter = airHash(vec3(floor(gl_FragCoord.xy), 19.0));\n"
    "#endif\n"
    "    vec2 crosswind = vec2(-uWind.y, uWind.x);\n"
    "    float trans = 1.0; vec3 scatter = vec3(0);\n"
    "    for (int step = 0; step < 12; step++) {\n"
    "        vec3 sp = start + rd * (enter + (float(step) + jitter)*dt);\n"
    "#ifdef AIR_SHAFTS\n"
    /* The weather's density, only where there is weather; its light loses the sun's
       share where the sun is not. The haze takes no part in the light below (local
       lights and Tiberium scatter in the weather alone, by the weather's own dial): it
       only absorbs, and only the shaded share of it, in hazeKeep. */
    "        float density = 0.0;\n"
    "        if (uDensity > 0.0) {\n"
    "            vec3 q = vec3(dot(sp.xz,uWind), sp.y*2.0, dot(sp.xz,crosswind))/uScale;\n"
    "            float broad = airNoise(q - vec3(uDrift.x,0,0));\n"
    "            float fine = airNoise(q*2.7 - vec3(uDrift.y,0,0) + vec3(17,9,23));\n"
    "            float wisp = smoothstep(0.25,0.8,broad*0.7+fine*0.3);\n"
    "            density = uDensity * wisp * exp(-max(sp.y,0.0)/uHeight);\n"
    "        }\n"
    "        float opacity = 1.0 - exp(-density*dt);\n"
    "        float shade = shaftShade(sp, shaftPh);\n"
    "        float hazeKeep = exp(-uShaftHaze * exp(-max(sp.y,0.0)/uShaftHazeHeight) * shade * dt);\n"
    "        vec3 light = uAirColour * (1.0 - shade);\n"
    "#else\n"
    "        vec3 q = vec3(dot(sp.xz,uWind), sp.y*2.0, dot(sp.xz,crosswind))/uScale;\n"
    "        float broad = airNoise(q - vec3(uDrift.x,0,0));\n"
    "        float fine = airNoise(q*2.7 - vec3(uDrift.y,0,0) + vec3(17,9,23));\n"
    "        float wisp = smoothstep(0.25,0.8,broad*0.7+fine*0.3);\n"
    "        float density = uDensity * wisp * exp(-max(sp.y,0.0)/uHeight);\n"
    "        float opacity = 1.0 - exp(-density*dt);\n"
    "        vec3 light = uAirColour;\n"
    "#endif\n"
    "        for (int i = 0; i < 24; i++) {\n"
    "            if (i >= uNLights) break;\n"
    "            vec3 L = uLightPos[i].xyz - sp;\n"
    "            float radius = max(uLightPos[i].w,1e-4);\n"
    "            float dist2 = dot(L,L);\n"
    "            if (dist2 < radius*radius) {\n"
    "                float att = pow(max(0.0,1.0-sqrt(dist2)/radius), uFalloff);\n"
    "                light += uLightCol[i].rgb * uLightCol[i].w * att * uScatter;\n"
    "            }\n"
    "        }\n"
    "        if (uTibLight > 0.0 && uScatter > 0.0) {\n"
    "            vec4 field = texture2D(uTibField, sp.xz*uTibInv);\n"
    "            float above = max(sp.y-(uTibH.x+field.b*uTibH.y),0.0);\n"
    "            float phase = dot(sp.xz,vec2(0.17,0.23));\n"
    "            float pulse = 1.0-uTibPulse*0.3+uTibPulse*0.3*sin(6.2831853*(0.21*uTibTime+phase));\n"
    "            light += uTibCol*field.r*exp(-above/uTibLightH)*pulse*uTibLight*uScatter;\n"
    "        }\n"
    "        scatter += trans * opacity * light;\n"
    "#ifdef AIR_SHAFTS\n"
    "        trans *= (1.0-opacity) * hazeKeep;\n"
    "#else\n"
    "        trans *= 1.0-opacity;\n"
    "#endif\n"
    "    }\n"
    "    gl_FragColor = vec4(scatter, trans);\n"
    "}\n";

static const char* ATMOSPHERE_COMPOSITE_HEADER =
    "#version 120\n"
    "varying vec2 uv;\n"
    "uniform sampler2D uScene, uVolume, uDepth, uVisibility;\n"
    "uniform vec2 uVolumeSize, uMapSize;\n"
    /* LINEAR LIGHT (fx_post.h, fx_lin_over): with uLinOn 1 the scene is linear radiance
       in a float target. The air was tuned as a layer over gamma encoded colour, and the
       same layer over linear values reads several times thicker on dark ground, so the
       scene is encoded for the composite and the result decoded again. */
    "uniform int uLinOn;\n";
static const char* ATMOSPHERE_COMPOSITE_BODY =
    "void main() {\n"
    "    vec3 scene = texture2D(uScene,uv).rgb;\n"
    "    if (uLinOn == 1) scene = pow(max(scene, vec3(0.0)), vec3(1.0 / 2.2));\n"
    "    float d = texture2D(uDepth,uv).r;\n"
    "    vec3 P = fx_world(uv,min(d,0.999989));\n"
    "    float visibility = texture2D(uVisibility,(P.xz+0.5)/(uMapSize+1.0)).r;\n"
    /* Fetch exact half-resolution texel centres, never a filtered volume across a
       silhouette. Weight by view depth and bilinear footprint, with closest-depth
       fallback where all four neighbours belong to a different surface. */
    "    vec2 pixel = uv*uVolumeSize-0.5, base = floor(pixel), f = fract(pixel);\n"
    "    float target = length(P-fx_world(uv,0.0));\n"
    "    vec4 air = vec4(0), best = vec4(0,0,0,1);\n"
    "    float sum = 0.0, closest = 1e20;\n"
    "#ifdef AIR_SHAFTS\n"
    /* LIGHT SHAFTS: sixteen texels under a tent two texels wide rather than the four
       of a bilinear footprint, still weighted by depth. The march's start offsets are
       a pattern that only averages out over a neighbourhood; this is that average. */
    "    for (int y = -1; y < 3; y++) for (int x = -1; x < 3; x++) {\n"
    "        vec2 offset = vec2(float(x),float(y));\n"
    "        vec2 weights = max(vec2(0.0), 1.0-abs(offset-f)*0.5);\n"
    "#else\n"
    "    for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) {\n"
    "        vec2 offset = vec2(float(x),float(y));\n"
    "#endif\n"
    "        vec2 tap = clamp((base+offset+0.5)/uVolumeSize,0.5/uVolumeSize,1.0-0.5/uVolumeSize);\n"
    "        float td = texture2D(uDepth,tap).r;\n"
    "        float dist = length(fx_world(tap,min(td,0.999989))-fx_world(tap,0.0));\n"
    "        float error = abs(dist-target);\n"
    "        vec4 v = texture2D(uVolume,tap);\n"
    "        if (error < closest) { closest = error; best = v; }\n"
    "#ifndef AIR_SHAFTS\n"
    "        vec2 weights = mix(1.0-f,f,offset);\n"
    "#endif\n"
    "        float w = weights.x*weights.y * exp(-error*8.0);\n"
    "        air += v*w; sum += w;\n"
    "    }\n"
    "    air = sum > 1e-5 ? air/sum : best;\n"
    "    if (d >= 0.99999) visibility = 0.0;\n"
    "    vec3 outc = mix(scene, scene*air.a+air.rgb,visibility);\n"
    "    if (uLinOn == 1) outc = pow(max(outc, vec3(0.0)), vec3(2.2));\n"
    "    gl_FragColor = vec4(outc,1.0);\n"
    "}\n";
#endif

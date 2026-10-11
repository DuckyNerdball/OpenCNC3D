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
    "    float top = 4.0 * uHeight, bottom = -0.5;\n"
    "    float enter = 0.0, leave = len;\n"
    "    if (abs(rd.y) < 1e-5) {\n"
    "        if (start.y < bottom || start.y > top) return;\n"
    "    } else {\n"
    "        float a = (bottom - start.y)/rd.y, b = (top - start.y)/rd.y;\n"
    "        enter = max(0.0, min(a,b)); leave = min(len, max(a,b));\n"
    "    }\n"
    "    if (leave <= enter) return;\n"
    "    float dt = (leave - enter)/12.0;\n"
    "    float jitter = airHash(vec3(floor(gl_FragCoord.xy), 19.0));\n"
    "    vec2 crosswind = vec2(-uWind.y, uWind.x);\n"
    "    float trans = 1.0; vec3 scatter = vec3(0);\n"
    "    for (int step = 0; step < 12; step++) {\n"
    "        vec3 sp = start + rd * (enter + (float(step) + jitter)*dt);\n"
    "        vec3 q = vec3(dot(sp.xz,uWind), sp.y*2.0, dot(sp.xz,crosswind))/uScale;\n"
    "        float broad = airNoise(q - vec3(uDrift.x,0,0));\n"
    "        float fine = airNoise(q*2.7 - vec3(uDrift.y,0,0) + vec3(17,9,23));\n"
    "        float wisp = smoothstep(0.25,0.8,broad*0.7+fine*0.3);\n"
    "        float density = uDensity * wisp * exp(-max(sp.y,0.0)/uHeight);\n"
    "        float opacity = 1.0 - exp(-density*dt);\n"
    "        vec3 light = uAirColour;\n"
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
    "        trans *= 1.0-opacity;\n"
    "    }\n"
    "    gl_FragColor = vec4(scatter, trans);\n"
    "}\n";

static const char* ATMOSPHERE_COMPOSITE_HEADER =
    "#version 120\n"
    "varying vec2 uv;\n"
    "uniform sampler2D uScene, uVolume, uDepth, uVisibility;\n"
    "uniform vec2 uVolumeSize, uMapSize;\n";
static const char* ATMOSPHERE_COMPOSITE_BODY =
    "void main() {\n"
    "    vec3 scene = texture2D(uScene,uv).rgb;\n"
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
    "    for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) {\n"
    "        vec2 offset = vec2(float(x),float(y));\n"
    "        vec2 tap = clamp((base+offset+0.5)/uVolumeSize,0.5/uVolumeSize,1.0-0.5/uVolumeSize);\n"
    "        float td = texture2D(uDepth,tap).r;\n"
    "        float dist = length(fx_world(tap,min(td,0.999989))-fx_world(tap,0.0));\n"
    "        float error = abs(dist-target);\n"
    "        vec4 v = texture2D(uVolume,tap);\n"
    "        if (error < closest) { closest = error; best = v; }\n"
    "        vec2 weights = mix(1.0-f,f,offset);\n"
    "        float w = weights.x*weights.y * exp(-error*8.0);\n"
    "        air += v*w; sum += w;\n"
    "    }\n"
    "    air = sum > 1e-5 ? air/sum : best;\n"
    "    if (d >= 0.99999) visibility = 0.0;\n"
    "    gl_FragColor = vec4(mix(scene, scene*air.a+air.rgb,visibility),1.0);\n"
    "}\n";
#endif

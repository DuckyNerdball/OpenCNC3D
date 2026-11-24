#ifndef CNC3D_SCREEN_REFLECTIONS_GLSL_H
#define CNC3D_SCREEN_REFLECTIONS_GLSL_H
/* Shared current-frame screen-space ray query. No history, reflected HUD or
 * feedback sampling. Perspective-correct interpolation also supports ortho.
 * Algorithm reference: McGuire/Mara, JCGT 3(4), Efficient GPU Screen-Space Ray
 * Tracing. This bounded implementation uses the existing compatibility renderer.
 */
static const char* CNC3D_SCREEN_REFLECTIONS_GLSL=R"(
uniform sampler2D uSSRScene,uSSRDepth;
uniform mat4 uSSRVP,uSSRInvVP;
uniform vec3 uSSREye,uSSRForward;
uniform vec2 uSSRSize;
uniform int uSSROn,uSSRLinear;
#ifdef GL_ARB_shader_texture_lod
#define SSR_SAMPLE(s,p,l) texture2DLod(s,p,l)
#else
#define SSR_SAMPLE(s,p,l) texture2D(s,p,l)
#endif
vec3 ssr_world(vec2 uv,float z){vec4 p=uSSRInvVP*vec4(uv*2.-1.,z*2.-1.,1.);return p.xyz/p.w;}
vec3 ssr_decode(vec3 c){return uSSRLinear==1?c:mix(c/12.92,pow((max(c,vec3(0))+.055)/1.055,vec3(2.4)),vec3(greaterThan(c,vec3(.04045))));}
// Trace an actual reflected ray against the visible scene, with finite thickness.
vec4 ssr_ray(vec3 P,vec3 N,vec3 R,float rough){
 const float reach=9.;
 vec3 start=P+N*.018+R*.025,end=start+R*reach;
 vec4 a=uSSRVP*vec4(start,1),b=uSSRVP*vec4(end,1);
 if(a.w<=.001)return vec4(0);
 // Clip a camera-facing ray before the near plane rather than projecting behind it.
 if(b.w<=.01){float t=(a.w-.01)/max(a.w-b.w,.0001);end=mix(start,end,t);b=uSSRVP*vec4(end,1);}
 vec2 auv=a.xy/a.w*.5+.5,buv=b.xy/b.w*.5+.5;
 float pixels=length((buv-auv)*uSSRSize);if(pixels<2.)return vec4(0);
 float stepT=max(1.5/pixels,1./72.),previous=0.,previousGap=-1e20;
 for(int i=1;i<=72;i++){
  float t=float(i)*stepT;if(t>1.)break;vec2 uv=mix(auv,buv,t);
  if(any(lessThanEqual(uv,vec2(.001)))||any(greaterThanEqual(uv,vec2(.999))))break;
  float z=texture2D(uSSRDepth,uv).r;if(z>=.999999){previous=t;previousGap=-1e20;continue;}
  vec3 ray=mix(start/a.w,end/b.w,t)/mix(1./a.w,1./b.w,t),surface=ssr_world(uv,z);
  float gap=dot(ray-surface,uSSRForward),thickness=.045+.012*length(ray-start);
  // Test the swept ray interval, not just isolated samples: a thin surface can
  // lie between steps. Refine the crossing, then validate its finite thickness.
  if(gap>=0.&&(gap<thickness||previousGap<=0.)&&length(surface-P)>.045){
   float lo=previous,hi=t;
   for(int j=0;j<5;j++){float m=(lo+hi)*.5;vec2 st=mix(auv,buv,m);float sd=texture2D(uSSRDepth,st).r;vec3 rp=mix(start/a.w,end/b.w,m)/mix(1./a.w,1./b.w,m);float g=dot(rp-ssr_world(st,sd),uSSRForward);if(g>0.&&sd<.999999)hi=m;else lo=m;}
   uv=mix(auv,buv,hi);surface=ssr_world(uv,texture2D(uSSRDepth,uv).r);
   vec3 refined=mix(start/a.w,end/b.w,hi)/mix(1./a.w,1./b.w,hi);
   if(abs(dot(refined-surface,uSSRForward))>thickness||length(surface-P)<=.045){previous=t;previousGap=gap;continue;}
   vec2 border=min(uv,1.-uv);float confidence=smoothstep(.005,.065,min(border.x,border.y))*(1.-smoothstep(6.,9.,length(surface-P)));
   // Roughness grows the source footprint with travel. The mip chain is generated
   // from the current scene, so moving/flashing lights cannot leave stale trails.
   float footprint=1.+rough*rough*length((uv-auv)*uSSRSize)*.38;
   vec3 radiance=ssr_decode(SSR_SAMPLE(uSSRScene,uv,clamp(log2(footprint),0.,7.)).rgb);
   return vec4(radiance,confidence);
  }
  previous=t;previousGap=gap;
 }
 return vec4(0);
}
vec4 ssr_radiance(vec3 P,vec3 N,vec3 V,float rough){
 if(uSSROn==0||rough>=.92)return vec4(0);
 vec3 R=reflect(-V,N);vec4 hit=ssr_ray(P,N,R,rough);
 // A rough specular lobe samples a cone, not a perfectly sharp mirror with dimmed RGB.
 if(rough>.22){vec3 T=normalize(abs(R.y)<.98?cross(R,vec3(0,1,0)):cross(R,vec3(1,0,0)));vec3 B=cross(R,T);float cone=rough*rough*.22;
  vec4 a=ssr_ray(P,N,normalize(R+T*cone+B*cone*.5),rough),b=ssr_ray(P,N,normalize(R-T*cone-B*cone*.5),rough);
  float w=hit.a*2.+a.a+b.a;hit=vec4((hit.rgb*hit.a*2.+a.rgb*a.a+b.rgb*b.a)/max(w,.00001),w*.25);
 }
 hit.a*=1.-smoothstep(.70,.92,rough);return hit;
}
)";
#endif

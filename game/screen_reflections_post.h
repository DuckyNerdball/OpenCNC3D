#ifndef CNC3D_SCREEN_REFLECTIONS_POST_H
#define CNC3D_SCREEN_REFLECTIONS_POST_H
/* Shared Dawn3D/RA3D material reflections. Optional capabilities fail locally:
 * no other post effect depends on MRT or mip generation being available. */
#include "screen_reflections_glsl.h"
static float rain_overcast(void);
typedef void (APIENTRY *CncDrawBuffers)(GLsizei,const GLenum*);
typedef void (APIENTRY *CncGenerateMipmap)(GLenum);
static CncDrawBuffers g_ssrDrawBuffers=0;
static CncGenerateMipmap g_ssrMipmap=0;
static FxRT g_ssrMaterial,g_ssrSky,g_ssrNormal,g_ssrResult;
static GLuint g_ssrProgram=0;
static int g_ssrCaps=0,g_ssrLive=0;
static void fx_ssr_shutdown(){fx_rt_free(&g_ssrMaterial);fx_rt_free(&g_ssrSky);fx_rt_free(&g_ssrNormal);fx_rt_free(&g_ssrResult);if(g_ssrProgram)fx_glDeleteProgram(g_ssrProgram);g_ssrProgram=0;g_ssrLive=0;}
static bool fx_ssr_attach(FxRT*target){
 g_ssrLive=0;
 if(!g_fx.pbr_on||!fx_unified_lighting()||getenv("CNC3D_SSR_OFF"))return false;
 if(!g_ssrCaps){
  g_ssrDrawBuffers=(CncDrawBuffers)SDL_GL_GetProcAddress("glDrawBuffers");if(!g_ssrDrawBuffers)g_ssrDrawBuffers=(CncDrawBuffers)SDL_GL_GetProcAddress("glDrawBuffersARB");
  g_ssrMipmap=(CncGenerateMipmap)SDL_GL_GetProcAddress("glGenerateMipmap");if(!g_ssrMipmap)g_ssrMipmap=(CncGenerateMipmap)SDL_GL_GetProcAddress("glGenerateMipmapEXT");
  GLint count=0;glGetIntegerv(0x8824,&count);g_ssrCaps=g_ssrDrawBuffers&&g_ssrMipmap&&count>=4?1:-1;
  printf("FX|ssr|capability=%d|draw-buffers=%d\n",g_ssrCaps,count);
 }
 if(g_ssrCaps<0)return false;
 FxRT* maps[]={&g_ssrMaterial,&g_ssrSky,&g_ssrNormal};
 for(int i=0;i<3;i++)if(!fx_rt_init(maps[i],target->w,target->h,FX_RT_COLOUR|FX_RT_FLOAT,GL_NEAREST)||!maps[i]->hdr){fx_rt_bind(target);return false;}
 fx_rt_bind(target);
 for(int i=0;i<3;i++)fx_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i+1,GL_TEXTURE_2D,maps[i]->tex,0);
 GLenum bufs[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT0+1,GL_COLOR_ATTACHMENT0+2,GL_COLOR_ATTACHMENT0+3};g_ssrDrawBuffers(4,bufs);
 if(fx_glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){GLenum one=GL_COLOR_ATTACHMENT0;g_ssrDrawBuffers(1,&one);for(int i=1;i<4;i++)fx_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,0,0);return false;}
 g_ssrLive=1;return true;
}
static void fx_ssr_detach(){if(!g_ssrLive)return;GLenum one=GL_COLOR_ATTACHMENT0;g_ssrDrawBuffers(1,&one);for(int i=1;i<4;i++)fx_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,0,0);}
static const FxRT* fx_ssr_apply(const FxRT*source,GLuint depth,const float*vp,const float*inv,const float*eye,int linear){
 if(!g_ssrLive||source!=&g_fxLit||!g_fx.pbr_on||!fx_unified_lighting()||getenv("CNC3D_SSR_OFF"))return source;
 if(!g_ssrProgram){
  const char*head="#version 120\n#extension GL_ARB_shader_texture_lod : enable\nvarying vec2 uv;uniform sampler2D uSSRMaterial,uSSRSky,uSSRNormal;uniform vec3 uSSRTint;\n";
  const char*body=R"(
vec3 encode(vec3 c){return uSSRLinear==1?c:mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,vec3(greaterThan(c,vec3(.0031308))));}
void main(){vec4 scene=texture2D(uSSRScene,uv),m=texture2D(uSSRMaterial,uv),sky=texture2D(uSSRSky,uv);gl_FragColor=scene;
 if(sky.a<=0.||m.a>=.92)return;float d=texture2D(uSSRDepth,uv).r;if(d>=.999999)return;
 vec4 normalWeight=texture2D(uSSRNormal,uv);vec3 P=ssr_world(uv,d),N=normalize(normalWeight.xyz*2.-1.),V=normalize(uSSREye-P);
 vec4 hit=ssr_radiance(P,N,V,m.a);if(hit.a<=0.)return;
 float nv=max(dot(N,V),.0001);vec4 r=m.a*vec4(-1.,-.0275,-.572,.022)+vec4(1.,.0425,1.04,-.04);float a=min(r.x*r.x,exp2(-9.28*nv))*r.x+r.y;vec2 brdf=vec2(-1.04,1.04)*a+r.zw;
 vec3 replacement=hit.rgb*(m.rgb*brdf.x+brdf.y)*sky.a*normalWeight.a*uSSRTint;
 gl_FragColor=vec4(encode(max(vec3(0),ssr_decode(scene.rgb)+(replacement-sky.rgb)*hit.a)),scene.a);
})";
  size_t n=strlen(head)+strlen(CNC3D_SCREEN_REFLECTIONS_GLSL)+strlen(body)+1;char*s=(char*)malloc(n);if(!s)return source;strcpy(s,head);strcat(s,CNC3D_SCREEN_REFLECTIONS_GLSL);strcat(s,body);g_ssrProgram=fx_program(s,"material-screen-reflections");free(s);if(!g_ssrProgram)return source;fprintf(stderr,"FX|ssr|ready|current-frame roughness-filtered material reflections\n");
 }
 if(!fx_rt_init(&g_ssrResult,source->w,source->h,FX_RT_COLOUR|(source->hdr?FX_RT_FLOAT:0),GL_LINEAR))return source;
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source->tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);g_ssrMipmap(GL_TEXTURE_2D);
 fx_glUseProgram(g_ssrProgram);fx_set1i(g_ssrProgram,"uSSROn",1);fx_set1i(g_ssrProgram,"uSSRLinear",linear);float overcast=rain_overcast();fx_set3f(g_ssrProgram,"uSSRTint",1.f-.20f*overcast,1.f-.18f*overcast,1.f-.14f*overcast);fx_set2f(g_ssrProgram,"uSSRSize",source->w,source->h);
 fx_setmat(g_ssrProgram,"uSSRVP",vp);fx_setmat(g_ssrProgram,"uSSRInvVP",inv);fx_set3f(g_ssrProgram,"uSSREye",eye[0],eye[1],eye[2]);
 float a[3],b[3],n=0;for(int i=0;i<3;i++){a[i]=(inv[12+i]-inv[8+i])/(inv[15]-inv[11]);b[i]=(inv[12+i]+inv[8+i])/(inv[15]+inv[11]);n+=(b[i]-a[i])*(b[i]-a[i]);}n=sqrtf(n);fx_set3f(g_ssrProgram,"uSSRForward",(b[0]-a[0])/n,(b[1]-a[1])/n,(b[2]-a[2])/n);
 const char*names[]={"uSSRScene","uSSRDepth","uSSRMaterial","uSSRSky","uSSRNormal"};GLuint tex[]={source->tex,depth,g_ssrMaterial.tex,g_ssrSky.tex,g_ssrNormal.tex};
 for(int i=0;i<5;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,tex[i]);fx_set1i(g_ssrProgram,names[i],i);}glActiveTexture(GL_TEXTURE0);
 fx_rt_bind(&g_ssrResult);fx_fullscreen_quad();fx_glUseProgram(0);
 glBindTexture(GL_TEXTURE_2D,source->tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
 for(int i=4;i>=0;i--){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,0);}return &g_ssrResult;
}
#endif

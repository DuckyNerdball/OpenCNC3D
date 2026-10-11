"""Reuse the delivered Yard foundation's material, with its exact map values.

The interior 1.1-cell square uses the measured affine BakeUV mapping of the
Yard's planar foundation. Reflect the patch in world space; transform its tangent
normal through the measured UV basis and reflection derivatives. No photograph,
albedo-derived relief, colour grading, or invented concrete shader is used.
"""
from pathlib import Path
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'art/refinery/texture_v03/source'
UV_A=((-0.12558578,-0.07570820),(0.07570822,-0.12558575),(0.19962576,0.20413985))

def build(g):
 def mul(a,b):return g.math('MULTIPLY',a,b)
 def add(a,b):return g.math('ADD',a,b)
 def pack(vals):
  n=g.node('ShaderNodeCombineXYZ')
  for inp,v in zip(n.inputs,vals):g.set(inp,v)
  return n.outputs[0]
 def axes(v):
  n=g.node('ShaderNodeSeparateXYZ');g.set(n.inputs[0],v);return list(n.outputs)
 pos=axes(g.vec('MULTIPLY',g.position,(1/10.24,)*3))
 normal=axes(g.geometry.outputs['True Normal'])
 weights=[g.math('POWER',g.math('ABSOLUTE',n),8) for n in normal]
 total=add(add(weights[0],weights[1]),weights[2]);weights=[g.math('DIVIDE',w,total) for w in weights]
 images={}
 for ch in ['BaseColor','Roughness','Metallic','Normal']:
  name='YARD_SHARED_'+ch;im=bpy.data.images.get(name)
  if im is None:
   im=bpy.data.images.load(str(SOURCE/f'Yard_Structure_{ch}.png'),check_existing=False);im.name=name;im.colorspace_settings.name='sRGB' if ch=='BaseColor' else 'Non-Color';im.pack()
  images[ch]=im
 result={ch:(0,0,0) for ch in ['BaseColor','Roughness','Metallic']};delta=(0,0,0)
 for axis,i,j in [(0,1,2),(1,0,2),(2,0,1)]:
  coords=[];flips=[]
  for k in [i,j]:
   t=add(g.math('DIVIDE',pos[k],1.1),.5)
   coords.append(add(mul(g.math('PINGPONG',t,1),1.1),-.55))
   flips.append(add(mul(g.math('PINGPONG',g.math('FLOOR',t),1),-2),1))
  u=add(add(mul(coords[0],UV_A[0][0]),mul(coords[1],UV_A[1][0])),UV_A[2][0]);v=add(add(mul(coords[0],UV_A[0][1]),mul(coords[1],UV_A[1][1])),UV_A[2][1]);uv=pack([u,v,0])
  textures={}
  for ch,im in images.items():
   n=g.node('ShaderNodeTexImage');n.name=f'YARD_EXACT_{ch}_{axis}';n.image=im;n.extension='EXTEND';g.set(n.inputs['Vector'],uv);textures[ch]=n.outputs['Color']
  for ch in result:result[ch]=g.vec('ADD',result[ch],g.vec('MULTIPLY',textures[ch],weights[axis]))
  n=axes(g.vec('SUBTRACT',g.vec('MULTIPLY',textures['Normal'],2),1))
  # The affine UV map is a uniform rotation: tangent/bitangent in Yard XY.
  tx=mul(add(mul(n[0],-.856401),mul(n[1],-.516311)),flips[0])
  ty=mul(add(mul(n[0], .516311),mul(n[1],-.856401)),flips[1])
  dz=mul(g.math('SUBTRACT',n[2],1),g.math('SIGN',normal[axis]))
  components=[0,0,0];components[i]=tx;components[j]=ty;components[axis]=dz
  delta=g.vec('ADD',delta,g.vec('MULTIPLY',pack(components),weights[axis]))
 return result,delta

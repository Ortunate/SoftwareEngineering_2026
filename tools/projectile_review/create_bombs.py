"""制作三种 Bomb 源模型、分层光效、独立波纹贴图及原始 ANM 配置，不启动游戏。"""
import bpy,bmesh,math,json,struct,sys,ast
import numpy as np
from pathlib import Path
ROOT=Path(sys.argv[sys.argv.index('--')+1]).resolve();BASE=ROOT/'assets/projectiles'
sys.dont_write_bytecode=True;sys.path.insert(0,str(ROOT/'tools/projectile_review'))
from asset_acceptance import carry_acceptance
bpy.context.preferences.filepaths.save_version=0;bpy.context.preferences.filepaths.temporary_directory=str(ROOT/'.cache/tmp')
# 只复用无副作用的网格导出函数，不执行其他类别的生成入口。
tree=ast.parse((ROOT/'tools/projectile_review/create_marisa.py').read_text(encoding='utf-8'))
for node in tree.body:
 if isinstance(node,ast.FunctionDef) and node.name in ('material','mesh','spindle','sphere','triangle_count','export_group'):exec(compile(ast.Module(body=[node],type_ignores=[]),'<mesh helpers>','exec'))
def sphere(name,r,mat,solid=True):
    profile=[(r,0)]+[(r*math.cos(math.pi*i/8),r*math.sin(math.pi*i/8)) for i in range(1,8)]+[(-r,0)]
    return spindle(name,profile,24,mat,solid=solid)
NAMES=('bomb_seal','bomb_barrier','bomb_star','bomb_spark')
def scripts(atlas,ids):
 """保留来源脚本全部指令，帧为 60 Hz；浮点参数仍保留原始位以避免误判指令类型。"""
 b=(ROOT/f'assets/runtime/battle/native/{atlas}.anm').read_bytes();n,m=struct.unpack_from('<II',b);result=[]
 for i in range(m):
  sid,at=struct.unpack_from('<II',b,64+4*n+8*i)
  if sid not in ids:continue
  ops=[]
  while True:
   frame,op,size=struct.unpack_from('<hBB',b,at);args=list(struct.unpack_from('<'+'I'*(size//4),b,at+4));ops.append(dict(frame=frame,opcode=op,args=args));at+=4+size
   if op==0:break
  result.append(dict(script=sid,instructions=ops))
 return result

def star(name,scale,mat,solid=True):
 """五角星为真实凹轮廓，前后切面与倒角闭合；不能用凸包填补凹角。"""
 v=[]
 for z,s in ((.045,1),(.09,.92),(-.09,.92),(-.045,1)):
  for i in range(10):
   a=math.pi/2+i*math.pi/5;r=(.65 if i%2==0 else .29)*s*scale;v.append((r*math.cos(a),r*math.sin(a),z*scale))
 v.extend(((0,0,.18*scale),(0,0,-.18*scale)));f=[]
 for i in range(10):
  j=(i+1)%10;f.extend(((40,10+i,10+j),(41,20+j,20+i),(i,j,10+j,10+i),(20+i,20+j,30+j,30+i),(30+i,30+j,j,i)))
 obj=mesh(name,v,f,mat,solid)
 for face in obj.data.polygons:face.use_smooth=False
 return obj

for kind,name in enumerate(NAMES):
 if "--only-spark" in sys.argv and kind!=3:continue
 if "--only-star" in sys.argv and kind!=2:continue
 if kind==1:continue # 用户取消封魔阵建模，保留原游戏素材与逻辑。
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);out=BASE/name;out.mkdir(parents=True,exist_ok=True)
 mat=material('Body',(1,1,1));fxmat=material('Glow',(1,1,1));body=[];shell=[];effect=[]
 if kind==0:
  body=[sphere('DangerCore',.5,mat)];effect=[sphere('SoftRadiance',.85,fxmat,False)]
 elif kind==2:
  body=[star('FivePointCore',1,mat)];shell=[star('StarHalo',1.12,fxmat,False)]
 else:
  # 模板轴 +Y，Y=0/1；近远端半径和长度在着色器中分别设置。
  v=[(math.cos(i*math.tau/24),y,math.sin(i*math.tau/24)) for y in (0,1) for i in range(24)];v.extend(((0,0,0),(0,1,0)));faces=[]
  for i in range(24):j=(i+1)%24;faces.extend(((i,j,24+j,24+i),(48,j,i),(49,24+i,24+j)))
  body=[mesh('DangerFrustum',v,faces,mat)];shell=[mesh('OpenOuterShell',v[:48],[f for f in faces if len(f)==4],fxmat,False)]
  # 新版用八条宽幅螺旋带包围束轴；UV.x 用于横向柔边，UV.y 为轴向进度。
  vertices=[];faces=[]
  for i in range(8):
   start=len(vertices)
   for j in range(17):
    t=j/16;angle=math.tau*i/8+math.pi*1.2*t;r=.68+.12*t
    for side in (-1,1):
     a=angle+side*.16;vertices.append((r*math.cos(a),t,r*math.sin(a)))
   for j in range(16):v=start+2*j;faces.append((v,v+1,v+3,v+2))
  ribbon=mesh('EightSpiralLightCurtains',vertices,faces,fxmat,False)
  uv=ribbon.data.uv_layers.new(name='SoftRibbonUV')
  for face in ribbon.data.polygons:
   for li in face.loop_indices:
    index=ribbon.data.loops[li].vertex_index;uv.data[li].uv=(index%2,(index%34)//2/16)
  effect=[ribbon]
 if kind!=3:
  with (out/'collision.bin').open('wb') as f:
   f.write(b'KCOL0001');f.write(struct.pack('<I',len(body)))
   for obj in body:
    triangle_count(obj);f.write(struct.pack('<I',len(obj.data.loop_triangles)))
    for t in obj.data.loop_triangles:
     coords=[]
     for i in t.vertices:p=obj.data.vertices[i].co;coords.extend((p.x,p.z,-p.y))
     f.write(struct.pack('<9f',*coords))
 bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
 for group,filename in ((body,name),(shell,'shell'),(effect,'effect')):
  if group:export_group(group,out/(filename+'.gltf'))
  else:
   # 空层不保留历史导出，避免已删除的小星形拖影再次被加载。
   for extension in ('.gltf','.bin'):
    stale=out/(filename+extension)
    if stale.exists():stale.unlink()
 meta=dict(asset=name,kind=kind,triangles=sum(triangle_count(o) for o in body+shell+effect),triangleHardLimit=[896,384,320,960][kind],parts={n:sum(map(triangle_count,g)) for n,g in (('body',body),('shell',shell),('effect',effect))},axis='+Y',humanAcceptance='pending',collision={'shape':'finite frustum','parameters':['origin','unitDirection','length','nearRadius','farRadius']} if kind==3 else {'shape':'closed mesh','file':'collision.bin'},decorationsCollide=False,sourceAtlas='player00' if kind<2 else 'player01',scripts=scripts('player00' if kind<2 else 'player01',range(133,137) if kind==0 else range(137,141) if kind==1 else range(5,8) if kind==2 else range(8,12)))
 carry_acceptance(meta,out/'asset.json',[ROOT/'tools/projectile_review/bomb_inspector.cpp',ROOT/'tools/projectile_review/bomb_asset.hpp',*sorted((ROOT/'tools/projectile_review/shaders').glob('bombs.*'))]);(out/'asset.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 accepted_review=(out/'REVIEW.md').read_text(encoding='utf-8') if meta['humanAcceptance']=='approved' and (out/'REVIEW.md').exists() else None
 (out/'REVIEW.md').write_text('# Bomb 人工验收\n\n待人工验收。模型碰撞与现有游戏攻击范围分别显示，不能混用。\n\n- [ ] 原图、正侧背面及灰模\n- [ ] 主体、球壳、光效和拖影分层\n- [ ] 模型碰撞及游戏范围对照\n- [ ] 单件、组合、动画关键帧、深浅背景\n',encoding='utf-8');print('CREATED',name,meta['triangles'])
 if accepted_review is not None:(out/'REVIEW.md').write_text(accepted_review,encoding='utf-8')

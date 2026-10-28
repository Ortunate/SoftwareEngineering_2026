"""生成灵梦三款实体、独立符纹贴图与同源封闭碰撞；不执行性能测试。"""
import bpy,bmesh,json,math,struct,sys
from pathlib import Path
import numpy as np
ROOT=Path(sys.argv[sys.argv.index('--')+1]).resolve()
bpy.context.preferences.filepaths.save_version=0
bpy.context.preferences.filepaths.temporary_directory=str(ROOT/'.cache/tmp')
sys.path.insert(0,str(ROOT/'tools/projectile_review'))
sys.dont_write_bytecode=True
from asset_acceptance import carry_acceptance
NAMES=('reimu_gold','reimu_cyan','reimu_needle')
def texture(out,cyan):
    """依据用户草图独立绘制对称弧框纹样与中央符印，RGBA 像素；不放大或拉伸原精灵。"""
    n=512;a=np.zeros((n,n,4),dtype=np.float32);a[:]=(.03,.24,.55,1) if cyan else (.62,.035,.018,1)
    tint=(.05,.9,1,1) if cyan else (1,.65,.025,1)
    def rect(x,y,w,h,c):a[y:y+h,x:x+w]=c
    rect(20,20,472,472,tint);rect(35,35,442,442,(1,.98,.88,1));rect(51,51,410,410,tint);rect(66,66,380,380,(.025,.48,.8,1) if cyan else (.95,.28,.025,1))
    # 用户根目录草图：左右长弧框、四角短钩与两侧三道横划。
    # 先绘制一侧，再绕像素间中心旋转复制；中央文字不属于这个纹样遮罩。
    yy,xx=np.mgrid[:n,:n];pattern=np.zeros((n,n),dtype=np.float32)
    def ornament(points,width=12):
        """以模型贴图像素为单位描线，半像素覆盖度抗锯齿；只修改外围遮罩。"""
        for (x0,y0),(x1,y1) in zip(points,points[1:]):
            dx=x1-x0;dy=y1-y0;length2=dx*dx+dy*dy
            if length2<1e-9:continue
            t=np.clip(((xx-x0)*dx+(yy-y0)*dy)/length2,0,1)
            distance=np.sqrt((xx-x0-t*dx)**2+(yy-y0-t*dy)**2)
            np.maximum(pattern,np.clip(width/2+.5-distance,0,1),out=pattern)
    def curve(a,b,c,d):
        """三次曲线只用于圆润转角，输出连续像素路径供纹样描线。"""
        return [(float((1-t)**3*a[0]+3*(1-t)**2*t*b[0]+3*(1-t)*t*t*c[0]+t**3*d[0]),
                 float((1-t)**3*a[1]+3*(1-t)**2*t*b[1]+3*(1-t)*t*t*c[1]+t**3*d[1])) for t in np.linspace(0,1,24)]
    # 竖段垂直、横段水平；所有圆角保持水平/竖直切线，去除草图笔势造成的歪斜。
    ornament([(220,66),(170,66)]+curve((170,66),(150,66),(140,76),(140,96))+[(140,415)]+curve((140,415),(140,435),(150,445),(170,445))+[(220,445)])
    ornament([(110,110),(98,110)]+curve((98,110),(84,110),(78,116),(78,130))+[(78,180)])
    ornament(((110,149),(110,178)),10)
    ornament(((84,216),(110,216)),12)
    ornament(((78,255.5),(140,255.5)),12)
    # 四角及两侧横划同时轴对齐；中心对称只应用于装饰，不作用于文字。
    pattern=np.maximum(pattern,pattern[::-1,:])
    pattern=np.maximum(pattern,pattern[:,::-1])
    a[:256]=a[::-1,::-1][:256].copy()
    ornament_color=np.array((1,1,.94,1),dtype=np.float32)
    a[:]=a*(1-pattern[:,:,None])+ornament_color*pattern[:,:,None]
    rect(163,78,186,356,(1,.97,.86,1));ink=(.36,.78,.96,1) if cyan else (1,.65,.38,1)
    # “博”按十、甫、寸的笔画结构独立绘制；左旁与右部留白，保留右上点和寸点。
    # 像素数组自下向上，对笔画坐标翻转，保证正反面 UV 中文字均正立。
    def stroke(points,width=13):
        for (x0,y0),(x1,y1) in zip(points,points[1:]):
            steps=max(abs(x1-x0),abs(y1-y0))*2+1
            for t in np.linspace(0,1,steps):
                x=round(x0+(x1-x0)*t);y=511-round(y0+(y1-y0)*t)
                rect(x-width//2,y-width//2,width,width,ink)
    for points in (
        ((181,206),(218,206)),((200,151),(200,363)),
        ((228,168),(331,168)),((278,139),(278,292)),
        ((234,195),(234,282)),((234,195),(324,195),(324,282)),
        ((234,224),(324,224)),((234,253),(324,253)),((234,282),(324,282)),
        ((310,143),(322,154)),
        ((224,316),(334,316)),((306,294),(306,375),(292,385)),
        ((244,340),(257,354))):stroke(points)
    image=bpy.data.images.new('seal' ,width=n,height=n,alpha=True);image.pixels.foreach_set(a.reshape(-1));image.filepath_raw=str(out/'seal.png');image.file_format='PNG';image.save();return image
for kind,name in enumerate(NAMES):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 out=ROOT/'assets/projectiles'/name;out.mkdir(parents=True,exist_ok=True)
 mat=bpy.data.materials.new(name);mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Roughness'].default_value=.65
 if kind<2:
  img=texture(out,kind==1);node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=img;mat.node_tree.links.new(node.outputs['Color'],bs.inputs['Base Color'])
  # Blender XY 对应导出后的 XY，正面沿 +Z；薄实体边缘有真实倒角。
  bpy.ops.mesh.primitive_cube_add();obj=bpy.context.object;obj.scale=(.5,.5,.035);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
  bevel=obj.modifiers.new('Edge bevel','BEVEL');bevel.width=.018;bevel.segments=2;bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=bevel.name)
  uv=obj.data.uv_layers.active
  for poly in obj.data.polygons:
   for li in poly.loop_indices:
    co=obj.data.vertices[obj.data.loops[li].vertex_index].co;uv.data[li].uv=(co.x+.5 if poly.normal.z>0 else .5-co.x,co.y+.5) if abs(poly.normal.z)>.9 else (.015,.015)
 else:
  v=[(0,1.1,0)];rings=[]
  for y,r in ((.65,.025),(-.65,.075),(-1.1,.035)):
   rings.append(len(v))
   for i in range(8):v.append((r*math.cos(i*math.tau/8),y,r*math.sin(i*math.tau/8)))
  v.append((0,-1.1,0));f=[]
  for i in range(8):f.append((0,1+i,1+(i+1)%8))
  for aa,bb in zip(rings,rings[1:]):
   for i in range(8):j=(i+1)%8;f.append((aa+i,bb+i,bb+j,aa+j))
  for i in range(8):f.append((len(v)-1,rings[-1]+(i+1)%8,rings[-1]+i))
  mesh=bpy.data.meshes.new(name);mesh.from_pydata(v,[],f);obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj);bpy.context.view_layer.objects.active=obj;obj.select_set(True);bs.inputs['Base Color'].default_value=(.88,.85,1,1)
 obj.name=name;obj.data.materials.append(mat)
 bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 # 预先绕 X 转换为 Blender Z-up，glTF 导出后保持约定 +Y 前方。
 for v in obj.data.vertices:x,y,z=v.co;v.co=(x,-z,y)
 obj.data.calc_loop_triangles();tris=obj.data.loop_triangles
 with (out/'collision.bin').open('wb') as stream:
  stream.write(b'KCOL0001');stream.write(struct.pack('<II',1,len(tris)))
  for t in tris:
   values=[]
   for i in t.vertices:p=obj.data.vertices[i].co;values.extend((p.x,p.z,-p.y))
   stream.write(struct.pack('<9f',*values))
 # 针的周边光效为开口圆锥侧面，尖端与针尖重合，尾端半径扩大。
 # 光效单独导出；不加入前面已经写出的封闭碰撞网格。
 glow_obj=None
 if kind==2:
  vertices=[(0,0,1.1)];faces=[];segments=32
  for j in range(1,6):
   t=j/5;r=.24*t;y=1.1-2.2*t
   for i in range(segments):
    angle=i*math.tau/segments;vertices.append((r*math.cos(angle),-r*math.sin(angle),y))
  for i in range(segments):faces.append((0,1+i,1+(i+1)%segments))
  for j in range(4):
   aa=1+j*segments;bb=aa+segments
   for i in range(segments):k=(i+1)%segments;faces.append((aa+i,bb+i,bb+k,aa+k))
  mesh=bpy.data.meshes.new('ConeGlow');mesh.from_pydata(vertices,[],faces);mesh.update()
  glow_obj=bpy.data.objects.new('ConeGlow',mesh);bpy.context.collection.objects.link(glow_obj)
  glow_mat=bpy.data.materials.new('ConeGlow');glow_mat.use_nodes=True
  shader=glow_mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Base Color'].default_value=(.56,.35,1,1);shader.inputs['Alpha'].default_value=.48
  glow_mat.surface_render_method='DITHERED';glow_obj.data.materials.append(glow_mat);glow_obj['collision']=False
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
 bpy.ops.export_scene.gltf(filepath=str(out/(name+'.gltf')),export_format='GLTF_SEPARATE',use_selection=True,export_yup=True)
 if glow_obj is not None:
  obj.select_set(False);glow_obj.select_set(True);bpy.context.view_layer.objects.active=glow_obj
  bpy.ops.export_scene.gltf(filepath=str(out/'glow.gltf'),export_format='GLTF_SEPARATE',use_selection=True,export_yup=True)
 meta=dict(asset=name,triangles=len(tris),displayTrianglesWithGlow=len(tris)*2 if kind<2 else len(tris)+288,triangleHardLimit=256 if kind<2 else 384,humanAcceptance='pending',axis='+Y',collision='closed solid; glow excluded',texture='seal.png' if kind<2 else None,referenceAnm=64+kind)
 if kind==2:meta['glow']={'asset':'glow.gltf','shape':'open cone side','triangles':288,'tipY':1.1,'baseY':-1.1,'baseRadius':.24,'collides':False}
 previous=out/'asset.json'
 carry_acceptance(meta,previous,[ROOT/'tools/projectile_review/reimu_inspector.cpp',*sorted((ROOT/'tools/projectile_review/shaders').glob('reimu_cone.*'))])
 accepted_review=(out/'REVIEW.md').read_text(encoding='utf-8') if meta['humanAcceptance']=='approved' and (out/'REVIEW.md').exists() else None
 (out/'asset.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
 (out/'REVIEW.md').write_text('# 灵梦弹体独立验收\n\n待人工验收。外围纹样依据用户根目录草图制作长弧框、角钩和横划，保持中心对称；中央仅一枚正立“博”字，不参与对称；针弹周围为增强亮度的圆锥侧面光效，不参与碰撞。需重点对照原图还原程度。\n\n- [ ] 正侧背轮廓和符纹\n- [ ] 光效关闭后实体完整\n- [ ] 碰撞叠加及仅碰撞\n- [ ] 深浅背景和游戏透明度\n',encoding='utf-8')
 if accepted_review is not None:(out/'REVIEW.md').write_text(accepted_review,encoding='utf-8')
 print('CREATED',name,meta)

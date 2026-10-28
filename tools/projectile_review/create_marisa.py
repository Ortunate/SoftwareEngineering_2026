"""生成魔理沙四种基础模型、原始动画档位和同源碰撞；工具依赖仅使用项目 Blender。"""
import bpy,bmesh,math,json,struct,sys,hashlib
from pathlib import Path
ROOT=Path(sys.argv[sys.argv.index('--')+1]).resolve()
bpy.context.preferences.filepaths.save_version=0
bpy.context.preferences.filepaths.temporary_directory=str(ROOT/'.cache/tmp')
sys.path.insert(0,str(ROOT/'tools/projectile_review'))
sys.dont_write_bytecode=True
from asset_acceptance import carry_acceptance
BASE=ROOT/'assets/projectiles'
NAMES=('marisa_primary','marisa_missile','marisa_laser','marisa_explosion')

def native_variants():
    """读取 ANM 原始脚本，保留指令时间和参数；帧单位为 1/60 秒。"""
    data=(ROOT/'assets/runtime/battle/native/player01.anm').read_bytes();n,m=struct.unpack_from('<II',data);result={}
    for i in range(m):
        sid,at=struct.unpack_from('<II',data,64+4*n+8*i)
        if sid not in list(range(64,72))+list(range(97,101)):continue
        ops=[];scale=[1.,1.];alpha=255;rgb=[255,255,255];growth=[0.,0.]
        while True:
            frame,op,size=struct.unpack_from('<hBB',data,at)
            args=list(struct.unpack_from('<'+'I'*(size//4),data,at+4))
            if op in (2,11):args=list(struct.unpack_from('<'+'f'*(size//4),data,at+4))
            ops.append(dict(frame=frame,opcode=op,args=args));at+=4+size
            if frame==0:
                if op==2:scale=args
                if op==3:alpha=args[0]
                if op==4:rgb=[args[0]>>16&255,args[0]>>8&255,args[0]&255]
                if op==11:growth=args
            if op==0:break
        result[sid]=dict(script=sid,scale=scale,alpha=alpha,color=rgb,scalePerFrame=growth,instructions=ops)
    return result

VARIANTS=native_variants()
def material(name,color):
    """建立 glTF 可携带的纯色材质；最终光效由独立着色器绘制。"""
    m=bpy.data.materials.new(name);m.use_nodes=True;m.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(*color,1);return m

def mesh(name,vertices,faces,mat,solid=True):
    """输入为 +Y 朝前的模型坐标；保存为 Blender Z-up，导出后恢复 +Y。"""
    data=bpy.data.meshes.new(name);data.from_pydata([(x,-z,y) for x,y,z in vertices],[],faces);data.update()
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);obj.data.materials.append(mat);obj['collision']=solid
    for p in data.polygons:p.use_smooth=True
    return obj

def spindle(name,profile,segments,mat,center=(0,0,0),solid=True):
    """按轴向 (Y,半径) 轮廓生成闭合旋转体，两端必须半径为零。"""
    v=[];rings=[]
    for y,r in profile:
        rings.append(list(range(len(v),len(v)+(1 if r==0 else segments))))
        for i in range(1 if r==0 else segments):
            a=math.tau*i/segments;v.append((center[0]+r*math.cos(a),center[1]+y,center[2]+r*math.sin(a)))
    f=[]
    for aa,bb in zip(rings,rings[1:]):
        for i in range(segments):
            j=(i+1)%segments
            if len(aa)==1:f.append((aa[0],bb[i],bb[j]))
            elif len(bb)==1:f.append((aa[i],bb[0],aa[j]))
            else:f.append((aa[i],bb[i],bb[j],aa[j]))
    return mesh(name,v,f,mat,solid)

def sphere(name,r,mat,solid=True):
    profile=[(r,0)]+[(r*math.cos(math.pi*i/8),r*math.sin(math.pi*i/8)) for i in range(1,8)]+[(-r,0)]
    return spindle(name,profile,20,mat,solid=solid)

def triangle_count(obj):
    """按导出三角化计数，包含显示装饰；不把四边形按一面计。"""
    obj.data.calc_loop_triangles();return len(obj.data.loop_triangles)

def export_group(objects,path):
    """显式选择组内对象，避免光效混进主体或碰撞导出。"""
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLTF_SEPARATE',use_selection=True,export_yup=True)

for kind,name in enumerate(NAMES):
    if "--only-trails" in sys.argv and kind>1:continue
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    out=BASE/name;out.mkdir(parents=True,exist_ok=True)
    bodymat=material('Body',(.9,.97,1) if kind==0 else (.4,1,.12) if kind in (1,3) else (1,.45,.55));glowmat=material('Glow',(1,1,1))
    body=[];glow=[]
    if kind==0:
        for x in (-.17,.17):
            body.append(spindle('TwinCore',[(.95,0),(.6,.055),(-.4,.085),(-.85,0)],8,bodymat,(x,0,0)))
            glow.append(spindle('TwinAura',[(.7,0),(.15,.075),(-.55,.1),(-1.45,.045),(-1.85,0)],12,glowmat,(x,0,0),False))
    elif kind==1:
        body.append(spindle('DiamondCore',[(.85,0),(.1,.2),(-.85,0)],8,bodymat))
        # 三层箭翼分别沿 X/Z 交叉；闭合三角棱柱避免侧视消失，翼尖均属于实体。
        for level,width in ((.58,.43),(.16,.55),(-.25,.48)):
            for axis in range(2):
                for sign in (-1,1):
                    points=[(0,level+.22),(.12,level-.06),(width,level-.12)]
                    v=[]
                    for z in (-.028,.028):
                        for x,y in points:v.append((x*sign,y,z) if axis==0 else (z,y,x*sign))
                    body.append(mesh('ArrowWing',v,[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],bodymat))
        glow.append(spindle('GreenAura',[(-.3,0),(-.55,.1),(-.8,.18),(-1.35,.11),(-1.9,0)],12,glowmat,solid=False))
    elif kind==2:
        # 参数化胶囊：端点在 Y=0/1，端帽单位半径；着色器只拉伸中段。
        profile=[(2,0)]+[(1+math.sin(a),math.cos(a)) for a in (math.pi/3,math.pi/6,0)]+[(math.sin(a),math.cos(a)) for a in (0,-math.pi/6,-math.pi/3)]+[(-1,0)]
        body.append(spindle('LaserCore',profile,12,bodymat));glow.append(spindle('LaserHalo',profile,12,glowmat,solid=False))
    else:
        body.append(sphere('DangerSphere',.5,bodymat))
        glow.append(sphere('OuterShell',.58,glowmat,False))
        # 火焰瓣沿球面均匀分布；每条带有弯曲与宽度变化，不用正面贴片组成圆环。
        for i in range(48):
            y=1-2*(i+.5)/48;a=i*math.pi*(3-math.sqrt(5));rad=math.sqrt(1-y*y)
            normal=(rad*math.cos(a),y,rad*math.sin(a));tangent=(-math.sin(a),0,math.cos(a));cross=(-y*math.cos(a),rad,-y*math.sin(a))
            v=[]
            for j in range(5):
                t=j/4;r=.52+.14*math.sin(t*math.pi/2);bend=.3*t*t;w=max(.002,.075*math.sin((.15+.85*t)*math.pi))
                for side in (-1,1):v.append(tuple(normal[k]*r+tangent[k]*bend+cross[k]*w*side for k in range(3)))
            glow.append(mesh('CurledFlame',v,[(2*j,2*j+1,2*j+3,2*j+2) for j in range(4)],glowmat,False))
    # 碰撞先按主体逐个闭合部件写入；光效从不写入 KCOL。
    if kind!=2:
        with (out/'collision.bin').open('wb') as f:
            f.write(b'KCOL0001');f.write(struct.pack('<I',len(body)))
            for o in body:
                triangle_count(o);f.write(struct.pack('<I',len(o.data.loop_triangles)))
                for tri in o.data.loop_triangles:
                    values=[]
                    for i in tri.vertices:p=o.data.vertices[i].co;values.extend((p.x,p.z,-p.y))
                    f.write(struct.pack('<9f',*values))
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
    export_group(body,out/(name+'.gltf'));export_group(glow,out/'glow.gltf')
    scripts=[[64],[65,66,67,68],[69,70,71],[97,98,99,100]][kind]
    meta=dict(asset=name,kind=kind,triangles=sum(map(triangle_count,body)),glowTriangles=sum(map(triangle_count,glow)),triangleHardLimit=[320,640,384,960][kind],axis='+Y',humanAcceptance='pending',variants=[VARIANTS[s] for s in scripts],collision={'shape':'parameterized capsule','parameters':['origin','unitDirection','length','radius']} if kind==2 else {'shape':'closed mesh union','file':'collision.bin'},glowCollides=False)
    meta['displayTrianglesWithGlow']=meta['triangles']+meta['glowTriangles']
    if kind<2:meta['trail']={'direction':'-Y','lengthScaleDefault':1,'widthScaleDefault':1,'periodSeconds':.6,'brightnessModulation':.1,'collides':False}
    if kind==3:meta['collisionDiametersPixels']=[32,42,48,48]
    carry_acceptance(meta,out/'asset.json',[ROOT/'tools/projectile_review/marisa_inspector.cpp',ROOT/'tools/projectile_review/marisa_asset.hpp',*sorted((ROOT/'tools/projectile_review/shaders').glob('marisa.*'))])
    accepted_review=(out/'REVIEW.md').read_text(encoding='utf-8') if meta['humanAcceptance']=='approved' and (out/'REVIEW.md').exists() else None
    (out/'asset.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (out/'REVIEW.md').write_text('# 魔理沙弹体验收\n\n待人工验收。\n\n- [ ] 原图与所有档位\n- [ ] 正侧背视图、灰模及独立光效\n- [ ] 实体碰撞，装饰无碰撞\n- [ ] 深浅背景及动画关键帧\n\n通过根目录 Review-Marisa.cmd 查看。\n',encoding='utf-8')
    if accepted_review is not None:(out/'REVIEW.md').write_text(accepted_review,encoding='utf-8')
    print('CREATED',name,meta['displayTrianglesWithGlow'])

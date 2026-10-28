"""逐件制作九类原版风格弹体；独立部件、角色遮罩和最终实体碰撞同源导出。"""
import bpy
import bmesh
import math
import struct
import json
import sys
from pathlib import Path

ROOT = Path(sys.argv[sys.argv.index('--')+1]).resolve()
BASE = ROOT/'assets/projectiles'
bpy.context.preferences.filepaths.temporary_directory = str(ROOT/'.cache/tmp')
bpy.context.preferences.filepaths.save_version = 0
# 编号、目录、有效色数、预算、特征及危险体积分别登记，颜色不复制几何。
SPECS = [
    (5,'needle_classic',16,96,'双端尖棱晶、直柱中段、白色主面和彩色侧棱','全部棱晶实体'),
    (8,'knife_classic',8,256,'短直刀、短护手、中央贯通孔的圆柱柄，纯色刃脊','刀刃、护手、穿孔圆柱柄实体并集；柄孔为空'),
    (2,'rice_classic',16,160,'圆收两端的细长米粒，不使用尖晶端部','闭合纺锤实体'),
    (0,'pellet_classic',16,160,'小型圆珠、明亮中心、较清楚的外边界','闭合圆珠实体'),
    (3,'orb_classic',16,320,'白亮内核、半透明球壳、柔和边缘','外壳外表面围成的整个实体，内部有碰撞'),
    (1,'ring_orb_classic',16,320,'白亮内核、暗间隔与独立彩色环带，最外层为半透明球壳','外壳外表面围成的整个实体；非环面'),
    (6,'large_orb_classic',8,512,'饱满亮核和更圆润半透明球壳','整个外球体内部有碰撞'),
    (9,'bubble_classic',5,768,'收紧内部球、加厚白亮外球壳与静态三维放射尖丝','白色光圈内缘以内的整个球体有碰撞（半径 0.40）；白色光圈不参与'),
    (7,'fireball_classic',1,384,'稳定白黄热核与红橙色立体火焰瓣、循环形变','稳定热核有碰撞，外焰和尾部是装饰'),
]
CHECKS={
    5:['双端尖晶、直柱中段与细彩棱符合 S94–109，不应像缝衣针','转到侧背面仍有实体厚度','探针检查上下尖端与柱体外侧；三维旋转和缩放后仍贴合'],
    8:['S122–129 的短直刀、护手和圆柱柄均可辨认，不是苦无三角刃','沿 Z 方向贯穿圆柱柄中部的纵孔；孔内 CLEAR，孔壁 CONTACT，侧面是完整圆柱','检查刀背、刃口、护手端部及部件交界，不允许整个外包盒冒充碰撞'],
    2:['S46–61 的细长米粒比例、圆收端与柔亮面均清楚','与针形晶体区分，侧面不能变成薄片','端点、弧面和外侧逐点检查，转向后危险体积随模型变化'],
    0:['S14–29 的小圆珠轮廓与较实边界清楚','在小尺寸画布下与柔边小玉有区别，色号可读','球面探针接触，体积中心为 CONTACT，光效外部为 CLEAR'],
    3:['S62–77 的明亮中心与柔边轮廓可辨，最外层确为三维透明球壳','关闭球壳时可独立查看内核；转到背面、浅色底和重叠场景检查层次','透明壳包围的内部有碰撞，不能只让小内核伤害'],
    1:['S30–45 的亮中心与明显外圈区别于 Type 3','外圈来自球壳表面，不能把弹体变成可穿过的圆环','球心 CONTACT；检查壳边界、透明度、颜色和重叠泛白'],
    6:['S110–117 的饱满大玉有白亮内核和较圆润外球壳','近镜头检查轮廓分段、内外层接缝及多个大玉重叠','整个外体积参与碰撞，实际游戏尺寸需后续接入时独立指定'],
    9:['etama4 S0–4 的暗透中心、白亮外缘和密集放射尖丝清楚；白色款尖丝弱化','48 根静态尖丝分布于两球之间，正侧背均有真实空间层次','内球 CONTACT；球间、尖丝和外壳 CLEAR；关闭显示层不改变碰撞'],
    7:['S118–121 的白黄热核、红橙外焰和向上伸展轮廓清楚','暂停并在 0/5/10/15 内容帧检查变化，20 帧循环接缝无突跳','热核 CONTACT，装饰火舌 CLEAR；危险核心可读，不被巨大拖尾混淆'],
}

def begin():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    mat=bpy.data.materials.new('Projectile_VertexRoles');mat.use_nodes=True
    nodes=mat.node_tree.nodes;bsdf=nodes.get('Principled BSDF');nodes.remove(bsdf)
    color=nodes.new('ShaderNodeVertexColor');color.layer_name='SurfaceRoles'
    channels=nodes.new('ShaderNodeSeparateColor');mat.node_tree.links.new(color.outputs['Color'],channels.inputs['Color'])
    tint=nodes.new('ShaderNodeMixRGB');tint.inputs[1].default_value=(1,.035,.06,1);tint.inputs[2].default_value=(1,.96,.92,1)
    mat.node_tree.links.new(channels.outputs['Red'],tint.inputs[0])
    emission=nodes.new('ShaderNodeEmission');mat.node_tree.links.new(tint.outputs[0],emission.inputs['Color'])
    transparent=nodes.new('ShaderNodeBsdfTransparent');mix=nodes.new('ShaderNodeMixShader')
    # 源文件材质预览将角色编码还原为红白实体/透明层，避免把 G/B 数据直接显示成绿色。
    alpha=nodes.new('ShaderNodeMath');alpha.operation='GREATER_THAN';alpha.inputs[1].default_value=.25
    mat.node_tree.links.new(channels.outputs['Green'],alpha.inputs[0])
    fade=nodes.new('ShaderNodeMath');fade.operation='MULTIPLY';fade.inputs[1].default_value=.6;mat.node_tree.links.new(alpha.outputs[0],fade.inputs[0])
    mat.node_tree.links.new(fade.outputs[0],mix.inputs[0]);mat.node_tree.links.new(emission.outputs[0],mix.inputs[1]);mat.node_tree.links.new(transparent.outputs[0],mix.inputs[2]);mat.node_tree.links.new(mix.outputs[0],nodes.get('Material Output').inputs['Surface'])
    mat.diffuse_color=(1,.035,.06,1)
    return mat,[]

def mesh(name, vertices, faces, role=0, collision=True, smooth=False, style='facet'):
    data=bpy.data.meshes.new(name);data.from_pydata([(x,-z,y) for x,y,z in vertices],[],faces);data.update()
    bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
    obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);obj.data.materials.append(material)
    attr=data.color_attributes.new(name='SurfaceRoles',type='FLOAT_COLOR',domain='CORNER');data.color_attributes.active_color=attr
    for face in data.polygons:
        face.use_smooth=smooth
        for loop in face.loop_indices:
            vertex=data.vertices[data.loops[loop].vertex_index].co
            normal=vertex.normalized() if smooth else face.normal
            front=abs(normal.y)
            mask=(.97 if normal.x<.02 and front>.15 else .10) if style=='facet' else .35
            # G=角色/2：0实体、0.5球壳、1无碰撞火焰或静态尖丝；B仅供火焰使用。
            attr.data[loop].color=(mask,role/2,((len(parts)*.271)%1),1)
    obj['surface_role']=role;obj['collision']=collision;parts.append(obj);return obj

def sphere(name,radii,segments,bands,center=(0,0,0),role=0,collision=True):
    # 每极点只保留一个顶点，避免 UV 球极点重合和退化三角形。
    rx,ry,rz=radii;cx,cy,cz=center;v=[(cx,cy+ry,cz)]
    for j in range(1,bands):
        a=math.pi*j/bands
        for i in range(segments):
            b=math.tau*i/segments;v.append((cx+rx*math.sin(a)*math.cos(b),cy+ry*math.cos(a),cz+rz*math.sin(a)*math.sin(b)))
    bottom=len(v);v.append((cx,cy-ry,cz));f=[]
    for i in range(segments):f.append((0,1+i,1+(i+1)%segments))
    for j in range(bands-2):
        for i in range(segments):
            a=1+j*segments+i;b=1+j*segments+(i+1)%segments;f.append((a,a+segments,b+segments,b))
    last=1+(bands-2)*segments
    for i in range(segments):f.append((bottom,last+(i+1)%segments,last+i))
    return mesh(name,v,f,role,collision,True,'round')

def prism(name,outline,depth):
    n=len(outline);v=[(x,y,z) for z in (-depth,depth) for x,y in outline]
    f=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    for i in range(n):j=(i+1)%n;f.append((i,j,j+n,i+n))
    return mesh(name,v,f)

def ring(name,cx,cy,major,minor,segments=12,cross=4):
    v=[];f=[]
    for i in range(segments):
        a=math.tau*i/segments
        for j in range(cross):
            b=math.tau*j/cross;r=major+minor*math.cos(b);v.append((cx+r*math.cos(a),cy+r*math.sin(a),minor*math.sin(b)))
    for i in range(segments):
        for j in range(cross):f.append((i*cross+j,((i+1)%segments)*cross+j,((i+1)%segments)*cross+(j+1)%cross,i*cross+(j+1)%cross))
    return mesh(name,v,f,smooth=True)

def needle():
    cross=[(-.125,-.045),(-.075,-.075),(.075,-.075),(.125,-.045),(.125,.045),(.075,.075),(-.075,.075),(-.125,.045)]
    v=[(0,.5,0)]+[(x,y,z) for y in (.32,-.32) for x,z in cross]+[(0,-.5,0)]
    f=[]
    for i in range(8):j=(i+1)%8;f.extend([(0,1+i,1+j),(1+i,9+i,9+j,1+j),(17,9+j,9+i)])
    mesh('FacetedCrystal',v,f)

def knife():
    # 封闭双斜面刀刃；圆柱柄通过精确布尔开孔，不能用环面替代实心圆柱轮廓。
    outline=[(-.105,-.13),(.105,-.13),(.105,.25),(.075,.39),(0,.5),(-.075,.39),(-.105,.25)]
    n=len(outline);v=[(x,y,0) for x,y in outline]+[(0,.05,.034),(0,.05,-.034)]
    f=[]
    for i in range(n):j=(i+1)%n;f.extend([(i,j,n),(j,i,n+1)])
    mesh('DoubleBevelBlade',v,f)
    prism('ShortGuard',[(-.19,-.165),(.19,-.165),(.19,-.12),(-.19,-.12)],.042)
    n=12;v=[(.085*math.cos(math.tau*i/n),y,.085*math.sin(math.tau*i/n)) for y in (-.46,-.155) for i in range(n)]
    f=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    for i in range(n):j=(i+1)%n;f.append((i,j,j+n,i+n))
    handle=mesh('PiercedCylinderHandle',v,f,smooth=True)
    cutter=prism('TemporarySlot',[(.035*math.cos(math.tau*i/12),-.305+.095*math.sin(math.tau*i/12)) for i in range(12)],.15)
    bpy.context.view_layer.objects.active=handle
    mod=handle.modifiers.new('RealThroughHole','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter
    bpy.ops.object.modifier_apply(modifier=mod.name)
    parts.remove(cutter);bpy.data.objects.remove(cutter,do_unlink=True)
    bm=bmesh.new();bm.from_mesh(handle.data)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-6)
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-6)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(handle.data);bm.free()
    # 布尔产生的孔壁也属于实体角色；统一重写顶点编码，避免工具面材质残留。
    attr=handle.data.color_attributes['SurfaceRoles']
    for face in handle.data.polygons:
        for loop in face.loop_indices:attr.data[loop].color=(.55 if abs(face.normal.y)>.5 else .12,0,0,1)

def bubble():
    sphere('InnerVisualSphere',(.19,)*3,12,6,role=0,collision=False)
    sphere('TransparentOuterShell',(.5,)*3,16,10,role=1,collision=False)
    # Fibonacci 球面分布避免只在正面圆周布丝；每根闭合四棱锥六面，尖端指向内球。
    from mathutils import Vector
    for i in range(48):
        y=1-2*(i+.5)/48;a=i*math.pi*(3-math.sqrt(5));r=math.sqrt(1-y*y)
        d=Vector((r*math.cos(a),y,r*math.sin(a)));u=d.cross(Vector((0,1,0))).normalized();w=d.cross(u)
        base=d*(.435+.018*math.sin(i*2.7));width=.029+.009*(.5+.5*math.sin(i*3.1))
        v=[tuple(base+width*(u*math.cos(j*math.pi/2)+w*math.sin(j*math.pi/2))) for j in range(4)]
        v.append(tuple(d*(.195+.055*(.5+.5*math.sin(i*1.7)))))
        mesh('StaticRadialFilament_%02d'%i,v,[(3,2,1,0)]+[(j,(j+1)%4,4) for j in range(4)],2,False,False,'round')
    # 危险体积包含光圈以内的空隙和尖丝，不以最内侧暗球或单根尖丝表面替代。
    # 与外球壳采用相同拓扑，按光圈内缘比例 0.8 缩放，独立导出且不加入显示网格。
    boundary=sphere('CollisionInsideHalo',(.40,)*3,16,10)
    boundary['display']=False;boundary.hide_render=True;boundary.hide_set(True)

def flame():
    sphere('StableHotCore',(.245,.27,.215),10,5,(0,-.22,0))
    # 四个闭合渐细火舌沿热核展开，截面有厚度，形变只作用于装饰角色。
    for k,(x,z,h,w) in enumerate([(-.11,0,.78,.15),(.12,.035,.66,.14),(-.01,-.105,.89,.13),(.025,.105,.72,.13)]):
        v=[]
        for j,(height,profile) in enumerate([(0,.42),(.14,1),(.38,.74),(.64,.43),(.83,.16)]):
            y=-.40+height*h;r=w*profile
            for i in range(6):
                a=math.tau*i/6;v.append((x+r*math.cos(a)+.065*math.sin(height*4+k*2)*height,y,z+r*.75*math.sin(a)))
        v.append((x+.075*math.sin(k*2+1),-.40+h,z+.04*math.cos(k)));f=[tuple(range(5,-1,-1))]
        for j in range(4):
            for i in range(6):a=j*6+i;b=j*6+(i+1)%6;f.append((a,b,b+6,a+6))
        for i in range(6):f.append((24+i,24+(i+1)%6,30))
        mesh('FlameTongue_'+str(k),v,f,2,False,True,'round')

for kind,name,colors,limit,description,danger in SPECS:
    # 单件返修只重建指定类别，避免覆盖其他类别的人工验收记录。
    if '--only-bubble' in sys.argv and kind!=9:continue
    material,parts=begin();out=BASE/name;out.mkdir(parents=True,exist_ok=True)
    if kind==5:needle()
    elif kind==8:knife()
    elif kind==2:sphere('RoundedRice',(.19,.5,.19),12,6)
    elif kind==0:sphere('SolidPellet',(.5,.5,.5),12,6)
    elif kind in (1,3):
        sphere('BrightCore',(.41 if kind==3 else .375,)*3,14,6,collision=False)
        sphere('TransparentOuterShell',(.5,)*3,14,7,role=1)
    elif kind==6:
        sphere('FullBrightCore',(.40,)*3,16,8,collision=False)
        sphere('TransparentOuterShell',(.5,)*3,16,10,role=1)
    elif kind==9:bubble()
    elif kind==7:flame()
    info={'asset':name,'nativeType':kind,'effectiveColors':colors,'representativeColor':1 if kind in (6,8,9) else 0 if kind==7 else 2,
          'renderMeshes':1,'textureMaps':0,'renderMaterial':'collection shader / vertex surface roles',
          'description':description,'collisionSemantics':danger,'humanAcceptance':'pending','tipAxis':'+Y','frontAxis':'+Z',
          'units':'normalized asset coordinates; sphere diameter 1; directional body length approximately 1',
          'triangleHardLimit':limit,'revision':2,'vertexChannels':{'r':'fixed highlight mask','g':'role / 2 (body, shell, decorative flame or static filament)','b':'flame phase'},'parts':[]}
    with (out/'collision.bin').open('wb') as stream:
        solids=[o for o in parts if o['collision']];stream.write(b'KCOL0001');stream.write(struct.pack('<I',len(solids)))
        for o in parts:
            o.data.calc_loop_triangles();tris=o.data.loop_triangles
            info['parts'].append({'name':o.name,'role':o['surface_role'],'collision':bool(o['collision']),'display':o.get('display',True),'triangles':len(tris)})
            if o['collision']:
                stream.write(struct.pack('<I',len(tris)))
                for t in tris:
                    values=[]
                    for i in t.vertices:p=o.data.vertices[i].co;values.extend((p.x,p.z,-p.y))
                    stream.write(struct.pack('<9f',*values))
    if kind==7:info['animation']={'referenceFrames':[0,5,10,15],'referenceSprites':[118,119,120,121],'loopFrames':20,'fps':60,'method':'continuous bounded deformation; stable collision core'}
    if kind==9:info['bubbleStructure']={'outerRadius':.5,'innerVisualRadius':.19,'collisionRadius':.40,'filaments':48,'animated':False,'radiusBasis':'entire volume inside white halo; same outer-shell topology scaled by 0.8'}
    info['triangles']=sum(p['triangles'] for p in info['parts'] if p['display'])
    info['collisionTriangles']=sum(p['triangles'] for p in info['parts'] if p['collision'])
    # 制作阶段仅导出，不在每件之后启动测试；全组导出后统一验证预算、流形和同源性。
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
    # .blend 保存可直接查看的材质预览；glTF 使用中性占位材质，角色编码由运行时着色器解释。
    # 显式连接原顶点属性，保持编码与已测版本一致；占位材质不是最终效果，须配套运行时解释。
    material.node_tree.nodes.clear()
    bsdf=material.node_tree.nodes.new('ShaderNodeBsdfPrincipled');bsdf.inputs['Base Color'].default_value=(1,1,1,1);bsdf.inputs['Roughness'].default_value=1
    color=material.node_tree.nodes.new('ShaderNodeVertexColor');color.layer_name='SurfaceRoles';material.node_tree.links.new(color.outputs['Color'],bsdf.inputs['Base Color'])
    output=material.node_tree.nodes.new('ShaderNodeOutputMaterial');material.node_tree.links.new(bsdf.outputs['BSDF'],output.inputs['Surface'])
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:
        if o.get('display',True):o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();bpy.context.object.name=name
    bpy.ops.export_scene.gltf(filepath=str(out/(name+'.gltf')),export_format='GLTF_SEPARATE',use_selection=True,export_yup=True,
        export_normals=True,export_materials='EXPORT',export_texcoords=False,export_tangents=False,
        export_vertex_color='ACTIVE',export_all_vertex_colors=False,export_active_vertex_color_when_no_material=True)
    (out/'asset.json').write_text(json.dumps(info,ensure_ascii=False,indent=2),encoding='utf-8')
    checks='\n'.join('- [ ] '+c for c in CHECKS[kind])
    composition='\n'.join(f"| {p['name']} | {p['triangles']} | {'实体碰撞' if p['collision'] else '显示装饰'} |" for p in info['parts'])
    (out/'REVIEW.md').write_text(f'''# {description}\n\n状态：待人工验收。Type {kind}，{colors} 个有效颜色，共享一个网格，代表色 C{info['representativeColor']:02}。\n\n几何总数 {info['triangles']} 三角形，实体碰撞 {info['collisionTriangles']} 三角形。预算硬上限 {limit}。\n\n| 部件 | 三角形 | 碰撞角色 |\n| --- | ---: | --- |\n{composition}\n\n碰撞约定：{danger}。颜色、透明度和外部光效不改变碰撞。\n\n无拉伸原精灵贴图；纯色、顶点亮面遮罩与独立程序化表面着色。源文件保留分部件及材质预览，glTF 合并网格，角色由 COLOR_0.g 编码，需配套运行时着色器解释。\n\n## 本件人工检查\n\n{checks}\n- [ ] 本类所有有效颜色、深浅背景、常用镜头尺寸及放大视图均检查\n- [ ] 独立碰撞单显与叠加已检查，明确同意本件通过\n\n使用根目录 Review-Projectiles.cmd 选择本件。参数、接口及统一测试记录见 [全组交付说明](../../../docs/PROJECTILE_COLLECTION_REVIEW.md)。本轮人工验收尚未通过，自动检查不能代签。\n''',encoding='utf-8')
    print('CREATED',kind,name,info['triangles'])

# 每类独立保存原目录预览；不是模型贴图。来源固定到已阅读的目录提交。
reference=BASE/'references';reference.mkdir(exist_ok=True)
commit='5b2e2dcc5248db518daff212f62f871f706bea11'
for n in list(range(10)):
    target=reference/f'enemy-{n:02}.png'
    if not target.exists():raise RuntimeError('Missing checked-in reference: '+str(target))
if not (reference/'fireball-frames.png').exists():raise RuntimeError('Missing checked-in fireball reference')
(reference/'SOURCE.md').write_text(f'# 参考图来源\n\n来自 heny/stages 提交 `{commit}` 的 `docs/images/projectiles/`，为原游戏素材的目录预览，仅用于项目内对照；不作为模型表面贴图。所有权归原素材权利人。\n\n火球时序另核对当前 etama3.anm 脚本 7：0/5/10/15 内容帧选择 S118/S119/S120/S121，第 20 帧跳回偏移 8，按 60 Hz 循环。三维版本以 1/3 秒连续形变表达这些相位，热核不形变。\n',encoding='utf-8')

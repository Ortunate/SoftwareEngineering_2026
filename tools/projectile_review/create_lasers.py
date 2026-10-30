"""生成两套可参数化激光源模型；长度只改变中段，端帽保持半径比例。"""
import bpy
import bmesh
import math
import json
import sys
import shutil
from pathlib import Path

root=Path(sys.argv[sys.argv.index('--')+1]).resolve()
bpy.context.preferences.filepaths.temporary_directory=str(root/'.cache/tmp')
bpy.context.preferences.filepaths.save_version=0
for variant,name in enumerate(('laser_default','laser_round')):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    out=root/'assets/projectiles'/name;out.mkdir(parents=True,exist_ok=True)
    mat=bpy.data.materials.new(name);mat.diffuse_color=(1,.04,.08,1)
    parts=[]
    for role in range(2):
        # Y∈[-1,0] 为起点半球，[0,1] 为中段，[1,2] 为终点半球。
        # 两层共 288 三角形；半径和光效扩展由运行时决定，不重复资产。
        v=[(0,-1,0)];rings=[]
        for end in (0,1):
            for j in range(3):
                a=(-math.pi/3+j*math.pi/6) if end==0 else j*math.pi/6
                rings.append(len(v))
                for i in range(12):
                    b=math.tau*i/12;v.append((math.cos(a)*math.cos(b),end+math.sin(a),math.cos(a)*math.sin(b)))
        tip=len(v);v.append((0,2,0));f=[]
        for i in range(12):f.append((0,1+(i+1)%12,1+i))
        for a,b in zip(rings,rings[1:]):
            for i in range(12):j=(i+1)%12;f.append((a+i,a+j,b+j,b+i))
        for i in range(12):f.append((tip,rings[-1]+i,rings[-1]+(i+1)%12))
        data=bpy.data.meshes.new('Core' if role==0 else 'Halo');data.from_pydata([(x,-z,y) for x,y,z in v],[],f);data.update()
        bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
        attr=data.color_attributes.new(name='Roles',type='FLOAT_COLOR',domain='CORNER');data.color_attributes.active_color=attr
        for p in data.polygons:
            p.use_smooth=True
            for loop in p.loop_indices:attr.data[loop].color=(1,role,0,1)
        obj=bpy.data.objects.new(data.name,data);bpy.context.collection.objects.link(obj);obj.data.materials.append(mat);obj['role']=role;parts.append(obj)
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
    mat.use_nodes=True;nodes=mat.node_tree.nodes;color=nodes.new('ShaderNodeVertexColor');color.layer_name='Roles';mat.node_tree.links.new(color.outputs['Color'],nodes.get('Principled BSDF').inputs['Base Color'])
    bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join()
    bpy.ops.export_scene.gltf(filepath=str(out/(name+'.gltf')),export_format='GLTF_SEPARATE',use_selection=True,export_yup=True,export_texcoords=False,export_vertex_color='ACTIVE')
    meta=dict(asset=name,variant=variant,colors=16,triangles=288,triangleHardLimit=384,humanAcceptance='pending',axis='+Y',units='parameterized; endpoints at Y=0,1; cap offsets in radius units',collision=dict(shape='capsule',parameters=['origin','unitDirection','length','radius'],haloCollides=False),reference=dict(anm=variant,sprites=[variant*16,variant*16+15]),interpretation='continuous band' if variant==0 else 'round-profile adaptation; not yet selectable by game')
    # 重新导出不能覆盖用户已确认的验收状态。
    previous=out/'asset.json'
    if previous.exists():
        meta['humanAcceptance']=json.loads(previous.read_text(encoding='utf-8')).get('humanAcceptance','pending')
    meta.update(localForward=[0,1,0],localUp=[0,0,1],surfaceFront=[0,0,1])
    (out/'asset.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
    accepted_review=(out/'REVIEW.md').read_text(encoding='utf-8') if meta['humanAcceptance']=='approved' and (out/'REVIEW.md').exists() else None
    (out/'REVIEW.md').write_text(f'''# 敌方激光套组 {variant}\n\n待人工验收。16 色共享模型，288 三角形（本体/外光层各 144）。\n\n源几何的端点在 Y=0、1，半球半径为 1；需配套 laser_renderer 和 laser 着色器按实际长度、半径重建。普通 DrawModel 不会得到成品比例。碰撞是同端点、半径的解析胶囊，包含端帽；外光层无碰撞。无固定 collision.bin，参数契约见 asset.json。\n\n- [ ] 正侧端视及 16 色符合原图特征\n- [ ] 长宽独立调节，端帽无拉伸\n- [ ] 主体/光效/胶囊叠加，轴身、端帽、外光层探针正确\n- [ ] 预警、展开、持续、收束时窗正确\n- [ ] 交叉、入束、浅色背景与性能报告已检查\n\n套组 1 是原圆形光斑的三维适配，未改变关卡绑定。运行 Review-Lasers.cmd 独立查看。\n''',encoding='utf-8')
    if accepted_review is not None:(out/'REVIEW.md').write_text(accepted_review,encoding='utf-8')
refs=root/'assets/projectiles/references'
for i in range(2):shutil.copyfile(root/f'docs/images/projectiles/enemy-laser-{i}.png',refs/f'enemy-laser-{i}.png')

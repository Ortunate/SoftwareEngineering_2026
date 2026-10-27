"""制作原版风格苦无：低面数封闭实体、白亮面遮罩与同源碰撞，不改写实样品。"""
import bpy
import bmesh
import math
import struct
import json
import sys
from pathlib import Path
from collections import Counter

ROOT = Path(sys.argv[sys.argv.index('--') + 1]).resolve()
OUT = ROOT / 'assets/projectiles/kunai_classic'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.preferences.filepaths.temporary_directory = str(ROOT / '.cache/tmp')
bpy.context.preferences.filepaths.save_version = 0
parts = []
mat = bpy.data.materials.new('Classic_VertexMask')
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get('Principled BSDF')
bsdf.inputs['Base Color'].default_value = (1,1,1,1)
bsdf.inputs['Roughness'].default_value = 1


def make(name, positions, faces, smooth=False):
    """输入为导出坐标 +Y 弹尖；Blender 存储为 Z 向上，按部件保留真实孔洞。"""
    data = bpy.data.meshes.new(name)
    data.from_pydata([(x,-z,y) for x,y,z in positions], [], faces)
    data.update()
    # 自动统一面朝向；一个部件必须是封闭流形，不能靠双面贴片掩盖缺面。
    bm = bmesh.new(); bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data); bm.free()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    attr = data.color_attributes.new(name='HighlightMask', type='FLOAT_COLOR', domain='CORNER')
    data.color_attributes.active_color = attr
    for face in data.polygons:
        face.use_smooth = smooth
        # 遮罩绑在实体上：前后各有白色亮面和彩色侧面，不追随观察相机。
        front = -face.normal.y
        left = face.normal.x < 0
        weight = .94 if abs(front)>.2 and left else .14 if abs(front)>.2 else .04
        if smooth:
            weight = .94 if abs(front)>.2 else .04
        for index in face.loop_indices:
            attr.data[index].color = (weight,weight,weight,1)
    parts.append(obj)
    return obj


# 宽三角刃：两个浅四面体共享三角外缘，六个三角面。
make('Blade', [(-.255,-.10,0),(.255,-.10,0),(0,.50,0),(0,-.085,.055),(0,-.085,-.055)],
     [(0,1,3),(1,2,3),(2,0,3),(1,0,4),(2,1,4),(0,2,4)])
# 短连接柄为封闭薄盒，12 三角形；原图不包含长缠柄。
v=[(x,y,z) for y in [-.22,-.095] for z in [-.026,.026] for x in [-.037,.037]]
make('ShortConnector',v,[(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)])
# 16 周向 x 4 截面 = 128 三角形；平滑法线不增加碰撞或渲染面数。
v=[]; f=[]
for i in range(16):
    a=math.tau*i/16
    for j in range(4):
        b=math.tau*j/4
        r=.115+.036*math.cos(b)
        v.append((r*math.cos(a),-.335+r*math.sin(a),.036*math.sin(b)))
for i in range(16):
    for j in range(4):
        f.append((i*4+j,((i+1)%16)*4+j,((i+1)%16)*4+(j+1)%4,i*4+(j+1)%4))
make('TailRing',v,f,True)

summary={'asset':'kunai_classic','nativeType':4,'representativeColor':2,
         'units':'normalized length approximately 1','tipAxis':'+Y','frontAxis':'+Z',
         'renderMeshes':1,'textureMaps':0,'vertexMask':'COLOR_0.r = highlight mix',
         'humanAcceptance':'pending','parts':[]}
with (OUT/'collision.bin').open('wb') as stream:
    stream.write(b'KCOL0001');stream.write(struct.pack('<I',len(parts)))
    for obj in parts:
        obj.data.calc_loop_triangles()
        triangles=obj.data.loop_triangles
        stream.write(struct.pack('<I',len(triangles)))
        edges=Counter()
        for tri in triangles:
            points=[]
            for i in tri.vertices:
                p=obj.data.vertices[i].co
                points.extend((p.x,p.z,-p.y))
            stream.write(struct.pack('<9f',*points))
            for k in range(3):edges[tuple(sorted((tri.vertices[k],tri.vertices[(k+1)%3])))]+=1
        assert all(n==2 for n in edges.values()),obj.name
        summary['parts'].append({'name':obj.name,'triangles':len(triangles),'closed':True})
summary['triangles']=sum(p['triangles'] for p in summary['parts'])
assert 128<=summary['triangles']<=160
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'kunai_classic.blend'))
# 保存源文件的独立部件；仅导出时合并为一个网格，减少实例绘制次数。
bpy.ops.object.select_all(action='DESELECT')
for obj in parts:obj.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.object.join()
bpy.context.object.name='KunaiClassic'
bpy.ops.export_scene.gltf(filepath=str(OUT/'kunai_classic.gltf'),export_format='GLTF_SEPARATE',
    use_selection=True,export_yup=True,export_normals=True,export_materials='EXPORT',
    export_texcoords=False,export_tangents=False,export_cameras=False,export_lights=False,
    export_vertex_color='ACTIVE',export_all_vertex_colors=False,export_active_vertex_color_when_no_material=True)
(OUT/'asset.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print('CLASSIC_COMPLETE',json.dumps(summary))

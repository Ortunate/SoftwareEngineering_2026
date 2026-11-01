"""逐款将原图主轮廓挤出为闭合薄板，正反 UV 保持文字正读；Blender 项目内运行。"""
import bpy,sys,json,math
from pathlib import Path
root=Path(sys.argv[sys.argv.index('--')+1]).resolve()
bpy.context.preferences.filepaths.save_version=0
bpy.context.preferences.filepaths.temporary_directory=str(root/'.cache/tmp')
names=['power_small','point','power_large','bomb','full_power','life','star']
colors=[(.65,.025,.025,1),(.03,.08,.6,1),(.65,.025,.025,1),(.02,.4,.06,1),(.65,.45,.01,1),(.65,.025,.35,1),(.3,.35,.3,1)]
def hull(points):
    """主牌为凸轮廓；凸包平滑像素边缘而不将字形误作孔洞。"""
    points=sorted(set(points))
    def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    lower=[];upper=[]
    for p in points:
        while len(lower)>1 and cross(lower[-2],lower[-1],p)<=0:lower.pop()
        lower.append(p)
    for p in reversed(points):
        while len(upper)>1 and cross(upper[-2],upper[-1],p)<=0:upper.pop()
        upper.append(p)
    return lower[:-1]+upper[:-1]
for index,name in enumerate(names):
    out=root/'assets/items'/name
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    image=bpy.data.images.load(str(out/'surface.png'),check_existing=False)
    w,h=image.size;pixels=list(image.pixels)
    points=[(x,y) for y in range(h) for x in range(w) if pixels[(y*w+x)*4+3]>.8]
    contour=hull(points);xs=[p[0] for p in contour];ys=[p[1] for p in contour]
    cx=(min(xs)+max(xs))/2;cy=(min(ys)+max(ys))/2;scale=max(ys)-min(ys)+1
    contour=[((x-cx)/scale,(y-cy)/scale) for x,y in contour];n=len(contour)
    vertices=[(x,y,z) for z in (-.03,.03) for x,y in contour]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new('closed plate');mesh.from_pydata(vertices,[],faces);mesh.update()
    body=bpy.data.objects.new('Body',mesh);bpy.context.collection.objects.link(body);bpy.context.view_layer.objects.active=body;body.select_set(True)
    material=bpy.data.materials.new('Edge');material.diffuse_color=colors[index];body.data.materials.append(material)
    bevel=body.modifiers.new('Small edge bevel','BEVEL');bevel.width=.01;bevel.segments=1
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    mat=bpy.data.materials.new('Original surface');mat.use_nodes=True
    bs=mat.node_tree.nodes.get('Principled BSDF');tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
    mat.node_tree.links.new(tex.outputs['Color'],bs.inputs['Base Color']);mat.node_tree.links.new(tex.outputs['Alpha'],bs.inputs['Alpha']);bs.inputs['Roughness'].default_value=1
    mat.surface_render_method='DITHERED';mat.use_backface_culling=True
    # 正反各自以观看者右方为 U 正向；背面不镜像文字。透明余边完整保留原柔光。
    for back in (False,True):
        z=-.0305 if back else .0305
        corners=[(0,0),(w,0),(w,h),(0,h)]
        verts=[(((-1 if back else 1)*(x-.5-cx))/scale,(y-.5-cy)/scale,z) for x,y in corners]
        face=(0,1,2,3) # 背面顶点已沿 X 翻转，绕序自然朝外，不能再反转一次。
        me=bpy.data.meshes.new('surface');me.from_pydata(verts,[],[face]);me.update();uv=me.uv_layers.new()
        for poly in me.polygons:
            for li in poly.loop_indices:
                x,y=corners[me.loops[li].vertex_index];uv.data[li].uv=(x/w,y/h)
        obj=bpy.data.objects.new('Back' if back else 'Front',me);bpy.context.collection.objects.link(obj);me.materials.append(mat)
    # Blender Z-up 导出换轴，预先反换以使 glTF 正面 +Z、长轴 +Y。
    for obj in list(bpy.context.scene.objects):
        if obj.type=='MESH':
            for v in obj.data.vertices:x,y,z=v.co;v.co=(x,-z,y)
    bpy.ops.object.select_all(action='SELECT')
    triangles=0
    for obj in bpy.context.selected_objects:obj.data.calc_loop_triangles();triangles+=len(obj.data.loop_triangles)
    assert triangles<=1000
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
    bpy.ops.export_scene.gltf(filepath=str(out/(name+'.gltf')),export_format='GLTF_SEPARATE',use_selection=True)
    meta=dict(asset=name,source=json.loads((out/'source.json').read_text()),height=1,thickness=.06,bevel=.01,triangles=triangles,localUp=[0,1,0],surfaceFront=[0,0,1],rotationAxis=[0,1,0],collision='none; pickup radius remains gameplay-owned',humanAcceptance='pending')
    (out/'asset.json').write_text(json.dumps(meta,indent=2)+'\n')
    (out/'REVIEW.md').write_text('# 道具验收\n\n待人工验收。运行 tools/windows/Review-Items.cmd；旋转 GIF 位于 output/projectile-review/items。\n',encoding='utf-8')
    print(name,triangles)

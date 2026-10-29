"""制作非攻击副机实体及独立颜色纹理；全部路径限制在项目内。"""
import bpy, math, json, sys
from pathlib import Path
import numpy as np

ROOT = Path(sys.argv[sys.argv.index('--') + 1]).resolve()
sys.dont_write_bytecode=True
sys.path.insert(0,str(ROOT/'tools/projectile_review'))
from asset_acceptance import carry_acceptance
bpy.context.preferences.filepaths.save_version = 0
bpy.context.preferences.filepaths.temporary_directory = str(ROOT / '.cache/tmp')

def texture(out, star):
    """独立绘制正交投影颜色，像素自下而上；正反球面共用图案，侧缘保持连续底色。"""
    y,x=np.mgrid[:512,:512];x=(x+0.5)/256-1;y=(y+0.5)/256-1
    a=np.ones((512,512,4),dtype=np.float32)
    if star:
        # 五星是球面的独立纹理，圆形轮廓由真实球体提供。
        # 顶点两侧截短后用二次曲线连接，尖角与凹角都保持连续圆角。
        points=[(.77*math.cos(math.pi/2+i*math.pi/5),.77*math.sin(math.pi/2+i*math.pi/5)) if i%2==0 else (.36*math.cos(math.pi/2+i*math.pi/5),.36*math.sin(math.pi/2+i*math.pi/5)) for i in range(10)]
        rounded=[]
        for i,p in enumerate(points):
            prev=points[(i-1)%10];nxt=points[(i+1)%10]
            start=np.array(p)*.84+np.array(prev)*.16
            end=np.array(p)*.84+np.array(nxt)*.16
            for t in np.linspace(0,1,12):
                rounded.append(tuple((1-t)**2*start+2*(1-t)*t*np.array(p)+t*t*end))
        points=rounded
        distance=np.full(x.shape,10.)
        inside=np.zeros(x.shape,dtype=bool)
        for (ax,ay),(bx,by) in zip(points,points[1:]+points[:1]):
            dx=bx-ax;dy=by-ay;t=np.clip(((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy),0,1)
            distance=np.minimum(distance,np.sqrt((x-ax-t*dx)**2+(y-ay-t*dy)**2))
            if abs(dy)>1e-9:inside^=((ay>y)!=(by>y))&(x<(bx-ax)*(y-ay)/dy+ax)
        radius=np.sqrt(x*x+y*y);depth=np.sqrt(np.maximum(0,1-radius*radius))
        # 白色球底保留轻微明暗；颜色只落在星形轮廓，内外区域均保持白色。
        a[:,:,:3]=(.76+.24*depth[...,None])
        t=np.clip((x+ .77)/1.54,0,1)[...,None]
        purple=np.array((.58,.16,.95));blue=np.array((.06,.48,1.));green=np.array((.2,.9,.48))
        color=np.where(t<.5,purple+(blue-purple)*t*2,blue+(green-blue)*(t-.5)*2)
        # 有符号距离向外为正。外层轮廓与粗内层之间留出白色间隙，柔光只向外扩散。
        signed=np.where(inside,-distance,distance)[...,None]
        outer=np.exp(-((signed-.105)/.018)**4)*.65
        spread=np.where(signed>.105,np.exp(-((signed-.105)/.09)**2)*.3,0)
        coverage=np.maximum(outer,spread)
        a[:,:,:3]=a[:,:,:3]*(1-coverage)+color*coverage
        line=np.exp(-(distance[...,None]/.043)**4)
        a[:,:,:3]=a[:,:,:3]*(1-line)+color*line
    else:
        red=x>0
        red=np.where(x*x+(y-.5)**2<.25,True,red)
        red=np.where(x*x+(y+.5)**2<.25,False,red)
        a[red]=(.86,.015,.025,1)
        a[x*x+(y-.5)**2<.025]=(1,1,1,1)
        a[x*x+(y+.5)**2<.025]=(.035,.035,.045,1)
    image=bpy.data.images.new('surface',width=512,height=512,alpha=True)
    image.pixels.foreach_set(a.reshape(-1));image.filepath_raw=str(out/'surface.png');image.file_format='PNG';image.save()
    return image

for star,name in enumerate(('reimu_option','marisa_option')):
    if '--only-marisa' in sys.argv and not star:continue
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    out=ROOT/'assets/projectiles'/name;out.mkdir(parents=True,exist_ok=True)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,radius=.42)
    obj=bpy.context.object
    for p in obj.data.polygons:p.use_smooth=True
    obj.name=name
    uv=obj.data.uv_layers.active or obj.data.uv_layers.new()
    for face in obj.data.polygons:
        for li in face.loop_indices:
            co=obj.data.vertices[obj.data.loops[li].vertex_index].co
            uv.data[li].uv=(co.x/.84+.5,co.y/.84+.5)
    mat=bpy.data.materials.new('Surface');mat.use_nodes=True
    shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=.65
    node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=texture(out,star)
    mat.node_tree.links.new(node.outputs['Color'],shader.inputs['Base Color']);obj.data.materials.append(mat)
    # Blender Z-up 转为 glTF Y-up，保持成品正面 +Z。
    for v in obj.data.vertices:x,y,z=v.co;v.co=(x,-z,y)
    obj.data.calc_loop_triangles();count=len(obj.data.loop_triangles)
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'.blend')))
    bpy.ops.export_scene.gltf(filepath=str(out/(name+'.gltf')),export_format='GLTF_SEPARATE',use_selection=True)
    meta=dict(asset=name,bodyTriangles=count,collision='none',humanAcceptance='pending',front='+Z',up='+Y',referenceAnm=[128,129])
    carry_acceptance(meta,out/'asset.json',[ROOT/'tools/projectile_review/options_inspector.cpp'])
    (out/'asset.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
    if meta['humanAcceptance']!='approved':
        (out/'REVIEW.md').write_text('# 副机验收\n\n待人工验收；运行根目录 Review-Options.cmd。\n',encoding='utf-8')

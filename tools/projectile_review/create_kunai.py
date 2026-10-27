"""制作苦无首件：实体网格、独立材质贴图、glTF 和同源碰撞体；仅写项目资产目录。"""
import bpy
import bmesh
import numpy as np
import math
import json
import struct
import sys
from pathlib import Path
from collections import Counter

ROOT = Path(sys.argv[sys.argv.index('--') + 1]).resolve()
OUT = ROOT / 'assets/projectiles/kunai_silver'
TEX = OUT / 'textures'
OUT.mkdir(parents=True, exist_ok=True)
TEX.mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.preferences.filepaths.temporary_directory = str(ROOT / '.cache/tmp')
bpy.context.preferences.filepaths.save_version = 0
N = 2048
rng = np.random.default_rng(41027)
y, x = np.mgrid[0:N, 0:N].astype(np.float32) / N


def noise(cells):
    """平滑二维确定性噪声，用于粗糙度/纤维，绝不从弹体截图拉伸生成纹理。"""
    grid = rng.random((cells + 1, cells + 1)).astype(np.float32)
    xx, yy = x * cells, y * cells
    ix, iy = xx.astype(int), yy.astype(int)
    fx, fy = xx - ix, yy - iy
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    return ((grid[iy, ix] * (1-fx) + grid[iy, ix+1] * fx) * (1-fy)
            + (grid[iy+1, ix] * (1-fx) + grid[iy+1, ix+1] * fx) * fy)


def image(name, values, color=False):
    """保存独立 PNG；颜色值按 sRGB 存储，数据贴图保持线性。"""
    rgba = np.ones((N, N, 4), np.float32)
    if values.ndim == 2:
        rgba[:, :, :3] = values[:, :, None]
    else:
        rgba[:, :, :3] = values
    im = bpy.data.images.new(name, width=N, height=N, alpha=True)
    im.colorspace_settings.name = 'sRGB' if color else 'Non-Color'
    im.pixels.foreach_set(np.clip(rgba, 0, 1).ravel())
    im.filepath_raw = str(TEX / (name + '.png'))
    im.file_format = 'PNG'
    im.save()
    im.source = 'FILE'
    im.reload()
    return im


def material(name, cloth=False):
    """独立制作底色、法线、粗糙度、金属度与 AO；保留源通道和 glTF ORM。"""
    cloud = noise(10)
    grain = noise(240)
    if cloth:
        weave = np.sin(x * math.tau * 300) * np.sin(y * math.tau * 340)
        height = .12 * weave + .04 * grain
        base = (.85 + .10 * cloud + .015 * weave)[:, :, None] * np.array([.115, .145, .18])
        rough = .78 + .12 * cloud
        metallic = np.zeros_like(x)
        ao = .88 + .12 * (weave * .5 + .5)
        normal_strength = .22
    else:
        brushed = np.sin(x * math.tau * 440 + 3 * np.sin(y * math.tau * 2))
        scratches = np.zeros_like(x)
        for _ in range(26):
            center, length, slope = rng.uniform(.02, .98), rng.uniform(.04, .55), rng.uniform(-.035, .035)
            start = rng.uniform(0, 1-length)
            line = np.exp(-((x - center - slope*y) / rng.uniform(.0002, .0007))**2)
            window = np.clip((y-start)*100, 0, 1) * np.clip((start+length-y)*100, 0, 1)
            scratches += line * window
        height = .002 * brushed + .01 * grain - .06 * scratches
        base = (.83 + .12 * cloud - .025 * scratches)[:, :, None] * np.array([.56, .60, .65])
        rough = np.clip(.28 + .13 * cloud + .018 * brushed + .09 * scratches, .20, .65)
        metallic = np.ones_like(x)
        ao = np.ones_like(x)
        normal_strength = .12
    dy, dx = np.gradient(height)
    normal = np.dstack((-dx * normal_strength, -dy * normal_strength, np.ones_like(x)))
    normal /= np.linalg.norm(normal, axis=2)[:, :, None]
    maps = {
        'baseColor': image(name + '_baseColor', base, True),
        'normal': image(name + '_normal', normal * .5 + .5),
        'roughness': image(name + '_roughness', rough),
        'metallic': image(name + '_metallic', metallic),
        'ao': image(name + '_ao', ao),
        'orm': image(name + '_orm', np.dstack((ao, rough, metallic))),
    }
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    for key, socket in [('baseColor', 'Base Color')]:
        n = nodes.new('ShaderNodeTexImage')
        n.image = maps[key]
        n.label = key
        links.new(n.outputs['Color'], bsdf.inputs[socket])
    packed = nodes.new('ShaderNodeTexImage')
    packed.image = maps['orm']
    split = nodes.new('ShaderNodeSeparateColor')
    links.new(packed.outputs['Color'], split.inputs['Color'])
    links.new(split.outputs['Green'], bsdf.inputs['Roughness'])
    links.new(split.outputs['Blue'], bsdf.inputs['Metallic'])
    n = nodes.new('ShaderNodeTexImage')
    n.image = maps['normal']
    nm = nodes.new('ShaderNodeNormalMap')
    links.new(n.outputs['Color'], nm.inputs['Color'])
    links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    return mat


steel = material('forged_steel')
cloth = material('indigo_wrap', True)
parts = []


def finish(obj, mat, bevel=0, smooth=False):
    """应用可见几何修改器后生成 UV；此最终网格同时用于导出和碰撞。"""
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    if bevel:
        m = obj.modifiers.new('Physical edge bevel', 'BEVEL')
        m.width, m.segments = bevel, 3
        m.limit_method = 'ANGLE'
        bpy.ops.object.modifier_apply(modifier=m.name)
        # 尖端倒角在浮点钳制处可能产生重合点和零面积薄片；清理后才生成渲染/碰撞。
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-6)
        bmesh.ops.dissolve_degenerate(bm, edges=list(bm.edges), dist=1e-6)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(obj.data)
        bm.free()
    for p in obj.data.polygons:
        p.use_smooth = smooth
    if not smooth:
        for p in obj.data.polygons:
            p.use_smooth = True
        m = obj.modifiers.new('Weighted face normals', 'WEIGHTED_NORMAL')
        m.keep_sharp = True
        m.weight = 50
        bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=.025)
    bpy.ops.object.mode_set(mode='OBJECT')
    parts.append(obj)
    return obj


def mesh(name, vertices, faces, mat, bevel=0, smooth=False):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    return finish(obj, mat, bevel, smooth)


# 刃部为具有中央脊的封闭双斜面实体，不用平面贴图表达厚度。
levels = [(-.070, .032, .017), (-.035, .112, .026), (.500, .0007, .00016)]
verts = []
for z, w, d in levels:
    for px, py in [(w, 0), (.87*w, d*.20), (0, d), (-.87*w, d*.20),
                   (-w, 0), (-.87*w, -d*.20), (0, -d), (.87*w, -d*.20)]:
        verts.append((px, py, z))
faces = [tuple(reversed(range(8)))]
for j in range(len(levels)-1):
    for k in range(8):
        faces.append((j*8+k, j*8+(k+1)%8, (j+1)*8+(k+1)%8, (j+1)*8+k))
faces.append(tuple(range((len(levels)-1)*8, len(levels)*8)))
mesh('01_forged_blade', verts, faces, steel, .0011)


def cylinder(name, radius, depth, z, mat, bevel=.0015, vertices=48):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=(0, 0, z))
    obj = bpy.context.object
    obj.name = name
    return finish(obj, mat, bevel, True)


cylinder('02_blade_socket', .037, .030, -.073, steel)
cylinder('03_grip_core', .0285, .239, -.201, cloth)
cylinder('04_grip_collar_upper', .034, .013, -.098, steel)
cylinder('05_grip_collar_lower', .034, .017, -.320, steel)

# 缠带是有厚度的连续螺旋带；纤维织纹留给独立 normal/roughness 贴图。
turns, steps = 8, 640
vertices, faces = [], []
for i in range(steps+1):
    t = i / steps
    angle = math.tau * turns * t
    z = -.313 + .208 * t
    radius = .0305 + .0015 * math.sin(math.pi*t)
    for dr, dz in [(0, -.0125), (.0025, -.0125), (.0025, .0125), (0, .0125)]:
        vertices.append(((radius+dr)*math.cos(angle), (radius+dr)*math.sin(angle), z+dz))
for i in range(steps):
    for j in range(4):
        faces.append((4*i+j, 4*i+(j+1)%4, 4*(i+1)+(j+1)%4, 4*(i+1)+j))
faces += [(3,2,1,0), tuple(range(4*steps, 4*steps+4))]
mesh('06_woven_grip_binding', vertices, faces, cloth, smooth=True)

# 尾环在 X/Z 平面，孔贯通厚度轴 Y；碰撞使用相同的环面网格。
bpy.ops.mesh.primitive_torus_add(major_segments=96, minor_segments=16,
    location=(0, 0, -.410), rotation=(math.pi/2, 0, 0), major_radius=.071, minor_radius=.018)
obj = bpy.context.object
obj.name = '07_pommel_ring'
finish(obj, steel, smooth=True)

# Blender Z 向上；glTF 和碰撞统一换为 +Y 长轴、+Z 正面。
def exported(p):
    return (p.x, p.z, -p.y)


summary = {'asset': 'kunai_silver', 'nativeType': 4, 'nativeColor': 0,
           'units': 'normalized; total length approximately 1', 'tipAxis': '+Y',
           'frontAxis': '+Z', 'parts': [], 'humanAcceptance': 'pending',
           'textureResolution': N, 'reference': 'real metal kunai; newly authored geometry and textures'}
with (OUT / 'collision.bin').open('wb') as f:
    f.write(b'KCOL0001')
    f.write(struct.pack('<I', len(parts)))
    for obj in parts:
        obj.data.calc_loop_triangles()
        triangles = obj.data.loop_triangles
        f.write(struct.pack('<I', len(triangles)))
        edge_counts = Counter()
        for tri in triangles:
            for j in range(3):
                edge_counts[tuple(sorted((tri.vertices[j], tri.vertices[(j+1)%3])))] += 1
            points = [exported(obj.matrix_world @ obj.data.vertices[v].co) for v in tri.vertices]
            f.write(struct.pack('<9f', *(v for p in points for v in p)))
        bad = sum(v != 2 for v in edge_counts.values())
        if bad:
            raise RuntimeError(f'{obj.name}: nonmanifold edges {bad}')
        summary['parts'].append({'name': obj.name, 'triangles': len(triangles), 'closed': True})

bpy.ops.object.select_all(action='DESELECT')
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'kunai_silver.blend'))
bpy.ops.export_scene.gltf(filepath=str(OUT / 'kunai_silver.gltf'), export_format='GLTF_SEPARATE',
    use_selection=True, export_texcoords=True, export_normals=True, export_tangents=False,
    export_materials='EXPORT', export_yup=True, export_image_format='AUTO',
    export_texture_dir='textures', export_cameras=False, export_lights=False)
summary['triangles'] = sum(p['triangles'] for p in summary['parts'])
(OUT / 'asset.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')
# .blend 中的贴图采用相对路径，仓库换位置后仍可编辑。
bpy.ops.file.make_paths_relative()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'kunai_silver.blend'))
print('KUNAI_ASSET_COMPLETE ' + json.dumps(summary))

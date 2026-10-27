"""独立检查 glTF/碰撞同源性、闭合拓扑、纹理尺寸及项目内资源路径。"""
import json
import struct
import math
import hashlib
from pathlib import Path
from collections import Counter, defaultdict
from itertools import product
import sys
import numpy as np

root = Path(sys.argv[1]).resolve()
asset = root / 'assets/projectiles/kunai_silver'
gltf = json.loads((asset / 'kunai_silver.gltf').read_text())
summary = json.loads((asset / 'asset.json').read_text())


def within(uri):
    """外置资源必须留在单件资产目录，不允许绝对路径、网络 URI 或目录穿越。"""
    p = (asset / uri).resolve()
    assert p.is_relative_to(asset.resolve()) and p.is_file(), uri
    return p


buffers = [within(b['uri']).read_bytes() for b in gltf['buffers']]
for im in gltf['images']:
    within(im['uri'])


def accessor(index):
    a = gltf['accessors'][index]
    view = gltf['bufferViews'][a['bufferView']]
    widths = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}
    dtype = {5123: '<u2', 5125: '<u4', 5126: '<f4'}[a['componentType']]
    count, width = a['count'], widths[a['type']]
    offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = view.get('byteStride', np.dtype(dtype).itemsize * width)
    return np.ndarray((count, width), dtype=dtype, buffer=buffers[view['buffer']], offset=offset,
                      strides=(stride, np.dtype(dtype).itemsize)).copy()


collision = (asset / 'collision.bin').read_bytes()
assert collision[:8] == b'KCOL0001'
count, = struct.unpack_from('<I', collision, 8)
offset = 12
parts = []
for _ in range(count):
    n, = struct.unpack_from('<I', collision, offset)
    offset += 4
    parts.append(np.frombuffer(collision, dtype='<f4', count=n*9, offset=offset).reshape(n, 3, 3))
    offset += n*36
assert offset == len(collision)
assert count == len(gltf['meshes']) == len(summary['parts']) == 7


def equivalent_vertices(a, b):
    """用空间网格比对导出与碰撞顶点，允许浮点变换的 2e-6 长度误差。"""
    epsilon = 2e-6
    grid = defaultdict(list)
    for p in np.unique(b.reshape(-1, 3), axis=0):
        grid[tuple(np.floor(p/epsilon).astype(int))].append(p)
    for p in np.unique(a.reshape(-1, 3), axis=0):
        cell = np.floor(p/epsilon).astype(int)
        candidates = [q for d in product((-1,0,1), repeat=3) for q in grid.get(tuple(cell+np.array(d)), [])]
        assert any(np.linalg.norm(p-q) <= epsilon for q in candidates), f'Unmatched render vertex: {p}'


total = 0
for node in gltf['nodes']:
    primitive = gltf['meshes'][node['mesh']]['primitives'][0]
    positions = accessor(primitive['attributes']['POSITION']).astype(np.float64)
    normals = accessor(primitive['attributes']['NORMAL'])
    uv = accessor(primitive['attributes']['TEXCOORD_0'])
    assert np.isfinite(positions).all() and np.isfinite(uv).all()
    assert np.all(np.linalg.norm(normals, axis=1) > .9)
    positions *= np.array(node.get('scale', [1,1,1]))
    q = np.array(node.get('rotation', [0,0,0,1]))
    positions += 2*np.cross(q[:3], np.cross(q[:3], positions) + q[3]*positions)
    positions += np.array(node.get('translation', [0,0,0]))
    indices = accessor(primitive['indices']).reshape(-1, 3)
    triangles = positions[indices]
    part = parts[node['mesh']]
    assert len(triangles) == len(part)
    equivalent_vertices(triangles, part)
    equivalent_vertices(part, triangles)
    # 对每个闭合部件独立核对焊接后的边次数，重叠部件不互相吞掉边界。
    welded = np.round(part, 6)
    edges = Counter()
    for triangle in welded:
        for j in range(3):
            edges[tuple(sorted((tuple(triangle[j]), tuple(triangle[(j+1)%3]))))] += 1
    assert all(v == 2 for v in edges.values()), node['name']
    area = np.linalg.norm(np.cross(part[:,1]-part[:,0], part[:,2]-part[:,0]), axis=1)
    assert np.all(area > 1e-10), 'Degenerate face'
    total += len(part)
assert total == summary['triangles']

pngs = list((asset/'textures').glob('*.png'))
assert len(pngs) == 12
for p in pngs:
    raw = p.read_bytes()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack_from('>II', raw, 16) == (2048,2048), p.name

report = {'passed': True, 'closedParts': 7, 'triangles': total,
          'renderCollisionVertexTolerance': 2e-6, 'independentTextureMaps': 12,
          'textureResolution': [2048,2048], 'humanAcceptance': 'pending',
          'sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in [asset/'kunai_silver.gltf',asset/'kunai_silver.bin',asset/'collision.bin']}}
out = root/'output/projectile-review'
out.mkdir(parents=True, exist_ok=True)
(out/'asset-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

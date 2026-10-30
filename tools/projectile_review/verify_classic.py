"""核对原版风格苦无的面预算、独立封闭部件和渲染/碰撞逐三角形同源。"""
from pathlib import Path
import json
import struct
import sys
from collections import Counter
import numpy as np

asset = Path(sys.argv[1]).resolve()/'assets/projectiles/kunai_classic'
g = json.loads((asset/'kunai_classic.gltf').read_text())
meta = json.loads((asset/'asset.json').read_text())
assert len(g['meshes']) == 1 and not g.get('images') and not g.get('textures')
assert len(g['materials']) == 1
buffers = []
for b in g['buffers']:
    path = (asset/b['uri']).resolve()
    assert path.is_relative_to(asset) and path.is_file()
    buffers.append(path.read_bytes())

def read(index):
    a = g['accessors'][index];v = g['bufferViews'][a['bufferView']]
    dt = np.dtype({5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']])
    width = {'SCALAR':1,'VEC3':3,'VEC4':4}[a['type']]
    return np.ndarray((a['count'],width),dtype=dt,buffer=buffers[v['buffer']],offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',width*dt.itemsize),dt.itemsize)).copy()

primitive = g['meshes'][0]['primitives'][0]
assert len(g['meshes'][0]['primitives']) == 1
p = read(primitive['attributes']['POSITION']).astype(float)
n = read(primitive['attributes']['NORMAL'])
assert np.isfinite(p).all() and np.all(np.linalg.norm(n,axis=1)>.99)
mask = read(primitive['attributes']['COLOR_0'])
assert len(np.unique(mask[:,0])) >= 2
node = next(node for node in g['nodes'] if 'mesh' in node)
p *= node.get('scale',[1,1,1]);q = np.array(node.get('rotation',[0,0,0,1]))
p += 2*np.cross(q[:3],np.cross(q[:3],p)+q[3]*p);p += node.get('translation',[0,0,0])
render = p[read(primitive['indices']).reshape(-1,3)]
raw = (asset/'collision.bin').read_bytes();assert raw[:8] == b'KCOL0001'
count, = struct.unpack_from('<I',raw,8);assert count == 3
offset = 12;parts = []
for _ in range(count):
    size, = struct.unpack_from('<I',raw,offset);offset += 4
    triangles = np.frombuffer(raw,dtype='<f4',offset=offset,count=size*9).reshape(size,3,3);offset += size*36
    assert np.all(np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1)>1e-10)
    edges = Counter()
    for t in np.round(triangles,6):
        for j in range(3):edges[tuple(sorted((tuple(t[j]),tuple(t[(j+1)%3]))))] += 1
    assert all(n == 2 for n in edges.values())
    parts.append(triangles)
assert offset == len(raw)
collision = np.concatenate(parts)
assert len(render) == len(collision) == meta['triangles'] == 146
# 同时比较三角形而非只有顶点集合，避免同点不同连边误判通过。
def keys(triangles):
    return Counter(tuple(sorted(tuple(int(round(float(v)*100000)) for v in point) for point in t)) for t in triangles)
assert keys(render) == keys(collision)
assert meta['humanAcceptance'] in ('pending', 'approved')
if meta['humanAcceptance'] == 'approved':
    assert meta['humanAcceptanceBy'] == ['user', 'leader'] and meta['humanAcceptanceDate']
print('Classic verified: 146 triangles, 3 closed parts, 1 mesh/material, no images, exact shared topology.')

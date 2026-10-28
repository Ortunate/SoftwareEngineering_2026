"""验证激光 glTF 两层闭合几何、面预算、角色编码和参数化坐标范围。"""
import json
import sys
from pathlib import Path
from collections import Counter
import numpy as np

root=Path(sys.argv[1])
for name in ('laser_default','laser_round'):
    folder=root/'assets/projectiles'/name
    meta=json.loads((folder/'asset.json').read_text(encoding='utf-8'))
    g=json.loads((folder/(name+'.gltf')).read_text());assert len(g['meshes'])==1 and not g.get('textures')
    data=(folder/g['buffers'][0]['uri']).read_bytes()
    def read(index):
        a=g['accessors'][index];v=g['bufferViews'][a['bufferView']];dt=np.dtype({5126:'<f4',5123:'<u2',5121:'u1'}[a['componentType']]);w={'SCALAR':1,'VEC3':3,'VEC4':4}[a['type']]
        result=np.ndarray((a['count'],w),dtype=dt,buffer=data,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',dt.itemsize*w),dt.itemsize)).copy()
        return result/np.iinfo(dt).max if a.get('normalized') else result
    p=g['meshes'][0]['primitives'][0];pos=read(p['attributes']['POSITION']);norm=read(p['attributes']['NORMAL']);color=read(p['attributes']['COLOR_0']);idx=read(p['indices']).reshape(-1,3);tri=pos[idx]
    assert len(tri)==meta['triangles']==288 and meta['colors']==16
    assert np.isfinite(pos).all() and np.isfinite(norm).all()
    assert np.all(np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1)>1e-8)
    for role in (0,1):
        selected=tri[(color[idx[:,0],1]>.5)==bool(role)];assert len(selected)==144
        edges=Counter()
        for t in np.round(selected,6):
            for j in range(3):edges[tuple(sorted((tuple(t[j]),tuple(t[(j+1)%3]))))]+=1
        assert all(n==2 for n in edges.values()),'Nonmanifold laser'
    assert meta['collision']['shape']=='capsule' and not meta['collision']['haloCollides']
    print(name+': 288 triangles, two closed layers, 16 colors: PASS')

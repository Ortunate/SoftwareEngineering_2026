"""核对魔理沙导出的资源、三角预算、碰撞流形和原始动画脚本，不进行性能测试。"""
import json,struct,sys,math
import numpy as np
from pathlib import Path
from collections import Counter
root=Path(sys.argv[1]);names=('marisa_primary','marisa_missile','marisa_laser','marisa_explosion')
def gltf_count(path):
    """校验外部资源和索引访问器，返回实际提交的三角形数量。"""
    g=json.loads(path.read_text());count=0
    for item in g.get('buffers',[])+g.get('images',[]):assert (path.parent/item['uri']).is_file()
    def accessor(index):
        a=g['accessors'][index];v=g['bufferViews'][a['bufferView']];b=(path.parent/g['buffers'][v['buffer']]['uri']).read_bytes()
        dtype={5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']];width={'SCALAR':1,'VEC3':3}[a['type']]
        offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',np.dtype(dtype).itemsize*width)
        return np.ndarray((a['count'],width),dtype=dtype,buffer=b,offset=offset,strides=(stride,np.dtype(dtype).itemsize)).copy()
    for mesh in g['meshes']:
        for p in mesh['primitives']:
            assert p.get('mode',4)==4
            indices=g['accessors'][p['indices']];assert indices['count']%3==0
            position=accessor(p['attributes']['POSITION']);index=accessor(p['indices']).reshape(-1,3);assert np.isfinite(position).all() and index.max()<len(position)
            tri=position[index];area=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);assert np.all(np.sum(area*area,axis=1)>1e-16),'degenerate display triangle'
            count+=indices['count']//3
    return count
for kind,name in enumerate(names):
    d=root/'assets/projectiles'/name;m=json.loads((d/'asset.json').read_text());assert m['kind']==kind and not m['glowCollides']
    count=gltf_count(d/(name+'.gltf'));glow=gltf_count(d/'glow.gltf');assert count==m['triangles'] and glow==m['glowTriangles'];assert count+glow==m['displayTrianglesWithGlow']<=m['triangleHardLimit']<=1000
    if kind!=2:
        b=(d/'collision.bin').read_bytes();assert b[:8]==b'KCOL0001';parts=struct.unpack_from('<I',b,8)[0];at=12;total=0
        for part in range(parts):
            n=struct.unpack_from('<I',b,at)[0];at+=4;edges=Counter();volume=0
            for i in range(n):
                values=struct.unpack_from('<9f',b,at);at+=36;assert all(math.isfinite(x) for x in values)
                a,c,e=[tuple(round(x,6) for x in values[j:j+3]) for j in (0,3,6)]
                cross=((c[1]-a[1])*(e[2]-a[2])-(c[2]-a[2])*(e[1]-a[1]),(c[2]-a[2])*(e[0]-a[0])-(c[0]-a[0])*(e[2]-a[2]),(c[0]-a[0])*(e[1]-a[1])-(c[1]-a[1])*(e[0]-a[0]));assert sum(x*x for x in cross)>1e-14
                for u,v in ((a,c),(c,e),(e,a)):edges[tuple(sorted((u,v)))]+=1
                volume+=a[0]*(c[1]*e[2]-c[2]*e[1])+a[1]*(c[2]*e[0]-c[0]*e[2])+a[2]*(c[0]*e[1]-c[1]*e[0])
            assert all(n==2 for n in edges.values()) and volume>0;total+=n
        assert at==len(b) and total==count
    else:assert m['collision']['shape']=='parameterized capsule'
    expected=([64],[65,66,67,68],[69,70,71],[97,98,99,100])[kind];assert [v['script'] for v in m['variants']]==expected
    print('PASS',name,'body',count,'glow',glow,'total',count+glow)
# 档位是原数据而非人为增添形状；保留四档爆破不同增长速度。
missile=json.loads((root/'assets/projectiles/marisa_missile/asset.json').read_text())
assert all(abs(v['scale'][0]-x)<1e-6 for v,x in zip(missile['variants'],(.65,.75,.9,1)))
explosion=json.loads((root/'assets/projectiles/marisa_explosion/asset.json').read_text())
assert explosion['collisionDiametersPixels']==[32,42,48,48]
assert all(abs(v['scalePerFrame'][0]-x)<1e-6 for v,x in zip(explosion['variants'],(.075,.1,.125,.15)))

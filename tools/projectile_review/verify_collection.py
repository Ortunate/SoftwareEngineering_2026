"""全组完成后统一检查面预算、闭合实体、角色分层、资源路径和逐三角形同源碰撞。"""
from pathlib import Path
from collections import Counter
import json
import struct
import sys
import numpy as np

root=Path(sys.argv[1]).resolve()
names=['pellet_classic','ring_orb_classic','rice_classic','orb_classic','kunai_classic','needle_classic','large_orb_classic','fireball_classic','knife_classic','bubble_classic']
limits=[160,320,160,320,192,96,512,384,256,768]
colors=[16,16,16,16,16,16,8,1,8,5]
report=[]

def keys(tris):
    return Counter(tuple(sorted(tuple(int(round(float(v)*100000)) for v in p) for p in t)) for t in tris)

for kind,name in enumerate(names):
    folder=root/'assets/projectiles'/name;meta=json.loads((folder/'asset.json').read_text(encoding='utf-8'))
    g=json.loads((folder/(name+'.gltf')).read_text(encoding='utf-8'))
    assert len(g['meshes'])==1 and len(g['meshes'][0]['primitives'])==1 and len(g['materials'])==1
    assert not g.get('images') and not g.get('textures')
    buffers=[]
    for b in g['buffers']:
        path=(folder/b['uri']).resolve();assert path.is_relative_to(folder) and path.is_file();buffers.append(path.read_bytes())
    def read(index):
        a=g['accessors'][index];v=g['bufferViews'][a['bufferView']];dt=np.dtype({5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']]);w={'SCALAR':1,'VEC3':3,'VEC4':4}[a['type']]
        values=np.ndarray((a['count'],w),dtype=dt,buffer=buffers[v['buffer']],offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',w*dt.itemsize),dt.itemsize)).copy()
        if a.get('normalized'):values=values.astype(float)/np.iinfo(dt).max
        return values
    pr=g['meshes'][0]['primitives'][0];pos=read(pr['attributes']['POSITION']).astype(float);normal=read(pr['attributes']['NORMAL']);color=read(pr['attributes']['COLOR_0'])
    assert np.isfinite(pos).all() and np.isfinite(normal).all() and np.all(np.linalg.norm(normal,axis=1)>.99)
    node=next(n for n in g['nodes'] if 'mesh' in n);pos*=node.get('scale',[1,1,1]);q=np.array(node.get('rotation',[0,0,0,1]));pos+=2*np.cross(q[:3],np.cross(q[:3],pos)+q[3]*pos);pos+=node.get('translation',[0,0,0])
    idx=read(pr['indices']).reshape(-1,3);tri=pos[idx]
    roles=np.zeros(len(tri),dtype=int) if kind==4 else np.rint(color[idx[:,0],1]*2).astype(int)
    if kind!=4:assert np.all(np.abs(color[idx,1]*2-roles[:,None])<.02), 'Mixed role within triangle'
    assert len(tri)==meta['triangles']<=limits[kind]
    assert np.all(np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1)>1e-10),f'Degenerate triangle: {name}'
    if kind==5:
        # 独立核对六棱双锥的尺寸契约；面轴厚度仅为原版 30%，正面轮廓不变。
        vertices=np.unique(np.round(pos,6),axis=0)
        assert len(tri)==24 and len(vertices)==14
        assert np.allclose(vertices[np.isclose(np.abs(vertices[:,1]),.5)],[[0,-.5,0],[0,.5,0]],atol=1e-6)
        rings=[]
        for y in (-.28,.28):
            ring=vertices[np.isclose(vertices[:,1],y)][:,[0,2]]
            # 还原基准截面后核对六边等长，最终厚度另按实际网格检查。
            ring[:,1]/=.3
            assert len(ring)==6 and np.allclose(np.linalg.norm(ring,axis=1),.2,atol=1e-6)
            ring=ring[np.argsort(np.arctan2(ring[:,1],ring[:,0]))]
            assert np.allclose(np.linalg.norm(np.roll(ring,1,axis=0)-ring,axis=1),.2,atol=1e-6)
            rings.append(ring)
        assert np.allclose(rings[0],rings[1],atol=1e-6)
        assert np.allclose(np.ptp(vertices,axis=0),[.4,1,.2*np.sqrt(3)*.3],atol=1e-6)
        face_normals=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])
        face_normals/=np.linalg.norm(face_normals,axis=1)[:,None]
        # Blender 导出会量化法线，比较方向余弦并要求同一三角形三顶点共用法线。
        vertex_normals=normal[idx]/np.linalg.norm(normal[idx],axis=2)[:,:,None]
        assert np.all(np.sum(vertex_normals*face_normals[:,None,:],axis=2)>.99999), 'Crystal facets must stay flat'
        assert np.allclose(normal[idx],normal[idx[:,0]][:,None,:],atol=1e-6)
    # 装饰火舌和内核也必须有厚度且闭合，不只检查参与碰撞的外表面。
    for role in set(roles):
        edges=Counter()
        for t in np.round(tri[roles==role],6):
            for j in range(3):edges[tuple(sorted((tuple(t[j]),tuple(t[(j+1)%3]))))]+=1
        assert all(c==2 for c in edges.values()),f'Open/nonmanifold visual role {name}/{role}'
    raw=(folder/'collision.bin').read_bytes();assert raw[:8]==b'KCOL0001';count,=struct.unpack_from('<I',raw,8);at=12;parts=[]
    for _ in range(count):
        n,=struct.unpack_from('<I',raw,at);at+=4;p=np.frombuffer(raw,dtype='<f4',count=n*9,offset=at).reshape(n,3,3);at+=n*36;parts.append(p)
        edges=Counter()
        for t in np.round(p,6):
            for j in range(3):edges[tuple(sorted((tuple(t[j]),tuple(t[(j+1)%3]))))]+=1
        assert all(c==2 for c in edges.values()), f'Open/nonmanifold collision {name}'
    assert at==len(raw)
    if kind==8:
        # 刀柄面轴不能凸出护手；主刀面是平行 XY 的竖直平面，只有刃口参与斜切。
        assert count==3
        blade,guard,handle=parts
        assert np.isclose(np.max(np.abs(handle[:,:,2])),.040,atol=1e-6)
        assert np.max(np.abs(handle[:,:,2]))<np.max(np.abs(guard[:,:,2]))
        assert np.isclose(np.max(np.abs(guard[:,:,2])),.042,atol=1e-6)
        assert np.allclose(np.ptp(handle.reshape(-1,3),axis=0),[.17,.305,.08],atol=1e-6)
        for z in (-.028,.028):
            main=blade[np.all(np.isclose(blade[:,:,2],z,atol=1e-6),axis=1)]
            assert len(main)==5 and np.ptp(main[:,:,1])>.55
            # 正式导出不能把竖直主面的法线重新平滑向斜切面；只检查主面三角形的顶点副本。
            main_ids=idx[np.all(np.isclose(tri[:,:,2],z,atol=1e-6),axis=1)]
            assert len(main_ids)==5 and np.all(np.abs(normal[main_ids,2])>.9999)
    collision=np.concatenate(parts)
    # 同源验证按元数据中的物理角色选取，不把所有球壳一律当成实体。
    physical_roles={p['role'] for p in meta.get('parts',[]) if p.get('collision')}
    if kind==4:physical_roles={0}
    physical=tri[np.isin(roles,list(physical_roles))]
    if kind==9:
        assert count==1 and len(parts[0])==288 and abs(np.max(np.linalg.norm(collision,axis=2))-.40)<.00001
        physical=tri[roles==1]*.8
        assert sum(p['name'].startswith('StaticRadialFilament_') for p in meta['parts'])==48
    assert keys(physical)==keys(collision),f'Render/collision topology mismatch: {name}'
    assert meta['humanAcceptance'] in ('pending','approved')
    if meta['humanAcceptance']=='approved':assert meta.get('humanAcceptanceBy') and meta.get('humanAcceptanceDate')
    if kind!=4:assert meta['effectiveColors']==colors[kind] and meta['collisionTriangles']==len(collision)
    report.append(dict(type=kind,asset=name,triangles=len(tri),collisionTriangles=len(collision),closedCollisionParts=count,roles={str(r):int(sum(roles==r)) for r in set(roles)},passed=True,humanAcceptance=meta['humanAcceptance']))

out=root/'output/projectile-review/collection';out.mkdir(parents=True,exist_ok=True)
(out/'asset-checks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))

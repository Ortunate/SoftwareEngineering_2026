"""使用项目内 Pillow 合成四秒旋转 GIF，并验证帧数、时长和完整模型几何。"""
import sys,json,struct
from pathlib import Path
root=Path(sys.argv[1]).resolve();sys.path.insert(0,str(root/'.deps/gif-py311'))
from PIL import Image
names=['power_small','point','power_large','bomb','full_power','life','star']
base=root/'output/projectile-review/items'
for name in names:
    folder=root/'assets/items'/name;meta=json.loads((folder/'asset.json').read_text())
    gltf=json.loads((folder/(name+'.gltf')).read_text())
    assert meta['triangles']<=1000 and meta['thickness']==.06
    for buffer in gltf['buffers']:assert (folder/buffer['uri']).is_file()
    for image in gltf['images']:assert (folder/image['uri']).is_file()
    # 首个网格为闭合挤出实体；每条无向边必须恰被两个三角形使用（合并导出的法线/UV 裂点）。
    primitive=gltf['meshes'][0]['primitives'][0];acc=gltf['accessors'];views=gltf['bufferViews']
    def read(index,fmt):
        a=acc[index];v=views[a['bufferView']];data=(folder/gltf['buffers'][v['buffer']]['uri']).read_bytes();offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',struct.calcsize(fmt))
        return [struct.unpack_from(fmt,data,offset+i*stride) for i in range(a['count'])]
    pos=read(primitive['attributes']['POSITION'],'<3f');ids=read(primitive['indices'],'<H' if acc[primitive['indices']]['componentType']==5123 else '<I');edges={}
    for i in range(0,len(ids),3):
        tri=[tuple(round(v,6) for v in pos[ids[i+j][0]]) for j in range(3)]
        for a,b in zip(tri,tri[1:]+tri[:1]):key=tuple(sorted((a,b)));edges[key]=edges.get(key,0)+1
    assert all(n==2 for n in edges.values()),name+' nonclosed body'
for name in names+['overview']:
    frames=[]
    for i in range(100):
        with Image.open(base/name/f'{i}.png') as im:frames.append(im.convert('RGB'))
    frames[0].save(base/(name+'.gif'),save_all=True,append_images=frames[1:],duration=40,loop=0,disposal=2,optimize=False)
    for i,label in [(0,'front'),(25,'side'),(50,'back')]:frames[i].save(base/(name+'-'+label+'.png'))
    with Image.open(base/(name+'.gif')) as check:
        assert check.n_frames==100
        assert sum(check.seek(i) or check.info['duration'] for i in range(100))==4000
    print(name,'PASS: geometry/resources, 100 frames, 4 seconds')

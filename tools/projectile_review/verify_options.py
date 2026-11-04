"""检查副机外部资源、球面几何和闭合性；不执行性能测试或代替人工验收。"""
import json,struct,sys,math
from pathlib import Path
from collections import Counter

root=Path(sys.argv[1]).resolve()
for name in ('reimu_option','marisa_option'):
    directory=root/'assets/projectiles'/name
    gltf=json.loads((directory/(name+'.gltf')).read_text(encoding='utf-8'))
    for item in gltf['buffers']+gltf['images']:assert (directory/item['uri']).is_file()
    def read(index):
        """读取当前导出的紧密排列 accessor；拒绝未支持的稀疏数据与步长布局。"""
        a=gltf['accessors'][index];v=gltf['bufferViews'][a['bufferView']]
        assert 'byteStride' not in v and 'sparse' not in a
        data=(directory/gltf['buffers'][v['buffer']]['uri']).read_bytes()
        fmt={5126:'f',5123:'H',5125:'I'}[a['componentType']];size={'VEC3':3,'SCALAR':1}[a['type']]
        return list(struct.iter_unpack('<'+fmt*size,data[v.get('byteOffset',0)+a.get('byteOffset',0):v.get('byteOffset',0)+a.get('byteOffset',0)+a['count']*struct.calcsize('<'+fmt*size)]))
    primitive=gltf['meshes'][0]['primitives'][0];positions=read(primitive['attributes']['POSITION']);indices=[i[0] for i in read(primitive['indices'])]
    assert len(indices)==528*3
    assert all(all(math.isfinite(v) for v in p) and abs(math.sqrt(sum(v*v for v in p))-.42)<1e-5 for p in positions)
    edges=Counter()
    for i in range(0,len(indices),3):
        vertices=[tuple(round(v,5) for v in positions[j]) for j in indices[i:i+3]]
        assert len(set(vertices))==3
        for a,b in zip(vertices,vertices[1:]+vertices[:1]):edges[tuple(sorted((a,b)))]+=1
    assert all(count==2 for count in edges.values())
    assert not (directory/'collision.bin').exists()
    print(name,': 528 triangles, closed sphere, external resources OK')

"""检查三种 Bomb 的全部显示层预算、外部资源与主体闭合碰撞；不进行性能测试。"""
import ast,json,struct,math,sys
from pathlib import Path
from collections import Counter
import numpy as np
root=Path(sys.argv[1])
# 复用只读 glTF 验证函数，不执行其他类别的校验入口。
tree=ast.parse((root/'tools/projectile_review/verify_marisa.py').read_text(encoding='utf-8-sig'))
for n in tree.body:
 if isinstance(n,ast.FunctionDef) and n.name=='gltf_count':exec(compile(ast.Module(body=[n],type_ignores=[]),'<gltf validator>','exec'))
for kind,name in ((0,'bomb_seal'),(2,'bomb_star'),(3,'bomb_spark')):
 d=root/'assets/projectiles'/name;m=json.loads((d/'asset.json').read_text());assert m['kind']==kind and not m['decorationsCollide']
 counts=[gltf_count(d/(file+'.gltf')) if (d/(file+'.gltf')).exists() else 0 for file in (name,'shell','effect')];assert sum(counts)==m['triangles']<=m['triangleHardLimit']<=1000
 assert counts==[m['parts'][x] for x in ('body','shell','effect')]
 if kind==2:assert counts[2]==0 and not (d/'effect.gltf').exists() and not (d/'effect.bin').exists(),'removed star trail must not remain'
 if kind<3:
  data=(d/'collision.bin').read_bytes();assert data[:8]==b'KCOL0001';assert struct.unpack_from('<I',data,8)[0]==1;n=struct.unpack_from('<I',data,12)[0];assert len(data)==16+n*36 and n==counts[0];edges=Counter();volume=0
  for values in struct.iter_unpack('<9f',data[16:]):
   assert all(math.isfinite(x) for x in values);a,b,c=[tuple(round(x,6) for x in values[i:i+3]) for i in (0,3,6)]
   for u,v in ((a,b),(b,c),(c,a)):edges[tuple(sorted((u,v)))]+=1
   volume+=a[0]*(b[1]*c[2]-b[2]*c[1])+a[1]*(b[2]*c[0]-b[0]*c[2])+a[2]*(b[0]*c[1]-b[1]*c[0])
  assert volume>0 and all(n==2 for n in edges.values())
 else:assert m['collision']['shape']=='finite frustum'
 assert [s['script'] for s in m['scripts']]==[list(range(133,137)),list(range(137,141)),list(range(5,8)),list(range(8,12))][kind]
 print('PASS',name,'body/shell/effect',counts,'total',sum(counts))

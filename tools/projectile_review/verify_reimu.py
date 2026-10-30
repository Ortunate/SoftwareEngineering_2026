"""检查灵梦资产封闭性、预算、有效贴图引用和显示/碰撞同源；不测性能。"""
from pathlib import Path
import json,struct,sys,math
from collections import Counter
root=Path(sys.argv[1])
for name in ('reimu_gold','reimu_cyan','reimu_needle'):
 d=root/'assets/projectiles'/name;m=json.loads((d/'asset.json').read_text());g=json.loads((d/(name+'.gltf')).read_text())
 assert m['displayTrianglesWithGlow']<=m['triangleHardLimit']<=1000
 for item in g.get('buffers',[])+g.get('images',[]):assert (d/item['uri']).is_file()
 data=(d/'collision.bin').read_bytes();assert data[:8]==b'KCOL0001';parts,count=struct.unpack_from('<II',data,8);assert parts==1 and count==m['triangles'] and len(data)==16+count*36
 edges=Counter();volume=0
 for t in struct.iter_unpack('<9f',data[16:]):
  assert all(math.isfinite(x) for x in t)
  a,b,c=[tuple(round(x,6) for x in t[i:i+3]) for i in (0,3,6)]
  cross=((b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]),(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]),(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))
  assert sum(x*x for x in cross)>1e-14
  for u,v in ((a,b),(b,c),(c,a)):edges[tuple(sorted((u,v)))]+=1
  volume+=a[0]*(b[1]*c[2]-b[2]*c[1])+a[1]*(b[2]*c[0]-b[0]*c[2])+a[2]*(b[0]*c[1]-b[1]*c[0])
 assert all(n==2 for n in edges.values()) and volume>0
 print('PASS',name,'body',count,'with glow',m['displayTrianglesWithGlow'])

# 开口光效允许边界，但所有索引、外部资源和三角面数必须有效；不能混入主体碰撞。
d=root/'assets/projectiles/reimu_needle';m=json.loads((d/'asset.json').read_text());g=json.loads((d/'glow.gltf').read_text())
for item in g.get('buffers',[])+g.get('images',[]):assert (d/item['uri']).is_file()
count=sum(g['accessors'][p['indices']]['count']//3 for mesh in g['meshes'] for p in mesh['primitives'])
assert count==288 and m['glow']['collides'] is False and m['displayTrianglesWithGlow']==m['triangles']+count
print('PASS needle cone glow: 288 triangles, excluded from collision')

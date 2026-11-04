"""按 ANM 精灵坐标裁切七款道具原图；只读取项目素材，使用项目内 Pillow。"""
import sys, struct, json
from pathlib import Path
root=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(root/'.deps/gif-py311'))
from PIL import Image
names=['power_small','point','power_large','bomb','full_power','life','star']
b=(root/'assets/runtime/battle/native/etama3.anm').read_bytes()
n,m=struct.unpack_from('<II',b);w,h=struct.unpack_from('<II',b,12)
atlas=Image.open(root/'assets/runtime/battle/native/etama3.dds').convert('RGBA')
sprites={}
for i in range(n):
    at=struct.unpack_from('<I',b,64+4*i)[0]
    sid,x,y,sw,sh=struct.unpack_from('<I4f',b,at);sprites[sid]=(x,y,sw,sh)
for i,name in enumerate(names):
    for j in range(m):
        sid,at=struct.unpack_from('<II',b,64+4*n+8*j)
        if sid==21+i:break
    _,op,size=struct.unpack_from('<hBB',b,at)
    assert op==1
    sprite=struct.unpack_from('<I',b,at+4)[0];x,y,sw,sh=sprites[sprite]
    out=root/'assets/items'/name;out.mkdir(parents=True,exist_ok=True)
    atlas.crop((round(x*atlas.width/w),round(y*atlas.height/h),round((x+sw)*atlas.width/w),round((y+sh)*atlas.height/h))).save(out/'surface.png')
    (out/'source.json').write_text(json.dumps(dict(item=i,script=21+i,sprite=sprite,rect=[x,y,sw,sh],atlas='etama3.dds'),indent=2)+'\n')
print('Extracted seven original textures')

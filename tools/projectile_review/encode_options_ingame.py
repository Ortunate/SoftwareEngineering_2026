"""将正式游戏副机帧合成为两张八秒 GIF：普通自转、Focus 收拢保持、松开展开。"""
import sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(root/'.deps/gif-py311'))
from PIL import Image
base=root/'output/projectile-review/options-ingame'
for name in ('reimu','marisa'):
    frames=[]
    for i in range(200):
        with Image.open(base/name/f'{i}.png') as im:frames.append(im.convert('RGB'))
    frames[0].save(base/(name+'-options.gif'),save_all=True,append_images=frames[1:],duration=40,loop=0,disposal=2,optimize=False)
    for i,label in ((25,'normal'),(100,'focus'),(175,'released')):frames[i].save(base/(name+'-'+label+'.png'))
    with Image.open(base/(name+'-options.gif')) as gif:
        duration=0
        for i in range(gif.n_frames):gif.seek(i);duration+=gif.info['duration']
        assert duration==8000
        print(name,gif.n_frames,'frames, 8000 ms verified')

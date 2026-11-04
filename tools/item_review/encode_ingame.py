"""合成正式 Game::Draw 输出的六秒掉落物场内验收 GIF，固定 25 fps，不插帧。"""
import sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(root/'.deps/gif-py311'))
from PIL import Image
base=root/'output/projectile-review/items'
frames=[]
for i in range(150):
    with Image.open(base/'ingame-frames'/f'{i}.png') as im:
        frames.append(im.convert('RGB'))
frames[0].save(base/'drops-ingame.gif',save_all=True,append_images=frames[1:],duration=40,loop=0,disposal=2,optimize=False)
frames[60].save(base/'drops-ingame-preview.png')
with Image.open(base/'drops-ingame.gif') as gif:
    duration=0
    for i in range(gif.n_frames):
        gif.seek(i);duration+=gif.info['duration']
    assert duration==6000
    print(f'{gif.n_frames} frames, {duration} ms, verified')

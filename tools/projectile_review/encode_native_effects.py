"""将原 ANM 或三维特效渲染帧合成为循环 GIF；使用项目内 Pillow。"""
import sys
from pathlib import Path
root=Path(sys.argv[1]).resolve()
sys.path.insert(0,str(root/'.deps/gif-py311'))
from PIL import Image

base=root/'output/projectile-review'/(sys.argv[2] if len(sys.argv)>2 else 'native-effects')
sequences=(('spawn-models-frames','spawn-model-reveal'),) if base.name=='effects-3d' else (('spawn-frames','bullet-spawn'),('defeat-frames','shared-defeat'))
for source,name in sequences:
    frames=[]
    for i in range(90):
        with Image.open(base/source/f'{i}.png') as image:
            frames.append(image.convert('RGB'))
    # GIF 时间以 10 ms 为单位，30/30/40 ms 循环保持每秒 30 张、原速播放。
    durations=[(30,30,40)[i%3] for i in range(len(frames))]
    frames[0].save(base/f'{name}.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0,disposal=2,optimize=False)
    with Image.open(base/f'{name}.gif') as check:
        assert check.n_frames==90
        assert sum(check.seek(i) or check.info['duration'] for i in range(check.n_frames))==3000
    frames[8].save(base/f'{name}-preview.png')
    print(name,': 90 frames, 3 seconds, loop, verified')

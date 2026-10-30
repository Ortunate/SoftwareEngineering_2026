# 本文件将 aim_lock_preview 的真实游戏帧编码为循环 GIF，默认使用 24 FPS。
"""用法：python tools/render_aim_lock_gif.py 帧目录 输出.gif。需要 Pillow。"""
import sys
from pathlib import Path
from PIL import Image


def main():
    """读取命名有序的 PNG 帧；所有输入必须同尺寸，输出目录自动创建。"""
    source, target = map(Path, sys.argv[1:])
    files = sorted(source.glob("frame-*.png"))
    if not files:
        raise ValueError("未找到验收帧")
    frames = []
    for file in files:
        with Image.open(file) as image:
            frames.append(image.convert("RGB"))
    # 共用锁定阶段的调色板，避免逐帧重选颜色造成静态背景闪烁和大面积差分。
    palette = frames[len(frames) // 2].quantize(colors=256)
    frames = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    # GIF 使用厘秒时间；40/40/40/40/40/50 毫秒的循环保持平均 24 FPS。
    durations = [50 if index % 6 == 5 else 40 for index in range(len(frames))]
    target.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(target, save_all=True, append_images=frames[1:], duration=durations,
                   loop=0, optimize=True, disposal=1)
    print(f"{target}: {len(frames)} frames, {sum(durations)/1000:.2f}s")


if __name__ == "__main__":
    main()

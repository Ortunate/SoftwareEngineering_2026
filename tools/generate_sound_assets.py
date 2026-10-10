#!/usr/bin/env python3
# 本文件生成可重复合成的短音效素材；不读取或提取参考游戏的音频。
"""使用标准库输出 44.1 kHz、单声道、16 位 PCM WAV。"""

import argparse
import math
from pathlib import Path
import random
import struct
import wave


# 参数为时长（秒）、起止频率（Hz）、噪声混合比例与峰值振幅；统一留出叠音余量。
SOUNDS = {
    "menu_move": (0.055, 900, 1400, 0, 0.16),
    "menu_confirm": (0.16, 600, 1400, 0, 0.24),
    "menu_cancel": (0.12, 600, 260, 0, 0.18),
    "shoot": (0.07, 1800, 550, 0.08, 0.10),
    "enemy_down": (0.22, 420, 55, 0.5, 0.24),
    "hit": (0.5, 700, 65, 0.7, 0.32),
    "bomb": (0.9, 180, 28, 0.7, 0.32),
    "graze": (0.075, 2300, 3400, 0.15, 0.10),
    "pickup": (0.10, 1000, 1700, 0, 0.15),
    "spell": (0.65, 320, 1800, 0.08, 0.24),
    "clear": (0.85, 440, 880, 0, 0.24),
    "full_power": (0.45, 660, 1320, 0, 0.22),
}


def write_sound(path: Path, spec: tuple, seed: int) -> None:
    """生成一个短促扫频音效；path 可覆盖同名素材，随机噪声由固定 seed 决定。"""
    duration, start, end, noise, amplitude = spec
    rate = 44100
    random_source = random.Random(seed)
    samples = bytearray()
    phase = 0.0
    for index in range(round(duration * rate)):
        t = index / rate
        progress = t / duration
        frequency = start * (end / start) ** progress
        phase += math.tau * frequency / rate
        # 4 ms 淡入避免波形起点爆音；指数淡出让连续射击与多次拾取仍可分辨。
        envelope = min(t / 0.004, 1.0) * (1.0 - progress) ** 2
        tone = (math.sin(phase) + 0.25 * math.sin(phase * 2)) / 1.25
        signal = ((1 - noise) * tone + noise * random_source.uniform(-1, 1))
        samples.extend(struct.pack("<h", round(signal * envelope * amplitude * 32767)))
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(samples)


def main() -> None:
    """写入指定素材目录；输出是运行时素材，常规构建不需要 Python。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    output = parser.parse_args().output
    output.mkdir(parents=True, exist_ok=True)
    for index, (name, spec) in enumerate(SOUNDS.items()):
        write_sound(output / f"{name}.wav", spec, index)


if __name__ == "__main__":
    main()

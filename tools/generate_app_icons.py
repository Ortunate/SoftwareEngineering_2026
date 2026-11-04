#!/usr/bin/env python3
# 本文件用 macOS 系统工具将已审查的芙兰朵露原图转换为跨平台应用图标，不改变原图内容。
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def generate_icons(root: Path) -> None:
    """从 assets/source/app_icon/flandre.png 生成 PNG、ICO 和 ICNS；覆盖对应衍生素材。

    需使用 macOS 的 sips；不需要 Python 第三方依赖，临时缩放文件自动清理。
    """
    source = root / 'assets/source/app_icon'
    master = source / 'flandre.png'
    runtime = root / 'assets/runtime/common/app_icon.png'
    runtime.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='koumakyou-icons-') as temporary:
        work = Path(temporary)

        def resize(size: int, destination: Path) -> None:
            """等比缩放方形原图为指定像素边长，保持颜色与构图。"""
            subprocess.run(['sips', '-z', str(size), str(size), str(master),
                            '--out', str(destination)], check=True, capture_output=True)

        resize(256, runtime)
        # ICO 每帧直接封装 PNG，保留 Alpha；256 的宽高字段按 ICO 约定写为零。
        sizes = (16, 24, 32, 48, 64, 128, 256)
        frames = []
        for size in sizes:
            path = work / f'{size}.png'
            resize(size, path)
            frames.append(path.read_bytes())
        offset = 6 + 16 * len(sizes)
        entries = []
        for size, frame in zip(sizes, frames):
            entries.append(struct.pack('<BBBBHHII', size % 256, size % 256,
                                       0, 0, 1, 32, len(frame), offset))
            offset += len(frame)
        (source / 'koumakyou.ico').write_bytes(
            struct.pack('<HHH', 0, 1, len(sizes)) + b''.join(entries) + b''.join(frames))

        # ICNS 使用现代 PNG 图块和大端长度字段；同时提供 Retina 图块，避免依赖桌面转换服务。
        chunks = []
        for tag, size in ((b'icp4', 16), (b'icp5', 32), (b'icp6', 64),
                          (b'ic07', 128), (b'ic08', 256), (b'ic09', 512),
                          (b'ic10', 1024), (b'ic11', 32), (b'ic12', 64),
                          (b'ic13', 256), (b'ic14', 512)):
            path = work / f'{size}.png'
            if not path.exists():
                resize(size, path)
            data = path.read_bytes()
            chunks.append(tag + struct.pack('>I', len(data) + 8) + data)
        body = b''.join(chunks)
        (source / 'koumakyou.icns').write_bytes(b'icns' + struct.pack('>I', len(body) + 8) + body)


def main() -> None:
    """可选指定项目根目录，默认从脚本位置定位项目。"""
    parser = argparse.ArgumentParser(description='转换 koumakyou 应用图标（macOS）')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    generate_icons(parser.parse_args().root.resolve())


if __name__ == '__main__':
    main()

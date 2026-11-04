#!/usr/bin/env python3
# 本文件将已构建的原生游戏和运行时素材打包为可双击的 macOS app，不依赖 Wine。
import argparse
from pathlib import Path
import plistlib
import shutil


def package_game(build: Path, destination: Path) -> Path:
    """复制 build 中的游戏与 assets 到新的 app；源须存在，目标已存在时拒绝覆盖。

    启动器将设置、成绩和录像写入用户的 Library/Application Support/koumakyou，
    不写入 app；以 exec 保持同一进程，便于 computer use 识别。直接运行构建目录的程序不受影响。
    ICNS 从项目源素材复制到 Contents/Resources，用于 Finder 与启动前的 Dock 图标。
    返回 app 绝对路径；不签名、不安装到系统 Applications，也不修改源构建目录。
    """
    executable = build / 'koumakyou'
    assets = build / 'assets'
    icon = Path(__file__).resolve().parents[1] / 'assets/source/app_icon/koumakyou.icns'
    if not executable.is_file() or not assets.is_dir():
        raise FileNotFoundError('先构建 koumakyou，确保同目录含 assets')
    if destination.exists():
        raise FileExistsError(f'目标已存在：{destination}')
    if not icon.is_file():
        raise FileNotFoundError(f'缺少应用图标：{icon}')
    macos = destination / 'Contents/MacOS'
    macos.mkdir(parents=True)
    resources = destination / 'Contents/Resources'
    resources.mkdir()
    shutil.copy2(icon, resources / 'koumakyou.icns')
    shutil.copy2(executable, macos / 'koumakyou')
    shutil.copytree(assets, macos / 'assets')
    launcher = macos / 'launch-koumakyou'
    launcher.write_text(
        '#!/bin/sh\n'
        '# 本文件只为 app 启动创建用户数据目录，素材仍由游戏按可执行文件路径读取。\n'
        'set -eu\n'
        'bundle_macos=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        'data_directory="${HOME:?无法确定用户主目录}/Library/Application Support/koumakyou"\n'
        'mkdir -p "$data_directory"\n'
        'cd "$data_directory"\n'
        'exec "$bundle_macos/koumakyou" "$@"\n', encoding='utf-8')
    launcher.chmod(0o755)
    with (destination / 'Contents/Info.plist').open('wb') as output:
        plistlib.dump({
            'CFBundleExecutable': 'launch-koumakyou',
            'CFBundleIdentifier': 'local.koumakyou.stage1',
            'CFBundleName': 'koumakyou',
            'CFBundleDisplayName': 'koumakyou',
            'CFBundleIconFile': 'koumakyou.icns',
            'CFBundlePackageType': 'APPL',
            'CFBundleShortVersionString': '0.1.0',
            'CFBundleVersion': '1',
            'NSHighResolutionCapable': True,
        }, output)
    return destination


def main() -> None:
    """接收构建目录和新 app 路径；默认使用当前项目的 macOS Release 构建。"""
    parser = argparse.ArgumentParser(description='打包原生 koumakyou macOS app')
    parser.add_argument('--build', type=Path, default=Path('build/macos-release'))
    parser.add_argument('--output', type=Path, default=Path('output/koumakyou.app'))
    args = parser.parse_args()
    print(package_game(args.build.resolve(), args.output.resolve()))


if __name__ == '__main__':
    main()

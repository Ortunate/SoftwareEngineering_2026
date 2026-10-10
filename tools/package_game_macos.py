#!/usr/bin/env python3
# 本文件将已构建的原生游戏和运行时素材打包为可双击的 macOS app，不依赖 Wine。
import argparse
from pathlib import Path
import plistlib
import shutil


def package_game(build: Path, destination: Path) -> Path:
    """复制 build 中的游戏与 assets 到新的 app；源须存在，目标已存在时拒绝覆盖。

    设置保存在包内 MacOS 目录；启动器以 exec 保持同一进程，便于 computer use 识别。
    返回 app 绝对路径；不签名、不安装到系统 Applications，也不修改源构建目录。
    """
    executable = build / 'koumakyou'
    assets = build / 'assets'
    if not executable.is_file() or not assets.is_dir():
        raise FileNotFoundError('先构建 koumakyou，确保同目录含 assets')
    if destination.exists():
        raise FileExistsError(f'目标已存在：{destination}')
    macos = destination / 'Contents/MacOS'
    macos.mkdir(parents=True)
    shutil.copy2(executable, macos / 'koumakyou')
    shutil.copytree(assets, macos / 'assets')
    launcher = macos / 'launch-koumakyou'
    launcher.write_text(
        '#!/bin/sh\n'
        '# 本文件将工作目录定位到可写 app 内，再进入原生游戏进程。\n'
        'set -eu\n'
        'bundle_macos=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        'cd "$bundle_macos"\n'
        'exec "$bundle_macos/koumakyou" "$@"\n', encoding='utf-8')
    launcher.chmod(0o755)
    with (destination / 'Contents/Info.plist').open('wb') as output:
        plistlib.dump({
            'CFBundleExecutable': 'launch-koumakyou',
            'CFBundleIdentifier': 'local.koumakyou.stage1',
            'CFBundleName': 'koumakyou',
            'CFBundleDisplayName': 'koumakyou',
            'CFBundlePackageType': 'APPL',
            'CFBundleShortVersionString': '0.2.0',
            'CFBundleVersion': '2',
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

#!/usr/bin/env python3
# 本文件负责生成依赖本机 CrossOver 的 th06nc.app 实验包装，不复制或修改游戏数据。

import argparse
import os
from pathlib import Path
import plistlib
import re
import shlex
import shutil


def package_app(destination: Path, crossover: Path, bottle: str, game: Path) -> Path:
    """创建新的 macOS 包；路径必须存在且目标不能覆盖旧包，返回包的绝对路径。

    包内的 Wine loader 与 ntdll 必须来自同一 CrossOver 安装；依赖该安装及原容器，
    不是可分发的独立游戏。bundle ID 是否保留到游戏进程必须另行实机验证。
    """
    runtime = crossover / "Contents/SharedSupport/CrossOver"
    loader = runtime / "lib/wine/x86_64-unix/wine"
    ntdll = runtime / "lib/wine/x86_64-unix/ntdll.so"
    wine = runtime / "bin/wine"
    for path in (game, loader, ntdll, wine):
        if not path.is_file():
            raise FileNotFoundError(path)
    if destination.exists():
        raise FileExistsError(f"目标已存在，拒绝覆盖：{destination}")

    steam_id = None
    # Steam 游戏直接运行时仍需 App ID；从安装清单读取，避免硬编码机器路径或游戏版本。
    for manifest in game.parent.parent.parent.glob("appmanifest_*.acf"):
        text = manifest.read_text(encoding="utf-8")
        folder = re.search(r'"installdir"\s+"([^\"]+)"', text)
        app_id = re.search(r'"appid"\s+"(\d+)"', text)
        if folder and app_id and folder[1] == game.parent.name:
            steam_id = app_id[1]
            break

    macos = destination / "Contents/MacOS"
    resources = destination / "Contents/Resources"
    macos.mkdir(parents=True)
    resources.mkdir()
    shutil.copy2(loader, macos / "wine-loader")
    # loader 首先按自身位置查找 ntdll；借用原安装保证二进制版本一致。
    (macos / "ntdll.so").symlink_to(ntdll)
    launcher = macos / "launch-th06nc"
    launcher.write_text(
        "#!/bin/sh\n"
        "# 本文件负责在原容器中初始化环境，再以同一进程运行包内 Wine loader。\n"
        "set -eu\n"
        'bundle_macos=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        f"cd {shlex.quote(str(game.parent))}\n"
        + (f"export SteamAppId={steam_id} SteamGameId={steam_id}\n" if steam_id else "")
        # 使用 exec 避免额外的常驻启动器；不启用重命名进程的 alternate loader。
        + f"exec {shlex.quote(str(wine))} --bottle {shlex.quote(bottle)} "
        '--no-update --no-gui --ux-app "$bundle_macos/wine-loader" '
        f"{shlex.quote(str(game))}\n",
        encoding="utf-8",
    )
    launcher.chmod(0o755)
    info = {
        "CFBundleIdentifier": "local.heny.th06nc.reference",
        "CFBundleName": "th06nc Reference",
        "CFBundleDisplayName": "th06nc Reference",
        "CFBundleExecutable": "launch-th06nc",
        "CFBundlePackageType": "APPL",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleVersion": "1",
        "CFBundleShortVersionString": "0.1",
        "NSHighResolutionCapable": True,
    }
    with (destination / "Contents/Info.plist").open("wb") as stream:
        plistlib.dump(info, stream)
    (destination / "Contents/PkgInfo").write_bytes(b"APPL????")
    (resources / "README.txt").write_text(
        "th06nc Reference 实验包装\n"
        "依赖本机 CrossOver、原 Steam 容器与已安装游戏，不能单独分发。\n"
        "启动前请关闭已有 th06nc.exe，避免重复实例导致初始化失败。\n"
        "此包装旨在验证 Wine 进程的 macOS 应用身份；生成成功不代表 computer use 已连接。\n"
        "移除这个 .app 即可撤销包装；游戏文件与容器配置未修改。\n",
        encoding="utf-8",
    )
    return destination


def main() -> None:
    """解析本机路径并生成包装；失败时保留异常原因供调用者定位。"""
    parser = argparse.ArgumentParser(description="生成 th06nc 的 CrossOver 实验包装")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--crossover", type=Path, default=Path("/Applications/CrossOver.app"))
    parser.add_argument("--bottle", default="Steam")
    parser.add_argument("--game", type=Path, required=True)
    args = parser.parse_args()
    if os.uname().sysname != "Darwin":
        parser.error("仅支持 macOS")
    print(package_app(args.output.resolve(), args.crossover.resolve(), args.bottle,
                      args.game.resolve()))


if __name__ == "__main__":
    main()

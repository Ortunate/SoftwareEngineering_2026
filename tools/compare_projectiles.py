#!/usr/bin/env python3
# 本文件构建并启动原作素材/正式模型对照查看器；只使用标准库，不启动游戏、不改玩家配置。
"""鼠标轨道查看全部弹体；--verify 可离屏检查并导出完整目录的对照图。"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def cache_values(directory: Path) -> dict[str, str]:
    """只读现有 CMake 缓存，避免误用其他源码目录的构建产物。"""
    path = directory / "CMakeCache.txt"
    if not path.is_file():
        return {}
    values = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith(("#", "//")) and ":" in line and "=" in line:
            key, value = line.split("=", 1)
            values[key.split(":", 1)[0]] = value
    return values


def executable(directory: Path, config: str) -> Path:
    """兼容 Make/Ninja 与 Windows 多配置目录；仅选择实际存在的查看器程序。"""
    for path in (directory / config / "projectile_comparison.exe",
                 directory / "projectile_comparison.exe",
                 directory / config / "projectile_comparison",
                 directory / "projectile_comparison"):
        if path.is_file():
            return path
    raise RuntimeError("projectile_comparison was not built; remove --no-build and try again")


def main(argv=None) -> int:
    """路径相对仓库根解释；子进程参数按数组传递，支持中文和空格路径。"""
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build-debug"))
    parser.add_argument("--config", choices=("Debug", "Release", "RelWithDebInfo"), default="Debug")
    parser.add_argument("--no-build", action="store_true", help="使用已经构建好的查看器")
    parser.add_argument("--verify", action="store_true", help="隐藏窗口检查全部变体，并导出对照图和 report.json")
    parser.add_argument("--capture-dir", type=Path, help="F12 / --verify 的输出目录")
    parser.add_argument("--entry", help="初始弹种标识，例如 rice、marisa-laser、bomb-spark")
    parser.add_argument("--variant", type=int, help="零起算颜色、档位或素材层")
    parser.add_argument("--view", choices=("orbit", "front", "side", "back", "top"))
    parser.add_argument("--frames", type=int, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    directory = (root / args.build_dir).resolve()
    cache = cache_values(directory)
    if cache.get("CMAKE_HOME_DIRECTORY") and Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() != root:
        raise RuntimeError("This build directory belongs to another project; choose --build-dir")
    if not args.no_build:
        cmake = shutil.which("cmake")
        if not cmake:
            raise RuntimeError("CMake is required to build the viewer")
        command = [cmake, "-S", str(root), "-B", str(directory), f"-DCMAKE_BUILD_TYPE={args.config}"]
        # 优先复用本项目现有 raylib 源码；没有本地副本时沿用根工程的 FetchContent 流程。
        sources = [Path(cache["FETCHCONTENT_SOURCE_DIR_RAYLIB"])] if cache.get("FETCHCONTENT_SOURCE_DIR_RAYLIB") else []
        sources += [root / "build/_deps/raylib-src", root / ".deps/raylib-5.5", directory / "_deps/raylib-src"]
        for source in sources:
            if (source / "CMakeLists.txt").is_file():
                command.append(f"-DFETCHCONTENT_SOURCE_DIR_RAYLIB={source.resolve()}")
                break
        subprocess.run(command, cwd=root, check=True)
        subprocess.run([cmake, "--build", str(directory), "--config", args.config,
                        "--target", "projectile_comparison", "-j", "6"], cwd=root, check=True)
    command = [str(executable(directory, args.config)), str(root)]
    if args.verify:
        command.append("--verify")
    if args.capture_dir is not None:
        command += ["--capture-dir", str((root / args.capture_dir).resolve())]
    if args.entry is not None:
        command += ["--entry", args.entry]
    if args.variant is not None:
        command += ["--variant", str(args.variant)]
    if args.view is not None:
        command += ["--view", args.view]
    if args.frames is not None:
        command += ["--frames", str(args.frames)]
    return subprocess.run(command, cwd=root).returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Projectile comparison: {error}", file=sys.stderr)
        sys.exit(1)

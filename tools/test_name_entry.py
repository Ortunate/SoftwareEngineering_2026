#!/usr/bin/env python3
# 本文件一键构建并启动公用命名界面的手动预览、自动回归或窗口冒烟检查，存档由测试程序隔离。
"""默认打开可操作窗口；--auto运行分数/Demo命名回归，--smoke短暂预览两种场景后自动退出。"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]


def run_step(command: list[str], log, cwd: Path) -> None:
    """在指定工作目录执行参数数组，逐行同时写控制台和UTF-8日志；失败保留退出码并中止后续步骤。"""
    print("\n> " + " ".join(command), flush=True)
    log.write("\n> " + " ".join(command) + "\n")
    log.flush()
    with subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace") as process:
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
            log.flush()
        code = process.wait()
        if code:
            raise subprocess.CalledProcessError(code, command)


def executable(build: Path, name: str, configuration: str) -> Path:
    """返回单配置或多配置构建生成的测试程序绝对路径；未生成时抛FileNotFoundError。"""
    suffix = ".exe" if sys.platform == "win32" else ""
    for directory in (build / configuration, build):
        path = directory / (name + suffix)
        if path.is_file():
            return path
    raise FileNotFoundError(f"未找到测试程序：{name}（构建目录：{build}）")


def main() -> int:
    """解析测试模式与目录，顺序构建、执行无窗口单元检查，再启动所选图形入口；失败返回非零码。"""
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--auto", action="store_true", help="自动验证两入口的输入、长度、取消和保存重试，并生成截图")
    mode.add_argument("--smoke", action="store_true", help="打开可见窗口，自动切换分数/Demo并截图后退出")
    parser.add_argument("--build-dir", type=Path, default=PROJECT_ROOT / "build-debug", help="CMake构建目录，默认复用项目build-debug")
    parser.add_argument("--output", type=Path, default=PROJECT_ROOT / "output/name-entry-review", help="截图及run.log输出目录")
    parser.add_argument("--config", choices=("Debug", "Release", "RelWithDebInfo"), default="Debug")
    parser.add_argument("--jobs", type=int, default=6, help="并行编译数，必须大于0")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs必须大于0")
    cmake = shutil.which("cmake")
    if not cmake:
        print("未找到cmake，请安装CMake并加入PATH。", file=sys.stderr)
        return 1
    build = args.build_dir.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    print(f"测试输出：{output}\n临时测试存档会在退出后清理，不修改实际成绩或Demo。", flush=True)
    try:
        with (output / "run.log").open("w", encoding="utf-8") as log:
            run_step([cmake, "-S", str(PROJECT_ROOT), "-B", str(build), "-DBUILD_TESTING=ON",
                      f"-DCMAKE_BUILD_TYPE={args.config}"], log, PROJECT_ROOT)
            # copy_assets会替换构建目录素材，因此必须完成全部构建后才能启动任何图形测试。
            run_step([cmake, "--build", str(build), "--config", args.config, "--target",
                      "koumakyou_ui_tests", "koumakyou_postgame_lifecycle_tests", "--parallel", str(args.jobs)],
                     log, PROJECT_ROOT)
            run_step([str(executable(build, "koumakyou_ui_tests", args.config))], log, PROJECT_ROOT)
            entry = "--names-only" if args.auto else "--names-preview-smoke" if args.smoke else "--names-preview"
            if not args.auto and not args.smoke:
                print("\nF1：分数命名  F2：Demo命名  F12：截图  F10／关闭窗口：退出\n"
                      "方向键/Tab选择按钮，直接键盘输入ASCII；Enter确认，Esc返回选存。", flush=True)
            run_step([str(executable(build, "koumakyou_postgame_lifecycle_tests", args.config)), str(output), entry],
                     log, PROJECT_ROOT)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"命名界面测试失败：{error}\n日志：{output / 'run.log'}", file=sys.stderr)
        return error.returncode if isinstance(error, subprocess.CalledProcessError) and error.returncode > 0 else 1
    except KeyboardInterrupt:
        print("\n已取消命名界面测试。", file=sys.stderr)
        return 130
    print(f"\n命名界面测试结束。截图及日志：{output}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

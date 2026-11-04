#!/bin/zsh
# 本文件提供 macOS 双击入口，参数原样传给跨平台 Python 启动脚本。
set -e
tool_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
exec python3 "$tool_dir/compare_projectiles.py" "$@"

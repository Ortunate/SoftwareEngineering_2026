#!/bin/zsh
# 本文件供macOS双击启动公用命名界面测试；全部参数原样交给同目录Python入口。
script_directory=${0:A:h}
python3 "$script_directory/test_name_entry.py" "$@"
entry_status=$?
if [[ -t 0 ]]; then
    print
    read '?按 Enter 关闭此终端窗口…'
fi
exit "$entry_status"

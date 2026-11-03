# 获取与开始

东方红魔乡：异典（koumakyou）是基于 C++20 与 raylib 的三维第一人称弹幕课设。使用键鼠在三轴空间中移动，并以第一人称观察与躲避弹幕。

## 获取游戏

<Downloads />

## 从源码启动

需要 Git、CMake 3.20 或更高版本以及支持 C++20 的编译器。首次配置需要网络下载 raylib 5.5。Windows 与 macOS 共用 CMake 工程。

```sh
git clone --branch dev/koumakyou https://github.com/Ortunate/SoftwareEngineering_2026.git
cd SoftwareEngineering_2026
```

Windows 可从仓库根目录双击 `tools/windows/Run-Model-Game.cmd`，按脚本提示准备工具并构建。平台预设、工具链要求和 macOS 启动步骤见 [仓库运行说明](/reference/README)。

## 第一次游戏

1. 在主菜单选择 Start。
2. 依次选择难度、角色、御札和模式，确认后进入关卡。
3. 通过 Option 检查移动、射击、Bomb 与低速操作绑定，调整灵敏度与音量。
4. 先使用练习熟悉第一人称视角；游戏内底部操作提示会随界面变化。

| 操作 | 默认方式 |
| --- | --- |
| 前后左右移动 | W / A / S / D |
| 上升 / 下降 | Space / Left Shift |
| 观察方向 | 鼠标 |
| 菜单确认 | Z / Enter |
| 常规菜单返回 | X / Esc / 鼠标右键 |
| 自定义操作 | Option → KEY BINDINGS |

战斗射击、Bomb、低速等操作以当前键位设置为准，支持键盘和鼠标绑定。

## 常见问题

### 为什么没有下载按钮？

当前尚未配置经过验证的公开发布包。源码可以构建，但不等同于已经完成可执行程序发布验收。

### 为什么首次启动需要较长时间？

首次构建需要准备工具链、依赖和运行素材；进入关卡也会加载本关资源。加载完成才推进战斗时间。

### 设置和录像在哪里？

设置文件与 `replays/` 位于游戏启动目录。切换启动目录可能使游戏读取另一套本地记录。录像仅支持相同构建，详见 [录像说明](/reference/REPLAY)。

### 这是浏览器游戏吗？

游戏运行于 Windows/macOS 桌面，网站提供介绍、使用说明与开发资料。

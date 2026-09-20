# Red Mist 3D Demo

一个使用 C++20 和 raylib 实现的原创 3D 弹幕射击课设原型。演示节奏参考《东方红魔乡》一面 Normal 难度，但不包含原作代码、关卡数据或美术音频素材。

## 已实现

- 边长 20 个逻辑单位的全三维立方体战场
- 第一人称 6DoF 视角，摄像机位置就是球形判定点
- 支持键盘和鼠标操作的开始界面
- WASD 前后左右、Space 上升、Left Shift 下降、鼠标观察
- OPTIONS 界面支持 FPS 上限调节，并通过 KEY BINDINGS 二级页面修改战斗键位
- FPS 和按键设置自动保存到本地配置文件，并在下次启动时加载
- 8192 容量的三维弹幕对象池
- Fibonacci 球面星状散射与三维锥形瞄准弹
- 擦弹、残机、Bomb、分数和暂停
- 事件驱动的道中编队
- 强化敌人与两阶段 Boss
- Windows/macOS 共用 CMake 工程
- 核心弹幕数学测试

## 构建要求

- CMake 3.20 或更高版本
- 支持 C++20 的编译器
- Git，用于 CMake 首次下载 raylib 5.5

raylib 会在首次配置时通过 CMake FetchContent 下载，项目没有其他运行时依赖或素材文件。

## macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake --build build --config Release
ctest --test-dir build --output-on-failure
./build/red_mist_demo
```

Apple Silicon 与 Intel Mac 均使用相同命令。首次启动时，系统可能要求允许程序接收键盘输入。

## Windows Visual Studio

在 Developer PowerShell 中执行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\red_mist_demo.exe
```

也可以使用 CMake GUI 打开项目根目录并选择 Visual Studio 生成器。

## Windows MinGW

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
.\build\red_mist_demo.exe
```

## 操作

| 按键 | 功能 |
|---|---|
| 鼠标 | 第一人称观察 |
| W / S | 沿视线前进、后退 |
| A / D | 左右横移 |
| Space | 上升 |
| Left Shift | 下降 |
| 鼠标左键 / Z | 射击 |
| C | 低速移动 |
| 鼠标右键 / X | Bomb |
| Esc / P | 暂停或继续 |
| R | 胜利或失败后重新开始 |
| M / Esc | 暂停、胜利或失败界面返回主菜单 |

开始界面的 `OPTIONS` 提供 30、60、120、144、240 和 Unlimited 六档 FPS 上限，使用左右方向键调节并立即生效。进入 `KEY BINDINGS` 二级页面后，可以修改前进、后退、左右移动、上升、下降、低速、射击、Bomb 和暂停键。将一个动作绑定到已使用的按键时，系统会自动交换两个动作的原按键。

程序启动时会在当前运行目录中查找 `red_mist_settings.cfg`。如果文件不存在，程序会加载默认设置并自动创建该文件；FPS 或按键发生变化时会立即写回。无效的 FPS、按键值或重复按键会回退为安全的默认值。

详细架构和数据结构见 `docs/DESIGN.md`。

## 开发截图

程序支持渲染一帧后自动退出，便于做视觉回归检查：

```bash
./build/red_mist_demo --capture red_mist_capture.png
./build/red_mist_demo --capture-gameplay gameplay_capture.png
./build/red_mist_demo --capture-options options_capture.png
```

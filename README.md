# Red Mist 3D Demo

一个使用 C++20 和 raylib 实现的三维弹幕射击课设原型。目前可玩范围为《东方红魔乡：新典》Normal 一面开场至道中露米娅退场。以[用户提供的通关录像](https://www.bilibili.com/video/BV1P2Yb6gE83/)作视觉参考；现有出怪时间、弹数和弹速仍为近似值，尚未完成逐帧校准，因此当前版本不能称为 1:1 复刻。

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
- Fairy、Orb、Elite 敌人子类分别定义并释放自己的弹幕技能
- 道中露米娅的一段普通攻击，右—中—左—中—右移动，击破或超时后结束演示
- 新绘制的露米娅、普通妖精、强化妖精与弹体透明贴图，分别用于 Boss、敌人和部分弹幕的三维朝向相机渲染
- Windows/macOS 共用 CMake 工程
- 核心弹幕数学测试

## 构建要求

- CMake 3.20 或更高版本
- 支持 C++20 的编译器
- Git，用于 CMake 首次下载 raylib 5.5

raylib 会在首次配置时通过 CMake FetchContent 下载。贴图位于 `assets/rumia_modern.png`、`assets/fairy_modern.png`、`assets/elite_modern.png` 和 `assets/bullet_pearl.png`，构建时会自动复制到可执行文件旁的 `assets` 目录。

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

## 弹幕与关卡结构

`BulletSystem` 只创建单颗弹体，并负责对象池、运动更新和清除；它不决定弹幕图案。`FairyEnemy`、`OrbEnemy`、`EliteEnemy` 各自实现 `UseSkill`，由 `Enemy::Update` 按开火计时器调用。`RumiaBoss` 的 `FireAimedVolley` 生成道中战的朝向玩家弹幕。Normal 道中露米娅没有符卡；项目中的符卡显示接口保留作后续关卡扩展，本段不会触发。

当前的三维化方式是将敌人与 Boss 放进可移动的立方体战场，并让弹幕在朝向玩家的三维锥面传播。玩家可以沿三个坐标轴移动和转向。此设计保留了参考战斗的出场顺序与道中 Boss 边界，但原作的二维运动投影、资源掉落、火力成长、对话、音效、精确时间轴和逐弹参数还未复刻。

## 开发截图

程序支持渲染一帧后自动退出，便于做视觉回归检查：

```bash
./build/red_mist_demo --capture red_mist_capture.png
./build/red_mist_demo --capture-gameplay gameplay_capture.png
./build/red_mist_demo --capture-options options_capture.png
```

# Red Mist 3D Demo

一个使用 C++20 和 raylib 实现的三维弹幕射击课设原型。目前可玩范围为《东方红魔乡：新典》Normal 一面开场至一面结算。流程依据用户提供的 187 秒本地录像重排；事件时间与弹幕参数仍是近似值，尚未完成逐帧校准，因此当前版本不能称为 1:1 复刻。

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
- 敌人自定义 P 点与分数点数量的概率分布，两类道具统一生成和拾取
- Power 道具收集、0–128 火力成长与 Miss 后减少 16 Power
- 金色直射符卡、青蓝追踪符卡，以及 Debug 射击测试场地
- 事件驱动的道中编队
- Fairy、DarkFairy、Orb 敌人子类分别定义并释放自己的弹幕技能
- 道中露米娅普通攻击、后续妖精编队、灵梦与露米娅对话、关底四阶段及一面结算
- 关底阶段按普通攻击 →「夜符 Night Bird」→ 普通攻击 →「境符 Demarcation」推进，两张符卡展示立绘与名称
- 程序生成的红雾天空盒，以及新绘制的灵梦和露米娅立绘、两类妖精与红蓝弹体贴图
- 通用素材在启动时加载，Stage 1 素材在进入关卡时加载，并在结算或离开关卡时卸载
- 标题、Stage 1 道中和关底 Boss 使用独立的流式 BGM
- 红雾背景使用六面天空盒，转动镜头时保持固定的世界方向，避免球面极区拉伸
- 子弹、敌人和 Boss 的贴图锚点与球形判定中心分别配置；大幅仰俯视时贴图仍面向镜头
- Windows/macOS 共用 CMake 工程
- 核心弹幕数学测试

## 项目结构

```text
.
├── CMakeLists.txt / CMakePresets.json   # 构建配置；引擎代码只编译一次
├── src/
│   ├── main.cpp                         # 窗口初始化与静默截图入口
│   ├── app/                             # Game 生命周期、调度与输入
│   ├── config/                          # 设置文件和按键配置
│   ├── core/                            # 定长对象池
│   ├── gameplay/
│   │   ├── actors/                      # 玩家、敌人和 Boss 基类
│   │   ├── bullets/                     # 弹幕对象池与方向算法
│   │   ├── combat/                      # 命中、炸弹与阶段结算
│   │   └── items/                       # 敌人掉落分布与道具生成
│   ├── render/                          # 世界、精灵、HUD 和菜单绘制
│   ├── resources/                       # 素材清单、GPU 资源与 AssetManager
│   ├── stage/                           # 关卡接口、注册和通用时间轴
│   └── stages/stage1/                  # 第一面的时间轴、素材、对话、敌人和露米娅
├── assets/
│   ├── runtime/common/                 # 启动时加载的天空盒、着色器和标题 BGM
│   ├── runtime/stages/stage1/          # 第一面贴图及道中、Boss BGM
│   ├── source/                         # 原始弹体图与旧背景参考图
│   └── library/bgm/                    # 尚未在当前关卡使用的 BGM
├── tools/                               # 素材生成器与渲染压力测试
├── tests/                               # 按设置、玩法、关卡拆分的测试
└── docs/                                # 设计说明与录像对照记录
```

运行时生成的 `red_mist_settings.cfg` 位于启动目录，不属于源码。详细类关系和数据结构见 [设计文档](docs/DESIGN.md)。

## 构建要求

- CMake 3.20 或更高版本
- 支持 C++20 的编译器
- Git，用于 CMake 首次下载 raylib 5.5

raylib 会在首次配置时通过 CMake FetchContent 下载。仅 `assets/runtime` 会在构建时复制到可执行文件旁的 `assets` 目录；原图和备用音乐不进入运行包。

`AssetManager` 在程序启动时加载菜单与关卡共用的天空盒和标题 BGM；开始 Stage 1 时加载角色、弹幕、对话立绘、着色器、道中 BGM 和 Boss BGM；一面结算、游戏失败或返回菜单时释放 Stage 1 资源。露米娅的世界贴图与演出立绘共用同一纹理。BGM 以流式方式播放，暂停游戏时暂停，静默截图模式不初始化音频设备。当前运行时素材中没有音效文件；以后可在 `Stage1Content::Assets()` 的关卡清单或通用音效清单登记文件，再通过 `PlaySoundEffect` 播放。

天空盒素材是 `assets/runtime/common/crimson_mist_skybox.png`，按 OpenGL 的 +X、-X、+Y、-Y、+Z、-Z 顺序横向排列。六面由同一个方向采样函数生成；如需重绘，在项目根目录运行 `cmake --build build --target generate_skybox`，然后运行 `./build/generate_skybox assets/runtime/common/crimson_mist_skybox.png`（Windows 下使用对应的可执行文件路径）。常规构建只加载现成贴图，不在启动时生成纹理。

`assets/source` 保留 `bullet_pearl.png` 与 `bullet_red.png` 原图；游戏使用裁去透明边并缩小到 512×512 的 `*_runtime.png`。重新生成时运行 `cmake --build build --target generate_sprite_assets`，然后运行 `./build/generate_sprite_assets assets/source assets/runtime/stages/stage1`。构建游戏目标时会把贴图和着色器复制到可执行文件旁。

需要检查高弹量绘制开销时，可运行 `cmake --build build --target sprite_benchmark`，再运行 `./build/sprite_benchmark assets/runtime`。该程序默认在隐藏窗口分别测量空场景和 8192 发弹幕场景的平均帧时间。需要测量实际可见窗口时，追加 `--visible`；两种模式的数值不可直接比较。

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
| F3（仅 Debug 构建） | 显示或隐藏判定球与发弹点调试图层 |
| F4（仅 Debug 构建） | 开启或关闭无敌调试 |
| F5 / F6（仅 Debug 射击测试） | Power 减少 / 增加 8 |

在 VS Code CMake Tools 中选择 `macos-debug`（Windows 选择对应的 Debug 预设）并启动游戏时，玩家默认开启无敌调试，HUD 右上角会显示状态。受击仍会清除接触到的敌弹，但不会扣残机或重置玩家位置；F4 可在游玩中切换，切换状态在重新开始一局后保持。F3 判定显示和截图参数 `--debug-render` 也只在 Debug 构建中编译。Release 构建始终使用正常受伤判定。

Power 从 0 开始，最大为 128。小怪死亡时按自身的联合概率表决定 P 点与分数点的数量：普通妖精有 75% 概率掉 1 个 P 点、25% 概率掉 1 个分数点；暗色妖精依次有 50% 概率掉 1 P＋1 分数、25% 概率掉 2 P、25% 概率掉 2 分数；光球有 75% 概率掉 8 P＋2 分数、25% 概率掉 4 P＋4 分数。普通 P 点增加 1 Power，分数点增加 1000 分；道中 Boss 暂时沿用固定掉落一个增加 8 Power 的大 P 点，其专属规则留待后续实现。两类道具向玩家移动并在靠近后吸附，15 秒后消失，场上共用 1024 槽的道具池；达到满火力时清除场上敌弹，Miss 后减少 16 点。直射弹使用金色符卡，追踪弹使用青蓝符卡，两者保留贴图透明通道并叠加半透明效果。[《东方红魔乡》说明书](https://www.thpatch.net/wiki/Th06/HTML_Manual/ja)记载了 128 上限与 Miss 扣 16 点；本项目在 8、16、32、48、64、80、96、128 点升级，具体弹数和三维追踪轨迹按当前战场调整。

Debug 构建的主菜单增加 `SHOT TEST`。进入后关卡时间轴停止，场中只有一只不会移动或攻击的靶怪；击空血量后会立即复原。使用 F5/F6 每次降低或增加 8 点 Power，HUD 显示当前数值和靶怪血量；按 Esc 暂停，再按 M 返回主菜单。Release 构建不显示这个入口。

开始界面的 `OPTIONS` 提供 30、60、120、144、240 和 Unlimited 六档 FPS 上限，使用左右方向键调节并立即生效。进入 `KEY BINDINGS` 二级页面后，可以修改前进、后退、左右移动、上升、下降、低速、射击、Bomb 和暂停键。将一个动作绑定到已使用的按键时，系统会自动交换两个动作的原按键。

程序启动时会在当前运行目录中查找 `red_mist_settings.cfg`。如果文件不存在，程序会加载默认设置并自动创建该文件；FPS 或按键发生变化时会立即写回。无效的 FPS、按键值或重复按键会回退为安全的默认值。

详细架构和数据结构见 `docs/DESIGN.md`。

## 扩展关卡

每个关卡实现 `StageContent`，自行提供素材清单、事件表、敌人工厂、Boss 工厂、事件处理和对话。第一面的实现集中在 `src/stages/stage1/`；通用 `Enemy` 只负责入场、边界检查和调用子类；各具体敌人类自行选择移动状态、推进技能计时并发弹。同一敌人类固定一种外观，可拥有多条轨迹与多个技能。时间轴只传出生位置、延迟和编队上下文，不指定通用运动路径。`StageEvent::action` 与 `archetype` 仅由所属关卡解释。新增关卡时创建自己的内容模块并在 `src/stage/catalog.cpp` 注册，`Game`、`StageDirector`、`AssetManager` 和 `SpriteRenderer` 无需添加新的敌人或素材分支。

弹幕、敌人与 Boss 记录 `VisualId`。关卡清单将该 ID 映射到图片路径、绘制方式、世界尺寸、锚点及光晕参数。`AssetManager` 在关卡开始时按清单加载，在结束时卸载；相同路径的角色贴图与立绘共享同一纹理。`SpriteRenderer` 按外观 ID 分组弹幕，每帧遍历弹池一次，避免随外观种类增加而重复扫描。

## 弹幕与关卡结构

`BulletSystem` 只创建单颗弹体，并负责对象池、运动更新和清除；它不决定弹幕图案。各敌人子类实现 `Move` 和 `UpdateSkills`，由 `Enemy::Update` 在入场后调用；具体技能与各自的计时器由子类持有。`RumiaBoss` 分别处理道中和关底遭遇，并在关底阶段调用两种普通攻击与两张符卡技能函数。道中露米娅没有符卡；关底符卡开始时清除旧弹幕，展示立绘与名称，再释放弹幕。

第一面的事件表由 `Stage1Content` 提供，`StageDirector` 只按顺序调度。`StageEvent::time` 默认表示距上一事件实际开始的秒数；设置 `afterEndOf` 后，表示距指定事件实际结束的秒数。道中 Boss 退场后才开始计算后续出怪的延迟，对话播放完毕后才触发关底 Boss。

当前的三维化方式是将敌人与 Boss 放进可移动的立方体战场，并让弹幕在朝向玩家的三维锥面传播。玩家可以沿三个坐标轴移动和转向。对话以英文简述表现，流程顺序参考录像；原作的二维运动投影、逐项掉落表、音效、精确时间轴和逐弹参数仍未复刻。录像观察与时间映射见 [Stage 1 对照记录](docs/STAGE1_REFERENCE.md)。

## 开发截图

程序支持渲染一帧后自动退出，便于做视觉回归检查：

所有 `--capture*` 命令都使用隐藏且不获焦的窗口和离屏渲染纹理，不会切换到游戏窗口；截图路径可使用相对或绝对路径。正常启动游戏仍显示窗口。

```bash
./build/red_mist_demo --capture red_mist_capture.png
./build/red_mist_demo --capture-gameplay gameplay_capture.png
./build/red_mist_demo --capture-options options_capture.png
./build/red_mist_demo --capture-stage 125 night_bird_capture.png
./build/red_mist_demo --capture-stage 6 side_view.png 90 20
./build/macos-debug/red_mist_demo --capture-stage 8 hitbox_capture.png 0 0 --debug-render
```

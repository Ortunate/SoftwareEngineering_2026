# koumakyou

一个使用 C++20 和 raylib 实现的三维弹幕射击课设原型。目前可玩范围为《东方红魔乡：新典》Normal 一面开场至一面结算。流程依据用户提供的 187 秒本地录像重排；事件时间与弹幕参数仍是近似值，尚未完成逐帧校准，因此当前版本不能称为 1:1 复刻。

## 已实现

- 尺寸 20×20×23（X×Y×Z）的全三维长方体战场，Z 为长轴
- 第一人称 6DoF 视角，摄像机位置就是球形判定点
- 浅色红月标题、深红花纹选项、现代化透明立绘，支持 Z / Enter、方向键和鼠标
- 灵梦两种御札、标准/无限残机挑战模式、整关练习、两张符卡单独练习，以及音乐室和最高通关成绩页
- WASD 前后左右、Space 上升、Left Shift 下降、鼠标观察
- Option 界面支持 FPS 上限、BGM / SE 音量、鼠标灵敏度、额外残机和开局 Bomb 调节；KEY BINDINGS 支持键盘和鼠标按钮
- FPS、BGM 音量、鼠标灵敏度和键鼠绑定即时保存，下次启动时自动加载
- 8192 容量的三维弹幕对象池
- 球面散射、三维锥形瞄准弹、反向旋转的 Night Bird 双层曲线与带共同开口的 Demarcation 红绿蓝三层环
- 具有 1.05 秒预警、0.65 秒生效和胶囊判定的固定三维光束，Bomb / 换阶段清除光束
- 战场边界外的三维森林与湖面，暂停冻结环境滚动
- 擦弹、残机、Bomb、分数和暂停
- 敌人自定义 P 点与分数点数量的概率分布，两类道具统一生成和拾取
- Power 道具收集、0–128 火力成长与 Miss 后减少 16 Power
- 金色直射符卡、青蓝追踪符卡，以及 Debug 射击测试场地
- 事件驱动的道中连续出怪
- 普通 Fairy 不攻击且一发击破；DarkFairy 前进后发射一次稀疏球面弹幕并原路退场，Orb 释放锥形弹幕
- 所有小怪与 Boss 共用淡入光环、收缩退场和三维击破粒子；暂停冻结效果，关底退场完毕后结算
- 道中露米娅普通攻击、后续妖精波次、灵梦与露米娅对话、关底四阶段及一面结算
- 关底阶段按普通攻击 →「夜符 Night Bird」→ 普通攻击 →「暗符 Demarcation」推进，两张符卡展示立绘与名称
- 程序生成的红雾天空盒，以及新绘制的灵梦和露米娅立绘、两类妖精与红蓝弹体贴图
- 通用素材在启动时加载，Stage 1 素材在进入关卡时加载，并在结算或离开关卡时卸载
- 标题、Stage 1 道中和关底 Boss 使用独立的流式 BGM
- 红雾背景使用六面天空盒，转动镜头时保持固定的世界方向，避免球面极区拉伸
- 子弹、敌人和 Boss 的贴图锚点与球形判定中心分别配置；大幅仰俯视时贴图仍面向镜头
- Windows/macOS 共用 CMake 工程
- 核心弹幕数学测试，以及 60 Hz 完整一面通关/符卡练习图形集成测试

标题页随窗口比例调整：背景等比裁剪铺满窗口，横屏菜单靠右，宽高比小于 1.2 的方形/竖屏将标题置顶并给菜单增加浅色底板。标题、菜单行、页脚和鼠标点击共用响应式布局，不依赖固定 1100×760 的留边画布。设置与其他页面继续采用等比居中的设计画布。

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

运行时生成的 `koumakyou_settings.cfg` 位于启动目录，不属于源码。详细类关系和数据结构见 [设计文档](docs/DESIGN.md)。

项目名、可执行文件和 C++ 命名空间统一使用 `koumakyou`，开发分支为 `dev/koumakyou`。旧版玩家设置需要将文件重命名为 `koumakyou_settings.cfg` 后放回启动目录；配置内容与格式保持兼容。

## 构建要求

- CMake 3.20 或更高版本
- 支持 C++20 的编译器
- Git，用于 CMake 首次下载 raylib 5.5

raylib 会在首次配置时通过 CMake FetchContent 下载。仅 `assets/runtime` 会在构建时复制到可执行文件旁的 `assets` 目录；原图和备用音乐不进入运行包。

`AssetManager` 在程序启动时加载菜单与关卡共用的天空盒和标题 BGM；开始 Stage 1 时加载角色、弹幕、对话立绘、着色器、道中 BGM 和 Boss BGM；一面结算、游戏失败或返回菜单时释放 Stage 1 资源。露米娅的世界贴图与演出立绘共用同一纹理。BGM 以流式方式播放，暂停游戏时暂停，静默截图模式不初始化音频设备。通用音效覆盖菜单、射击、击破、受击、Bomb、擦弹、拾取、符卡、通关与满火力，独立 SE 音量与 BGM 分开控制。新增插画、音效及字体的来源和重建方法见 [素材说明](docs/STAGE1_ASSETS.md)。

天空盒素材是 `assets/runtime/common/crimson_mist_skybox.png`，按 OpenGL 的 +X、-X、+Y、-Y、+Z、-Z 顺序横向排列。六面由同一个方向采样函数生成；如需重绘，在项目根目录运行 `cmake --build build --target generate_skybox`，然后运行 `./build/generate_skybox assets/runtime/common/crimson_mist_skybox.png`（Windows 下使用对应的可执行文件路径）。常规构建只加载现成贴图，不在启动时生成纹理。

`assets/source` 保留 `bullet_pearl.png` 与 `bullet_red.png` 原图；游戏使用裁去透明边并缩小到 512×512 的 `*_runtime.png`。重新生成时运行 `cmake --build build --target generate_sprite_assets`，然后运行 `./build/generate_sprite_assets assets/source assets/runtime/stages/stage1`。构建游戏目标时会把贴图和着色器复制到可执行文件旁。

需要检查高弹量绘制开销时，可运行 `cmake --build build --target sprite_benchmark`，再运行 `./build/sprite_benchmark assets/runtime`。该程序默认在隐藏窗口分别测量空场景和 8192 发弹幕场景的平均帧时间。需要测量实际可见窗口时，追加 `--visible`；两种模式的数值不可直接比较。

角色表现的无窗口测试随 `ctest` 运行。具有桌面图形会话时，还可构建 `koumakyou_lifecycle_tests` 并运行该程序，在隐藏窗口验证击破奖励、清场、Boss 换阶段、暂停和关卡素材释放。可选首个参数是截图路径前缀，例如 macOS Debug：

```bash
cmake --build --preset macos-debug --target koumakyou_lifecycle_tests
./build/macos-debug/koumakyou_lifecycle_tests /tmp/actor-effects
```

测试不初始化音频，设置和成绩写入独立临时目录，测试结束时清理，不修改玩家记录。

## macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake --build build --config Release
ctest --test-dir build --output-on-failure
./build/koumakyou
```

Apple Silicon 与 Intel Mac 均使用相同命令。首次启动时，系统可能要求允许程序接收键盘输入。

### 可双击的原生 app

在项目根目录使用 Release 预设构建并打包：

```bash
cmake --preset macos-release
cmake --build --preset macos-release --target koumakyou
python3 tools/package_game_macos.py
```

生成 `output/koumakyou.app`，包含原生游戏和运行时素材，可双击运行，也可由 computer use 按完整路径连接。此 app 使用当前机器的构建架构；本次已在 Apple Silicon 上实际启动验证。设置保存在包内 `Contents/MacOS/koumakyou_settings.cfg`，打包工具不复制构建目录的玩家记录。无需 Wine，也不安装到系统 Applications。

工具拒绝覆盖已有 app；再次打包时可用 `--output output/koumakyou-new.app` 指定新目标，或用 `--build` 指定其他构建目录。生成的 app 与截图不提交到 Git。

## Windows Visual Studio

本次验证环境为 VS 2026、CMake 4.4.3、raylib 6.0。默认依赖仍为 raylib 5.5；VS 2022 用户将生成器改为 `Visual Studio 17 2022`。VS 2026 需要支持该生成器的 CMake 4.2+。

在 Developer PowerShell 中执行：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\koumakyou.exe
```

若 GitHub 下载失败，配置命令末尾追加 `-DFETCHCONTENT_SOURCE_DIR_RAYLIB="D:/Libraries/raylib-5.5"`（改为实际 raylib 源码目录，不是游戏项目）。

Windows/MSVC 构建自动嵌入 UTF-8 应用清单，保留中文音乐名；无需修改系统全局 UTF-8 设置，也无需每次手工修补 exe。

也可以使用 CMake GUI 打开项目根目录并选择对应的 Visual Studio 生成器。

## Windows MinGW

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
.\build\koumakyou.exe
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
| Mouse 4 / 第一颗侧键（可改键） | Focus / 低速移动 |
| 鼠标右键 / X | Bomb |
| Esc / P | 暂停或继续 |
| R | 胜利或失败后重新开始 |
| Z / Enter、方向键、鼠标 | 菜单选择；对话推进 |
| Ctrl | 跳过对话 |
| M | 暂停、胜利或失败界面返回主菜单 |
| Esc | 胜利或失败界面返回主菜单；暂停界面恢复游戏 |
| F3（仅 Debug 构建） | 显示或隐藏判定球与发弹点调试图层 |
| F4（仅 Debug 构建） | 开启或关闭无敌调试 |
| F5 / F6（仅 Debug 构建） | Power 减少 / 增加 8，正常 START 与射击测试均可使用 |

在 VS Code CMake Tools 中选择 `macos-debug`（Windows 选择对应的 Debug 预设）并启动游戏时，玩家默认开启无敌调试，HUD 右上角会显示状态。受击仍会清除接触到的敌弹，但不会扣残机或重置玩家位置；F4 可在游玩中切换，切换状态在重新开始一局后保持。F3 判定显示和截图参数 `--debug-render` 也只在 Debug 构建中编译。Release 构建始终使用正常受伤判定。

Power 从 0 开始，最大为 128。小怪死亡时按自身的联合概率表决定 P 点与分数点的数量：普通妖精有 75% 概率掉 1 个 P 点、25% 概率掉 1 个分数点；暗色妖精依次有 50% 概率掉 1 P＋1 分数、25% 概率掉 2 P、25% 概率掉 2 分数；光球有 75% 概率掉 8 P＋2 分数、25% 概率掉 4 P＋4 分数。普通 P 点增加 1 Power，分数点增加 1000 分；道中 Boss 暂时沿用固定掉落一个增加 8 Power 的大 P 点，其专属规则留待后续实现。两类道具生成时以 4 单位/秒向世界 +Z 上抛，受沿 -Z 的 8 单位/秒² 加速度影响，0.5 秒后到达比生成点高 1 单位的顶点，再回落加速至 2.5 单位/秒后匀速下落；玩家进入战场 +Z 端五分之一区域（z ≥ 6.9）时，场上所有道具以 20 单位/秒飞向玩家，离开该区域后仍持续追踪；之后新掉落的道具重新从上抛开始，靠近即可拾取。未进入追踪状态的道具沿 −Z 下落至场地后边界之外（z < −11.5）时消失，追踪中的道具持续到拾取或清场，普通掉落物共用 1024 槽的道具池。Power 从未满升到 128 时显示 `Full Power` 提示 2 秒，并将当前全部敌弹在原位置转成 `star_point` 星点；星点立即以 20 单位/秒追踪玩家，无需进入自动吸取区域，每吸收一个增加 100 分。星点独立存储，不占普通道具池，也不会因普通道具池已满而漏掉转换。满级后继续收集 P 点不会重复触发；Miss 后减少 16 点，再次升满可重新触发。直射弹使用金色符卡，追踪弹使用青蓝符卡，gold 弹由视线前方偏下的单个中央模块对称发射，cyan 弹从其左右两个独立发射点射出，各侧与 gold 发射点相距 0.78 世界单位；三个发射点整体沿屏幕下方偏移 0.6 世界单位；两类弹按小角度横向散射，张角随 Power 档位增大；金色弹单侧最大偏转从 2° 增至满级 8°，青蓝弹从 Power 8 时的 3.5° 增至满级 10°，并在散射出射后继续追踪目标。两者保留贴图透明通道，并统一使用 20% 不透明度（Alpha 51）。[《东方红魔乡》说明书](https://www.thpatch.net/wiki/Th06/HTML_Manual/ja)记载了 128 上限与 Miss 扣 16 点；本项目在 8、16、32、48、64、80、96、128 点升级，具体弹数和三维追踪轨迹按当前战场调整。

Debug 构建的主菜单增加 `SHOT TEST`。进入后关卡时间轴停止，场中只有一只不会移动或攻击的靶怪；击空血量后会立即复原。正常 START 与射击测试中都可以使用 F5/F6 每次降低或增加 8 点 Power，升满时同样触发 `Full Power` 提示与敌弹转星点；HUD 显示调节按键，射击测试还显示靶怪血量。暂停时不调整 Power；按 Esc 暂停，再按 M 返回主菜单。Release 构建不显示射击测试入口，也不提供这些 Power 调节键。

开始界面的 `Option` 提供以下设置，修改立即生效并自动保存：

- `FPS LIMIT`：30、60、120、144、240 和 Unlimited。
- `BGM Vol`：0～100%，步进 5%，默认 50%；0 为静音，菜单、道中和 Boss 音乐统一使用该设置，不改变音效音量。
- `MOUSE SENSITIVITY`：0.1～3.0 倍，步进 0.1，默认 1.0 倍（原来的 0.0024 弧度/像素）。重开和切换关卡保持设置，不额外乘帧时间。
- `SE Vol`：0～100%，步进 5%，默认 60%。
- `Player`：额外残机 1～5，默认 2；`Bomb`：开局 0～3，默认 3；下次开局生效。
- `KEY BINDINGS`：键盘和鼠标左/右/中键、Mouse 4～7；Focus 默认 Mouse 4（第一颗侧键）。滚轮滚动不属于按钮。无侧键鼠标可改回 C。

上下键选择项目，左右键调整数值；也可以点击独立的左/右箭头按钮进行减小/增大；点击标签、数值及空白不会调整。数值到达边界后停止，帧率不循环跳档。选择改键行后，先松开用于确认的按键，再按新的键盘键或鼠标按钮；Esc 取消，F3～F6 留给调试功能。冲突时交换绑定。`FIRE (ALTERNATE)` / `BOMB (ALTERNATE)` 分别是可编辑的第二个射击/Bomb 入口，默认左键/右键；不存在额外硬编码鼠标快捷键。选项页的 `RESET DEFAULTS` 重置全部设置并保留成绩，改键页的同名按钮只重置绑定。

macOS 和 Windows 启动游戏后会在游戏窗口内禁用输入法文字合成，移动、射击及改键继续使用物理按键，不会弹出拼音候选框。该策略覆盖菜单、战斗和暂停，不切换系统输入源或修改其他应用的输入法设置；菜单栏或任务栏的输入法图标仍可能显示中文。macOS 对游戏视图禁用文字输入上下文并阻断文字解释，Windows 解除本窗口的 IMM 上下文关联；未来若增加文本输入框，需要为文字录入单独恢复输入法。

可在具有图形会话的 macOS/Windows 环境构建并运行 `koumakyou_window_input_tests`，隐藏窗口测试原生按下/松开、文字合成屏蔽、重复应用策略、调整尺寸及窗口重建。例如 macOS：`cmake --build --preset macos-debug --target koumakyou_window_input_tests`，随后运行 `./build/macos-debug/koumakyou_window_input_tests`。

程序启动时会在当前运行目录中查找 `koumakyou_settings.cfg`。如果文件不存在，程序会加载默认设置并自动创建该文件；FPS、音量、灵敏度或按键发生变化时会立即写回。无效的 FPS、音量、灵敏度、按键值或重复按键会回退为安全默认值。配置格式升级为 version=3；旧版的默认 Focus=C 会迁移为 Mouse 4，旧版其他自定义 Focus 键与 FPS 会保留。新版自行绑定 C 后不会在下次启动被迁移。

详细架构和数据结构见 `docs/DESIGN.md`。

## 扩展关卡

每个关卡实现 `StageContent`，自行提供素材清单、事件表、敌人工厂、Boss 工厂和对话。第一面的实现集中在 `src/stages/stage1/`；通用 `Enemy` 负责出生后的年龄、边界检查和调用子类，各具体敌人类持有自己的运动状态，有攻击能力的敌人自行管理技能计时器。同一类固定一种外观，普通妖精与光球的轨迹由 `FairyParams`、`OrbParams` 显式指定，`DarkFairyParams` 只提供出生位置与发弹后等待时间。`EnemySpec` 使用 `std::variant` 保存对应类型的参数，不再通过原型编号、编队或槽位选择敌人行为。新增关卡时在 `src/stage/catalog.cpp` 注册内容，并在 `src/stage/enemy_spec.hpp` 登记其参数类型、扩展配置校验；`Game`、`AssetManager` 和 `SpriteRenderer` 无需添加新的敌人或素材分支。公共事件由 `StageContent::HandleEvent` 统一执行。

弹幕、敌人与 Boss 记录 `VisualId`。关卡清单将该 ID 映射到图片路径、绘制方式、世界尺寸、锚点及光晕参数。`AssetManager` 在关卡开始时按清单加载，在结束时卸载；相同路径的角色贴图与立绘共享同一纹理。`SpriteRenderer` 按外观 ID 分组弹幕，每帧遍历弹池一次，避免随外观种类增加而重复扫描。

## 弹幕与关卡结构

一面 Normal 的 222 次逐只出生与 570 条帧指令来自本机《东方红魔乡：新典》数据。`ReferenceScript` 在原像素坐标与 60 Hz 时基执行调用、循环、镜像移动、插值、瞄准、速度层、延迟转向和光束；`ReferenceEnemy`、`RumiaBoss` 将状态接入原生演员生命周期。旧的球面/锥面演员仍可用于调试，实际一面不再使用概括式技能。

`StageEvent::atFrame` 是开局起算的绝对帧，不能同时设置 `afterEndOf`；普通关卡仍可用 `delay` 相对上一事件或指定事件结束调度。道中 Boss 在 1882 帧入场，战斗期间时间轴继续运行并跳过到期杂鱼；5092 帧开始战前对话，之后依次为普通攻击一、夜符「Night Bird」、普通攻击二、暗符「Demarcation」、退场和战后独白。

敌机航线与弹幕映射到 X/Z 平面，敌人沿 Z 从远处入场；整轮弹幕增加朝玩家高度的 Y 速度，继续支持 6DoF 操作。HP/阈值统一转换到当前伤害单位，原作随机范围和 rank 修正保留；玩家火力、道具运动、碰撞与计分继续适配三维玩法。

15 句战前、2 句战后台词采用新典 zh-CN 原文，按原等待时间和双行延迟展示，Z / Enter 推进一句、Ctrl 跳过。场地、侧栏和一面题签使用原始图集，按 16:9 等比留边；角色继续采用现代重绘，当前仍为统一表情。具体数据来源、帧时序、三维约定与验证见 [Stage 1 对照记录](docs/STAGE1_REFERENCE.md)。

## 开发截图

程序支持渲染一帧后自动退出，便于做视觉回归检查：

所有 `--capture*` 命令都使用隐藏且不获焦的窗口和离屏渲染纹理，不会切换到游戏窗口；截图路径可使用相对或绝对路径。正常启动游戏仍显示窗口。

```bash
./build/koumakyou --capture koumakyou_capture.png
./build/koumakyou --capture-gameplay gameplay_capture.png
./build/koumakyou --capture-options options_capture.png
./build/koumakyou --capture-stage 215 night_bird_capture.png
./build/koumakyou --capture-stage 6 side_view.png 90 20
./build/macos-debug/koumakyou --capture-stage 8 hitbox_capture.png 0 0 --debug-render
```

# 红雾幻想 3D 第一人称空间弹幕演示版设计

## 1 设计目标

项目实现一个可在 Windows 和 macOS 运行的真正三维弹幕射击演示。战斗发生在边长 20 个逻辑单位的立方体内部，玩家可以沿 X、Y、Z 三轴移动并自由观察。摄像机位置与玩家碰撞球中心始终完全一致，因此画面视点就是判定点。

当前战斗范围为《东方红魔乡：新典》Normal 开场至一面结算。事件时刻和攻击参数根据用户提供的本地通关录像近似设置，尚未逐帧核对。天空盒由程序生成，角色和弹体贴图为新绘制素材。

## 2 项目结构

```text
├── CMakeLists.txt
├── README.md
├── docs/
│   └── DESIGN.md
├── src/
│   ├── main.cpp
│   ├── core/fixed_pool.hpp
│   └── game/
│       ├── game_types.hpp
│       ├── bullet_system.hpp/.cpp
│       ├── bullet_patterns.hpp
│       ├── enemy.hpp/.cpp
│       ├── boss.hpp/.cpp
│       ├── settings.hpp/.cpp
│       ├── stage_system.hpp/.cpp
│       ├── sprite_renderer.hpp/.cpp
│       └── game.hpp/.cpp
├── assets/rumia_modern.png
├── assets/reimu_modern.png
├── assets/fairy_modern.png
├── assets/elite_modern.png
├── assets/bullet_pearl.png
├── assets/bullet_red.png
├── assets/bullet_pearl_runtime.png
├── assets/bullet_red_runtime.png
├── assets/crimson_mist_stage.png
├── assets/crimson_mist_skybox.png
├── assets/shaders/skybox.vs/.fs
├── assets/shaders/sprite_cutout.fs
├── assets/shaders/bullet_core.fs
├── tools/generate_skybox.cpp
├── tools/generate_sprite_assets.cpp
├── tools/sprite_benchmark.cpp
└── tests/core_tests.cpp
```

依赖方向为 `main -> Game -> StageDirector/Enemy/Boss/BulletSystem`。敌人和 Boss 子类控制技能释放，`bullet_patterns.hpp` 只计算弹幕方向，`BulletSystem` 管理单颗弹体及其定长对象池。raylib 提供窗口、输入、贴图和基础绘制。

## 3 空间与第一人称摄像机

- 立方体逻辑边界为 `[-10,10]^3`。
- 玩家碰撞中心为 `Player::position`。
- 每帧强制执行 `camera.position = player.position`。
- 鼠标控制 yaw 和 pitch，pitch 被限制在约正负 83 度，避免摄像机翻转。
- W/S 沿观察方向移动，A/D 横移，Space 上升，Left Shift 下降。
- 玩家不能越过立方体六个表面。

所有战斗键位都保存在 `KeyBindings` 中。主菜单的 OPTIONS 一级页面负责 FPS 上限调节，并通过 KEY BINDINGS 进入按键配置二级页面。按键发生重复时交换两个动作的原按键，确保一个按键不会同时触发两个动作。低速移动默认使用 C，避免与默认下降键 Left Shift 冲突。

FPS 上限提供 30、60、120、144、240 和 Unlimited 六档，通过 raylib 的 `SetTargetFPS` 即时应用。窗口未强制启用垂直同步，因此各档位不会被 VSync 设置覆盖。

设置持久化由独立的 `settings` 模块负责。程序启动时先使用 `std::filesystem::exists` 检查当前运行目录中的 `red_mist_settings.cfg`：文件不存在时构造默认 `GameSettings` 并创建文件；文件存在时逐项读取 FPS 和 `KeyBindings`。修改 FPS、完成改键或恢复默认键位后立即保存。读取过程中会校验 FPS 档位、按键有效性和键位唯一性，损坏的数据会回退并重写为有效设置。

立方体六个表面绘制空间网格，中央绘制红、绿、蓝三条坐标轴，帮助玩家判断深度、速度和朝向。

红雾背景由六面 cubemap 和一个立方体模型绘制。顶点着色器从视图矩阵中去掉平移，只保留旋转，因此玩家移动时背景保持无限远、转动时显示固定世界方向。天空绘制时关闭背面剔除和深度写入，随后恢复这两项状态供敌人和弹幕使用。六面贴图由同一个方向采样函数生成，云、月亮和远山跨面连续；旧的 `crimson_mist_stage.png` 保留作原始画风参考，不参与运行时渲染。

`position` 始终是三维判定球中心。`SpriteRenderer` 为各类敌人和 Boss 定义立绘的归一化躯干锚点及世界尺寸，用 `DrawBillboardPro` 将该锚点对齐到 `position`；屏幕上方向由摄像机朝向计算，因此高仰俯角下仍完整显示。敌人与 Boss 的发弹点从判定中心沿瞄准方向向前偏移，调试图层按 F3 显示判定球和发弹点。延迟入场的敌人在 `age < 0` 时不参与绘制或碰撞。

弹体由明确的 `BulletVisualKind` 指定蓝珠、红弹或实体球，不再根据颜色猜贴图。两张运行时弹体贴图由原图裁去透明外边并缩小至 512×512；显示直径相对碰撞半径校准。绘制先用透明裁剪着色器写入弹体实心部分的深度，再绘制角色，最后在关闭深度写入后用加色混合绘制近处弹体光晕。渲染按弹体贴图分组，近镜头弹体只显示较淡的光晕，以免遮蔽视线。

## 4 核心数据结构

### 4.1 Bullet

```cpp
struct Bullet {
    bool active;
    bool grazed;
    BulletOwner owner;
    BulletVisualKind visual;
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    Vector3 rotationAxis;
    float radius;
    float age;
    float maxAge;
    float angularVelocity;
    Color color;
};
```

弹幕的位置、速度、加速度和旋转轴全部使用 `Vector3`。曲线弹通过 Rodrigues 旋转公式绕任意三维轴旋转速度向量。

### 4.2 FixedPool

`FixedPool<Bullet, 8192>` 使用连续定长数组；运行期生成弹幕时不发生堆内存分配。敌人以多态对象放入 128 槽的 `EnemyPool`，生成敌人时发生一次堆分配。两种池都记录容量耗尽时的丢弃数量。

### 4.3 Player

玩家保存三维位置、yaw、pitch、球形判定半径和擦弹半径。摄像机不拥有独立位置，避免视觉位置与碰撞位置不同步。

### 4.4 Enemy 与 Boss

`Enemy` 是抽象基类；`FairyEnemy`、`DarkFairyEnemy`、`OrbEnemy`、`EliteEnemy` 分别实现唯一的 `UseSkill`，由共用的 `Enemy::Update` 按计时器调用。敌人航线包括直线、三维螺旋和左右交叉。`Boss` 是独立抽象基类，`RumiaBoss` 区分道中与关底遭遇。道中只有普通攻击；关底依次包含普通攻击、「Night Bird」、第二段普通攻击和「Demarcation」。

### 4.5 StageEvent

关卡由带时间戳的事件表驱动。生成事件包含三维中心坐标、敌人数、间距、敌人类型和队形类型。当前事件表覆盖开场、道中 Boss、后半段编队、对话和关底 Boss。事件时刻是近似值，录像时间映射见 `docs/STAGE1_REFERENCE.md`。

## 5 三维弹幕算法

### 5.1 球面星状散射

二维星状散射通常在圆周上均匀取角。三维版本需要在单位球面上均匀取方向。本项目采用 Fibonacci 球面：

```text
t = (i + 0.5) / n
y = 1 - 2t
r = sqrt(1 - y²)
phi = i * goldenAngle + rotation
direction = (r cos(phi), y, r sin(phi))
```

这种分布不会像经纬度网格一样在球体两极聚集。`pattern::ForEachSphereDirection` 提供方向，具体敌人的技能函数决定弹体参数并调用 `BulletSystem::Spawn`。当前只有强化敌人的技能使用球面散射。

### 5.2 三维锥形瞄准弹

首先计算敌人到玩家的单位方向，再构造与该方向垂直的 right/up 正交基。`pattern::ForEachConeDirection` 只计算方向；Orb、Elite 和 Boss 的技能函数负责生成弹体。弹幕按多个圆环分布在锥面内，因此玩家需要同时沿水平、垂直和纵深方向躲避。

### 5.3 三维碰撞与擦弹

所有判定均为球体碰撞，比较三维距离平方：

```text
(dx² + dy² + dz²) <= (r1 + r2)²
```

玩家只有一个判定球，因此敌弹检测为 O(B)。玩家弹和敌人数量较少，首版采用 O(Bp * E) 检测。

## 6 游戏流程

```text
主菜单
  -> 三维道中编队
  -> 蓝色与紫色妖精编队
  -> 道中露米娅普通攻击
  -> 后半段妖精编队
  -> 灵梦与露米娅对话
  -> 关底露米娅普通攻击
  -> 夜符 Night Bird
  -> 第二段普通攻击
  -> 境符 Demarcation
  -> 一面结算
```

## 7 当前限制

- 立方体战场与红雾天空盒由程序生成；灵梦、露米娅、两类妖精和部分弹体已使用新绘制贴图。
- 当前出怪时间、弹数、弹速和敌机航线按录像概括设置，仍需逐帧校准。
- 尚未实现原作的掉落、火力成长、音频及二维投影一致性；对话为英文简述。
- 当前使用球形碰撞，不考虑模型网格的精确碰撞。
- 关卡事件编译在程序中，后续可迁移到 JSON 和可视化编辑器。
- 尚未加入雷达；当前通过立方体网格、三轴参考线和近弹警告辅助空间判断。

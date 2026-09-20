# 红雾幻想 3D 第一人称空间弹幕演示版设计

## 1 设计目标

项目实现一个可在 Windows 和 macOS 运行的真正三维弹幕射击演示。战斗发生在边长 20 个逻辑单位的立方体内部，玩家可以沿 X、Y、Z 三轴移动并自由观察。摄像机位置与玩家碰撞球中心始终完全一致，因此画面视点就是判定点。

演示节奏参考《东方红魔乡》一面 Normal 难度，但不复制原作代码、精确关卡数据、模型、贴图或音乐。

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
│       ├── settings.hpp/.cpp
│       ├── stage_system.hpp/.cpp
│       └── game.hpp/.cpp
└── tests/core_tests.cpp
```

依赖方向为 `main -> Game -> StageDirector/BulletSystem/FixedPool`。raylib 只提供窗口、输入和基础绘制，空间运动、弹幕生成、碰撞和关卡逻辑由项目实现。

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

## 4 核心数据结构

### 4.1 Bullet

```cpp
struct Bullet {
    bool active;
    bool grazed;
    BulletOwner owner;
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

`FixedPool<T, Capacity>` 使用连续定长数组。弹幕池容量为 8192，敌人池容量为 128。运行期生成弹幕时不发生堆内存分配，池满时记录丢弃数量。

### 4.3 Player

玩家保存三维位置、yaw、pitch、球形判定半径和擦弹半径。摄像机不拥有独立位置，避免视觉位置与碰撞位置不同步。

### 4.4 Enemy 与 Boss

敌人和 Boss 使用三维位置。敌人航线包括直线、三维螺旋和左右交叉；Boss 同时沿 X、Y、Z 三轴缓慢移动，并使用空间弹幕攻击。

### 4.5 StageEvent

关卡由带时间戳的事件表驱动。生成事件包含三维中心坐标、敌人数、间距、敌人类型和队形类型。三维星形编队同样使用球面分布生成。

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

这种分布不会像经纬度网格一样在球体两极聚集。Boss 每次旋转 `rotation`，连续发射后形成旋转的空间星海。

### 5.2 三维锥形瞄准弹

首先计算敌人到玩家的单位方向，再构造与该方向垂直的 right/up 正交基。弹幕按多个圆环分布在锥面内，因此玩家需要同时沿水平、垂直和纵深方向躲避。

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
  -> 空间星状散射强化敌人
  -> 三维交叉编队
  -> Boss 三维锥形弹幕阶段
  -> Boss 球面星海阶段
  -> 关卡完成
```

## 7 当前限制

- 几何体是程序生成的占位表现，尚未加入正式模型和动画。
- 当前使用球形碰撞，不考虑模型网格的精确碰撞。
- 关卡事件编译在程序中，后续可迁移到 JSON 和可视化编辑器。
- 尚未加入雷达；当前通过立方体网格、三轴参考线和近弹警告辅助空间判断。

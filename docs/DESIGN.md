# 红雾幻想 3D 第一人称空间弹幕演示版设计

## 1 设计目标

项目实现一个可在 Windows 和 macOS 运行的真正三维弹幕射击演示。战斗发生在尺寸为 20×20×23（X×Y×Z）个逻辑单位的长方体内部，Z 为长轴，玩家可以沿 X、Y、Z 三轴移动并自由观察。摄像机位置与玩家碰撞球中心始终完全一致，因此画面视点就是判定点。

当前战斗范围为《东方红魔乡：新典》Normal 开场至一面结算。事件时刻和攻击参数根据用户提供的本地通关录像近似设置，尚未逐帧核对。天空盒由程序生成，角色和弹体贴图为新绘制素材。

## 2 项目结构

```text
├── CMakeLists.txt
├── README.md
├── docs/
│   └── DESIGN.md
├── src/
│   ├── main.cpp
│   ├── app/                     # Game 主循环、输入与状态
│   ├── config/                  # 设置与键位
│   ├── core/                    # 定长对象池
│   ├── gameplay/actors/         # 玩家、敌人与 Boss
│   ├── gameplay/bullets/        # 弹幕类型、弹池与方向算法
│   ├── gameplay/combat/         # 碰撞、射击与阶段推进
│   ├── gameplay/items/          # 掉落分布与道具生成、拾取
│   ├── render/                  # 世界、精灵与 UI 绘制
│   ├── resources/               # 素材清单、加载资源与所有权
│   ├── stage/                   # 关卡接口、注册与时间轴
│   └── stages/stage1/           # 第一面专用时间轴、素材、对话、敌人、Boss
├── assets/runtime/common/      # 天空盒、着色器、标题 BGM
├── assets/runtime/stages/stage1/ # 第一面贴图与音乐
├── assets/source/              # 原始素材和旧背景参考
├── assets/library/bgm/         # 尚未接入的音乐
├── tools/                      # 资源生成与性能测量
└── tests/                      # 设置、玩法、关卡测试
```

依赖方向为 `main -> Game -> StageContent/StageDirector/Enemy/Boss/BulletSystem/DropSystem/AssetManager`。`Stage1Content` 负责第一面敌人工厂和事件解释，时间轴、素材清单及对话分别定义在独立文件中；`StageDirector` 仅处理时序。敌人和 Boss 子类控制技能释放，`src/gameplay/bullets/patterns.hpp` 只计算弹幕方向，`BulletSystem` 管理单颗弹体及其定长对象池。具体敌人提供只含 P 点和分数点数量的掉落结果表，`DropSystem` 负责随机抽取和生成。`AssetManager` 独占图片、天空盒、着色器、流式 BGM 和音效句柄；`SpriteRenderer` 只借用句柄绘制。raylib 提供窗口、输入、音频与基础绘制。

素材按生命周期分组：程序启动时加载通用天空盒和标题 BGM；Stage 1 开始时加载角色、弹幕、对话立绘、战斗着色器以及道中和 Boss BGM。胜利、失败或返回菜单时停止关卡 BGM 并释放专属素材，菜单素材在程序退出时释放。露米娅的世界精灵和立绘共享一张纹理。音效清单目前为空，新增音效时由同一模块按所属生命周期加载和释放。截图模式不初始化音频设备，因此只加载图形素材。

## 3 空间与第一人称摄像机

- 长方体逻辑边界为 X/Y ∈ `[-10,10]`、Z ∈ `[-11.5,11.5]`，长宽比约 1.15。
- 玩家碰撞中心为 `Player::position`。
- 每帧强制执行 `camera.position = player.position`。
- 鼠标控制 yaw 和 pitch，pitch 被限制在约正负 83 度，避免摄像机翻转。
- W/S 沿观察方向移动，A/D 横移，Space 上升，Left Shift 下降。
- 玩家不能越过长方体六个表面。

所有战斗键位都保存在 `KeyBindings` 中。OPTIONS 一级页面调节 FPS、BGM 音量、鼠标灵敏度，KEY BINDINGS 二级页面编辑绑定。键盘沿用 raylib 键码，鼠标使用 1000+按钮编号（0～6）避免编码冲突。12 个入口包含原有十个动作与射击/Bomb 的第二绑定，所有入口统一参与冲突交换；Focus 默认 Mouse 4。Esc 和 F3～F6 保留，录入需先释放激活弹窗的按钮，避免误录。

FPS 上限提供 30、60、120、144、240 和 Unlimited 六档，通过 raylib 的 `SetTargetFPS` 即时应用。窗口未强制启用垂直同步，因此各档位不会被 VSync 设置覆盖。

设置持久化由独立的 `settings` 模块负责。程序启动时先使用 `std::filesystem::exists` 检查当前运行目录中的 `red_mist_settings.cfg`：文件不存在时构造默认 `GameSettings` 并创建文件；文件存在时逐项读取 FPS、BGM 音量（0～100）、灵敏度（10～300，100 为原速度）和 `KeyBindings`。修改后立即保存 version=2 配置，校验数值范围及键位唯一性。旧版默认 C 迁移为 Mouse 4，其他自定义值保留。AssetManager 把音量同步到菜单、道中、Boss 已有音乐流，并在加载新流后重新应用；静音不停止流的计时。Game 保存灵敏度设置，每帧通过 PlayerInput 的倍率传入 Player，玩家重置不会丢失设置。

长方体边框和六个表面的空间网格帮助玩家判断深度、速度和朝向。

红雾背景由六面 cubemap 和一个立方体模型绘制。顶点着色器从视图矩阵中去掉平移，只保留旋转，因此玩家移动时背景保持无限远、转动时显示固定世界方向。天空绘制时关闭背面剔除和深度写入，随后恢复这两项状态供敌人和弹幕使用。六面贴图由同一个方向采样函数生成，云、月亮和远山跨面连续；旧的 `crimson_mist_stage.png` 保留作原始画风参考，不参与运行时渲染。

`position` 始终是三维判定球中心。关卡素材清单为各类敌人和 Boss 定义立绘的归一化躯干锚点及世界尺寸；`SpriteRenderer` 用 `DrawBillboardPro` 将该锚点对齐到 `position`；屏幕上方向由摄像机朝向计算，因此高仰俯角下仍完整显示。敌人与 Boss 的发弹点从判定中心沿瞄准方向向前偏移，调试图层按 F3 显示判定球和发弹点。延迟入场的敌人在 `age < 0` 时不参与绘制或碰撞。

弹体由当前关卡的 `VisualId` 指定蓝珠、红弹、实体球或两张玩家符卡；`VisualMode` 决定是公告板还是实体球。两张敌弹运行时贴图由原图裁去透明外边并缩小至 512×512；玩家金色直射符卡和青蓝追踪符卡保持独立透明贴图。敌弹先用透明裁剪着色器写入实心部分的深度，再绘制角色；玩家符卡随后保留贴图与 tint 的透明度绘制，不写入深度。最后以加色混合绘制近处敌弹光晕。渲染按弹体贴图分组，近镜头敌弹只显示较淡的光晕，以免遮蔽视线。

## 4 核心数据结构

### 4.1 Bullet

```cpp
struct Bullet {
    bool active;
    bool grazed;
    BulletOwner owner;
    VisualId visual;
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    Vector3 rotationAxis;
    float radius;
    float age;
    float maxAge;
    float angularVelocity;
    float homingTurnRate;
    float damage;
    Color color;
};
```

弹幕的位置、速度、加速度和旋转轴全部使用 `Vector3`。曲线弹通过 Rodrigues 旋转公式绕任意三维轴旋转速度向量；玩家追踪弹每帧寻找最近的活动目标，并在 `homingTurnRate` 限制下转向。

### 4.2 FixedPool

`FixedPool<Bullet, 8192>` 使用连续定长数组；运行期生成弹幕时不发生堆内存分配。敌人以多态对象放入 128 槽的 `EnemyPool`，生成敌人时发生一次堆分配。`DropSystem` 另用 1024 槽固定池持有 P 点与分数点。各池都记录容量耗尽时的丢弃数量。

### 4.3 Player

`Player` 封装三维位置、yaw、pitch、判定半径、射击、Bomb 和 Power。摄像机不拥有独立位置，避免视觉位置与碰撞位置不同步。Power 范围为 0–128，普通 P 点增加 1、大 P 点增加 8，Miss 减少 16；8、16、32、48、64、80、96、128 是火力升级门槛。直射和追踪弹数按三维场地重新设计，追踪弹单发伤害较低。`DropSystem` 持有两类道具共用的 1024 槽对象池，按敌人静态结果表的相对权重抽取一次 P 点与分数点数量，将道具在敌人死亡位置附近错开生成。道具生成时以 4 单位/秒向世界 +Z 上抛，沿 -Z 以 8 单位/秒² 持续加速，0.5 秒后在比生成点高 1 单位处回落，下落速度达到 2.5 单位/秒后保持匀速；加速段和匀速段按限速时刻拆分并精确积分，保证不同帧长下位移一致；玩家进入战场 +Z 端五分之一区域（z ≥ 6.9）时，所有活动道具立即中断上抛或下落，记录追踪状态并以 20 单位/秒向玩家移动；该状态在玩家离开后仍保留，新生成道具需再次满足触发条件，靠近后拾取；未进入追踪状态的道具沿 −Z 下落至场地后边界之外（z < −11.5）时消失，追踪中的道具持续到拾取或清场。`Game` 应用拾取奖励；当前分数点固定增加 1000 分，Boss 暂沿用固定大 P 点。道具使用当前关卡提供的 `power_point` 与 `score_point` 外观，交给 `SpriteRenderer` 与敌人和 Boss 一起绘制。Debug 射击测试停用时间轴并保留固定靶怪，用 F5/F6 调整 Power。

### 4.4 Enemy 与 Boss

`Enemy` 是抽象基类，只推进入场年龄并检查边界。`stage1::FairyEnemy`、`DarkFairyEnemy`、`OrbEnemy` 各自实现 `Move` 与 `UpdateSkills`，在类内选择运动状态并持有技能计时器。每个具体敌人类固定一种外观，同类实例可因编队上下文选择不同轨迹。时间轴传递的 `EnemySpawn` 只描述出生位置、延迟、编队和槽位，不指定通用路径。`Boss` 是独立抽象基类，`stage1::RumiaBoss` 区分道中与关底遭遇。道中只有普通攻击；关底依次包含普通攻击、「Night Bird」、第二段普通攻击和「Demarcation」。

### 4.5 StageEvent

关卡由按顺序排列的事件表驱动；`StageDirector` 只调度时间，`StageContent` 根据关卡局部的 `action`、`archetype` 编号解释事件。`StageEvent::time` 默认是相对上一事件实际开始时刻的延迟；首个事件相对开局。若设置 `afterEndOf`，则从指定事件实际结束时刻开始计时。可被依赖的事件用唯一 `id` 标识；即时事件由回调同步结束，Boss 战和对话在游戏逻辑完成时向 `StageDirector` 报告结束。事件表覆盖开场、道中 Boss、后半段编队、对话和关底 Boss。生成事件还包含三维中心坐标、敌人数、间距、敌人类型和队形类型。事件间隔是近似值，录像时间映射见 `docs/STAGE1_REFERENCE.md`。

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

- 长方体战场与红雾天空盒由程序生成；灵梦、露米娅、两类妖精和部分弹体已使用新绘制贴图。
- 当前出怪时间、弹数、弹速和敌机航线按录像概括设置，仍需逐帧校准。
- 已实现基础 Power 道具掉落和火力成长；原作的逐敌掉落表、音频及二维投影一致性仍待补充，对话为英文简述。
- 当前使用球形碰撞，不考虑模型网格的精确碰撞。
- 关卡事件编译在程序中，后续可迁移到 JSON 和可视化编辑器。
- 尚未加入雷达；当前通过长方体边框、表面网格和近弹警告辅助空间判断。

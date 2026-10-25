# 代码注释要求

修改或新增代码时，必须遵守以下要求：

1. 每个源码文件开头简要说明该文件的职责。
2. 对供其他文件使用的类、函数和重要数据结构，说明用途，以及调用方必须知道的参数单位、取值约束、返回值、所有权、副作用或失败条件；不适用的项目无需凑齐。
3. 在定义处说明不直观的数据约定和不变量，例如时间基准、坐标系、编号作用域、对象池容量及资源生命周期。
4. 对复杂分支、算法和时序，解释为什么采用当前做法，以及后续修改必须保持什么条件。
5. 不写仅复述代码的注释；优先用清晰的命名和结构表达显而易见的行为。
6. 修改行为或接口时，必须同步更新受影响的注释；过时或与实现矛盾的注释必须修正或删除。
7. 注释使用中文，保留代码标识符原文，并沿用相邻代码的注释格式。

# Stage 编排约定

首次移植或修改任何关卡、敌人行为及 Boss 演出时，均遵循 [docs/STAGE_PORTING_METHOD.md](docs/STAGE_PORTING_METHOD.md) 的通用方法与以下分层。Stage 1 的配方、数量和时序仅作为案例，不限制后续关卡；具体案例见 [docs/STAGE1_BEHAVIORS.md](docs/STAGE1_BEHAVIORS.md)。

## 文件职责与调用链

- `src/stage/system.*`：定义 `StageEvent` / `StageAction`，由 `StageDirector` 保存关卡时钟、事件进度、批次出怪进度和结束依赖；不实现具体关卡的移动、攻击或素材选择。
- `src/stage/content.*`：`StageContent` 提供素材、事件表、Boss 工厂、对话及战斗规则；`HandleEvent` 将动作交给 `StageEventSink`。`Game` 实现接收端，负责演员池、Boss、对话和事件完成通知。
- `src/stages/stageN/timeline.cpp`：直接定义静态事件表；一面当前为 222 次逐只出生和 4 个 Boss/对话事件。事件表的存储必须覆盖 `StageDirector` 的借用期。
- `src/stages/stageN/behaviors.*`：定义关卡专用的命名行为配方与阶段协程，复用 `src/gameplay/actors/movement.*`、`skill.*` 和 `behavior.*` 的通用能力。
- `src/stages/stageN/boss.*`：将行为阶段接入公共 `Boss` 生命周期、伤害判定、HUD、符卡与退场接口；`content.*` 负责创建道中或关底遭遇。
- `manifest.cpp` / `ids.hpp` 管理关卡素材清单与编号，`dialogue.cpp` 管理台词和演出指令，`scenery.*` 管理背景。新增可玩关卡时，在 `src/stage/catalog.*` 注册内容，并在 `CMakeLists.txt` 登记源码。

## 时间轴事件

1. 时间轴直接列出 `StageEvent`，按预期触发顺序排列；相同帧的表内顺序也是演出的一部分。各关均保留显式编排，不通过初始化 lambda、循环、原始数据目录或子程序编号工厂生成生产事件表。局部辅助函数只封装世界坐标出生参数，不执行原作坐标转换、不推导事件时刻或隐式补充其他事件。
2. `atFrame` 为开局起算的非负整数绝对帧，固定按 60 Hz 换算；设置后忽略 `delay`，不能同时设置 `afterEndOf`。`delay` 的单位为秒，无依赖时相对上一事件的实际开始时间，首个事件相对关卡起点；有 `afterEndOf` 时相对指定事件的实际结束时间。
3. `id` 仅在本张事件表内唯一，`afterEndOf` 只能引用表中更早的 ID。调度器按表顺序处理，遇到未到期或依赖未完成的事件会停止启动后续事件，不会跳过它扫描后面的行。
4. `StageAction` 当前包含 `SpawnActors`、`StartBoss`、`StartDialogue`、`ShowMessage`。增加动作类型时，同步更新 `StageDirector` 的参数校验和 `StageContent::HandleEvent` 的执行访问器。
5. `SpawnActors` 携带 `behavior::Spawn`，`count >= 1`，`interval` 为非负有限秒数；第一只立即生成，零间隔同帧生成全部。批次不阻塞后续事件；调度器跨更新补发到期请求，每次交付的 `count` 规范为 1。最后一次请求发出即完成，不等待敌人死亡，也不因池满丢弃而延后。每只的出生参数或种子不同时，应分别列事件。
6. `StartBoss` 和 `StartDialogue` 必须带 `id`，处理时返回 `false`；遭遇或对话实际结束后，由游戏调用 `StageDirector::EndEvent(id)`。等待其结束的后续演出使用 `afterEndOf`，不能用预估固定帧代替。同步动作返回 `true`；出怪批次的完成由调度器管理。
7. `CancelEnemySpawns()` 只取消已开始批次中未发出的请求，不取消未来事件，也不清除存活演员。当前 Boss 入场取消旧批次并保留场上小怪、弹幕、道具和效果；Boss 活动期间新到期的小怪请求直接跳过，关卡时钟继续。对话开始会取消旧批次、清除小怪与敌弹。这是已验证的一面策略；后续 Stage 须核对其时钟、出怪和清场语义，差异通过明确参数或最小通用接口表达，不能直接照抄或在 Game 中堆叠关卡编号判断。
8. `StageDirector` 用 `deltaTime` 秒数累计关卡时间，跨更新触发到期事件；演员行为默认按固定 120 Hz 补算，60 Hz 模式只用于原作差分；内容帧号仍按 60 Hz 解释。显示帧率不能作为逻辑帧计数，暂停时不得继续推进。当前一面在 1882 帧开始道中 Boss、5092 帧开始战前对话，关底 Boss 等待战前对话结束，战后对话等待关底 Boss 结束。

## 普通演员与可复用行为

1. 普通敌人的生成链为 `StageEvent → SpawnActors → StageContent::HandleEvent → Game::SpawnStageActor → behavior::ActorPool::Spawn → Actor::Activate/Follow`。直接使用行为组件，不恢复已删除的 `Enemy` 继承体系、`EnemySpec`、`StageContent::CreateEnemy` 或按子程序编号选择行为的工厂。
2. `behavior::Pattern` 是无可变执行状态的配方，包含 `movement::Path`、可选 `skill::TimedShot`、动画、发射偏移、击破效果和逻辑寿命。共享配方在关卡 `behaviors.hpp` 中定义为有语义名称的 `inline constexpr` 常量；各关按真实行为差异拆分，数量不限于一面示例的 `kTurningFairy`、`kGentleTurningFairy`、`kLateTurningFairy`、`kShootingDarkFairy`、`kPassingDarkFairy` 五种。
3. `behavior::Spawn` 保存每只实例的出生位置、配方、HP、分数、掉落、镜像、随机横坐标标记、固定种子、外观、判定半径与素材基址。路线差异可以由同一配方结合不同出生位置和镜像构成；外观独立选择，不从移动或技能反推。配置按值复制，每个 `Actor` 独占运动、技能、随机数、动画与协程状态。
4. 通用移动和技能放在 `gameplay/actors`，不得依赖具体关卡或 ECL 操作码。`movement::Path` 当前至多四个按非递减帧号排列的关键帧；`TimedShot` 表达一次定时发射。复杂多轮攻击、循环、随机分支和阶段准备使用命名 `behavior::Routine` 协程，复用参数化辅助行为，不把攻击逻辑塞进时间轴。
5. 运行时统一使用世界坐标：`Spawn::position`、运动状态、插值目标、发射 XYZ 偏移、判定半径、光束尺寸、随机出生范围和回收/巡航边界均为世界单位；速度为世界单位/秒，加速度为世界单位/秒²，角速度为弧度/秒。行为等待、寿命与技能延迟仍使用 60 Hz 内容帧；120 Hz 每步推进 0.5 内容帧，不能把帧号直接当更新次数。关卡事件的 `delay` / `interval` 使用秒，不能混用。
6. 世界 +Y 为高度，敌人从 +Z 向 -Z 推进；极坐标方向从 +X 朝 -Z 为正，单位弧度，镜像只反转最终 X 位移。`movement::Bounds` 的二维分量表示世界 X/Z，不限制高度；随机横坐标和回收边界由 `Spawn::randomXRange` / `retirementBounds` 明确配置。源坐标换算只保留在 tests 下的独立参考侧，当前一面为 `tests/reference_coordinates.hpp`；生产代码不恢复 `movement::Space` 或像素/世界往返映射。
7. 每个固定更新步保持“应用上一帧位移 → 执行行为 → 计算下一帧速度”的顺序；`Follow` 在同一时刻先应用移动变更再发弹。子行为局部时钟从零开始，调用期间父时钟冻结，返回后父行为在同一更新步继续；阶段总时钟独立推进。阶段回调保留尚未消费的逻辑时间，不能在切阶段时丢弃。
8. 出生种子显式固定，不由事件表行号推导；随机横坐标在出生时求值一次。调整行为时保持随机读取顺序、插值的浮点求值边界和跨阶段技能上下文。已发射弹保存自己的运动参数副本；清弹只恢复既有 rank 修正，不顺带清空演员运动或后续发弹配置。
9. 道中 HP、分数与掉落直接写入事件配置，使用原始点数；`sourceItem=-2/-1/0/1/2` 分别为无掉落、全关轮换、小 P、分数、大 P。生成前保留 `behavior::Valid` 校验。演员池容量耗尽时丢弃该次请求；启用场外回收的演员须先满足入场条件，避免刚在场外缓冲带出生就被回收。
10. 演员及动画游标借用当前关卡素材；重开、退出或卸载关卡前，先清演员、Boss、弹幕及效果等借用者，再释放关卡资源。演员池清理和槽位复用必须释放旧协程及执行状态。

## Boss 与对话衔接

1. 当前 Boss 仍使用公共 `Boss` 接口；一面的 `RumiaBoss` 持有 `behavior::Actor`，负责把生命、位置、动画、音效、掉落与阶段完成状态接入游戏。移动与攻击编排在 `stage1::rumia` 命名行为中，不能在 `timeline.cpp` 按固定时刻展开各战斗阶段。
2. Boss 阶段生命、生命阈值和超时从 `native_combat` 统一目录读取，阶段回调使用命名行为。当前关底顺序为非符 → `NightBird` → 非符 → `Demarcation`，道中遭遇独立初始化。
3. 保持受伤开关、生命阈值、击破和超时的交接语义；碰撞发生在行为更新之后，阶段切换必须看到当帧扣血。准备期保护要在交接时立即生效，不能等下一次 120 Hz Tick；HUD 的非符/符卡分组血条与行为当前 HP 上限分别维护。
4. 战斗入口、阶段推进和完整退场分别处理；只有遭遇真正结束才通知对应 StageEvent 完成。对话台词和 `DialogueCue` 使用独立内容表，由游戏完成对话后报告事件结束，不通过演员行为伪造对话完成。

## 参考数据与验证

1. `tests/reference_script.*` 与 `tests/reference_program.hpp` 只作为独立旧行为基准，不能重新链接进生产引擎。原子程序编号映射留在差分测试；新典原始解包、报告、截图及本机安装路径不进入运行时编排。
2. 已有实现的重构以可信旧行为为基准；首次移植的关卡须独立建立源语义或实测记录对照，保持明确的三维适配边界。当前一面覆盖 Normal 的 28 个可达子程序与 222 次出生；后续 Stage 独立统计覆盖，不能沿用这些数值或把一面专用解释器当作新关卡的完整基准。未接入的难度、关卡和原生入口不因参考数据存在就视为已实现。
3. 修改时序、配方、投影或生命周期时，同步维护 `tests/stage_tests.cpp`、`tests/behavior_tests.cpp` 和受影响的集成用例。原作差分须显式使用 `behavior::UpdateRate::Hz60`，生产演员池和 Boss 默认使用 `Hz120`；另测 120 Hz 半帧移动、内容事件/插值/超时不加速，以及不同显示帧率的一致性。参考期望值应独立于新配方；对照出生参数、世界位置、动画、随机分支、实际弹幕、回调、掉落及退场，并覆盖不同显示帧率；离散状态精确比较，世界位置允许不超过 `5e-4` 世界单位的累计舍入误差，不能以放宽误差掩盖时序或分支变化。详细不变量与对照范围见上述行为文档。
4. 按改动范围运行构建、CTest；涉及完整遭遇或资源生命周期时，运行相应图形集成目标。常用命令：

   ```bash
   cmake --build build-debug --target koumakyou koumakyou_tests -j 6
   ctest --test-dir build-debug --output-on-failure
   cmake --build build-debug --target koumakyou_lifecycle_tests -j 6
   ./build-debug/koumakyou_lifecycle_tests /tmp/stage-lifecycle
   ```

# 提交规范

1. 提交标题使用 `<type>(scope): <描述>` 格式；`scope` 可省略，跨模块改动也可省略。描述使用中文，简短说明完成了什么，不以句号结尾。
2. `type` 使用以下类别：`feat`（新功能）、`fix`（修复）、`refactor`（行为不变的重构）、`perf`（性能改进）、`docs`（文档）、`test`（测试）、`build`（构建配置）、`assets`（素材）或 `chore`（其他维护工作）。`scope` 使用受影响的模块名，例如 `stage`、`app`、`bullets`。
3. 一次提交只表达一件事。与该改动直接相关的代码、测试和文档放在同一提交；独立改动分开提交。只暂存本次提交需要的文件，并在提交前查看暂存差异。
4. 标题不足以说明原因或影响时，在标题后空一行写正文，说明修改原因、行为变化或迁移要求。破坏兼容性的改动在 `type` 或 `scope` 后加 `!`，并在正文中说明破坏点和迁移方法，例如 `feat(stage)!: 调整关卡接口`。
5. 提交前运行与改动相关的构建或测试，并检查暂存差异的空白错误。不要提交生成文件、缓存或敏感信息。无法完成的检查应在交付说明中如实说明。

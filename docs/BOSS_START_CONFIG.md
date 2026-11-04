# Boss 角色、技能与阶段配置

每个角色的全部专属行为由 `src/gameplay/bosses/<角色名>.hpp` 的内嵌 `Behaviors` 定义，实现放在对应的 `<角色名>_behaviors.cpp`。移动、准备、非符、符卡、特殊反应和退场都归角色模块；通用移动与发弹继续复用 `gameplay/actors`。咲夜的 Eternal Meek 行为单独放在 `sakuya_eternal_meek.cpp`，仍属于同一个 `SakuyaBoss::Behaviors`。

| 角色类 | 当前使用场合 |
| --- | --- |
| `RumiaBoss` | 一面道中、关底 |
| `DaiyouseiBoss` | 二面道中 |
| `CirnoBoss` | 二面关底 |
| `MeilingBoss` | 三面道中、关底 |
| `KoakumaBoss` | 四面道中 |
| `PatchouliBoss` | 四面关底 |
| `SakuyaBoss` | 五面道中、关底和六面道中 |
| `RemiliaBoss` | 六面关底 |

## 血量属于技能

角色的 `ResolveSkill` 将其 `Behavior` 标识解析为 `SkillDefinition`，绑定可消耗血量、时限、命名攻击、符卡信息、准备演出及特殊反应。生命和时间数据仍从 `native_combat` 的可追溯记录读取，不复制到战斗组合配置。

原行为使用“生命寄存器初值＋结束阈值”，技能定义将其表达为实际可消耗 `health` 和原寄存器适配偏移 `reserveHealth`。例如露米娅第一非符具有 6100 可消耗 HP，寄存器偏移 900；演员和公共 `Boss::health/maxHealth` 保留原寄存器量纲，进入时为 7000，触发阈值为 900，便于原作差分和当帧伤害交接。后继 Night Bird 的 900 HP 由它自己的定义初始化，不从前一技能转移生命状态。

帕秋莉的 Water Elf / Emerald Megalith 首组合与尾组合使用不同生命定义，拥有不同 Behavior 标识并复用相同弹幕算法。六面咲夜的机体相关生命由其角色技能目录在启动解析时绑定。配置更换技能组合不会覆盖这些技能血量。

## 阶段组合与血条外观

每关的 `src/stages/stageN/boss_config.hpp/.cpp` 提供命名 `StartConfig` 和静态 `Battle`。时间轴使用同一种异步事件，例如 `StartBoss{&kRumiaMidboss}` 和 `StartBoss{&kRumiaFinal}`，完整退场后才报告事件结束。

`StartConfig::phases` 是阶段数组，每个阶段的 `skills` 顺序组合多个技能；配置只保存带角色作用域的 Behavior 引用，不接受攻击闭包或技能生命。`phase`、`skill` 分别表示从 1 起算的运行阶段和组内技能，`attack` 是从 1 起算的展开技能号，供练习入口和遇见记录定位。露米娅关底是两个阶段、四个技能；琪露诺关底第二阶段包含一张非符和两张符卡。

血条每段显式配置本阶段的 `skillIndex`（0 起算）、`ratio` 和 RGBA `color`；数组顺序就是屏幕从左到右的排列，与技能执行顺序独立：

```cpp
using S = RumiaBoss::Behavior;
Phase phase{
    .skills = {S::FirstNonspell, S::NightBird},
    .healthBar = {.segments = {
        {.skillIndex=1, .ratio=0.2F, .color=BLUE},
        {.skillIndex=0, .ratio=0.8F, .color=GREEN},
    }},
};
```

比例完全由 config 决定，不从 HP 自动分配，也不按非符/符卡自动选色；`healthBar.background` 同样配置空段底色。Boss 在 `StartBoss` 时复制、解析并校验：阶段和技能不可为空，每次技能使用恰好对应一段，比例须为正且总和在浮点误差内等于 1，行为须属于该角色且为有效技能。段数不限制为三段，同一技能可重复使用，段按使用索引区分。

Boss 的 `HealthBar()` 产生检查用快照；实际绘制由 `Boss::DrawHealthBar(area)` 执行，Game HUD 只提供窗口逻辑像素区域。已完成技能为空段，尚未开始的技能为满段，当前技能的填充率为 `clamp((原寄存器生命-reserveHealth)/health,0,1)`；固定段宽始终保留。

| 关卡 | 道中阶段／技能数 | 关底阶段／技能数 |
| --- | --- | --- |
| 一面 | 1／1 | 2／4 |
| 二面 | 1／1 | 2／5 |
| 三面 | 1／2 | 3／6 |
| 四面 | 1／1 | 3／6 |
| 五面 | 1／2 | 3／6 |
| 六面 | 1／2 | 5／9 |

## 交接、演出与所有权

每次技能使用分别配置 `onDamage` 和 `onTimeout`。默认 Next 在阶段内前进，末技能进入下一阶段，全部完成后开始退场；Exit 直接退场；Jump 使用 0 起算的阶段和组内技能索引，启动时校验目标。三面道中和五、六面道中咲夜的第一技能超时直接 Exit，伤害完成才继续符卡。

技能切换保留未消费的逻辑时间、运动上下文和随机流，在碰撞前立即交接新生命与准备期保护。技能内的移动、清弹、音效、掉落及周期不变，下一技能由配置决定。四面组合符卡的独立 120 帧准备和二、六面尾符的延迟计时仍由对应技能定义维护。

`intro` 和 `resumeSteps` 引用角色内部演出；蕾米莉亚第一次通知开始翅膀演出，下一次通知才开战。角色跨技能记忆、时停状态和化蝠计时由实例持有，配置闭包不共享可变状态。所有时间仍为 60 Hz 内容帧，默认行为频率为 120 Hz，空间量统一为世界单位。

启动配置和已解析技能按值保存，外部容器修改不会影响活动战斗；非法新配置在替换旧状态前拒绝。素材编号和静态符卡文本依赖当前关卡资源，卸载前先清 Boss、协程和其他借用者。行为中的 `SkillComplete` 只请求交接，`Actor::finished` 表示脚本已经完成整场退场。

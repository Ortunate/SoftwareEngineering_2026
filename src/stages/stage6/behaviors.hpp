// 本文件声明六面普通演员的命名行为；路线/攻击配方共享，实例初始动画独立选择红蓝外观。
#pragma once
#include "gameplay/actors/behavior.hpp"
namespace koumakyou::stage6 {
// 以下协程由 Actor::Start/Call 持有；等待采用 60 Hz 内容帧，空间量使用世界单位。
// actor 与 PhaseMemory 的借用期覆盖协程执行期；阶段回调由 Boss 包装层推进，不在协程内销毁演员。
/// @brief 隐藏环境演出保留周期位置随机读取和寿命，不参与碰撞或索敌。
behavior::Routine AmbientPositions(behavior::Actor& actor);
/// @brief 两类弯曲晶体保留各自转速、停火帧和随机初相，不由外观脚本反推路线。
behavior::Routine BlueHook(behavior::Actor& actor);
behavior::Routine RedHook(behavior::Actor& actor);
/// @brief 横穿蓝环在第 40 内容帧发射并选择随机退场方向；击破掉落由配方的独立回调负责。
behavior::Routine CrossingRing(behavior::Actor& actor);
/// @brief 下行蓝环保留与横穿款相同的发射/随机读取契约，出生方向由配方决定。
behavior::Routine DescendingRing(behavior::Actor& actor);
/// @brief 击破时请求散落八个普通道具并结束演员；调用方仍负责一次性的分数及源掉落。
behavior::Routine DropEight(behavior::Actor& actor);
inline constexpr behavior::Pattern kAmbientPositions{.movement={.speed=0},.initialAnimation=12,.routine=AmbientPositions};
inline constexpr behavior::Pattern kBlueHook{.movement={.angle=0.523598790F,.speed=12.0535717F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=BlueHook};
inline constexpr behavior::Pattern kRedHook{.movement={.angle=-1.04719758F,.speed=10.7142859F},.initialAnimation=8,.deathEffect=0,.spatialShots=true,.routine=RedHook};
inline constexpr behavior::Pattern kCrossingRing{.movement={.angle=0,.speed=5.35714293F},.initialAnimation=11,.deathEffect=0,.spatialShots=true,.routine=CrossingRing,.onDefeated=DropEight};
inline constexpr behavior::Pattern kDescendingRing{.movement={.speed=5.35714293F},.initialAnimation=11,.deathEffect=0,.spatialShots=true,.routine=DescendingRing,.onDefeated=DropEight};
/// @brief 安装六面弹幕资源并选择真实三维/中性平面模式；不重置实例随机流。
void ConfigureBehavior(behavior::Actor& actor,bool spatial);
namespace sakuya {
/// @brief 保护状态下入场并持续等待，由 Boss 收到道中对白结束通知后替换协程。
behavior::Routine Entrance(behavior::Actor& actor);
/// @brief 先瞬移到中心再交替发射带延迟转向的扇组；生命阈值/超时由包装层设置。
behavior::Routine Midboss(behavior::Actor& actor);
/// @brief Eternal Meek 的准备与随机弹组；保留机体生命分支，超时后进入独立退场。
behavior::Routine EternalMeek(behavior::Actor& actor);
/// @brief 停止持续发射、清弹并关闭伤害/接触，标记整个道中遭遇结束。
behavior::Routine Retreat(behavior::Actor& actor);
/// @brief 在 Retreat 前请求二十个散落道具、Bomb 和击破音效；超时不能调用此入口。
behavior::Routine Defeated(behavior::Actor& actor);
}
namespace remilia {
/// @brief 源回调保留的语义状态：蓄力剩余脉冲在第二非符的首段等待继续消费，不能伪造为九十帧。
struct PhaseMemory {int chargingPulsesLeft=0;};
/// @brief 保护状态下移动到战斗区域并等待第一次对白通知，不自行开始阶段超时。
behavior::Routine Entrance(behavior::Actor& actor);
/// @brief 保留第二段对白等待；只有包装层的第二次恢复通知才进入非符。
behavior::Routine Prelude(behavior::Actor& actor);
/// @brief 第一非符的双向螺旋扫描与随机巡航，持续到外部生命/超时回调。
behavior::Routine FirstNonspell(behavior::Actor& actor);
/// @brief 星阵光束和节点发射交替循环；memory 由 Boss 独占，须存活至此协程及后继非符结束。
behavior::Routine StarOfVanities(behavior::Actor& actor,PhaseMemory& memory);
/// @brief 先消费前符卡未完成的蓄力脉冲，再执行四段弹组；不得在调用前清空 memory。
behavior::Routine SecondNonspell(behavior::Actor& actor,PhaseMemory& memory);
/// @brief 循环交付四种带旋转/加速度的弹组，各轮等待保留子时钟冻结语义。
behavior::Routine ScarletNetherworld(behavior::Actor& actor);
/// @brief 第三非符的正反向逐发刀阵；跨轮随机方向只在源配置点读取。
behavior::Routine ThirdNonspell(behavior::Actor& actor);
/// @brief 递增弹组和大弹地雷播种/解放，阶段交接须保留已发弹自身运动副本。
behavior::Routine VladTepesCurse(behavior::Actor& actor);
/// @brief 每轮召唤六十只独立化蝠演员；出生请求由 Game 在池遍历结束后消费。
behavior::Routine BatNonspell(behavior::Actor& actor);
/// @brief 锁定发射锥轴的随机角度/速率弹组，保持原机体无关的循环帧距。
behavior::Routine ScarletShoot(behavior::Actor& actor);
/// @brief 最终符卡先等待宣告再准备和循环攻击；Bomb 化蝠保护由 Boss 每步同步。
behavior::Routine RedMagic(behavior::Actor& actor);
/// @brief 请求击破反馈并保留六十内容帧退场，结束后才由包装层报告 StageEvent 完成。
behavior::Routine Defeated(behavior::Actor& actor);
/// @brief 仅标记当前阶段协程完成；下一阶段及整场退场由公共 Boss 生命周期决定。
behavior::Routine PhaseComplete(behavior::Actor& actor);
/// @brief 六面原生扩展的世界实现；只能在 Actor.Update 内使用借用弹池。
/// 星阵产生两组三层八向光束或从相同节点交付已配置配方；地雷先收集大弹位置再生成，避免遍历重入。
void StarLattice(behavior::Actor& actor,bool emitProjectiles);
/// @brief 从活动大弹请求生成静止地雷并返回候选数；每个候选消费一次随机方向，池满不重试，返回值不代表成功数。
int SeedMines(behavior::Actor& actor);
/// @brief 为静止小弹安装按到 Boss 距离分配的定向加速度；只消费一次整体相位，不重置视觉年龄。
void ReleaseMines(behavior::Actor& actor);
}
}

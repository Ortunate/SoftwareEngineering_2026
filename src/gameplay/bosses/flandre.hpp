// 本文件声明芙兰朵露角色及已接入的命名行为；技能数值由角色从原生战斗目录绑定。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Extra单套芙兰朵露实例，独占演员与技能上下文；尚未完成的符卡不能作为技能启动。
class FlandreBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    enum class Behavior { Entrance,FirstNonspell,SecondNonspell,ThirdNonspell,FourthNonspell,FifthNonspell,SixthNonspell,SeventhNonspell,EighthNonspell,CranberryTrap,Laevatein,FourOfAKind,KagomeKagome,MazeOfLove,StarbowBreak,Catadioptric,ClockOfThePast,SurvivalPrelude,AndThenWillThereBeNone,FinalPrelude,RipplesOf495Years,FinalExit,EncoreEntrance,FragileWing,CreepingBloodstainPrelude,CreepingBloodstain,DanmakuHeartPrelude,DanmakuHeart,EncoreFinalExit };
    /// @brief 协程只借用Actor；空间量使用世界单位，等待为60 Hz内容帧。
    struct Behaviors {
        /// @brief 配置已接入段的姿态/边界/公共弹幕；spatial选择生产三维适配，不消费随机流。
        /// 本体须提供Flandre图集，kOrb须绑定ExtraEnemies，辅助光圈从后者借用。
        static void ConfigurePractice(behavior::Actor& actor,bool spatial);
        /// @brief 120帧受保护入场，随后等待外部ResumeCombat；不能按估计的对白时长自行开战。
        static behavior::Routine Entrance(behavior::Actor& actor);
        /// @brief 条件后继入场：隐藏本体并召唤11个短命演出对象，100内容帧显现后等待外部ResumeCombat。
        /// 子演员借用Flandre图集，独占种子和协程；外部118粒子仅保留等待，不伪装为攻击弹。
        static behavior::Routine EncoreEntrance(behavior::Actor& actor);
        /// @brief 八段非符在30帧开放受伤，70/90帧配置环弹并启动独立周期发射；第八段每轮重新瞄准。
        /// 第一段在30/40/50/60/70帧依次创建五条附着轨道；外部粒子不进入攻击随机流。
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        static behavior::Routine SecondNonspell(behavior::Actor& actor);
        static behavior::Routine ThirdNonspell(behavior::Actor& actor);
        static behavior::Routine FourthNonspell(behavior::Actor& actor);
        static behavior::Routine FifthNonspell(behavior::Actor& actor);
        static behavior::Routine SixthNonspell(behavior::Actor& actor);
        static behavior::Routine SeventhNonspell(behavior::Actor& actor);
        static behavior::Routine EighthNonspell(behavior::Actor& actor);
        /// @brief 120帧准备后每807帧召唤16个边界发射体；子演员独占路线、动画、随机流和逐发状态。
        static behavior::Routine CranberryTrap(behavior::Actor& actor);
        /// @brief 120帧准备后交替挥动长激光并逐帧留下短激光；每五内容帧按当前光束几何采样发弹。
        /// 槽位引用与光束寿命独立，子行为调用冻结父局部时钟，不冻结移动和战斗时限。
        static behavior::Routine Laevatein(behavior::Actor& actor);
        /// @brief 120帧准备、96帧分裂演出后召唤三个独立分身；各自周期发射、巡航并停火蓄力。
        /// 分身借用本体外观，由普通演员池持有；118外部粒子暂不渲染，但保留全部演出等待。
        static behavior::Routine FourOfAKind(behavior::Actor& actor);
        /// @brief 120帧准备后逐步执行大小弹解放；96帧粒子等待后每690帧显式编排16路边界发射体。
        /// 发射体借用Flandre动画167但本体隐藏，关闭受伤/接触/交互，按64×80源逻辑精灵的场外边界回收。
        static behavior::Routine KagomeKagome(behavior::Actor& actor);
        /// @brief 120帧准备后边向中央移动边发射正反向条件扇；每轮300/180次、间隔2内容帧。
        /// 相位和门角为实例局部弧度，保留float32运算边界及非最短角差判断，整轮1027内容帧。
        static behavior::Routine MazeOfLove(behavior::Actor& actor);
        /// @brief 120帧准备后按717内容帧一轮召唤27个彩色雨弹发射体，保留两段60帧外部粒子等待。
        /// 发射体从出生起隐藏、独占双随机读取与最多200帧寿命，清场无奖励；本体沿用入场前移动边界。
        static behavior::Routine StarbowBreak(behavior::Actor& actor);
        /// @brief 120帧准备后发射多速随机角反弹弹簇，首组两次反弹，后续各组一次；下边界离场只消费次数。
        /// 保留两个60帧粒子等待、沿边移动中的五次扫描，以及子行为冻结父时钟的回跳约定。
        static behavior::Routine Catadioptric(behavior::Actor& actor);
        /// @brief 120帧准备后按1097帧一轮编排三段定向/瞄准扇和三对旋转激光使魔。
        /// 使魔独占四光束句柄、移动/旋转状态，390内容帧删除；出生与移动目标均为明确世界位置。
        static behavior::Routine ClockOfThePast(behavior::Actor& actor);
        /// @brief 零有效HP的生存符前过渡：保留1000原生命寄存器，30帧开放受伤/掉落，90帧报告接续。
        /// 与一般非符不同，不巡航/发弹；读取源中未采用的随机角，不把尚未完成的后继视为可玩。
        static behavior::Routine SurvivalPrelude(behavior::Actor& actor);
        /// @brief 120帧准备后隐藏本体，召唤六名按出生HP决定追踪次数的使魔，随后加速切换五组边界弹幕。
        /// 生存超时是成功条件；结束清弹由后继准备行为负责，使魔隐藏且清场直接移除。
        static behavior::Routine AndThenWillThereBeNone(behavior::Actor& actor);
        /// @brief 生存符后的90帧过渡；30帧同时创建五条附着轨道，始终保护，直接接续最终符准备。
        static behavior::Routine FinalPrelude(behavior::Actor& actor);
        /// @brief 120帧准备及60帧蓄力后发射88向单次反弹环；每轮按当前整数HP与阶段总钟计算速度和等待。
        /// 随机角先于下轮发射点的X/Z读取；7200帧起只在计算参数时视作零HP，不修改实际生命。
        static behavior::Routine RipplesOf495Years(behavior::Actor& actor);
        /// @brief 120帧准备、80帧蓄力后交替召唤扫描使魔；按值传递双角步长和发射次数，子体停止发弹后仍存留至场外回收。
        static behavior::Routine FragileWing(behavior::Actor& actor);
        /// @brief 132后的210帧受保护过渡；30帧掉落40组道具及一枚Bomb，回调仅报告接续，不重建Boss。
        static behavior::Routine CreepingBloodstainPrelude(behavior::Actor& actor);
        /// @brief 额外第二符第0帧开放受伤，200帧首发十向使魔，随后交替五只瞄准下落使魔；保留全部随机读取。
        static behavior::Routine CreepingBloodstain(behavior::Actor& actor);
        /// @brief 133后的210帧保护过渡，110帧缓出至战斗位置，30帧请求40散落，实际到210帧才交回配置层。
        static behavior::Routine DanmakuHeartPrelude(behavior::Actor& actor);
        /// @brief 6000生命驱动双旋臂快照；第0帧开放受伤，60帧切176，120帧安装独立Bomb恢复姿态。
        static behavior::Routine DanmakuHeart(behavior::Actor& actor);
        /// @brief 条件后继最终退场；damage表示此前伤害击破已消费模式3，60帧结束；超时保留模式3，再等一个固定步。
        static behavior::Routine EncoreFinalExit(behavior::Actor& actor,bool damage);
        /// @brief 最终击破60帧退场；超时爆破后再等一个固定步。damage标记此前模式3已消费，实际结束才释放遭遇。
        static behavior::Routine FinalExit(behavior::Actor& actor,bool damage);
        /// @brief 按当前弹池槽序解放Kagome Kagome的小弹，返回逻辑宽度至少30源像素的大弹数。
        /// 仅在Actor::Update内调用；严格邻域半径为2.85714286世界单位，零加速度小弹只接受首个大弹。
        /// 保留视觉年龄和当前速度，重启120内容帧的加速度；不处理Bomb或外部粒子，不消费随机流。
        /// 平面模式保留原角差分支；三维适配使用球形邻域并保留小弹相对本体的径向高度角。
        static int ReleaseCageBullets(behavior::Actor& actor);
    };
    /// @brief 重启时清空本角色的Bomb恢复记忆；输入配置验证失败保留旧战斗。
    void StartBoss(const StartConfig& config) override;
    void SetPlayerBombActive(bool activeBomb) override {bombActive_=activeBomb;}
    const char* DisplayName() const override {return "Flandre Scarlet";}
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    SkillDefinition ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
    void AfterUpdate(float seconds) override;
private:
    bool bombActive_=false,batTransformed_=false;
    double batRecoveryFrames_=0; // 原生10在Bomb每步补满60内容帧，解除后递减；不冻结战斗时钟。

};
}

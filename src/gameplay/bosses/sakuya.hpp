// 本文件定义 SakuyaBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Sakuya Izayoi 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class SakuyaBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    enum class Behavior { Entrance, MisdirectionNonspell, Misdirection, MisdirectionRetreat, MisdirectionDefeated, FirstNonspell, ClockCorpse, SecondNonspell, LunaClock, ThirdNonspell, Marionette, Defeated, SkillComplete, EternalMeekEntrance, EternalMeekNonspell, EternalMeek, EternalMeekRetreat, EternalMeekDefeated, JackTheLudoBile, TheWorld, KillingDoll };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        /// @brief 单技能练习恢复素材、移动姿态和关底世界边界；没有入场协程时必须先建立这些继承状态。
        static void ConfigureFinalPractice(behavior::Actor& actor,bool spatial);
        /// @brief 道中Misdirection练习另恢复其更宽的Z边界，不能复用关底的纵向范围。
        static void ConfigureMisdirectionPractice(behavior::Actor& actor,bool spatial);
        /// @brief 入场只移动和等待；对白完成由 Boss 接收端切入 MisdirectionNonspell 或 FirstNonspell。
        static behavior::Routine Entrance(behavior::Actor& actor);
        static behavior::Routine MisdirectionNonspell(behavior::Actor& actor);
        static behavior::Routine Misdirection(behavior::Actor& actor);
        static behavior::Routine MisdirectionRetreat(behavior::Actor& actor);
        static behavior::Routine MisdirectionDefeated(behavior::Actor& actor);
        /// @brief 首次非符在0…40帧创建五条同色附着，回拨仅重启攻击，不重复轨道。
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        /// @brief 三组时停符保留攻击局部时钟中的蓄力请求；默认120Hz不改变其内容帧间隔。
        static behavior::Routine ClockCorpse(behavior::Actor& actor);
        static behavior::Routine JackTheLudoBile(behavior::Actor& actor);
        static behavior::Routine SecondNonspell(behavior::Actor& actor);
        static behavior::Routine LunaClock(behavior::Actor& actor);
        static behavior::Routine TheWorld(behavior::Actor& actor);
        static behavior::Routine ThirdNonspell(behavior::Actor& actor);
        static behavior::Routine Marionette(behavior::Actor& actor);
        static behavior::Routine KillingDoll(behavior::Actor& actor);
        /// @brief 六轮五色爆散在60内容帧末清弹并完成遭遇，调用方持续消费演出请求至资源释放。
        static behavior::Routine Defeated(behavior::Actor& actor);
        /// @brief 阶段回调只标记交接，实际生命、血条和完整退场由 SakuyaBoss 统一推进。
        static behavior::Routine SkillComplete(behavior::Actor& actor);
        /// @brief 原生时停弧线与改向的世界坐标实现；只在 Actor 的 Update 内调用，使用其借用弹池。
        /// 弧线每次九枚、pulse 为从零起算的九帧周期编号；改向最多成功修改十四枚，保留槽位和随机读取顺序。
        static void EmitClockArc(behavior::Actor& actor,int pulse);
        static void RedirectStoppedBullets(behavior::Actor& actor);

        /// @brief 六面单符入口恢复非符留下的边界、姿态及素材；不运行非符或消耗攻击随机数。
        static void ConfigureEternalMeekPractice(behavior::Actor& actor,bool spatial);
        /// @brief 保护状态下入场并持续等待，由 Boss 收到道中对白结束通知后替换协程。
        static behavior::Routine EternalMeekEntrance(behavior::Actor& actor);
        /// @brief 先瞬移到中心再按 actor.difficulty 交替发射带延迟转向的扇组；生命阈值/超时由包装层设置。
        static behavior::Routine EternalMeekNonspell(behavior::Actor& actor);
        /// @brief Eternal Meek 的准备与分难度随机弹组；保留机体生命分支，超时后进入独立退场。
        static behavior::Routine EternalMeek(behavior::Actor& actor);
        /// @brief 停止持续发射、清弹并关闭伤害/接触，标记整个道中遭遇结束。
        static behavior::Routine EternalMeekRetreat(behavior::Actor& actor);
        /// @brief 在 EternalMeekRetreat 前请求二十个散落道具、Bomb 和击破音效；超时不能调用此入口。
        static behavior::Routine EternalMeekDefeated(behavior::Actor& actor);
    };
    /// @brief 复制并解析阶段组合；非法角色引用/血条比例/跳转由公共启动接口拒绝。
    void StartBoss(const StartConfig& config) override;
    const char* DisplayName() const override {return "Sakuya Izayoi";}
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    /// @brief 按难度绑定生命/时限和符卡编号；拒绝不存在的替换卡及六面 Easy 的 Eternal Meek 符卡。
    SkillDefinition ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

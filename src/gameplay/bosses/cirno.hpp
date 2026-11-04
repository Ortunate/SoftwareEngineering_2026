// 本文件定义 CirnoBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Cirno 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class CirnoBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    enum class Behavior { FirstNonspell, IcicleFall, SecondNonspell, PerfectFreeze, DiamondBlizzard, Defeated, End };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        /// @brief 按当前难度执行技能行为；首段非符20/30/40/50内容帧安装四条世界轨道，70帧后攻击。
        /// 继承生命与技能上下文；各阶段通过命名回调衔接，准备期不能提前受击。
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        static behavior::Routine IcicleFall(behavior::Actor& actor);
        static behavior::Routine SecondNonspell(behavior::Actor& actor);
        static behavior::Routine PerfectFreeze(behavior::Actor& actor);
        /// @brief 开场单粒蓝色闪光后等待60帧进入准备；正式Easy技能目录不开放本符。
        static behavior::Routine DiamondBlizzard(behavior::Actor& actor);
        /// @brief 六轮五色爆散共60内容帧；最后交付音效/清弹并等待公共结束回调。
        static behavior::Routine Defeated(behavior::Actor& actor);
        static behavior::Routine End(behavior::Actor& actor);
    };
    /// @brief 复制并解析阶段组合；非法角色引用/血条比例/跳转由公共启动接口拒绝。
    void StartBoss(const StartConfig& config) override;
    const char* DisplayName() const override {return "Cirno";}
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    SkillDefinition ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

// 本文件定义 RumiaBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Rumia 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class RumiaBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    enum class Behavior { Midboss, MoonlightRay, MidbossDefeated, MidbossTimeout, FirstNonspell, SecondNonspell, NightBird, Demarcation, Defeated, End };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        using Actor=behavior::Actor;
        using Routine=behavior::Routine;
        /// @brief 以下入口编排移动、攻击和退场；道中60/80/100帧安装三条世界轨道，生命/时限由角色技能目录提供。
        static behavior::Routine Midboss(Actor& actor);
        static behavior::Routine MoonlightRay(Actor& actor);
        static behavior::Routine MidbossDefeated(Actor& actor);
        static behavior::Routine MidbossTimeout(Actor& actor);
        /// @brief 首段0/20/40帧附着，100帧后攻击；后续正生命阈值交接保留同一演员的轨道中心。
        static behavior::Routine FirstNonspell(Actor& actor);
        static behavior::Routine SecondNonspell(Actor& actor);
        static behavior::Routine NightBird(Actor& actor);
        static behavior::Routine Demarcation(Actor& actor);
        /// @brief 六轮五色爆散共60内容帧，最后一组交付后清弹、安装结束回调，不以表现层替代请求。
        static behavior::Routine Defeated(Actor& actor);
        static behavior::Routine End(Actor& actor);
    };
    /// @brief 复制并解析阶段组合；非法角色引用/血条比例/跳转由公共启动接口拒绝。
    void StartBoss(const StartConfig& config) override;
    const char* DisplayName() const override {return "Rumia";}
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    SkillDefinition ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

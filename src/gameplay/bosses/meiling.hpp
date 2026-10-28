// 本文件定义 MeilingBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Hong Meiling 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class MeilingBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    enum class Behavior { Midboss, GorgeousSweetFlower, MidbossDefeated, MidbossTimeout, Entrance, FirstNonspell, RainbowWindChime, SummoningNonspell, RainNonspell, ColorfulRain, DazzlingColorTyphoon, Defeated, End };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        /// @brief 入场/阶段入口由 Boss 包装层调用；阶段 HP、生命阈值及超时从 native_combat 读取。
        static behavior::Routine Midboss(behavior::Actor& actor);
        static behavior::Routine GorgeousSweetFlower(behavior::Actor& actor);
        static behavior::Routine MidbossDefeated(behavior::Actor& actor);
        static behavior::Routine MidbossTimeout(behavior::Actor& actor);
        static behavior::Routine Entrance(behavior::Actor& actor);
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        static behavior::Routine RainbowWindChime(behavior::Actor& actor);
        static behavior::Routine SummoningNonspell(behavior::Actor& actor);
        static behavior::Routine RainNonspell(behavior::Actor& actor);
        static behavior::Routine ColorfulRain(behavior::Actor& actor);
        static behavior::Routine DazzlingColorTyphoon(behavior::Actor& actor);
        static behavior::Routine Defeated(behavior::Actor& actor);
        static behavior::Routine End(behavior::Actor& actor);
    };
    /// @brief 复制并解析阶段组合；非法角色引用/血条比例/跳转由公共启动接口拒绝。
    void StartBoss(const StartConfig& config) override;
    const char* DisplayName() const override {return "Hong Meiling";}
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

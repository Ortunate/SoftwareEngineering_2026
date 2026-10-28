// 本文件定义 PatchouliBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Patchouli Knowledge 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class PatchouliBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    enum class Behavior { Entrance, FirstNonspell, SecondNonspell, AgniShine, AgniShineAdvanced, PrincessUndine, SylphyHorn, SylphyHornAdvanced, LazyTrilithon, LazyTrilithonAdvanced, MetalFatigue, LavaCromlech, ForestBlaze, WaterElf, MercuryPoison, EmeraldMegalith, CombinationEntrance, Combination, SkillComplete, Defeated, WaterElfFinal, EmeraldMegalithFinal };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        /// @brief 帕秋莉入场等待独立开战通知；两个非符阶段保存自己的激光槽与跨子行为的自动发射。
        static behavior::Routine Entrance(behavior::Actor& actor);
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        static behavior::Routine SecondNonspell(behavior::Actor& actor);
        /// @brief Normal 按机体选用的元素符卡及其强化版；均含 120 内容帧准备期，返回条件由 Boss 生命周期设置。
        static behavior::Routine AgniShine(behavior::Actor& actor);
        static behavior::Routine AgniShineAdvanced(behavior::Actor& actor);
        static behavior::Routine PrincessUndine(behavior::Actor& actor);
        static behavior::Routine SylphyHorn(behavior::Actor& actor);
        static behavior::Routine SylphyHornAdvanced(behavior::Actor& actor);
        static behavior::Routine LazyTrilithon(behavior::Actor& actor);
        static behavior::Routine LazyTrilithonAdvanced(behavior::Actor& actor);
        static behavior::Routine MetalFatigue(behavior::Actor& actor);
        /// @brief 组合符卡的命名攻击，含自身准备期；组合阶段额外的演出等待由 CombinationEntrance 负责。
        static behavior::Routine LavaCromlech(behavior::Actor& actor);
        static behavior::Routine ForestBlaze(behavior::Actor& actor);
        static behavior::Routine WaterElf(behavior::Actor& actor);
        static behavior::Routine MercuryPoison(behavior::Actor& actor);
        static behavior::Routine EmeraldMegalith(behavior::Actor& actor);
        static behavior::Routine CombinationEntrance(behavior::Actor& actor);
        /// @brief 组合段在包装层等待后重设自身超时并执行命名攻击；entry 必须为本关的组合符卡入口，timeout 为内容帧。
        static behavior::Routine Combination(behavior::Actor& actor,behavior::Entry entry,int timeout);
        /// @brief 供统一阶段回调使用的交接标记；必须由 Boss 在 Update 外推进，不能在协程中销毁当前调用链。
        static behavior::Routine SkillComplete(behavior::Actor& actor);
        static behavior::Routine Defeated(behavior::Actor& actor);
    };
    /// @brief 复制并解析阶段组合；非法角色引用/血条比例/跳转由公共启动接口拒绝。
    void StartBoss(const StartConfig& config) override;
    const char* DisplayName() const override {return "Patchouli Knowledge";}
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

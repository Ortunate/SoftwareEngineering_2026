// 本文件定义 SakuyaBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Sakuya Izayoi 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class SakuyaBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    enum class Behavior { Entrance, MisdirectionNonspell, Misdirection, MisdirectionRetreat, MisdirectionDefeated, FirstNonspell, ClockCorpse, SecondNonspell, LunaClock, ThirdNonspell, Marionette, Defeated, SkillComplete, EternalMeekEntrance, EternalMeekNonspell, EternalMeek, EternalMeekRetreat, EternalMeekDefeated };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        /// @brief 入场只移动和等待；对白完成由 Boss 接收端切入 MisdirectionNonspell 或 FirstNonspell。
        static behavior::Routine Entrance(behavior::Actor& actor);
        static behavior::Routine MisdirectionNonspell(behavior::Actor& actor);
        static behavior::Routine Misdirection(behavior::Actor& actor);
        static behavior::Routine MisdirectionRetreat(behavior::Actor& actor);
        static behavior::Routine MisdirectionDefeated(behavior::Actor& actor);
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        static behavior::Routine ClockCorpse(behavior::Actor& actor);
        static behavior::Routine SecondNonspell(behavior::Actor& actor);
        static behavior::Routine LunaClock(behavior::Actor& actor);
        static behavior::Routine ThirdNonspell(behavior::Actor& actor);
        static behavior::Routine Marionette(behavior::Actor& actor);
        static behavior::Routine Defeated(behavior::Actor& actor);
        /// @brief 阶段回调只标记交接，实际生命、血条和完整退场由 SakuyaBoss 统一推进。
        static behavior::Routine SkillComplete(behavior::Actor& actor);
        /// @brief 原生时停弧线与改向的世界坐标实现；只在 Actor 的 Update 内调用，使用其借用弹池。
        /// 弧线每次九枚、pulse 为从零起算的九帧周期编号；改向最多成功修改十四枚，保留槽位和随机读取顺序。
        static void EmitClockArc(behavior::Actor& actor,int pulse);
        static void RedirectStoppedBullets(behavior::Actor& actor);

        /// @brief 保护状态下入场并持续等待，由 Boss 收到道中对白结束通知后替换协程。
        static behavior::Routine EternalMeekEntrance(behavior::Actor& actor);
        /// @brief 先瞬移到中心再交替发射带延迟转向的扇组；生命阈值/超时由包装层设置。
        static behavior::Routine EternalMeekNonspell(behavior::Actor& actor);
        /// @brief Eternal Meek 的准备与随机弹组；保留机体生命分支，超时后进入独立退场。
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
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

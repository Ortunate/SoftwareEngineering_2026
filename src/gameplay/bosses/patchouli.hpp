// 本文件定义 PatchouliBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
namespace koumakyou::bosses {
/// @brief Patchouli Knowledge 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class PatchouliBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    /// 组合技能的 Middle/Final 区分原生命寄存器保留偏移，复用同一攻击协程，不由启动配置覆盖 HP。
    enum class Behavior { Entrance, FirstNonspell, SecondNonspell, AgniShine, AgniShineAdvanced, PrincessUndine, SylphyHorn, SylphyHornAdvanced, LazyTrilithon, LazyTrilithonAdvanced, MetalFatigue, LavaCromlech, ForestBlaze, WaterElf, MercuryPoison, EmeraldMegalith, CombinationEntrance, Combination, SkillComplete, Defeated, WaterElfFinal, EmeraldMegalithFinal, AgniRadiance, GreenStorm, TrilithonShake, SilverDragon, BuryInLake, LavaCromlechFinal, ForestBlazeFinal, MercuryPoisonMiddle, EmeraldMegalithMiddle, WaterElfMiddle, SilentSelene, RoyalFlare, ExtraRetreat, PhilosophersStone, ExtraEntrance, ExtraDefeated };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        /// @brief 独立符卡练习的初始配置：建立前段非符留下的姿态、素材和世界边界，不推进时钟或随机流。
        /// spatial 为 true 时沿生产三维移动/发射契约配置；Actor 由启动配置独占。
        static void ConfigurePractice(behavior::Actor& actor,bool spatial);
        /// @brief Extra单套道中技能的素材、姿态与边界；不推进时钟或随机流，不开放主线难度变体。
        /// 调用方提供ExtraEnemies的65…71本体脚本及公共弹幕清单，Actor仅借用当前关卡资源。
        static void ConfigureExtraPractice(behavior::Actor& actor,bool spatial);
        /// @brief Extra月符的随机落点加速弹与反向扇；包含120内容帧准备，巡航每32轮一次。
        static behavior::Routine SilentSelene(behavior::Actor& actor);
        /// @brief Extra日符按已核验的原生13沿扩张圆周发射，独立六次调用计数跨四段和循环保留。
        static behavior::Routine RoyalFlare(behavior::Actor& actor);
        /// @brief Extra道中清弹、停止发射并立即结束本体；击破奖励由遭遇配置/前置行为独立提供。
        static behavior::Routine ExtraRetreat(behavior::Actor& actor);
        /// @brief Extra三符遭遇的受保护入场与最后奖励；开战仍由外部对白信号ResumeCombat放行。
        static behavior::Routine ExtraEntrance(behavior::Actor& actor);
        static behavior::Routine ExtraDefeated(behavior::Actor& actor);
        /// @brief 第三符在120帧准备后生成五种固定使魔，父本体等待生命/超时交接，不代替子演员发弹。
        static behavior::Routine PhilosophersStone(behavior::Actor& actor);
        /// @brief 五种元素使魔只借用Actor；帧为60 Hz内容帧、移动/弹速为世界单位，实例独占随机流。
        static behavior::Routine FireStone(behavior::Actor& actor);
        static behavior::Routine WaterStone(behavior::Actor& actor);
        static behavior::Routine WoodStone(behavior::Actor& actor);
        static behavior::Routine MetalStone(behavior::Actor& actor);
        static behavior::Routine EarthStone(behavior::Actor& actor);
        /// @brief 使魔击破后交付8个源掉落、中断本体和辅助光圈并等待30帧；不清除已生弹。
        static behavior::Routine StoneDefeated(behavior::Actor& actor);
        /// @brief 帕秋莉入场等待独立开战通知；两个非符阶段保存自己的激光槽与跨子行为的自动发射。
        static behavior::Routine Entrance(behavior::Actor& actor);
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        static behavior::Routine SecondNonspell(behavior::Actor& actor);
        /// @brief 按机体和难度选用的元素符卡及强化版；均含 120 内容帧准备期，返回条件由 Boss 生命周期设置。
        static behavior::Routine AgniShine(behavior::Actor& actor);
        static behavior::Routine AgniShineAdvanced(behavior::Actor& actor);
        /// @brief 高难火符的双旋转环与随机追加弹，保留每发随机读取及 128 内容帧角速度窗口。
        static behavior::Routine AgniRadiance(behavior::Actor& actor);
        static behavior::Routine PrincessUndine(behavior::Actor& actor);
        /// @brief 高难水符使用预警后变实的成对固定长光束，停止自动射击后切换扇形攻击。
        static behavior::Routine BuryInLake(behavior::Actor& actor);
        static behavior::Routine SylphyHorn(behavior::Actor& actor);
        static behavior::Routine SylphyHornAdvanced(behavior::Actor& actor);
        /// @brief 高难木符交替从左右边界释放受定向加速度影响的叶弹；不消耗额外三维随机数。
        static behavior::Routine GreenStorm(behavior::Actor& actor);
        static behavior::Routine LazyTrilithon(behavior::Actor& actor);
        static behavior::Routine LazyTrilithonAdvanced(behavior::Actor& actor);
        /// @brief 高难土符保留三轮追加环与独立的一次延迟转向快照。
        static behavior::Routine TrilithonShake(behavior::Actor& actor);
        static behavior::Routine MetalFatigue(behavior::Actor& actor);
        /// @brief 高难金符在发射时快照瞄准角和随机转向速率，60 内容帧后按该绝对方向转向。
        static behavior::Routine SilverDragon(behavior::Actor& actor);
        /// @brief 组合符卡的命名攻击，含自身准备期；组合阶段额外的演出等待由 CombinationEntrance 负责。
        static behavior::Routine LavaCromlech(behavior::Actor& actor);
        static behavior::Routine ForestBlaze(behavior::Actor& actor);
        static behavior::Routine WaterElf(behavior::Actor& actor);
        static behavior::Routine MercuryPoison(behavior::Actor& actor);
        static behavior::Routine EmeraldMegalith(behavior::Actor& actor);
        static behavior::Routine CombinationEntrance(behavior::Actor& actor);
        /// @brief 组合段在包装层等待后重设自身超时并执行命名攻击；entry 必须为本关的组合符卡入口，timeout 为内容帧。
        /// transitionFlash仅用于中段/末段的开场闪光与音效；首段不得发出该请求。
        static behavior::Routine Combination(behavior::Actor& actor,behavior::Entry entry,int timeout,bool transitionFlash);
        /// @brief 供统一阶段回调使用的交接标记；必须由 Boss 在 Update 外推进，不能在协程中销毁当前调用链。
        static behavior::Routine SkillComplete(behavior::Actor& actor);
        /// @brief 六轮五色爆散共60内容帧；最后一轮末清弹并报告结束，不提前释放当前关卡素材。
        static behavior::Routine Defeated(behavior::Actor& actor);
    };
    /// @brief 复制并解析阶段组合；非法角色引用/血条比例/跳转由公共启动接口拒绝。
    void StartBoss(const StartConfig& config) override;
    const char* DisplayName() const override {return "Patchouli Knowledge";}
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    /// @brief 非符、元素和组合按难度解析；组合位置变体保留原生生命偏移，非法难度/技能组合抛 invalid_argument。
    SkillDefinition ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

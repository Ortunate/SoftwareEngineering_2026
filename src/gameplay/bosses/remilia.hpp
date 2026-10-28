// 本文件定义 RemiliaBoss 的全部行为入口和技能目录；血量属于技能，阶段组合由启动配置提供。
#pragma once
#include "gameplay/bosses/configured_boss.hpp"
#include "gameplay/bosses/remilia_state.hpp"
namespace koumakyou::bosses {
/// @brief Remilia Scarlet 的角色实例，独占技能运行状态；同角色不同战斗复用本类。
class RemiliaBoss final : public ConfiguredBoss {
public:
    using ConfiguredBoss::ConfiguredBoss;
    /// @brief 本角色的技能和演出标识；其他角色的同名/同编号行为不能混用。
    enum class Behavior { Entrance, Prelude, FirstNonspell, StarOfVanities, SecondNonspell, ScarletNetherworld, ThirdNonspell, VladTepesCurse, BatNonspell, ScarletShoot, RedMagic, Defeated, SkillComplete };
    /// @brief 内部行为实现入口，公开供独立源语义测试；生产组合只引用 Behavior，不直接传协程。
    /// Actor 和可选记忆的借用期覆盖协程执行，等待为 60 Hz 内容帧，所有空间量为世界单位。
    struct Behaviors {
        /// @brief 源回调保留的语义状态：蓄力剩余脉冲在第二非符的首段等待继续消费，不能伪造为九十帧。
        using PhaseMemory=RemiliaPhaseMemory;
        /// @brief 保护状态下移动到战斗区域并等待第一次对白通知，不自行开始阶段超时。
        static behavior::Routine Entrance(behavior::Actor& actor);
        /// @brief 保留第二段对白等待；只有包装层的第二次恢复通知才进入非符。
        static behavior::Routine Prelude(behavior::Actor& actor);
        /// @brief 第一非符的双向螺旋扫描与随机巡航，持续到外部生命/超时回调。
        static behavior::Routine FirstNonspell(behavior::Actor& actor);
        /// @brief 星阵光束和节点发射交替循环；memory 由 Boss 独占，须存活至此协程及后继非符结束。
        static behavior::Routine StarOfVanities(behavior::Actor& actor,PhaseMemory& memory);
        /// @brief 先消费前符卡未完成的蓄力脉冲，再执行四段弹组；不得在调用前清空 memory。
        static behavior::Routine SecondNonspell(behavior::Actor& actor,PhaseMemory& memory);
        /// @brief 循环交付四种带旋转/加速度的弹组，各轮等待保留子时钟冻结语义。
        static behavior::Routine ScarletNetherworld(behavior::Actor& actor);
        /// @brief 第三非符的正反向逐发刀阵；跨轮随机方向只在源配置点读取。
        static behavior::Routine ThirdNonspell(behavior::Actor& actor);
        /// @brief 递增弹组和大弹地雷播种/解放，阶段交接须保留已发弹自身运动副本。
        static behavior::Routine VladTepesCurse(behavior::Actor& actor);
        /// @brief 每轮召唤六十只独立化蝠演员；出生请求由 Game 在池遍历结束后消费。
        static behavior::Routine BatNonspell(behavior::Actor& actor);
        /// @brief 锁定发射锥轴的随机角度/速率弹组，保持原机体无关的循环帧距。
        static behavior::Routine ScarletShoot(behavior::Actor& actor);
        /// @brief 最终符卡先等待宣告再准备和循环攻击；Bomb 化蝠保护由 Boss 每步同步。
        static behavior::Routine RedMagic(behavior::Actor& actor);
        /// @brief 请求击破反馈并保留六十内容帧退场，结束后才由包装层报告 StageEvent 完成。
        static behavior::Routine Defeated(behavior::Actor& actor);
        /// @brief 仅标记当前阶段协程完成；下一阶段及整场退场由公共 Boss 生命周期决定。
        static behavior::Routine SkillComplete(behavior::Actor& actor);
        /// @brief 六面原生扩展的世界实现；只能在 Actor.Update 内使用借用弹池。
        /// 星阵产生两组三层八向光束或从相同节点交付已配置配方；地雷先收集大弹位置再生成，避免遍历重入。
        static void StarLattice(behavior::Actor& actor,bool emitProjectiles);
        /// @brief 从活动大弹请求生成静止地雷并返回候选数；每个候选消费一次随机方向，池满不重试，返回值不代表成功数。
        static int SeedMines(behavior::Actor& actor);
        /// @brief 为静止小弹安装按到 Boss 距离分配的定向加速度；只消费一次整体相位，不重置视觉年龄。
        static void ReleaseMines(behavior::Actor& actor);
    };
    /// @brief 复制并解析阶段组合；非法角色引用/血条比例/跳转由公共启动接口拒绝。
    void StartBoss(const StartConfig& config) override;
    const char* DisplayName() const override {return "Remilia Scarlet";}
    ~RemiliaBoss() override;
    void SetPlayerBombActive(bool activeBomb) override {bombActive_=activeBomb;}
    /// @brief 命名攻击借用独占充能记忆；借用期覆盖协程执行，仅整场重开清零。
    RemiliaPhaseMemory& PhaseState() {return memory_;}
protected:
    void AfterUpdate(float seconds) override;
private:
    RemiliaPhaseMemory memory_;
    bool bombActive_=false,batTransformed_=false;
    double batRecoveryFrames_=0; // Bomb 停止后等待的 60 Hz 内容帧，活动 Bomb 每步续为 60。
protected:
    SkillDefinition ResolveSkill(BehaviorId id,ShotType shot) const override;
    Attack ResolveBehavior(BehaviorId id) const override;
};
}

// 本文件保留重构前的一面 Normal ECL 解释器，仅作为测试差分基准，不链接到生产引擎。
#pragma once
#include "reference_program.hpp"
#include "reference_coordinates.hpp"
#include "gameplay/bullets/system.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace koumakyou::stage1 {
/// @brief 单个演员的 Normal ECL 上下文；固定 60 Hz，调用栈最多 32 层，寄存器属于各层而非演员共享。
/// 移动在原作像素坐标求解，发弹保留 X/Z 投影，并沿 Y 增加朝玩家的高度速度；不增加弹数或改变投影角间距。
class ReferenceScript {
public:
    std::array<bool,38> visited{}; // 仅测试覆盖率观测，不影响解释器执行、时钟或随机序列。
    /// @brief 初始化 Normal 子程序、像素出生点、原作 HP 与独立随机种子；mirrored 仅反转移动的 X 分量。
    /// 越界编号抛 invalid_argument；表中未用于 Normal 的特殊指令在执行时抛 runtime_error。
    void Reset(int sub, Vector3 source, float sourceHealth, std::uint32_t seed, bool mirrored = false);
    /// @brief 推进非负秒数；跨帧补算 60 Hz 指令，回调到期后停止，等待 Transition。
    void Update(float seconds, Vector3 player, BulletSystem& bullets);
    /// @brief 切换到生命/超时回调，保留演员位置与发弹属性，清空旧调用栈并重新计时。
    void Transition();
    /// @brief 设置下一阶段超时默认使用的死亡回调子程序；练习跳过准备脚本时由 Boss 适配器补齐。
    void SetDeathCallback(int sub) { deathCallback_ = sub; }
    /// @brief Normal 自适应 rank 为 10…32；影响脚本指定的弹速和弹数，初始为 16。
    void SetRank(int rank) { rank_ = std::clamp(rank,10,32); }
    Vector3 sourcePosition{};
    float health = 1, maxHealth = 1;
    bool damageable = true, finished = false;
    int pendingCallback = -1;
    int phaseFrames = 0, timeoutFrames = -1;
    /// @brief 本次 Update 产生的原作音效编号；下次 Update 清空，调用方仅借用并即时播放。
    std::vector<int> soundEvents;
    /// @brief 本次更新 ins_119 的散落道具数量；下一次更新清空，Game 负责按当前 Power 实际生成。
    std::vector<int> dropEvents;
    int deathEffect = 0; // ECL 100 打包参数的低字节，0/1/2 对应小爆散/旋转爆散/大爆散。
    int animationScript = -1; // 原 ECL 97/98 选择的主动画，编号属于当前敌人/Boss 图集。
    double animationAge = 0; // 本次脚本启动后的秒数；ECL 子程序跳转不重置它。
    std::uint32_t animationRevision = 0;
    [[nodiscard]] Vector3 Position() const { return ReferencePosition(sourcePosition); }
private:
    struct Context { int sub = 0, pc = 0, frame = 0, compare = 0; std::array<float,12> variables{}; };
    Context context_{};
    std::vector<Context> stack_;
    std::uint32_t random_ = 1;
    double remainingFrames_ = 0;
    bool mirrored_ = false, clamped_ = false, interpolating_ = false;
    float angle_ = 0, speed_ = 0, acceleration_ = 0, angularVelocity_ = 0;
    Vector3 velocity_{}, offset_{}, interpolationStart_{}, interpolationDelta_{};
    int interpolationFrames_ = 0, interpolationLeft_ = 0, interpolationPower_ = 1;
    Vector2 lower_{32,48}, upper_{352,144};
    float lifeThreshold_ = -1;
    int lifeCallback_ = -1, deathCallback_ = -1, timerCallback_ = -1;
    std::array<float,8> effects_{};
    int rank_ = 16;
    std::array<int,5> poses_{-1,-1,-1,-1,-1}; // 待机、左收势、右收势、左移、右移。
    int pose_ = -1; // -1 尚未首次选择；0 静止、-2 左移、2 右移。
    std::array<float,6> rankInfluence_{-0.5F,0.5F,0,0,0,0};
    void Tick(Vector3 player, BulletSystem& bullets);
    void Execute(const reference::Instruction& instruction, Vector3 player, BulletSystem& bullets);
    void Call(int sub, float integer, float floating);
    void Interpolate(int duration, Vector3 target, int power);
    void Emit(const reference::Instruction& instruction, Vector3 player, BulletSystem& bullets);
    [[nodiscard]] float Value(float value) const;
    float& Variable(float id);
    float Random(float range);
    void SetAnimation(int script);
};
} // namespace koumakyou::stage1

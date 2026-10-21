// 本文件负责新典一面 Normal 的帧脚本上下文、坐标映射与原作移动/发弹状态。
#pragma once
#include "stages/stage1/reference_program.hpp"
#include "gameplay/bullets/system.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace koumakyou::stage1 {
// 384×448 像素场地映射到 X/Z 平面；原作深度映射到高度 Y，统一比例保持投影角度与轨迹。
inline constexpr float kReferenceScale = 20.0F / 448.0F;
// 当前 3D 玩家弹的伤害单位换算；所有原作 HP 和阶段阈值同时除以此值，不改变阶段血量比例。
inline constexpr float kReferenceHealthUnit = 25.0F;
/// @brief 将原作像素坐标转换为世界坐标；X 居中，原作 Y=0…448 对应世界 Z=+10…-10，向玩家推进。
/// 原作 Z=0 对应世界 Y=0，与玩家初始高度一致；原作 Z 正方向对应世界 -Y。
inline Vector3 ReferencePosition(Vector3 source) {
    return {(source.x - 192.0F) * kReferenceScale, -source.z*kReferenceScale,
            10-source.y*kReferenceScale};
}
/// @brief 将世界坐标还原为原作像素坐标；供演员初始化使用，与 ReferencePosition 互为逆映射。
inline Vector3 ReferenceSourcePosition(Vector3 world) {
    return {world.x/kReferenceScale+192, (10-world.z)/kReferenceScale,
            -world.y/kReferenceScale};
}
/// @brief 单个演员的 Normal ECL 上下文；固定 60 Hz，调用栈最多 32 层，寄存器属于各层而非演员共享。
/// 移动在原作像素坐标求解，发弹保留 X/Z 投影，并沿 Y 增加朝玩家的高度速度；不增加弹数或改变投影角间距。
class ReferenceScript {
public:
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

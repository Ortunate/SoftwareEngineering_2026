// 本文件定义世界坐标下的极坐标移动和三维插值；默认 120 Hz 积分，内容时长以 60 Hz 帧表示。
#pragma once
#include "raylib.h"
#include <array>
#include <optional>

namespace koumakyou::movement {
/// @brief 世界 X/Z 平面的闭区间边界，lower/upper 分量依次为 X、Z，不限制高度 Y。
struct Bounds {
    Vector2 lower{-9.28571429F,-10.7142857F},upper{9.28571429F,10.7142857F};
    /// @brief 查询世界位置的 X/Z 是否位于闭区间内。
    [[nodiscard]] bool Contains(Vector3 p) const {
        return p.x>=lower.x&&p.x<=upper.x&&p.z>=lower.y&&p.z<=upper.y;
    }
};
/// @brief 出生起算的 60 Hz 内容帧上的移动变更；速度为世界单位/秒，加速度为世界单位/秒²，角速度为弧度/秒。
/// 角度从世界 +X 朝 -Z 为正；缺省字段保留当前值，镜像只作用于最终 X 位移。
struct Keyframe {
    int frame = 0;
    std::optional<float> speed, angularVelocity, acceleration;
};
/// @brief 有限关键帧的移动路径，关键帧按非递减帧号排列，最多四个；配置按值复制到演员。
struct Path {
    float angle = 1.5707964F, speed = 5.35714286F;
    std::array<Keyframe,4> keys{};
    int count = 0;
};
/// @brief 每个演员独立持有的运动状态；BeginFrame 应先于行为执行，EndFrame 必须在行为执行之后。
/// 保留上一帧位移再计算下一帧速度的顺序，避免转弯和插值终点出现一帧偏差。
class State {
public:
    Vector3 position{}, velocity{}; // 世界位置与世界单位/秒速度；插值可同时改变 XYZ。
    Bounds bounds{{-7.14285714F,3.57142857F},{7.14285714F,7.85714286F}};
    Vector2 reflectionMargin{4.28571429F,2.14285714F}; // 随机巡航反射带的 X/Z 世界长度。
    bool mirrored = false, clamped = false;
    float angle = 0, speed = 0, acceleration = 0, angularVelocity = 0;
    /// @brief 按 1/updateHz 秒应用上一更新步的世界速度，并按需钳制世界 X/Z 边界。
    /// updateHz 须为正，且与对应 EndFrame 一致；生产为 120，原作差分测试为 60。
    void BeginFrame(int updateHz=120);
    /// @brief 从当前参数计算下一更新步速度；updateHz 约束同 BeginFrame，插值结束时精确落到目标点。
    void EndFrame(int updateHz=120);
    /// @brief 设置极坐标运动分量并中断插值；缺省分量保持原值。
    void Apply(const Keyframe& key);
    /// @brief 设置有限的弧度方向和世界单位/秒速度，中断插值，保留加速度和角速度。
    void Polar(float radians, float unitsPerSecond);
    /// @brief 在 duration 个 60 Hz 内容帧内插值到世界坐标；duration 必须为正，power=1 线性、2 减速、-2 加速。
    void Interpolate(int duration, Vector3 target, int power);
    /// @brief 按当前方向与速度在 duration 个 60 Hz 内容帧内减速停止，保持当前镜像/边界约定。
    void Decelerate(int duration);
private:
    bool interpolating_ = false;
    Vector3 start_{}, delta_{};
    int duration_ = 0, power_ = 1;
    double left_ = 0; // 60 Hz 内容帧；120 Hz 每步减 0.5，不能取整而使插值加速一倍。
};
/// @brief 检查路径关键帧顺序、有限数值与容量；不修改配置。
[[nodiscard]] bool Valid(const Path& path);
} // namespace koumakyou::movement

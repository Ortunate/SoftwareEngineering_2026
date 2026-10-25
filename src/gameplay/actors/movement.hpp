// 本文件定义可复用的极坐标移动、定时转弯和插值移动，计算单位为逻辑像素与 60 Hz 帧。
#pragma once
#include "raylib.h"
#include <array>
#include <optional>

namespace koumakyou::movement {
/// @brief 384×448 逻辑场地到三维世界的等比映射；scale 为正的世界单位/像素。
/// 逻辑 X=192 对应世界 X=0，逻辑 Y=0 对应世界 Z=10；逻辑 Z 映射世界 -Y。
struct Space {
    float scale = 20.0F / 448.0F;
    /// @brief 逻辑像素坐标转换为世界坐标。
    [[nodiscard]] Vector3 World(Vector3 p) const {
        return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};
    }
    /// @brief 世界坐标转换为逻辑像素，与 World 互为逆映射。
    [[nodiscard]] Vector3 Source(Vector3 p) const {
        return {p.x/scale+192,(10-p.z)/scale,-p.y/scale};
    }
};
/// @brief 一个出生局部帧上的移动变更；角为弧度，速度/加速度为像素每帧/每帧平方。
/// 缺省字段保留当前值；角速度每帧累加，镜像只作用于最终 X 位移。
struct Keyframe {
    int frame = 0;
    std::optional<float> speed, angularVelocity, acceleration;
};
/// @brief 有限关键帧的移动路径，关键帧按非递减帧号排列，最多四个；配置按值复制到演员。
struct Path {
    float angle = 1.5707964F, speed = 2;
    std::array<Keyframe,4> keys{};
    int count = 0;
};
/// @brief 每个演员独立持有的运动状态；BeginFrame 应先于行为执行，EndFrame 必须在行为执行之后。
/// 保留上一帧位移再计算下一帧速度的顺序，避免转弯和插值终点出现一帧偏差。
class State {
public:
    Vector3 position{}, velocity{};
    Vector2 lower{32,48}, upper{352,144};
    bool mirrored = false, clamped = false;
    float angle = 0, speed = 0, acceleration = 0, angularVelocity = 0;
    /// @brief 应用上一帧位移并按需钳制逻辑 X/Y 边界，不修改极坐标参数。
    void BeginFrame();
    /// @brief 从当前参数计算下一帧位移；插值结束时精确落到目标点并停止。
    void EndFrame();
    /// @brief 设置极坐标运动分量并中断插值；缺省分量保持原值。
    void Apply(const Keyframe& key);
    /// @brief 设置有限的弧度方向和像素/帧速度，中断插值，保留加速度和角速度。
    void Polar(float radians, float pixelsPerFrame);
    /// @brief 在 duration 个逻辑帧内插值到逻辑坐标；duration 必须为正，power=1 线性、2 减速、-2 加速。
    void Interpolate(int duration, Vector3 target, int power);
    /// @brief 按当前方向与速度在 duration 帧内减速停止，保持当前镜像/边界约定。
    void Decelerate(int duration);
private:
    bool interpolating_ = false;
    Vector3 start_{}, delta_{};
    int duration_ = 0, left_ = 0, power_ = 1;
};
/// @brief 检查路径关键帧顺序、有限数值与容量；不修改配置。
[[nodiscard]] bool Valid(const Path& path);
} // namespace koumakyou::movement

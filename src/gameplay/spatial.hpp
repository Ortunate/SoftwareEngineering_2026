// 本文件提供世界坐标下的等面积方向采样和低差异分位采样；不依赖关卡、随机源或显示帧率。
#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace koumakyou::spatial {
/// @brief 按无量纲标量缩放世界向量。
inline Vector3 Scale(Vector3 v,float n) { return {v.x*n,v.y*n,v.z*n}; }
/// @brief 逐分量相加两个采用相同世界单位的向量。
inline Vector3 Add(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
/// @brief 返回向量点积；单位方向的点积是夹角余弦。
inline float Dot(Vector3 a,Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
/// @brief 返回右手系叉积，用于构造局部切向轴。
inline Vector3 Cross(Vector3 a,Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
/// @brief 返回单位方向，长度接近零时退化为世界 -Z；输入分量须有限。
inline Vector3 Normalize(Vector3 v) {
    const float length=std::sqrt(Dot(v,v));return length>1e-6F ? Scale(v,1/length) : Vector3{0,0,-1};
}
/// @brief 正交局部基底；forward 是采样中心轴，right/up 构成与其垂直的截面，均为单位向量。
struct Frame {
    Vector3 forward,right,up;
    /// @brief 从有限轴向建立右手基底，零轴按 -Z 处理；分母绝对值至少为 1，竖直/反向轴均无奇点。
    static Frame FromAxis(Vector3 axis) {
        const Vector3 n=Normalize(axis);
        const float sign=n.z>=0 ? 1.0F : -1.0F,a=-1/(sign+n.z),b=n.x*n.y*a;
        return {n,{1+sign*n.x*n.x*a,sign*b,-sign*n.x},{b,sign+n.y*n.y*a,-n.y}};
    }
    /// @brief cosine 为相对中心轴夹角的余弦，范围 [-1,1]；azimuth 为截面内弧度，返回单位方向。
    Vector3 Direction(float cosine,float azimuth) const {
        const float radius=std::sqrt(std::max(0.0F,1-cosine*cosine));
        return Add(Scale(forward,cosine),Add(Scale(right,radius*std::cos(azimuth)),Scale(up,radius*std::sin(azimuth))));
    }
};
/// @brief 第 index 个等面积球带/球冠采样，index∈[0,count)，count>0，minimumCosine∈[-1,1]。
/// -1 覆盖整球，cos(半张角) 覆盖锥内立体角；phase 为弧度，不改变面积密度。
inline Vector3 EqualAreaDirection(const Frame& frame,int index,int count,float minimumCosine,float phase=0) {
    constexpr double goldenAngle=2.39996322972865332;
    const float cosine=1-(1-minimumCosine)*(static_cast<float>(index)+0.5F)/count;
    return frame.Direction(cosine,static_cast<float>(std::fmod(index*goldenAngle+phase,2*std::numbers::pi)));
}
/// @brief base≥2 的基数倒序值，结果为 [0,1)；不读取或改变演员的随机状态。
inline float RadicalInverse(std::uint32_t index,std::uint32_t base) {
    double result=0,factor=1.0/base;
    while(index) {result+=(index%base)*factor;index/=base;factor/=base;}
    return std::min(static_cast<float>(result),std::nextafter(1.0F,0.0F));
}
} // namespace koumakyou::spatial

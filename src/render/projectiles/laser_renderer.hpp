// 本文件定义独立激光参数、胶囊接触和共享实例渲染接口。
#pragma once
#include "classic_renderer.hpp"
#include <vector>
namespace koumakyou::projectiles {
/// @brief 世界单位；方向必须归一化，length/radius 正，variant 0..1、color 0..15。
/// 时序以秒计；warning/growEnd/dangerEnd/end 单调，age 非负，光效不改变碰撞。
struct LaserInstance {
    Vector3 origin{},direction{0,1,0};float length=4,radius=.12F;
    int variant=0,color=2;float age=3,warning=2,growEnd=2,dangerEnd=5,end=6,fadeSeconds=1;
    float glowStrength=.55F;
    // 运行时可提供既有时序采样，-1 表示使用本实例默认时序；裁分只裁片元，不重复生成端帽。
    float visualScaleOverride=-1;int dangerOverride=-1;
    bool useColorOverride=false;Color tintOverride=WHITE; // 无原生色号的手工光束可直接提供 RGBA。
};
/// @brief 非法参数抛异常；危险时窗 [warning,dangerEnd)，end 后不显示。
void ValidateLaser(const LaserInstance&);
bool LaserDangerous(const LaserInstance&);
float LaserVisualScale(const LaserInstance&);
/// @brief 球探针与解析胶囊相交；预警/退场无碰撞，probeRadius 为非负世界单位。
bool LaserTouches(const LaserInstance&,Vector3 probe,float probeRadius);
/// @brief GL 上下文中构造/销毁；共享总容量，不按颜色分配资源。
class LaserRenderer {
public:
    LaserRenderer(const std::filesystem::path& root,size_t capacity);
    ~LaserRenderer();
    /// @brief mode 0成品、1灰模、2主体、3外光；在 BeginMode3D 内调用。
    /// 每帧复用实例缓冲，长度仅伸缩中段，透明层不写深度。
    void Draw(std::span<const LaserInstance>,const Camera3D&,float aspect,int mode=0,bool glow=true,float clipFrom=0,float clipTo=1);
    const BatchStats& Stats()const;
private:struct Impl;std::unique_ptr<Impl> p_;
};
}

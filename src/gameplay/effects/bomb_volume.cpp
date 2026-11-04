// 本文件实现球、场地符阵薄盒及有限圆台的三维距离，供伤害、清弹与几何回归共用。
#include "gameplay/effects/bomb_volume.hpp"
#include "app/math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace koumakyou {
namespace {
using namespace game_math;
float Dot(Vector3 a,Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float SegmentDistanceSquared(Vector2 p,Vector2 a,Vector2 b) {
    const float x=b.x-a.x,y=b.y-a.y;
    const float t=std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/std::max(1e-12F,x*x+y*y),0.0F,1.0F);
    const float dx=p.x-a.x-t*x,dy=p.y-a.y-t*y;
    return dx*dx+dy*dy;
}
}
float BombVolume::DistanceSquared(Vector3 point) const {
    const auto offset=Subtract(point,origin);
    if(shape==Shape::Sphere) {
        const float distance=std::max(0.0F,BulletSystem::Length(offset)-radius);
        return distance*distance;
    }
    if(shape==Shape::Slab) {
        const float x=std::max(0.0F,std::abs(offset.x)-halfExtents.x);
        const float y=std::max(0.0F,std::abs(offset.y)-halfExtents.y);
        const float z=std::max(0.0F,std::abs(offset.z)-halfExtents.z);
        return x*x+y*y+z*z;
    }
    const auto axis=Subtract(end,origin);
    const float length=BulletSystem::Length(axis);
    if(length<1e-6F)return std::numeric_limits<float>::infinity();
    const float along=Dot(offset,Scale(axis,1/length));
    const float radial=BulletSystem::Length(Subtract(offset,Scale(axis,along/length)));
    if(along>=0 && along<=length && radial<=radius+(endRadius-radius)*along/length)return 0;
    // 圆台旋转对称，距离退化为 (径向距离, 轴向距离) 到三个轮廓线段的距离；端面不能漏掉。
    const Vector2 p{radial,along};
    return std::min({SegmentDistanceSquared(p,{0,0},{radius,0}),
                     SegmentDistanceSquared(p,{radius,0},{endRadius,length}),
                     SegmentDistanceSquared(p,{endRadius,length},{0,length})});
}
bool BombVolume::IntersectsSphere(Vector3 center,float targetRadius) const {
    return DistanceSquared(center)<=targetRadius*targetRadius;
}
bool BombVolume::IntersectsCapsule(Vector3 start,Vector3 finish,float targetRadius) const {
    if(IntersectsSphere(start,targetRadius) || IntersectsSphere(finish,targetRadius))return true;
    const auto direction=Subtract(finish,start);
    if(shape==Shape::Sphere) {
        const float t=std::clamp(Dot(Subtract(origin,start),direction)/std::max(1e-12F,Dot(direction,direction)),0.0F,1.0F);
        return IntersectsSphere(Add(start,Scale(direction,t)),targetRadius);
    }
    // 到凸实体的距离平方沿线段为凸函数；仅对敌方激光使用搜索，普通弹直接测试球。
    // 32 次收缩使战场尺度下的误差小于 0.0001 世界单位，不把圆台误判成无限直线。
    float low=0,high=1;
    for(int i=0;i<32;++i) {
        const float a=(2*low+high)/3,b=(low+2*high)/3;
        if(DistanceSquared(Add(start,Scale(direction,a)))<DistanceSquared(Add(start,Scale(direction,b))))high=b;
        else low=a;
    }
    return IntersectsSphere(Add(start,Scale(direction,(low+high)/2)),targetRadius+0.0001F);
}
} // namespace koumakyou

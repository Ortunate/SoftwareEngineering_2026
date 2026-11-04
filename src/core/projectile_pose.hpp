// 本文件将资产本地正向对齐世界运动方向；显示与碰撞应共用返回的正常姿态。
#pragma once
#include "raymath.h"
#include "resources/projectile_catalog.hpp"
#include <cmath>
#include <stdexcept>
namespace koumakyou {
struct ProjectilePose {Vector3 position{};Quaternion rotation{0,0,0,1};float scale=1;};
class ProjectilePoseBuilder {
    static bool Unit(Vector3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(Vector3LengthSqr(v)-1)<1e-4F;}
    static Quaternion Align(Vector3 from,Vector3 to,Vector3 fallback){
        float dot=Clamp(Vector3DotProduct(from,to),-1,1);
        Vector3 cross=Vector3CrossProduct(from,to);float length=Vector3Length(cross);
        // atan2 避免接近反向时 1+dot 消失；仅真正共线时选择确定的备用轴。
        if(length>1e-7F)return QuaternionFromAxisAngle(Vector3Scale(cross,1/length),std::atan2(length,dot));
        if(dot<0)return QuaternionFromAxisAngle(Vector3Normalize(Vector3Subtract(fallback,Vector3Scale(from,Vector3DotProduct(from,fallback)))),PI);
        return QuaternionIdentity();
    }
    static Quaternion Frame(Vector3 forward,Vector3 up){
        Vector3 right=Vector3Normalize(Vector3CrossProduct(up,forward));up=Vector3CrossProduct(forward,right);
        Matrix m=MatrixIdentity();m.m0=right.x;m.m1=right.y;m.m2=right.z;m.m4=up.x;m.m5=up.y;m.m6=up.z;m.m8=forward.x;m.m9=forward.y;m.m10=forward.z;return QuaternionFromMatrix(m);
    }
public:
    /// @brief direction 为世界单位向量，scale>0；previous 为上帧单位四元数，可空。
    /// 首帧依据世界上方向定横滚，后续最小旋转传输；rollDelta 为本帧绕运动方向自转的弧度增量。
    /// 非法方向、基底或变换抛 invalid_argument；不推进运动或 spawn 动画。
    static ProjectilePose BuildPose(const ModelDirection&model,Vector3 direction,Vector3 position,float scale,const Quaternion*previous=nullptr,float rollDelta=0){
        if(!Unit(model.forward)||!Unit(model.up)||std::abs(Vector3DotProduct(model.forward,model.up))>1e-4F||!Unit(direction)||!std::isfinite(scale)||scale<=0||!std::isfinite(rollDelta)||!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::invalid_argument("Invalid projectile pose");
        Quaternion q;
        if(previous){float n=QuaternionLength(*previous);if(!std::isfinite(n)||std::abs(n-1)>1e-4F)throw std::invalid_argument("Invalid previous pose");q=QuaternionMultiply(Align(Vector3RotateByQuaternion(model.forward,*previous),direction,Vector3RotateByQuaternion(model.up,*previous)),*previous);}
        else{Vector3 up=std::abs(direction.y)<.999F?Vector3{0,1,0}:Vector3{0,0,1};q=QuaternionMultiply(Frame(direction,up),QuaternionInvert(Frame(model.forward,model.up)));}
        q=QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(direction,rollDelta),q));return {position,q,scale};
    }
};
}

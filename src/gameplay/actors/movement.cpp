// 本文件按给定频率在世界坐标中积分移动；空间量使用世界单位，内容时长保持 60 Hz 帧基准。
#include "gameplay/actors/movement.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace koumakyou::movement {
namespace {
Vector3 Add(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Multiply(Vector3 v,float n) { return {v.x*n,v.y*n,v.z*n}; }
}
void State::BeginFrame(int updateHz) {
    position.x+=velocity.x/updateHz*(mirrored ? -1 : 1);
    position.y+=velocity.y/updateHz; position.z+=velocity.z/updateHz;
    if(clamped) { position.x=std::clamp(position.x,bounds.lower.x,bounds.upper.x); position.z=std::clamp(position.z,bounds.lower.y,bounds.upper.y); }
}
void State::EndFrame(int updateHz) {
    if(interpolating_) {
        left_-=60.0/updateHz;
        const float remaining=std::max(0.0F,static_cast<float>(left_)/duration_);
        const float fraction=power_<0 ? std::pow(1-remaining,static_cast<float>(-power_)) : 1-std::pow(remaining,static_cast<float>(power_));
        const Vector3 next=Add(start_,Multiply(delta_,fraction));
        velocity=Multiply(Add(next,Multiply(position,-1)),static_cast<float>(updateHz));
        if(left_<=0) { position=next; velocity={}; interpolating_=false; speed=0; }
    } else {
        angle+=angularVelocity/updateHz; speed+=acceleration/updateHz;
        velocity={std::cos(angle)*speed,0,-std::sin(angle)*speed};
    }
}
void State::Apply(const Keyframe& key) {
    if(key.speed)speed=*key.speed;
    if(key.angularVelocity)angularVelocity=*key.angularVelocity;
    if(key.acceleration)acceleration=*key.acceleration;
    interpolating_=false;
}
void State::Polar(float radians,float unitsPerSecond) { angle=radians;speed=unitsPerSecond;interpolating_=false; }
void State::Interpolate(int duration,Vector3 target,int power) {
    if(duration<=0)throw std::invalid_argument("Movement duration must be positive");
    start_=position; delta_=Add(target,Multiply(position,-1));
    duration_=left_=duration;power_=power;interpolating_=true;velocity={};
}
void State::Decelerate(int duration) {
    // 内容帧换算秒数再取匀减速平均速度，故 duration/60/2=duration/120；与积分频率无关。
    Interpolate(duration,Add(position,{std::cos(angle)*speed*duration/120,0,-std::sin(angle)*speed*duration/120}),2);
}
bool Valid(const Path& path) {
    if(!std::isfinite(path.angle)||!std::isfinite(path.speed)||path.count<0||path.count>4)return false;
    int previous=0;
    for(int i=0;i<path.count;++i) {
        const auto& key=path.keys[i];
        if(key.frame<previous)return false;
        for(auto value:{key.speed,key.angularVelocity,key.acceleration})if(value&&!std::isfinite(*value))return false;
        previous=key.frame;
    }
    return true;
}
} // namespace koumakyou::movement

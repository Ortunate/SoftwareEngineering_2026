// 本文件按给定频率在世界坐标中积分移动；空间量使用世界单位，内容时长保持 60 Hz 帧基准。
#include "gameplay/actors/movement.hpp"
#include "gameplay/spatial.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace koumakyou::movement {
namespace {
Vector3 Add(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Multiply(Vector3 v,float n) { return {v.x*n,v.y*n,v.z*n}; }
}
void State::BeginFrame(int updateHz) {
    // 插值速度已经指向世界目标；仅极坐标速度需要最终 X 镜像。
    position.x+=velocity.x/updateHz*(mirrored&&!interpolating_ ? -1 : 1);
    position.y+=velocity.y/updateHz; position.z+=velocity.z/updateHz;
    if(clamped) {
        position.x=std::clamp(position.x,bounds.lower.x,bounds.upper.x); position.z=std::clamp(position.z,bounds.lower.y,bounds.upper.y);
        if(heightBounds)position.y=std::clamp(position.y,heightBounds->x,heightBounds->y);
    }
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
        const float lateral=std::cos(angle)*speed;
        velocity={lateral,0,-std::sin(angle)*speed};

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
    const float lateral=std::cos(angle)*speed*duration/120;
    Vector3 target=Add(position,{lateral*(mirrored ? -1 : 1),0,-std::sin(angle)*speed*duration/120});
    if(clamped&&heightBounds)target.y=std::clamp(target.y,heightBounds->x,heightBounds->y);
    Interpolate(duration,target,2);
}
Vector3 State::Target() const { return interpolating_ ? Add(start_,delta_) : position; }
void LiftedPath::Aim(const State& world,float height) {
    const Vector3 target=planar.Target();
    const Vector3 delta{target.x-planar.position.x,0,target.z-planar.position.z};
    const float length=std::sqrt(spatial::Dot(delta,delta));
    if(length<1e-6F) {direction_=planarDirection_={};return;}
    planarDirection_=Multiply(delta,1/length);
    const float vertical=std::clamp((height-world.position.y)/length,-0.25F,0.25F);
    const float horizontal=std::sqrt(1-vertical*vertical);
    direction_={planarDirection_.x*horizontal,vertical,planarDirection_.z*horizontal};
}
void LiftedPath::Rebase(const State& world) {
    planar.position={world.position.x,0,world.position.z};
    stepPlanar_=planar.position;stepWorld_=world.position;
}
void LiftedPath::Move(State& world,Vector3 delta) const {
    const float length=std::sqrt(spatial::Dot(delta,delta));
    if(spatial::Dot(direction_,direction_)<0.5F) {world.position=Add(world.position,delta);return;}
    // 原边界夹取可能造成回退；保留其方向符号和实际距离，不把夹取位移再次变成向前冲刺。
    const float sign=spatial::Dot(delta,planarDirection_)<0 ? -1.0F : 1.0F;
    // 边界夹取后的实际 X/Z 位移可能不再平行于目标方向，必须沿实际位移分配高度。
    // 否则被挡住的横向分量仍会推进世界对象，高速巡航会逐段漂出战斗边界。
    const float vertical=direction_.y*sign,horizontal=std::sqrt(std::max(0.0F,1-vertical*vertical));
    world.position=Add(world.position,{delta.x*horizontal,length*vertical,delta.z*horizontal});
}
void LiftedPath::BeginFrame(State& world,int updateHz) {
    stepPlanar_=planar.position;stepWorld_=world.position;
    const auto before=planar.position;planar.BeginFrame(updateHz);
    Move(world,Add(planar.position,Multiply(before,-1)));
}
void LiftedPath::EndFrame(State& world,int updateHz) {
    const auto before=planar.position;planar.EndFrame(updateHz);
    Move(world,Add(planar.position,Multiply(before,-1)));
    // BeginFrame 的边界夹取和 EndFrame 的终点补偿可能方向不同；按本步合位移守恒，
    // 不能把两段长度相加后用于三维方向，否则原边界附近会出现额外位移。
    const Vector3 planarStep=Add(planar.position,Multiply(stepPlanar_,-1));
    const Vector3 worldStep=Add(world.position,Multiply(stepWorld_,-1));
    const float oldLength=std::sqrt(spatial::Dot(planarStep,planarStep));
    const float newLength=std::sqrt(spatial::Dot(worldStep,worldStep));
    if(newLength>1e-7F)world.position=Add(stepWorld_,Multiply(worldStep,oldLength/newLength));
    else world.position=Add(stepWorld_,planarStep);
    Vector3 next=planar.velocity;if(planar.mirrored)next.x=-next.x;
    const float sign=spatial::Dot(next,planarDirection_)<0 ? -1.0F : 1.0F;
    world.velocity=Multiply(direction_,std::sqrt(spatial::Dot(next,next))*sign);
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

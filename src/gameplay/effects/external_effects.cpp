// 本文件实现etama4独立粒子池、环境节拍与世界坐标附着轨道及释放淡出。
#include "gameplay/effects/external_effects.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace koumakyou::effects {
namespace {
bool Finite(Vector3 value) {return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
Vector3 Cross(Vector3 a,Vector3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vector3 Normalize(Vector3 value) {
    const double length=std::hypot(static_cast<double>(value.x),static_cast<double>(value.y),static_cast<double>(value.z));
    if(length<=1e-6)return {};
    return {static_cast<float>(value.x/length),static_cast<float>(value.y/length),static_cast<float>(value.z/length)};
}
// 源工厂只重置动画/活动状态，半径与相位属于槽的历史；释放弱引用仍必须立即进行。
void Retire(Particle& particle) {
    const float radius=particle.orbitRadius,phase=particle.orbitPhase;
    particle={};particle.orbitRadius=radius;particle.orbitPhase=phase;
}
Vector3 OrbitOffset(Vector3 axis,float radius,float phase) {
    // 世界+Y为高度。初始径向由向下轴叉乘轨道轴获得；源平面的手性改变后，旋转角取-2*phase。
    // 直接在世界坐标旋转，不在运行时转换像素；高度分量的六倍拉伸属于该演出的三维轨迹。
    Vector3 radial=Cross({0,-1,0},axis);
    const float squared=radial.x*radial.x+radial.y*radial.y+radial.z*radial.z;
    if(squared>=1e-5F)radial=Normalize(radial);
    radial={radial.x*radius,radial.y*radius,radial.z*radius};
    const float sine=-std::sin(phase),cosine=std::cos(phase);
    Vector3 q{axis.x*sine,axis.y*sine,axis.z*sine};
    const float norm=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+cosine*cosine);
    if(norm>1e-6F)q={q.x/norm,q.y/norm,q.z/norm};
    const float w=norm>1e-6F?cosine/norm:0;
    const auto first=Cross(q,radial),second=Cross(q,first);
    return {radial.x+2*(w*first.x+second.x),6*(radial.y+2*(w*first.y+second.y)),radial.z+2*(w*first.z+second.z)};
}
}
void AttachmentAnchor::Follow(Vector3 position,float radius) {
    if(!Finite(position)||!std::isfinite(radius)||radius<0)throw std::invalid_argument("Invalid attachment anchor");
    position_=position;radius_=radius;
}
void AmbientEmitter::Reset(std::uint32_t seed) {random_=seed?seed:1;clock_=0;previous_=0;phase_=0;}
std::uint32_t AmbientEmitter::NextRandom() {random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return random_;}
std::vector<Request> AmbientEmitter::Step(double contentFrames,Vector3 position,double phaseFrame) {
    if((contentFrames!=.5&&contentFrames!=1)||!Finite(position)||!std::isfinite(phaseFrame)||phaseFrame<0)
        throw std::invalid_argument("Invalid ambient effect step");
    constexpr float pi=std::numbers::pi_v<float>;
    phase_+=.01745329238474369F*static_cast<float>(contentFrames);
    if(phase_>=pi/4)phase_-=pi/2;
    const auto frame=static_cast<std::uint64_t>(clock_);
    const int period=frame<=30?8:frame<=60?4:frame<=120?2:1;
    std::vector<Request> requests;
    // 120Hz的第二半步仍推进相位，但不能重复同一整数内容帧的发射；时钟独立于技能和子协程。
    if(frame!=previous_&&frame%period==0) {
        const auto bound=(frame%16)/2;
        const auto radial=bound+(bound?(NextRandom()&65535U)%bound:0);
        const float radius=static_cast<float>(radial)*.446428571F+1.42857143F;
        const float angle=phase_-static_cast<float>(radial)*pi/40;
        const float dx=std::cos(angle)*radius,dz=-std::sin(angle)*radius;
        for(float sign:{1.0F,-1.0F}) {
            const float uniform=static_cast<float>(NextRandom()>>8)/16777216.0F;
            const Vector3 velocity{uniform*1.78571427F-.892857134F,0,
                (.2666666805744171F-static_cast<float>(radial)*8/60)*2.67857143F};
            // 加速度是出生速度乘固定-1/120每源帧，对应每秒速度乘-0.5；不能逐步重新按速度衰减。
            requests.push_back({Kind::Ambient,{position.x+sign*dx,position.y,position.z+dz},1,{48,48,255,255},phaseFrame,
                velocity,{-velocity.x*.5F,0,-velocity.z*.5F}});
        }
    }
    previous_=frame;clock_+=contentFrames;
    return requests;
}
int AnimationScript(Kind kind) {
    switch(kind) {
        case Kind::Burst:return 6;
        case Kind::Flash:return 17;
        case Kind::Charge:return 7;
        case Kind::SlowCharge:return 8;
        case Kind::Ambient:return 19;
        case Kind::Attached:return 18;
        case Kind::Drift:return 21;
        case Kind::Graze:return 13;
    }
    throw std::invalid_argument("Unknown external effect kind");
}
ExternalEffectSystem::ExternalEffectSystem(int updateHz):updateHz_(updateHz) {
    if(updateHz!=60&&updateHz!=120)throw std::invalid_argument("External effects require 60 or 120 Hz");
}
void ExternalEffectSystem::Bind(const anm::Archive& archive,VisualId visual) {
    if(ActiveCount())throw std::logic_error("Cannot rebind active external effects");
    if(visual==kInvalidVisualId)throw std::invalid_argument("External effect visual is missing");
    for(int script:{6,7,8,13,17,18,19,21})if(!archive.FindScript(script))throw std::invalid_argument("External effect animation is missing");
    archive_=&archive;visual_=visual;
}
std::uint32_t ExternalEffectSystem::NextRandom() {
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return random_;
}
float ExternalEffectSystem::Random() {return static_cast<float>(NextRandom()>>8)/16777216.0F;}
std::size_t ExternalEffectSystem::Spawn(const Request& request) {
    const int script=AnimationScript(request.kind);
    if(request.count<1||!Finite(request.position)||!Finite(request.velocity)||!Finite(request.acceleration)||
       !std::isfinite(request.contentFrame)||request.contentFrame<0||!Finite(request.axis)||
       (request.kind==Kind::Attached&&request.count!=1))throw std::invalid_argument("Invalid external effect request");
    if(!archive_)throw std::logic_error("External effects have no bound archive");
    std::size_t created=0;
    // 原生工厂一批最多扫描整池一次；游标先推进、再检查旧槽，满池不覆盖活动实例。
    for(std::size_t scanned=0;scanned<particles_.size()&&created<static_cast<std::size_t>(request.count);++scanned) {
        auto& particle=particles_[cursor_];cursor_=(cursor_+1)%particles_.size();
        if(particle.active)continue;
        Retire(particle);particle.kind=request.kind;particle.position=particle.origin=request.position;particle.visual=visual_;
        // 环境/漂散/擦弹粒子的spriteRand在创建时执行；运动随机初始化仍在首个池更新。
        // ANM使用池的局部视觉种子；不模拟原生全局RNG在动画与攻击间的交错读取。
        particle.animation.Start(*archive_,script,0,-1,request.kind==Kind::Ambient||request.kind==Kind::Drift||request.kind==Kind::Graze?NextRandom():1);
        if(request.kind==Kind::Ambient) {particle.velocity=request.velocity;particle.acceleration=request.acceleration;}
        if(request.kind==Kind::Attached) {particle.axis=Normalize(request.axis);particle.attachment=request.attachment;}
        particle.animation.SetColor(request.color);particle.active=true;++created;
    }
    dropped_+=static_cast<std::size_t>(request.count)-created;
    return created;
}
void ExternalEffectSystem::Tick() {
    const float seconds=1.0F/updateHz_;
    for(auto& particle:particles_)if(particle.active) {
        if(!particle.initialized) {
            if(particle.kind==Kind::Burst) {
                // 独立源公式为每轴(U*256-128)*4/33像素/帧；这里只保存对应世界单位/秒常量。
                constexpr float range=83.1168823F;
                particle.velocity={Random()*range-range/2,0,-(Random()*range-range/2)};
                particle.acceleration={-particle.velocity.x*3,0,-particle.velocity.z*3};
            } else if(particle.kind==Kind::Graze) {
                // 每轴源初速(U*256-128)/12，固定加速度为出生速度的-1/19每源帧。
                // 这里只保存世界单位/秒及每秒的固定衰减系数；速度反向后仍沿原加速度继续积分。
                constexpr float range=57.1428571F,drag=3.15789485F;
                particle.velocity={Random()*range-range/2,0,-(Random()*range-range/2)};
                particle.acceleration={-particle.velocity.x*drag,0,-particle.velocity.z*drag};
            } else if(particle.kind==Kind::Drift) {
                // 源每轴初速(U*256-128)/60像素/帧，固定加速度为初速的-1/95每帧。
                // 转为世界量后仍先位移再加速；120Hz按半内容帧积分，不能借用Burst的快散参数。
                constexpr float range=11.4285714F,drag=.631578975F;
                particle.velocity={Random()*range-range/2,0,-(Random()*range-range/2)};
                particle.acceleration={-particle.velocity.x*drag,0,-particle.velocity.z*drag};
            } else if(particle.kind==Kind::Charge||particle.kind==Kind::SlowCharge) {
                const float angle=Random()*2*std::numbers::pi_v<float>-std::numbers::pi_v<float>;
                particle.direction={std::cos(angle),0,-std::sin(angle)};
            }
            particle.initialized=true;
        }
        if(particle.kind==Kind::Attached) {
            const float step=60.0F/updateHz_;
            if(!particle.released) {
                if(const auto anchor=particle.attachment.lock()) {
                    particle.origin=anchor->Position();
                    // 必须先比较再增长，不钳制目标；源在临界半径会越过一次步长。
                    if(particle.orbitRadius<anchor->Radius())particle.orbitRadius+=.0133928577F*step;
                    particle.orbitPhase+=.03141592815518379F*step;
                    constexpr float pi=std::numbers::pi_v<float>;
                    if(particle.orbitPhase>pi)particle.orbitPhase-=2*pi;
                } else {particle.released=true;particle.attachment.reset();}
            }
            const auto offset=OrbitOffset(particle.axis,particle.orbitRadius,particle.orbitPhase);
            particle.position={particle.origin.x+offset.x,particle.origin.y+offset.y,particle.origin.z+offset.z};
            if(particle.released) {
                particle.fadeFrames+=step;
                if(particle.fadeFrames>=16) {Retire(particle);continue;}
                auto color=particle.animation.Sample().color;
                color.a=static_cast<unsigned char>((1-particle.fadeFrames/16)*255);
                particle.animation.SetColor(color);
                const float size=1+static_cast<float>(particle.fadeFrames/16);particle.animation.SetScale({size,size});
            }
        } else if(particle.kind==Kind::Burst||particle.kind==Kind::Ambient||particle.kind==Kind::Drift||particle.kind==Kind::Graze) {
            particle.position.x+=particle.velocity.x*seconds;particle.position.y+=particle.velocity.y*seconds;particle.position.z+=particle.velocity.z*seconds;
            particle.velocity.x+=particle.acceleration.x*seconds;particle.velocity.y+=particle.acceleration.y*seconds;particle.velocity.z+=particle.acceleration.z*seconds;
        } else if(particle.kind==Kind::Charge||particle.kind==Kind::SlowCharge) {
            const float duration=particle.kind==Kind::Charge?60.0F:240.0F;
            const float radius=11.4285714F-static_cast<float>(particle.frame)*11.4285714F/duration;
            particle.position={particle.origin.x+particle.direction.x*radius,particle.origin.y,
                               particle.origin.z+particle.direction.z*radius};
        }
        particle.animation.Advance(1.0/updateHz_);
        if(particle.animation.Finished())Retire(particle);
        else particle.frame+=60.0/updateHz_;
    }
}
void ExternalEffectSystem::Update(float seconds) {
    if(!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("Invalid external effect delta time");
    ticks_+=static_cast<double>(seconds)*updateHz_;
    while(ticks_+0.00001>=1) {ticks_-=1;Tick();}
}
void ExternalEffectSystem::Clear(std::uint32_t seed) {
    for(auto& particle:particles_)particle={};
    archive_=nullptr;visual_=kInvalidVisualId;cursor_=dropped_=0;random_=seed?seed:1;ticks_=0;
}
std::size_t ExternalEffectSystem::ActiveCount() const {
    return std::count_if(particles_.begin(),particles_.end(),[](const auto& p){return p.active;});
}
} // namespace koumakyou::effects

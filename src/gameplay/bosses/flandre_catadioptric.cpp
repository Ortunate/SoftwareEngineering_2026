// 本文件实现芙兰朵露Catadioptric的多速反弹弹簇及沿边扫描；空间量使用世界单位，等待为60 Hz内容帧。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include <numbers>

namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void Height(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
void Move(Actor& actor,int frames,Vector3 target,int power=2) {
    Motion(actor).Interpolate(frames,target,power);Height(actor);
}
void Kick(Actor& actor,float angle) {
    auto target=Motion(actor).position;
    // 三种反冲均为正交方向。源像素目标舍入后垂轴位移严格为零，世界原点附近不能保留cos(π/2)残差触发转身。
    if(angle==pi/2)target.z-=0.892857134F;
    else target.x+=angle==pi?-0.892857134F:0.892857134F;
    Motion(actor).Interpolate(10,target,2);Height(actor);
}
// 各随机弹每发依次读取角度和速率；四档速度分别夹在中央大弹周围，不能合并为一次平均速率发射。
struct Spray {int type,color,count;float first,last,halfAngle;};
constexpr Spray kSprays[]{
    {6,3,9,13.3928571F,9.375F,0.0245436933F},
    {6,3,3,9.375F,6.69642878F,0.0654498488F},
    {1,6,5,6.69642878F,4.01785707F,0.0785398185F},
    {1,6,8,4.01785707F,1.33928571F,0.0981747732F},
};
void ConfigureBounce(Actor& actor,int type,int count) {
    const auto size=battle::EnemyBulletLogicalSize(type);
    // 半尺寸来自主体逻辑精灵，独立于碰撞半径。三维高度边界与X/Z一起反射，不增取随机数。
    actor.skills.projectile={.sampleAtStepStart=true,.bounceCount=count,
        .bounceLower={-8.57142857F,-10,-10},.bounceUpper={8.57142857F,10,10},
        .bounceHalfExtents={size.x/2,size.x/2,size.y/2}};
}
void Cluster(Actor& actor,float angle,int bounces,std::uint32_t& sample) {
    ConfigureBounce(actor,9,bounces);
    actor.Fire({skill::Formation::FixedFan,9,1,1,1,13.9285707F,3.21428585F,angle,0,2048});
    for(const auto& spray:kSprays) {
        ConfigureBounce(actor,spray.type,bounces);
        skill::Shot shot{skill::Formation::RandomAngleSpeed,spray.type,spray.color,spray.count,1,
            spray.first,spray.last,angle-spray.halfAngle,angle+spray.halfAngle,2048};
        // 弹幕面积序列独立于种子携带的高度序号，始终落在Shot允许的非负int域；不消费随机流。
        shot.sampleIndex=static_cast<int>(sample);actor.Fire(shot);sample=(sample+spray.count)&0x7fffffffU;
    }
}
Routine Charge(Actor& actor) {
    // 子时钟隔离父局部时钟回拨；最后一组在58帧，60帧才放行后续弹簇。
    actor.soundEvents.push_back(5);
    for(int n=0;n<30;++n) {co_await actor.At(n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
    co_await actor.At(60);
}
void Fan(Actor& actor,std::initializer_list<float> angles,int bounces,std::uint32_t& sample) {
    actor.soundEvents.push_back(7);for(float angle:angles)Cluster(actor,angle,bounces,sample);
}
}
Routine FlandreBoss::Behaviors::Catadioptric(Actor& actor) {
    actor.ClearBullets();actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);actor.defeatActors=true;
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};Move(actor,120,{0,0,6.42857143F});
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;
    co_await Charge(actor);std::uint32_t sample=0;
    for(;;) {
        Fan(actor,{-2.3561945F,-1.96349537F,-pi/2,-1.17809725F,-pi/4},2,sample);Kick(actor,pi/2);
        co_await actor.At(180);Move(actor,90,{4.28571415F,0,5.71428585F});
        co_await actor.At(290);Fan(actor,{pi/4,0.392699093F,0,-0.392699093F,-pi/4},1,sample);Kick(actor,pi);
        co_await actor.At(350);Move(actor,60,{-4.28571415F,0,5.71428585F});
        co_await actor.At(410);Fan(actor,{2.3561945F,2.7488935F,pi,-2.7488935F,-2.3561945F},1,sample);Kick(actor,0);
        co_await actor.At(470);Move(actor,60,{-7.14285714F,0,4.28571415F});
        co_await actor.At(530);Move(actor,120,{7.14285714F,0,4.28571415F},1);
        for(int n=0;n<5;++n) {
            co_await actor.At(530+n*24);Fan(actor,{-pi/2},1,sample);
        }
        co_await actor.At(680);Move(actor,60,{0,0,4.28571415F});
        co_await actor.At(740);co_await Charge(actor);
        // 第二轮118跳回同一首轮循环，计数归零后落入最初的五向弹簇；后面的七向弹簇/655回跳不可达。
        // 保留这一实际控制流：首发180帧，此后680帧一轮，每轮20簇，不按表末时间推导周期。
        actor.Rewind(120);
    }
}
}

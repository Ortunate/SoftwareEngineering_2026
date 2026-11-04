// 本文件实现额外最终符的生命驱动双旋臂、独立扫描使魔及真实210帧过渡；空间常量均为世界单位。
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include "gameplay/spatial.hpp"
#include <numbers>
namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
constexpr movement::Bounds kEmitterBounds{{-10,-11.7857143F},{10,11.7857143F}};
constexpr movement::Bounds kBattleBounds{{-7.14285714F,0},{7.14285714F,7.85714293F}};
void Normalize(float& angle) {while(angle>pi)angle-=2*pi;while(angle<-pi)angle+=2*pi;}
float Aim(const Actor& actor) {return actor.motion.AngleTo(actor.PlayerPosition());}
void Precision(Actor& actor) {
    // 经明确批准，仅这批使魔的数值积分在组件内部保留原float格；公开位置、速度与全部素材/碰撞仍用世界单位。
    actor.motion.EnableFloatIntegration({-8.5714283F,0,10},{0.0446428582F,0.0446428582F,-0.0446428582F});
}
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void SpawnHeart(Actor& actor,Vector3 position,behavior::Entry routine,const skill::SpiralTrail& parameters,float direction=0) {
    const behavior::Pattern pattern{.movement={.angle=direction,.speed=0},.initialAnimation=167,
        .spatialShots=actor.skills.spatial,.routine=routine,.spiralTrail=parameters};
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=1200,.sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=-2,.visual=actor.visual,
        .hitRadius=native_sizes::kDefaultEnemyRadius,.retirementBounds=kEmitterBounds,
        .bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,.defeatBase=stage_assets::kNativeDefeat,
        .fallback=actor.skills.fallback,.retireOutside=false,.damageable=false,.visible=false,
        .stageClearResponse=behavior::StageClearResponse::Remove});
}
skill::SpiralTrail Parameters(const Actor& actor) {
    if(!actor.spiralTrail)throw std::invalid_argument("Heart emitter requires spiral trail parameters");
    return *actor.spiralTrail;
}
void Hide(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(167);
    actor.visible=actor.interactable=actor.damageable=actor.contactEnabled=false;
}
Routine HeartEmitter(Actor& actor) {
    const auto p=Parameters(actor);Precision(actor);Hide(actor);actor.soundEvents.push_back(22);
    const float direction=actor.motion.angle;actor.motion.Polar(direction,2.67857143F);
    actor.skills.offset={};actor.skills.projectile={.sampleAtStepStart=true};
    actor.Fire({skill::Formation::FixedFan,6,1,1,1,2.67857143F,0,direction,0,514});
    actor.skills.projectile={.acceleration=p.acceleration,.accelerationFrames=static_cast<float>(p.accelerationFrames),.sampleAtStepStart=true};
    float angle=p.axisAngle;int round=0;
    for(;;) {
        if(!kEmitterBounds.Contains(actor.Position())) {actor.finished=true;co_return;}
        if(actor.Frame()>=1+round*6) {
            actor.skills.offset={std::cos(angle)*1.07142854F,0,-std::sin(angle)*1.07142854F};
            // 每轮只取一次随机微角；先除100再加轴角，保留源float运算边界和2/3交替色。
            const float shotAngle=(actor.Random(2*pi)-pi)/100+angle;
            skill::Shot shot{skill::Formation::FixedFan,5,2+round%2,1,1,0,2.67857143F,shotAngle,pi/8,530};
            if(actor.skills.spatial) {shot.sampleIndex=round;shot.coneHalfAngle=0.5F;}
            actor.Fire(shot);angle+=p.angleStep;Normalize(angle);++round;
        }
        co_await actor.At(actor.Frame()+0.5);
    }
}
template<bool Clockwise>
Routine HeartArm(Actor& actor) {
    auto p=Parameters(actor);Precision(actor);Hide(actor);
    float direction=Aim(actor)-pi+p.aimOffset;p.axisAngle=Aim(actor)+p.aimOffset;
    p.angleStep=Clockwise?pi/14:-pi/14;
    for(int n=0;n<6;++n) {
        co_await actor.At(n*p.emitterInterval);SpawnHeart(actor,actor.Position(),HeartEmitter,p,direction);
        direction+=Clockwise?-0.654500008F:0.654500008F;Normalize(direction);
    }
    // 源还等待最后一段间隔，再停止执行；静止控制器没有删除，保持占槽直到明确清场或卸载。
    co_await actor.At(6*p.emitterInterval);
}
struct Snapshot {skill::SpiralTrail trail;int repeat;};
Snapshot Capture(const Actor& actor) {
    // 原生18只在取快照时于7200帧后视作零HP；不写回生命，也不在后继使魔上重新计算。
    const int hp=actor.phaseFrames<7200?static_cast<int>(actor.health):0;
    return {{.acceleration=0.964285731F-static_cast<float>(hp)*0.160714284F/6000,
        .accelerationFrames=400-hp*250/6000,.emitterInterval=22+hp*25/6000},370+hp*350/6000};
}
void Pair(Actor& actor,Vector3 position,const skill::SpiralTrail& p) {
    SpawnHeart(actor,position,HeartArm<false>,p);SpawnHeart(actor,position,HeartArm<true>,p);
}
}
Routine FlandreBoss::Behaviors::DanmakuHeartPrelude(Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Active);
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(7);
    actor.visible=actor.interactable=actor.contactEnabled=true;actor.damageable=false;actor.deathEffect=2;
    actor.SetPoses({160,163,164,161,162});actor.skills.offset={0,0,0.535714269F};
    auto& motion=Motion(actor);motion.bounds=kBattleBounds;motion.clamped=true;motion.Interpolate(110,{0,0,2.41071439F},2);
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
    co_await actor.At(30);actor.dropEvents.push_back(40);
    co_await actor.At(210);actor.pendingCallback=SkillComplete;
}
Routine FlandreBoss::Behaviors::DanmakuHeart(Actor& actor) {
    actor.ClearBullets();actor.defeatActors=true;actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);
    actor.visible=actor.interactable=actor.contactEnabled=actor.damageable=true;
    actor.skills.offset={};actor.SetPoses({160,166,166,161,162});
    co_await actor.At(60);actor.Animate(176);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.soundEvents.push_back(5);
    // 首次40轮外部粒子等待80帧，后续每次30轮等待60帧；不把粒子当作攻击弹。
    for(int n=0;n<40;++n) {co_await actor.At(120+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
    co_await actor.At(200);auto captured=Capture(actor);auto point=actor.Position();point.z-=1.42857146F;
    Pair(actor,point,captured.trail);
    double next=800;co_await actor.At(next);actor.soundEvents.push_back(5);
    for(;;) {
        for(int n=0;n<30;++n) {co_await actor.At(next+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
        next+=60;co_await actor.At(next);captured=Capture(actor);
        point=actor.Position();point.x+=actor.Random(1.07142854F)-0.535714269F;
        point.z-=actor.Random(1.07142854F)-0.535714269F;point.z-=1.42857146F;
        captured.trail.aimOffset=(actor.Random(2*pi)-pi)/30;Pair(actor,point,captured.trail);
        // i2已包含下轮60帧蓄力；计数回跳不额外消费一帧，故这里只等repeat-60。
        next+=captured.repeat-60;co_await actor.At(next);actor.soundEvents.push_back(5);
    }
}
Routine FlandreBoss::Behaviors::EncoreFinalExit(Actor& actor,bool damage) {
    actor.SetAmbientEffects(effects::AmbientMode::Disabled);
    actor.interactable=true;actor.damageable=actor.contactEnabled=false;actor.SetEmitterPeriod(0);
    if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
    actor.RandomDirection(-pi,pi,false);auto& motion=Motion(actor);float angle=motion.angle;
    // 原50的反射带固定，不随112扩大的战斗边界改变；此处不能使用默认RandomDirection的派生阈值。
    if(motion.position.x<-2.85714293F&&std::abs(angle)>pi/2)angle=std::copysign(pi,angle)-angle;
    if(motion.position.x>2.85714293F&&std::abs(angle)<pi/2)angle=std::copysign(pi,angle)-angle;
    if(motion.position.z>5.71428585F&&angle<0)angle=-angle;
    if(motion.position.z<6.78571415F&&angle>0)angle=-angle;
    motion.Polar(angle,1.60714293F);motion.Decelerate(60);
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
    constexpr std::array<Color,5> colors{WHITE,Color{255,128,128,255},Color{128,255,128,255},Color{128,128,255,255},Color{255,255,128,255}};
    for(int round=0;round<6;++round) {
        for(int n=0;n<5;++n) {co_await actor.At(round*10+(n+1)*2);actor.EmitEffect(effects::Kind::Burst,2,colors[n]);}
        actor.soundEvents.push_back(7);
    }
    actor.soundEvents.push_back(18);actor.ClearBullets();actor.defeatActors=true;
    // 伤害击破进入119前已将模式3消费为0；超时只替换程序，Boss标记令清敌循环跳过本体，模式3仍保留。
    // 因此伤害路径本步清活动位；超时本步恢复HP1、安装96，下一固定步才执行删除。
    actor.health=damage?0:1;
    if(!damage)co_await actor.At(60+actor.ContentStepFrames());
    actor.finished=true;
}
}

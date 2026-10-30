// 本文件实现芙兰朵露Starbow Break及直行/弯行彩雨发射体，保持世界单位、独立随机流与显式出生编排。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <numbers>

namespace koumakyou::bosses {
namespace {
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
// 隐藏本体仍使用167的64×80逻辑精灵做入场/回收；全部本符出生点均已触及此扩展矩形。
constexpr movement::Bounds kEmitterBounds{{-10,-11.7857143F},{10,11.7857143F}};
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void Height(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
void Wander(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
    actor.RandomDirection(-pi,pi);Motion(actor).Apply({.speed=6.69642878F});Motion(actor).Decelerate(90);Height(actor);
}
template<int Color,bool Turning>
Routine RainbowEmitter(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(167);
    actor.damageable=actor.contactEnabled=actor.interactable=actor.visible=false;actor.skills.rankInfluence={};
    actor.skills.offset=Turning?Vector3{}:Vector3{0,0,0.535714269F};actor.skills.projectile.sampleAtStepStart=true;
    actor.SetEmitterSound(22);int round=0;
    for(;;) {
        if(!kEmitterBounds.Contains(actor.Position())||actor.Frame()>=200) {actor.finished=true;co_return;}
        if(actor.Frame()>=round*4) {
            // 源顺序为一次速率读取、一次加速度读取；所有范围已换成世界单位/秒或世界单位/秒²。
            const float speed=actor.Random(2.67857143F);
            const float acceleration=actor.Random(Turning?2.08928585F:1.60714281F)+(Turning?1.92857146F:2.41071415F);
            actor.skills.projectile.accelerationVector=Vector3{std::cos(pi/2)*acceleration,0,-std::sin(pi/2)*acceleration};
            actor.skills.projectile.accelerationFrames=Turning?120:180;
            skill::Shot shot{skill::Formation::FixedFan,3,Color,1,1,speed,0,-pi/2,0,19};
            // 三维将50轮单弹扫描为等立体角球冠；高度序列独立于原速率/加速度随机流，固定加速轴不跟随散射方向。
            if(actor.skills.spatial) {shot.sampleIndex=round;shot.coneHalfAngle=0.5F;}
            actor.Fire(shot);
            if constexpr(Turning)actor.motion.Polar(actor.motion.angle+0.0149599649F,13.3928571F);
            ++round;
        }
        co_await actor.At(actor.Frame()+0.5);
    }
}
// 生命仅作为原始出生元数据保存，发弹色与路线由独立语义配方选择；不从演员HP解码生产行为。
struct Rain {int health;float speed;behavior::Entry routine;};
constexpr Rain kRed{102,15.5357151F,RainbowEmitter<2,false>},kPink{104,15.5357151F,RainbowEmitter<4,false>},
    kBlue{106,15.5357151F,RainbowEmitter<6,false>},kCyan{108,15.5357151F,RainbowEmitter<8,false>},
    kLime{111,15.5357151F,RainbowEmitter<11,false>},kYellow{113,15.5357151F,RainbowEmitter<13,false>},
    kOrange{114,15.5357151F,RainbowEmitter<14,false>};
constexpr Rain kTurningBlue{106,13.3928571F,RainbowEmitter<6,true>},kTurningLime{111,13.3928571F,RainbowEmitter<11,true>},
    kTurningOrange{114,13.3928571F,RainbowEmitter<14,true>};
void SpawnRain(Actor& actor,Vector3 position,float angle,const Rain& rain) {
    const behavior::Pattern pattern{.movement={.angle=angle,.speed=rain.speed},.initialAnimation=167,
        .spatialShots=actor.skills.spatial,.routine=rain.routine};
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=static_cast<float>(rain.health),.sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=-2,.visual=actor.visual,
        .hitRadius=1.25F,.retirementBounds=kEmitterBounds,.bulletBase=actor.skills.bulletBase,.laserBase=actor.skills.laserBase,
        .defeatBase=stage_assets::kNativeDefeat,.fallback=actor.skills.fallback,.retireOutside=true,.damageable=false,
        .visible=false,.stageClearResponse=behavior::StageClearResponse::Remove});
}
}
Routine FlandreBoss::Behaviors::StarbowBreak(Actor& actor) {
    actor.ClearBullets();actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);actor.defeatActors=true;
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);Height(actor);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;
    for(int cycle=0;;++cycle) {
        const int start=120+cycle*717;
        // 两段蓄力各30轮，每轮六粒子；独立视觉随机流不影响后续彩雨的攻击随机读取。
        actor.soundEvents.push_back(5);
        for(int n=0;n<30;++n) {co_await actor.At(start+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
        co_await actor.At(start+60);actor.soundEvents.push_back(7);
        const auto point=actor.Position();
        SpawnRain(actor,point,2.356194496F,kRed);SpawnRain(actor,point,2.748893499F,kOrange);
        SpawnRain(actor,point,pi,kYellow);SpawnRain(actor,point,0.785398185F,kPink);
        SpawnRain(actor,point,0.392699093F,kCyan);SpawnRain(actor,point,0,kLime);
        co_await actor.At(start+120);Wander(actor);actor.soundEvents.push_back(5);
        for(int n=0;n<30;++n) {co_await actor.At(start+120+n*2);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
        co_await actor.At(start+180);actor.soundEvents.push_back(7);SpawnRain(actor,{-10,0,-5},-0.589048624F,kPink);
        co_await actor.At(start+190);SpawnRain(actor,{-10,0,-3.57142854F},-0.589048624F,kTurningBlue);
        co_await actor.At(start+200);SpawnRain(actor,{-10,0,-2.14285707F},-0.589048624F,kCyan);
        co_await actor.At(start+210);SpawnRain(actor,{-10,0,-0.714285731F},-0.589048624F,kTurningLime);
        co_await actor.At(start+220);SpawnRain(actor,{-10,0,0.714285731F},-0.589048624F,kYellow);
        co_await actor.At(start+230);SpawnRain(actor,{-10,0,2.14285707F},-0.589048624F,kTurningOrange);
        co_await actor.At(start+240);SpawnRain(actor,{-10,0,3.57142854F},-0.589048624F,kRed);
        co_await actor.At(start+330);Wander(actor);
        co_await actor.At(start+390);actor.soundEvents.push_back(7);SpawnRain(actor,{10,0,-5},3.337942123F,kPink);
        co_await actor.At(start+400);SpawnRain(actor,{10,0,-3.57142854F},3.337942123F,kTurningBlue);
        co_await actor.At(start+410);SpawnRain(actor,{10,0,-2.14285707F},3.337942123F,kCyan);
        co_await actor.At(start+420);SpawnRain(actor,{10,0,-0.714285731F},3.337942123F,kTurningLime);
        co_await actor.At(start+430);SpawnRain(actor,{10,0,0.714285731F},3.337942123F,kYellow);
        co_await actor.At(start+440);SpawnRain(actor,{10,0,2.14285707F},3.337942123F,kTurningOrange);
        co_await actor.At(start+450);SpawnRain(actor,{10,0,3.57142854F},3.337942123F,kRed);
        co_await actor.At(start+590);actor.soundEvents.push_back(7);SpawnRain(actor,{-10,0,-5},0,kPink);
        co_await actor.At(start+600);SpawnRain(actor,{10,0,-3.57142854F},pi,kBlue);
        co_await actor.At(start+610);SpawnRain(actor,{-10,0,-2.14285707F},0,kCyan);
        co_await actor.At(start+620);SpawnRain(actor,{10,0,-0.714285731F},pi,kLime);
        co_await actor.At(start+630);SpawnRain(actor,{-10,0,0.714285731F},0,kYellow);
        co_await actor.At(start+640);SpawnRain(actor,{10,0,2.14285707F},pi,kOrange);
        co_await actor.At(start+650);SpawnRain(actor,{-10,0,3.57142854F},0,kRed);
        Motion(actor).Interpolate(60,{0,0,0},2);Height(actor);
        co_await actor.At(start+717);
    }
}
}

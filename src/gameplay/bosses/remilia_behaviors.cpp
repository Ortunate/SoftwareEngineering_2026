// 本文件实现 RemiliaBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/remilia.hpp"
#include "stages/stage6/behaviors.hpp"
#include "stages/stage6/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage6;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& a) {if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);return a.liftedMotion ? a.liftedMotion->planar : a.motion;}
void Height(Actor& a) {if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));}
void MoveTo(Actor& a,int frames,Vector3 target) {
    auto& motion=Motion(a);if(a.liftedMotion)a.liftedMotion->Rebase(a.motion);
    motion.Interpolate(frames,target,2);Height(a);
}

void Wander(Actor& a,float speed,int frames) {
    auto& motion=Motion(a);if(a.liftedMotion)a.liftedMotion->Rebase(a.motion);
    a.RandomDirection(-pi,pi);motion.Apply({.speed=speed});motion.Decelerate(frames);Height(a);
}
void Configure(Actor& a,skill::Shot& shot) {
    a.skills.projectile.sampleAtStepStart=true;
    if(a.skills.spatial) {
        shot.spatialCount=shot.count*3;
        if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed) {
            shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;
        }
    }
    a.ConfigureEmitter(shot);
}
void Fire(Actor& a,skill::Shot shot) {Configure(a,shot);a.Fire(shot);}
void Initialize(Actor& a) {
    Motion(a).bounds={{-7.14285714F,4.64285714F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
    a.skills.offset={0,0,0.535714269F};a.SetEmitterPeriod(0);a.skills.projectile={};a.contactEnabled=true;a.interactable=true;a.damageable=true;a.deathEffect=2;
}
Routine Prepare(Actor& a,Vector3 target) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.damageable=false;a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,target);co_await a.At(120);a.SetPoses({166,166,166,161,162});a.damageable=true;
}

Routine FirstSpiral(Actor& a) {
    float angle=0.0490873866F,large=-pi/2,balls=a.Random(2*pi)-pi;a.SetEmitterSound(7);
    for(int i=0;i<32;++i) {
        co_await a.At(i*8);Fire(a,{Formation::FixedFan,3,5,3,1,4.8214283F,3.21428585F,angle,0.098174773F,512});angle+=0.628318548F;
        Fire(a,{Formation::FixedFan,9,0,4,1,9.375F,3.21428585F,large,1.04719758F,512});large-=0.392699093F;
        Fire(a,{Formation::FixedFan,6,3,2,1,6.69642878F,3.21428585F,balls,pi/4,512});balls+=pi;
        Fire(a,{Formation::FixedFan,6,3,2,1,6.69642878F,3.21428585F,balls,pi/4,512});balls-=pi;balls+=0.157079637F;
    }
    co_await a.At(266);
}
void RandomCone(Actor& a,int type,int color,int count,float first,float last,float center,float halfAngle,int flags=4,
                std::optional<Vector3> axis=std::nullopt) {
    skill::Shot shot{Formation::RandomAngleSpeed,type,color,count,1,first,last,center-halfAngle,center+halfAngle,flags};
    shot.spatialAxis=axis;Fire(a,shot);
}
Routine LayeredAim(Actor& a) {
    a.soundEvents.push_back(5);co_await a.At(100);
    for(int i=0;i<5;++i) {
        co_await a.At(100+i*20);a.soundEvents.push_back(7);
        Fire(a,{Formation::AimedFan,9,0,1,1,16.0714283F,3.21428585F,0,0,4});
        const auto p=a.Position(),t=a.PlayerPosition();const float aim=std::atan2(p.z-t.z,t.x-p.x);
        const Vector3 axis=spatial::Normalize({t.x-p.x,t.y-p.y,t.z-p.z});
        RandomCone(a,6,1,5,16.0714283F,13.3928576F,aim,0.098174773F,4,axis);
        RandomCone(a,6,1,5,13.3928576F,10.7142859F,aim,0.157079637F,4,axis);
        RandomCone(a,1,2,5,10.7142859F,5.35714293F,aim,0.314159274F,4,axis);
        RandomCone(a,1,2,5,5.35714293F,2.67857143F,aim,pi/4,4,axis);
    }
    co_await a.At(200);
}
Routine RotatingCones(Actor& a,bool reverse) {
    float angle=reverse ? pi : 0;a.soundEvents.push_back(5);co_await a.At(100);
    for(int i=0;i<8;++i) {
        co_await a.At(100+i*15);a.soundEvents.push_back(7);
        Fire(a,{Formation::FixedFan,9,0,1,1,18.75F,3.21428585F,angle,0,4});
        RandomCone(a,6,1,3,16.0714283F,13.3928576F,angle,0.098174773F);
        RandomCone(a,6,1,5,13.3928576F,10.7142859F,angle,0.157079637F);
        RandomCone(a,1,2,5,10.7142859F,5.35714293F,angle,0.314159274F);
        RandomCone(a,1,2,5,5.35714293F,2.67857143F,angle,pi/4);angle+=reverse ? -pi/8 : pi/8;
    }
    co_await a.At(220);
}
Routine CounterRotatingKnives(Actor& a,bool reverse) {
    float angle=a.Random(2*pi)-pi;
    for(int i=0;i<60;++i) {
        co_await a.At(i*2);a.skills.projectile={.bounceCount=1};Fire(a,{Formation::FixedRing,8,reverse ? 3 : 1,4,2,8.03571415F,3.21428585F,angle,0,512});
        angle+=reverse ? -0.098174773F : 0.098174773F;
    }
    co_await a.At(120);
}
Routine BatFamiliar(Actor& a) {
    a.Animate(165);a.interactable=false;const float angle=a.Random(pi),angular=(a.Random(pi/4)-pi/8)/20;
    a.motion.Polar(angle,a.Random(10.7142859F)+13.3928576F);co_await a.At(30);a.motion.Apply({.angularVelocity=angular*60});co_await a.At(10000);a.finished=true;
}
inline constexpr behavior::Pattern kBatFamiliar{.initialAnimation=165,.deathEffect=0,.routine=BatFamiliar};
void SpawnBat(Actor& a) {
    auto pattern=kBatFamiliar;pattern.spatialShots=a.skills.spatial;
    a.spawnEvents.push_back({.position=a.Position(),.pattern=pattern,.sourceHealth=100,.sourceScore=0,.seed=++a.spawnIndex,
        .sourceItem=0,.visual=kRemilia,.hitRadius=0.625F,.bulletBase=kNativeBulletBase,.laserBase=kNativeLaserBase,
        .defeatBase=kNativeDefeat,.fallback=kBlueBullet});
}
Routine BatPrepare(Actor& a,int variant) {
    a.soundEvents.push_back(5);co_await a.At(100);a.SetPoses({-1,-1,-1,-1,-1});a.Animate(165);a.interactable=false;
    for(int i=0;i<30;++i) {co_await a.At(100+i);SpawnBat(a);SpawnBat(a);}
    co_await a.At(130);a.skills.projectile={.accelerationVector=Vector3{0,0,-4.01785707F}};
    skill::Shot shot=variant==0 ? skill::Shot{Formation::RandomAngleSpeed,2,2,11,1,5.35714293F,2.67857143F,pi,-pi,528} :
        variant==1 ? skill::Shot{Formation::AimedFan,9,0,9,1,13.3928576F,2.67857143F,0,0.34906584F,528} :
        skill::Shot{Formation::AimedRing,7,1,12,2,13.3928576F,5.35714293F,0,0,528};
    Configure(a,shot);a.SetEmitterSound(25);a.SetEmitterPeriod(variant==0 ? 8 : variant==1 ? 16 : 10);
}
Routine MineSeeds(Actor& a,int repeats,int interval) {
    for(int i=0;i<repeats;++i) {
        co_await a.At(i*interval);if(RemiliaBoss::Behaviors::SeedMines(a)==0)co_return;
    }
    co_await a.At(repeats*interval);
}
void ScarletRay(Actor& a,float angle) {
    skill::Shot marker{Formation::FixedFan,9,0,1,1,16.6071434F,3.21428585F,angle,0,2};
    const auto p=a.Position(),player=a.PlayerPosition();const auto axis=spatial::Frame::FromAxis({player.x-p.x,player.y-p.y,player.z-p.z});
    if(a.skills.spatial)marker.spatialAxis=axis.Direction(std::cos(angle-std::atan2(p.z-player.z,player.x-p.x)),2.39996323F*a.spatialIndex++);
    Fire(a,marker);
    RandomCone(a,6,1,3,16.0714283F,10.7142859F,angle,0.098174773F,2,marker.spatialAxis);
    RandomCone(a,6,1,5,10.7142859F,8.03571415F,angle,0.157079637F,2,marker.spatialAxis);
    RandomCone(a,1,2,5,8.03571415F,5.35714293F,angle,0.314159274F,2,marker.spatialAxis);
    RandomCone(a,1,2,10,5.35714293F,2.67857143F,angle,pi/6,2,marker.spatialAxis);
}
void AimedScarletRays(Actor& a,bool narrow) {
    const auto p=a.Position(),player=a.PlayerPosition();const float aim=std::atan2(p.z-player.z,player.x-p.x);
    const std::array<float,5> wide{0,pi/4,-pi/4,pi/2,-pi/2};const std::array<float,3> tight{0,pi/40,-pi/40};
    a.soundEvents.push_back(7);
    if(narrow)for(float offset:tight)ScarletRay(a,aim+offset);
    else for(float offset:wide)ScarletRay(a,aim+offset);
}

}

Routine RemiliaBoss::Behaviors::Entrance(Actor& a) {
    a.damageable=false;a.contactEnabled=false;a.SetPoses({160,163,164,161,162});a.skills.offset={0,0,0.535714269F};
    Motion(a).clamped=false;MoveTo(a,120,{0,0,5.71428585F});co_await a.At(120);
    Motion(a).bounds={{-7.14285714F,4.64285714F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;co_await a.At(1000000);
}
Routine RemiliaBoss::Behaviors::Prelude(Actor& a) {co_await a.At(1000000);}
Routine RemiliaBoss::Behaviors::FirstNonspell(Actor& a) {
    Initialize(a);
    for(;;) {co_await a.At(100);co_await FirstSpiral(a);Wander(a,6.69642878F,90);co_await FirstSpiral(a);co_await a.At(210);a.Rewind(100);}
}
void RemiliaBoss::Behaviors::StarLattice(Actor& a,bool emitProjectiles) {
    const float random=a.Random(2*pi);const auto origin=a.Position();
    for(int pass=0;pass<2;++pass) {
        const float first=pass ? -7*pi/8+random : -pi+random,step=pass ? -pi/4 : pi/4;
        std::array<Vector3,8> positions{};
        for(int i=0;i<8;++i) {
            const float angle=first+i*pi/4;Vector3 radial{std::cos(angle),0,-std::sin(angle)};
            if(a.skills.spatial)radial=spatial::EqualAreaDirection(spatial::Frame::FromAxis({0,0,-1}),i,8,-1,first);
            positions[i]=spatial::Add(origin,spatial::Scale(radial,1.42857143F));
        }
        for(int ring=0;ring<3;++ring)for(int i=0;i<8;++i) {
            const float angle=first+ring*step+i*pi/4,length=ring<2 ? 5.0F : 21.4285714F;
            Vector3 direction{std::cos(angle),0,-std::sin(angle)};
            if(a.skills.spatial) {
                const auto basis=spatial::Frame::FromAxis(spatial::Add(positions[i],spatial::Scale(origin,-1)));
                direction=basis.Direction(std::cos(ring*step),first+2.39996323F*i);
            }
            if(emitProjectiles)a.FireEmitterAt(positions[i]);
            else {
                auto config=a.skills;config.offset={};
                a.FireAt(skill::Laser{.color=2,.length=length,.width=1.25F,.grow=60.0F+ring*16,.sustain=90.0F-ring*16,
                    .fade=16,.warning=50,.harmlessTail=16,.aimed=false,.direction=direction,.animationScript=1},positions[i],&config);
            }
            positions[i]=spatial::Add(positions[i],spatial::Scale(direction,length));
        }
    }
}
int RemiliaBoss::Behaviors::SeedMines(Actor& a) {
    std::vector<Vector3> positions;
    a.ModifyEnemyProjectiles([&](Bullet& b){if(!b.pendingExpiration&&b.nativeType>=6&&b.nativeType<=9)positions.push_back(b.position);});
    auto config=a.skills;config.offset={};config.rankInfluence={};config.projectile={};
    for(auto position:positions)a.FireAt({Formation::FixedFan,3,1,1,1,0,0,a.Random(2*pi)-pi,0,8},position,&config);
    return static_cast<int>(positions.size());
}
void RemiliaBoss::Behaviors::ReleaseMines(Actor& a) {
    const float phase=a.Random(2*pi)-pi;const auto center=a.Position();
    a.ModifyEnemyProjectiles([&](Bullet& b) {
        if(b.pendingExpiration||b.nativeType<0||b.nativeType>5||BulletSystem::Length(b.velocity)>0||BulletSystem::Length(b.acceleration)>0)return;
        const Vector3 relative{b.position.x-center.x,b.position.y-center.y,b.position.z-center.z};
        const float distance=BulletSystem::Length(relative);const float angle=(distance>0.014117F ? distance*pi/11.4285714F : 0)+phase;
        Vector3 direction{std::cos(angle),0,-std::sin(angle)};
        if(a.skills.spatial)direction=spatial::Frame::FromAxis(relative).Direction(std::cos(angle),2.39996323F*a.spatialIndex++);
        b.acceleration=spatial::Scale(direction,1.60714281F);b.accelerationSeconds=2;b.motionClock=0;b.sampleMotionAtStart=true;
        b.nativeColor=2;b.visual=static_cast<VisualId>(a.skills.bulletBase+b.nativeType*16+2);
    });
}
Routine RemiliaBoss::Behaviors::StarOfVanities(Actor& a,PhaseMemory& memory) {
    co_await Prepare(a,{0,0,5});int cycle=0;
    for(;;) {
        a.soundEvents.push_back(5);
        for(int i=0;i<16;++i) {co_await a.At(i*4);memory.chargingPulsesLeft=16-i;}
        co_await a.At(64);memory.chargingPulsesLeft=0;a.Rewind(4);StarLattice(a,false);
        a.Random(2*pi);a.Random(2*pi);const float angle=a.Random(2*pi)-pi;
        a.skills.projectile={.acceleration=1.60714281F,.accelerationFrames=120};
        skill::Shot shot{cycle%2 ? Formation::RandomAngleSpeed : Formation::FixedRing,6,3,cycle%2 ? 3 : 5,1,0.535714269F,2.67857143F,angle,0,528};
        Configure(a,shot);co_await a.At(64);StarLattice(a,true);Wander(a,6.69642878F,90);++cycle;
        co_await a.At(124);a.Rewind(0);
    }
}
Routine RemiliaBoss::Behaviors::SecondNonspell(Actor& a,PhaseMemory& memory) {
    Initialize(a);a.ClearBullets();a.dropEvents.push_back(12);a.SetPoses({160,163,164,161,162});
    for(;;) {
        co_await a.At(60);Wander(a,6.69642878F,90);
        // 源设置了另一个计数器为 90，却消费上一阶段的蓄力计数；保留这一跨回调约定。
        const int frames=std::max(1,memory.chargingPulsesLeft);
        for(int i=1;i<=frames;++i) {co_await a.At(60+i);--memory.chargingPulsesLeft;}
        memory.chargingPulsesLeft=0;a.Rewind(61);co_await LayeredAim(a);Wander(a,6.69642878F,90);
        co_await a.At(62);co_await RotatingCones(a,false);Wander(a,6.69642878F,90);
        co_await a.At(63);co_await RotatingCones(a,true);co_await LayeredAim(a);co_await a.At(73);a.Rewind(60);
    }
}
Routine RemiliaBoss::Behaviors::ScarletNetherworld(Actor& a) {
    co_await Prepare(a,{0,0,3.57142854F});a.SetEmitterSound(23);
    for(;;) {
        float angle=a.Random(2*pi)-pi;
        for(int i=0;i<6;++i)for(int part=0;part<4;++part) {
            co_await a.At(i*21+part*4);
            if(part<2)a.skills.projectile={.angularVelocity=part==0 ? 1.47262156F : -1.47262156F,.angularFrames=128};
            else a.skills.projectile={.accelerationVector=Vector3{0,0,-3.21428585F},.accelerationFrames=240};
            Fire(a,{Formation::FixedRing,part<2 ? 2 : 5,2,part<2 ? 24 : 16,1,part<2 ? 4.8214283F : 5.89285707F,
                2.67857143F,angle,-0.0157079641F,part<2 ? 544 : 528});angle+=0.098174773F;
        }
        co_await a.At(126);Wander(a,4.01785707F,90);co_await a.At(216);a.Rewind(0);
    }
}
Routine RemiliaBoss::Behaviors::ThirdNonspell(Actor& a) {
    Initialize(a);a.ClearBullets();a.dropEvents.push_back(12);a.SetPoses({160,163,164,161,162});
    for(;;) {co_await a.At(60);co_await CounterRotatingKnives(a,false);Wander(a,6.69642878F,90);
        co_await a.At(188);co_await CounterRotatingKnives(a,true);co_await a.At(298);a.Rewind(60);}
}
Routine RemiliaBoss::Behaviors::VladTepesCurse(Actor& a) {
    co_await Prepare(a,{0,0,3.57142854F});const float angle=a.Random(2*pi)-pi;int cycle=0;
    for(;;) {
        a.soundEvents.push_back(5);a.skills.projectile={.angularVelocity=cycle%2 ? 1.17809725F : -1.17809725F,.angularFrames=108};
        Fire(a,{Formation::FixedRing,8,3,std::min(18,cycle+13),1,8.03571415F,2.67857143F,angle,-0.0157079641F,544});
        co_await MineSeeds(a,32,14);ReleaseMines(a);Wander(a,6.69642878F,60);++cycle;
    }
}
Routine RemiliaBoss::Behaviors::BatNonspell(Actor& a) {
    Initialize(a);a.ClearBullets();a.dropEvents.push_back(12);int cycle=0;
    for(;;) {
        co_await a.At(60);co_await BatPrepare(a,cycle%3);
        for(int i=0;i<7;++i) {co_await a.At(60+i*30);Wander(a,i%2 ? 18.75F : 10.7142859F,30);}
        co_await a.At(270);a.SetEmitterPeriod(0);a.interactable=true;a.SetPoses({160,163,164,161,162});a.Animate(160);
        co_await a.At(360);++cycle;a.Rewind(60);
    }
}
Routine RemiliaBoss::Behaviors::ScarletShoot(Actor& a) {
    co_await Prepare(a,{0,0,5});a.soundEvents.push_back(5);
    for(;;) {
        a.soundEvents.push_back(5);co_await a.At(64);AimedScarletRays(a,false);
        co_await a.At(124);AimedScarletRays(a,false);Wander(a,6.69642878F,60);
        co_await a.At(214);AimedScarletRays(a,true);
        co_await a.At(334);AimedScarletRays(a,false);
        co_await a.At(364);
        const auto p=a.Position(),player=a.PlayerPosition();const float aim=std::atan2(p.z-player.z,player.x-p.x);
        a.soundEvents.push_back(7);for(float offset:std::array{0.0F,pi/3,-pi/3,2*pi/3,-2*pi/3})ScarletRay(a,aim+offset);
        co_await a.At(484);a.Rewind(0);
    }
}
Routine RemiliaBoss::Behaviors::RedMagic(Actor& a) {
    Initialize(a);a.damageable=false;a.ClearBullets();a.dropEvents.push_back(12);co_await a.At(60);
    co_await Prepare(a,{0,0,4.28571415F});a.Rewind(0);
    for(;;) {
        a.soundEvents.push_back(5);const auto size=battle::EnemyBulletLogicalSize(9);
        a.skills.projectile={.bounceCount=1,.bounceLower={-8.57142857F,-10,-10},.bounceUpper={8.57142857F,10,10},.bounceHalfExtents={size.x/2,size.x/2,size.y/2}};
        Fire(a,{Formation::FixedRing,9,1,10,1,5.35714293F,2.67857143F,a.Random(2*pi)-pi,-0.0157079641F,2560});Wander(a,6.69642878F,60);
        co_await MineSeeds(a,10,17);co_await a.At(16);ReleaseMines(a);Wander(a,6.69642878F,60);
        Fire(a,{Formation::FixedRing,9,1,12,1,5.35714293F,2.67857143F,a.Random(2*pi)-pi,-0.0157079641F,2560});Wander(a,6.69642878F,60);
        co_await MineSeeds(a,10,17);ReleaseMines(a);
        Fire(a,{Formation::FixedRing,9,1,8,1,5.35714293F,2.67857143F,a.Random(2*pi)-pi,-0.0157079641F,2560});
        co_await MineSeeds(a,10,17);ReleaseMines(a);Wander(a,6.69642878F,60);float angle=a.Random(2*pi)-pi;
        co_await a.At(66);Fire(a,{Formation::FixedRing,9,1,15,1,5.35714293F,2.67857143F,angle,-0.0157079641F,2560});
        co_await MineSeeds(a,10,17);ReleaseMines(a);Wander(a,6.69642878F,60);angle=a.Random(2*pi)-pi;
        co_await a.At(106);Fire(a,{Formation::FixedRing,9,1,8,2,10.7142859F,2.67857143F,angle,-0.392699093F,512});
        co_await MineSeeds(a,10,17);ReleaseMines(a);co_await a.At(206);a.Rewind(0);
    }
}
Routine RemiliaBoss::Behaviors::Defeated(Actor& a) {
    a.SetEmitterPeriod(0);a.interactable=true;a.contactEnabled=false;a.damageable=false;a.SetPoses({160,163,164,161,162});Wander(a,1.60714281F,60);
    for(int i=0;i<6;++i) {co_await a.At((i+1)*10);a.soundEvents.push_back(7);}
    a.soundEvents.push_back(18);a.ClearBullets();a.finished=true;
}
Routine RemiliaBoss::Behaviors::SkillComplete(Actor& a) {a.finished=true;co_return;}

}

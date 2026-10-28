// 本文件实现 SakuyaBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/sakuya.hpp"
#include "stages/stage5/behaviors.hpp"
#include "stages/stage5/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage5;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& a) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    return a.liftedMotion ? a.liftedMotion->planar : a.motion;
}
void Height(Actor& a) {if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));}
void MoveTo(Actor& a,int frames,Vector3 target) {Motion(a).Interpolate(frames,target,2);Height(a);}
void Wander(Actor& a,float speed,int frames) {
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=speed});Motion(a).Decelerate(frames);Height(a);
}
void Teleport(Actor& a,Vector3 target) {
    // 瞬移同时替换两个运动状态，避免下一步把旧平面位移再次加到世界位置。
    const bool lifted=a.liftedMotion.has_value();auto state=Motion(a);state.position=target;state.Polar(0,0);state.velocity={};
    a.liftedMotion.reset();a.motion=state;if(lifted)a.liftedMotion.emplace(state);
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
int Period(const Actor& a,int frames) {return frames+frames/5-(frames/5*2*a.skills.rank)/32;}
void AutoRing(Actor& a,int frames,bool randomInitial) {
    skill::Shot shot{Formation::AimedRing,8,3,16,1,5.35714293F,2.67857143F,0,0.157079637F,516};
    Configure(a,shot);const int period=Period(a,frames);
    a.SetEmitterPeriod(period,randomInitial ? static_cast<int>(a.RandomIndex(period)) : 0);a.SetEmitterSound(22);
}
void Initialize(Actor& a) {
    Motion(a).bounds={{-7.14285714F,4.10714286F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
    a.skills.offset={0,0,0.535714269F};a.skills.projectile={};a.phaseClockPaused=false;
    a.interactable=true;a.contactEnabled=true;a.damageable=true;a.deathEffect=2;
}
void Bounce(Actor& a,bool kunai) {
    const auto size=battle::EnemyBulletLogicalSize(kunai ? 8 : 4);
    a.skills.projectile={.bounceCount=1,.bounceLower={-8.57142857F,-10,-10},.bounceUpper={8.57142857F,10,10},
        .bounceHalfExtents={size.x/2,size.x/2,size.y/2}};
}
Routine FamiliarDefeated(Actor& a) {a.interactable=false;a.InterruptAnimation(1);co_await a.At(60);a.finished=true;}
Routine Familiar(Actor& a,float heading,int color,float step,bool ricochet) {
    float angle=a.Random(2*pi)-pi;a.Animate(16);a.interactable=false;a.SetEmitterSound(22);
    a.motion.Polar(heading,2.14285714F);a.motion.Decelerate(370);if(ricochet)Bounce(a,false);
    const int count=ricochet ? 17 : 40,interval=ricochet ? 14 : 6;
    for(int i=0;i<count;++i) {
        co_await a.At(50+i*interval);
        Fire(a,{Formation::FixedFan,ricochet ? 4 : 2,color,ricochet ? 6 : 5,1,4.28571415F,3.21428585F,angle,
            ricochet ? 0.0157079641F : 0.0261799395F,ricochet ? 2560 : 512});angle+=step;
    }
    co_await a.At(50+count*interval);a.InterruptAnimation(1);
    co_await a.At(110+count*interval);a.finished=true;
}
Routine BlueDiagonal(Actor& a) {co_await Familiar(a,pi/4,6,0.628318548F,false);}
Routine GreenRight(Actor& a) {co_await Familiar(a,0,10,-0.314159274F,false);}
Routine GreenDiagonal(Actor& a) {co_await Familiar(a,3*pi/4,10,-0.628318548F,false);}
Routine BlueLeft(Actor& a) {co_await Familiar(a,pi,6,0.314159274F,false);}
Routine RedRicochetDiagonal(Actor& a) {co_await Familiar(a,pi/4,2,0.628318548F,true);}
Routine BlueRicochetRight(Actor& a) {co_await Familiar(a,0,6,-0.196349546F,true);}
Routine BlueRicochetDiagonal(Actor& a) {co_await Familiar(a,3*pi/4,6,-0.628318548F,true);}
Routine RedRicochetLeft(Actor& a) {co_await Familiar(a,pi,2,0.196349546F,true);}
constexpr behavior::Pattern FamiliarPattern(behavior::Entry entry) {
    return {.initialAnimation=16,.deathEffect=0,.spatialShots=true,.routine=entry,.onDefeated=FamiliarDefeated};
}
constexpr std::array kFirstFamiliars{FamiliarPattern(BlueDiagonal),FamiliarPattern(GreenRight),FamiliarPattern(GreenDiagonal),FamiliarPattern(BlueLeft)};
constexpr std::array kRicochetFamiliars{FamiliarPattern(RedRicochetDiagonal),FamiliarPattern(BlueRicochetRight),FamiliarPattern(BlueRicochetDiagonal),FamiliarPattern(RedRicochetLeft)};
Routine Summon(Actor& a,bool ricochet) {
    for(auto pattern:ricochet ? kRicochetFamiliars : kFirstFamiliars) {
        pattern.spatialShots=a.skills.spatial;
        a.spawnEvents.push_back({.position=a.Position(),.pattern=pattern,.sourceHealth=100,.sourceScore=0,.seed=++a.spawnIndex,
            .sourceItem=0,.visual=kOrb,.hitRadius=0.625F,.bulletBase=kNativeBulletBase,.laserBase=kNativeLaserBase,
            .defeatBase=kNativeDefeat,.fallback=kBlueBullet,.retireOutside=false});
    }
    co_await a.At(60);
}
Routine MidFans(Actor& a,bool kunai,float angle,float step) {
    if(kunai)a.soundEvents.push_back(5);
    for(int i=0;i<10;++i) {
        co_await a.At(i*(kunai ? 8 : 3));
        Fire(a,{Formation::FixedFan,kunai ? 8 : 4,kunai ? 3 : 2,kunai ? 8 : 4,kunai ? 1 : 2,
            kunai ? 8.5714283F : 5.35714293F,2.67857143F,angle,kunai ? 0.285599321F : 0.044879895F,0});
        angle-=step;a.soundEvents.push_back(7);
    }
    co_await a.At(kunai ? 80 : 30);
}
Routine MisdirectionSide(Actor& a,bool left) {
    MoveTo(a,40,{left ? -4.28571415F : 4.28571415F,0,3.57142854F});Bounce(a,false);
    skill::Shot shot{Formation::AimedRing,4,2,24,3,8.03571415F,3.21428585F,0,0,4};
    Configure(a,shot);a.SetEmitterSound(7);a.SetEmitterPeriod(6);
    co_await a.At(30);a.Animate(left ? 135 : 137);
    co_await a.At(60);a.Animate(134);Teleport(a,{left ? 4.28571415F : -4.28571415F,0,5.71428585F});a.SetEmitterPeriod(0);
    for(int i=0;i<3;++i) {co_await a.At(80+i*18);Fire(a,{Formation::AimedFan,8,3,11,4,12.0535717F,3.21428585F,0,0.209439516F,516});}
    co_await a.At(134);a.Animate(left ? 139 : 140);
    co_await a.At(194);a.Animate(133);
    co_await a.At(254);a.Animate(134);Teleport(a,{0,0,3.57142854F});
    co_await a.At(284);
}
Routine KnifeSweep(Actor& a) {
    float spread=0.0490873866F;co_await a.At(170);a.Animate(149);a.SetEmitterSound(7);
    for(int i=0;i<12;++i) {co_await a.At(170+i*8);Fire(a,{Formation::FixedFan,8,2,2,8,10.7142859F,3.21428585F,pi/2,spread,512});spread+=0.314159274F;}
    co_await a.At(326);
}
Routine FixedKnifeRing(Actor& a) {
    co_await a.At(100);a.Animate(Motion(a).position.x<0 ? 140 : 139);a.SetEmitterSound(7);
    Fire(a,{Formation::FixedRing,8,3,24,1,5.35714293F,3.21428585F,pi/2,-0.0490873866F,512});
    co_await a.At(160);a.Animate(Motion(a).position.x<0 ? 143 : 142);
}
Routine RotatingKnifeRings(Actor& a,bool reverse) {
    float angle=a.Random(2*pi)-pi;Bounce(a,true);
    for(int i=0;i<8;++i) {
        co_await a.At(i*5);Fire(a,{Formation::FixedRing,8,reverse ? 1 : 3,reverse ? 6 : 5,2,4.8214283F,3.21428585F,angle,0.0157079641F,2560});
        angle+=reverse ? -0.098174773F : 0.098174773F;
    }
    co_await a.At(100);
}
Routine PrepareSpell(Actor& a,bool center) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.damageable=false;a.skills.offset={};
    a.phaseClockPaused=false;a.interactable=true;a.phaseFrames=0;a.SetPoses({128,131,132,129,130});
    MoveTo(a,120,{0,0,center ? 3.57142854F : 5.0F});co_await a.At(120);a.damageable=true;
}
void StopWorld(Actor& a,bool stopped) {a.phaseClockPaused=stopped;a.interactable=!stopped;}
void RestorePose(Actor& a,bool moving) {a.Animate(Motion(a).position.x<0 ? (moving ? 143 : 140) : (moving ? 142 : 139));}
Routine FrozenArcEmitter(Actor& a,int duration) {
    // 原生逐帧回调从计数零开始；父协程的等待在调用期间冻结，返回处显式补回同一段墙钟帧。
    for(int frame=0;frame<duration;frame+=9) {co_await a.At(frame);SakuyaBoss::Behaviors::EmitClockArc(a,frame/9);}
    co_await a.At(duration);
}
Routine LockedKnifeBurst(Actor& a,bool red) {
    const auto from=a.Position(),player=a.PlayerPosition();const Vector3 axis{player.x-from.x,player.y-from.y,player.z-from.z};
    const float angle=std::atan2(-axis.z,axis.x);Bounce(a,true);
    for(int i=0;i<(red ? 5 : 4);++i) {
        co_await a.At(i*(red ? 3 : 5));
        skill::Shot shot{Formation::FixedFan,8,red ? 1 : 3,4,red ? 3 : 2,red ? 7.5F : 5.35714293F,2.67857143F,
            angle,red ? 0.0785398185F : 0.157079637F,red ? 512 : 2560};
        if(a.skills.spatial)shot.spatialAxis=spatial::Normalize(axis);Fire(a,shot);
    }
    co_await a.At(red ? 35 : 40);
}

}

Routine SakuyaBoss::Behaviors::Entrance(Actor& a) {
    a.damageable=false;a.contactEnabled=false;a.SetPoses({128,131,132,129,130});a.skills.offset={0,0,0.535714269F};
    Motion(a).clamped=false;MoveTo(a,60,{0,0,4.28571415F});co_await a.At(60);
    Motion(a).bounds={{-7.14285714F,4.10714286F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;co_await a.At(1000000);
}
Routine SakuyaBoss::Behaviors::MisdirectionNonspell(Actor& a) {
    Initialize(a);Motion(a).bounds.lower.y=2.14285714F;a.skills.offset={};a.Animate(128);
    a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.interactable=false;Teleport(a,{0,0,4.28571415F});
    a.soundEvents.push_back(5);co_await a.At(30);a.interactable=true;
    for(;;) {
        a.Animate(148);co_await MidFans(a,true,0,0.314159274F);Wander(a,4.01785707F,90);
        co_await MidFans(a,false,pi,-0.314159274F);co_await a.At(90);
        co_await MidFans(a,true,pi,-0.314159274F);co_await MidFans(a,false,0,0.314159274F);
        co_await a.At(91);a.Rewind(30);
    }
}
Routine SakuyaBoss::Behaviors::Misdirection(Actor& a) {
    co_await PrepareSpell(a,true);bool left=a.RandomIndex(2)==0;
    for(;;) {co_await MisdirectionSide(a,left);left=!left;}
}
Routine SakuyaBoss::Behaviors::MisdirectionRetreat(Actor& a) {
    StopWorld(a,false);a.SetEmitterPeriod(0);a.ClearBullets();a.damageable=false;a.contactEnabled=false;
    Motion(a).Polar(0,0);Motion(a).clamped=false;a.Animate(128);
    co_await a.At(130);MoveTo(a,60,{0,0,12.8571424F});co_await a.At(190);a.finished=true;
}
Routine SakuyaBoss::Behaviors::MisdirectionDefeated(Actor& a) {a.soundEvents.push_back(18);a.itemEvents.push_back(5);a.dropEvents.push_back(10);co_await MisdirectionRetreat(a);}
Routine SakuyaBoss::Behaviors::FirstNonspell(Actor& a) {
    Initialize(a);
    for(;;) {
        co_await a.At(100);a.Animate(148);co_await Summon(a,false);AutoRing(a,50,true);Wander(a,4.5535717F,60);
        co_await a.At(160);Wander(a,4.5535717F,60);co_await a.At(220);Wander(a,4.5535717F,60);
        a.SetEmitterPeriod(0);co_await KnifeSweep(a);a.InterruptAnimation(1);co_await a.At(300);a.Rewind(100);
    }
}
Routine SakuyaBoss::Behaviors::SecondNonspell(Actor& a) {
    Initialize(a);a.dropEvents.push_back(12);a.ClearBullets();
    for(;;) {
        co_await a.At(60);a.Animate(148);co_await Summon(a,true);AutoRing(a,50,true);Wander(a,4.01785707F,60);
        co_await a.At(120);Wander(a,4.01785707F,60);co_await a.At(180);Wander(a,4.01785707F,60);
        a.SetEmitterPeriod(0);co_await FixedKnifeRing(a);co_await a.At(260);a.Rewind(60);
    }
}
Routine SakuyaBoss::Behaviors::ThirdNonspell(Actor& a) {
    Initialize(a);a.dropEvents.push_back(12);a.ClearBullets();
    for(;;) {
        co_await a.At(60);a.Animate(148);co_await RotatingKnifeRings(a,false);Wander(a,5.35714293F,50);
        co_await a.At(120);RestorePose(a,true);Wander(a,5.35714293F,50);co_await RotatingKnifeRings(a,true);
        co_await a.At(230);a.Rewind(60);
    }
}
void SakuyaBoss::Behaviors::EmitClockArc(Actor& a,int pulse) {
    const auto boss=a.Position(),player=a.PlayerPosition();const Vector3 relative{player.x-boss.x,player.y-boss.y,player.z-boss.z};
    const Vector3 axis=spatial::Normalize(relative);const float radius=(pulse%2 ? -1 : 1)*11.4285714F;
    const Vector3 base=spatial::Add(spatial::Scale(relative,0.5F-pulse*0.5F/9),spatial::Scale(axis,radius));
    auto config=a.skills;config.offset={};config.rankInfluence={};config.projectile={};
    const auto frame=spatial::Frame::FromAxis(axis);
    // 平面参照绕 +Y 转动；空间版在同一瞄准轴的截面内铺开圆弧，不增加随机数或复制平面高度。
    const Vector3 tangent=a.skills.spatial ? frame.right : Vector3{axis.z,0,-axis.x};
    for(int i=0;i<9;++i) {
        const float rotation=-pi/4+(i+1)*pi/18;
        const Vector3 arm=spatial::Add(spatial::Scale(axis,-radius*std::cos(rotation)),spatial::Scale(tangent,-radius*std::sin(rotation)));
        const Vector3 muzzle=spatial::Add(boss,spatial::Add(base,arm));
        skill::Shot shot{Formation::AimedFan,8,3,1,1,5.35714293F,0,0,pi/6,0};
        shot.aimAxisOffset=pulse%2 ? -pi/4+i*pi/18 : 0;
        if(!a.skills.spatial)shot.angle=shot.aimAxisOffset;
        a.FireAt(shot,muzzle,&config);
    }
    a.soundEvents.push_back(7);
}
void SakuyaBoss::Behaviors::RedirectStoppedBullets(Actor& a) {
    int changed=0;const auto player=a.PlayerPosition();
    a.ModifyEnemyProjectiles([&](Bullet& bullet) {
        // 原生筛选读取逻辑高而非裁图高；6～9 才满足阈值，退场弹不参与读取随机数。
        if(changed>=14||bullet.pendingExpiration||bullet.nativeType<6||bullet.nativeType>9||bullet.nativeColor==5||a.RandomIndex(4)!=0)return;
        const Vector3 offset{bullet.position.x-player.x,bullet.position.y-player.y,bullet.position.z-player.z};
        const float distance=std::sqrt(spatial::Dot(offset,offset));
        const float angle=distance>5.71428571F ? a.Random(3*pi/4)+pi/4 : std::atan2(-offset.z,offset.x)+pi/2+a.Random(2*pi);
        Vector3 direction{std::cos(angle),0,-std::sin(angle)};
        if(a.skills.spatial) {
            const auto frame=spatial::Frame::FromAxis(distance>5.71428571F ? Vector3{0,0,-1} : offset);
            const float polar=distance>5.71428571F ? angle-pi/2 : angle-std::atan2(-offset.z,offset.x);
            direction=frame.Direction(std::cos(polar),2.39996323F*static_cast<float>(a.spatialIndex++));
        }
        const float speed=std::sqrt(spatial::Dot(bullet.velocity,bullet.velocity));
        bullet.velocity=spatial::Scale(direction,speed);bullet.baseVelocity=bullet.velocity;bullet.launchDirection=direction;
        bullet.nativeColor=5;bullet.visual=static_cast<VisualId>(a.skills.bulletBase+bullet.nativeType*16+5);++changed;
    });
}
Routine SakuyaBoss::Behaviors::ClockCorpse(Actor& a) {
    co_await PrepareSpell(a,false);
    for(;;) {
        a.soundEvents.push_back(5);co_await a.At(148);a.Animate(149);
        co_await a.At(168);Fire(a,{Formation::RandomAngleSpeed,2,6,128,1,10.7142859F,3.21428585F,pi,-pi,512});
        co_await a.At(218);StopWorld(a,true);Wander(a,6.69642878F,90);co_await FrozenArcEmitter(a,90);a.ElapseLocalFrames(90);
        a.soundEvents.push_back(5);co_await a.At(370);RestorePose(a,false);StopWorld(a,false);
        co_await a.At(400);RestorePose(a,true);a.Rewind(0);
    }
}
Routine SakuyaBoss::Behaviors::LunaClock(Actor& a) {
    co_await PrepareSpell(a,false);
    for(;;) {
        a.soundEvents.push_back(5);co_await a.At(148);a.Animate(148);
        Fire(a,{Formation::FixedRing,2,6,32,4,8.03571415F,3.21428585F,0,0,512});
        co_await a.At(198);StopWorld(a,true);Wander(a,6.69642878F,60);co_await FrozenArcEmitter(a,60);a.ElapseLocalFrames(60);
        a.soundEvents.push_back(5);
        for(int i=0;i<6;++i) {co_await a.At(258+i*5);RedirectStoppedBullets(a);}
        co_await a.At(318);RestorePose(a,false);StopWorld(a,false);
        co_await a.At(378);RestorePose(a,true);co_await a.At(418);a.Rewind(0);
    }
}
Routine SakuyaBoss::Behaviors::Marionette(Actor& a) {
    co_await PrepareSpell(a,true);
    for(;;) {
        co_await LockedKnifeBurst(a,false);a.soundEvents.push_back(5);co_await LockedKnifeBurst(a,true);
        StopWorld(a,true);Wander(a,6.69642878F,60);
        co_await a.At(20);a.soundEvents.push_back(5);
        for(int i=0;i<7;++i) {co_await a.At(20+i*4);RedirectStoppedBullets(a);}
        co_await a.At(78);RestorePose(a,false);StopWorld(a,false);
        co_await a.At(138);RestorePose(a,true);if(Motion(a).position.x>=0)co_await a.At(178);a.Rewind(0);
    }
}
Routine SakuyaBoss::Behaviors::Defeated(Actor& a) {
    StopWorld(a,false);a.SetEmitterPeriod(0);a.damageable=false;a.contactEnabled=false;a.lifeThreshold=-1;a.timeoutFrames=-1;
    Wander(a,1.60714281F,60);
    for(int i=0;i<6;++i) {co_await a.At((i+1)*10);a.soundEvents.push_back(7);}
    a.soundEvents.push_back(18);a.ClearBullets();a.finished=true;
}
Routine SakuyaBoss::Behaviors::SkillComplete(Actor& a) {a.finished=true;co_return;}

}

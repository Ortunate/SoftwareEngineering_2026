// 本文件实现 PatchouliBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/patchouli.hpp"
#include "stages/stage4/behaviors.hpp"
#include "stages/stage4/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage4;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
void Fire(Actor& a,skill::Shot shot) {
    a.skills.projectile.sampleAtStepStart=true;
    if(a.skills.spatial) {
        shot.spatialCount=shot.count*3;
        if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed) {
            shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;
        }
    }
    a.ConfigureEmitter(shot);a.Fire(shot);
}

/// @brief 自动射击的参数在配置时快照；首发按整数初相并在内容帧末递增，停止帧先关闭再判断发射。

movement::State& Motion(Actor& a) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    return a.liftedMotion ? a.liftedMotion->planar : a.motion;
}
void Height(Actor& a) {if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));}
void MoveTo(Actor& a,int frames,Vector3 target) {Motion(a).Interpolate(frames,target,2);Height(a);}
void Wander(Actor& a,float speed,int frames) {
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=speed});Motion(a).Decelerate(frames);Height(a);
}

int Period(const Actor& a,int frames) {return frames+frames/5-(frames/5*2*a.skills.rank)/32;}
void ConfigureEmitter(Actor& a,skill::Shot shot) {
    a.skills.projectile.sampleAtStepStart=true;
    if(a.skills.spatial)shot.spatialCount=shot.count*3;
    a.ConfigureEmitter(shot);
}

}

namespace {
void Initialize(Actor& a) {
    a.SetPoses({65,69,70,67,68});a.deathEffect=2;
    Motion(a).bounds={{-7.14285714F,3.57142857F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
}
Vector3 LaserDirection(const Actor& a,int slot,float angle) {
    if(!a.skills.spatial)return {std::cos(angle),0,-std::sin(angle)};
    // 四条光束直接分配四个等面积球带；第二组独立反向旋转，不复制某个平面形成厚片。
    return spatial::Frame::FromAxis({0,0,-1}).Direction(-0.75F+0.5F*(slot%4),angle);
}
/// @brief 每个难度独立定义激光的内容帧窗口；两组光束的引用和寿命不能随子行为返回重置。
struct NonspellLasers {
    int cycle,rotationEnd,secondStart,grow,firstSustain,secondSustain,fade,emitPeriod;
    float angularStep;
};
constexpr std::array<NonspellLasers,4> kNonspellLasers{{
    {180,120,60,30,70,90,20,60,0.00604152447F},
    {180,120,60,30,70,90,20,50,0.00682954909F},
    {150,100,50,25,58,75,16,40,0.00819545891F},
    {140,93,46,23,54,70,15,40,0.00878084917F},
}};
void BeginLaser(Actor& a,int slot,float angle,bool second,bool red,const NonspellLasers& recipe) {
    a.FireBeam(slot,skill::Laser{.color=red ? 2 : 6,.length=19.4642849F,.width=1.07142854F,
        .grow=float(recipe.grow),.sustain=float(second ? recipe.secondSustain : recipe.firstSustain),
        .fade=float(recipe.fade),.warning=float(recipe.grow),.harmlessTail=14,
        .aimed=false,.startOffset=2.85714293F,.direction=LaserDirection(a,slot,angle),.animationScript=1});
}
void RotateLaser(Actor& a,int slot,float angle) {
    if(auto* beam=a.Beam(slot)) {
        beam->beamDirection=LaserDirection(a,slot,angle);
        beam->position={beam->beamAnchor.x+beam->beamDirection.x*beam->beamStartOffset,
            beam->beamAnchor.y+beam->beamDirection.y*beam->beamStartOffset,
            beam->beamAnchor.z+beam->beamDirection.z*beam->beamStartOffset};
    }
}
/// @brief 新典 121(12) 原位旋转的第二分量读取已改写的第一分量，必须保留此求值顺序。
/// 中性 X/Z 不把它“修正”为标准旋转；空间适配仅将该异常半径沿对应光束方向分配。
void EmitFromLasers(Actor& a,const std::array<float,8>& angles) {
    for(int slot=0;slot<8;++slot)if(const auto* beam=a.Beam(slot)) {
        const float x=2.85714293F*std::cos(angles[slot]),z=x*std::sin(angles[slot]);
        Vector3 offset{x,0,z};
        if(a.skills.spatial) {const float length=std::hypot(x,z);offset={beam->beamDirection.x*length,beam->beamDirection.y*length,beam->beamDirection.z*length};}
        const auto p=a.Position();a.FireEmitterAt({p.x+offset.x,p.y+offset.y,p.z+offset.z});
    }
}
Routine LaserCycle(Actor& a,bool reverse,bool red,int cycle) {
    const int d=native_combat::DifficultyIndex(a.difficulty);const auto& recipe=kNonspellLasers[d];
    constexpr int ringCounts[]{8,10,14,10};constexpr float ringSpeeds[]{4.01785707F,5.35714293F,6.4285717F,8.03571415F};
    const bool fan=reverse&&d>=2;
    ConfigureEmitter(a,{fan?Formation::AimedFan:Formation::AimedRing,3,2,reverse?(fan?2:1):ringCounts[d],
        reverse?2:d==3?2:1,reverse?(d==3?6.69642878F:5.35714293F):ringSpeeds[d],3.21428585F,
        0,fan?0.0981747732F:0,4});
    a.ClearBeamSlots();std::array<float,8> angles{pi/4,3*pi/4,-3*pi/4,-pi/4,pi/4,3*pi/4,-3*pi/4,-pi/4};
    for(int slot=0;slot<4;++slot)BeginLaser(a,slot,angles[slot],false,red,recipe);a.soundEvents.push_back(16);
    const float step=reverse ? -recipe.angularStep : recipe.angularStep;
    for(int frame=0;frame<recipe.cycle;++frame) {
        co_await a.At(frame);
        if(frame<recipe.rotationEnd)for(int slot=0;slot<4;++slot) {angles[slot]+=step;RotateLaser(a,slot,angles[slot]);}
        if(frame==recipe.secondStart) {for(int slot=4;slot<8;++slot)BeginLaser(a,slot,angles[slot],true,red,recipe);a.soundEvents.push_back(16);}
        if(frame>=recipe.secondStart)for(int slot=4;slot<8;++slot) {angles[slot]-=step;RotateLaser(a,slot,angles[slot]);}
        if(cycle!=0&&frame%recipe.emitPeriod==0)EmitFromLasers(a,angles);
    }
    co_await a.At(recipe.cycle);
}
Routine Maneuver(Actor& a,bool second,int cycle) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int firstCounts[]{12,16,24,24},firstLayers[]{3,4,4,4},secondLayers[]{2,3,4,5};
    constexpr int fastPeriods[]{20,15,15,10},slowPeriods[]{70,60,60,60},slowCounts[]{6,12,16,20};
    Wander(a,6.69642878F,90);
    ConfigureEmitter(a,{Formation::AimedRing,3,second ? 6 : 2,cycle+(second ? 10 : firstCounts[d]),second ? secondLayers[d] : firstLayers[d],
        second ? (d>=2?14.7321434F:10.7142859F) : (d==0?6.69642878F:9.375F),second ? 4.01785707F : 3.21428585F,0,0,4});
    a.SetEmitterSound(7);a.SetEmitterPeriod(Period(a,second ? fastPeriods[d] : 40));
    co_await a.At(90);MoveTo(a,90,{0,0,4.28571415F});
    co_await a.At(180);
    a.SetEmitterPeriod(second ? Period(a,slowPeriods[d]) : 0);
    if(second)ConfigureEmitter(a,{Formation::AimedRing,3,6,slowCounts[d],1,d==0?2.67857143F:4.01785707F,2.67857143F,0,0,4});
}
Routine Nonspell(Actor& a,bool second) {
    Initialize(a);a.SetEmitterPeriod(0);a.damageable=true;a.interactable=true;a.contactEnabled=true;a.skills.offset={0,0,0.535714269F};
    if(second) {a.ClearBullets();a.defeatActors=true;a.dropEvents.push_back(8);}
    if(!second) {
        // 五条轨道在首次非符的原时钟安装；不得用额外子行为等待推迟第40帧姿态和第100帧激光。
        constexpr std::array<Color,5> colors{Color{224,224,255,255},Color{255,224,224,255},
            Color{224,255,224,255},Color{255,255,224,255},Color{255,224,224,255}};
        for(int i=0;i<5;++i) {
            co_await a.At(i*10);
            a.AttachEffect({std::array{-.3F,-1.0F,-.3F,.4F,1.1F}[i],.3F,.7F},2.14285714F,colors[i]);
        }
    }
    for(int cycle=0;;++cycle) {
        co_await a.At(second ? 120 : 40);a.Animate(66);
        // 跳过其他难度分支时源脚本同时前移局部时钟，故四难度的实际等待都为 60 帧。
        co_await a.At(second ? 180 : 100);co_await LaserCycle(a,cycle%2==0,second,cycle);
        co_await Maneuver(a,second,cycle);a.Rewind(second ? 120 : 40);
    }
}
}
void PatchouliBoss::Behaviors::ConfigurePractice(Actor& a,bool spatial) {
    ConfigureBehavior(a,spatial);Initialize(a);
}
Routine PatchouliBoss::Behaviors::Entrance(Actor& a) {
    Initialize(a);a.damageable=false;a.contactEnabled=false;a.skills.offset={0,0,0.535714269F};
    // 入场从战斗边界外开始，源边界在插值完成后才启用，不能提前夹断左上方入场路线。
    Motion(a).clamped=false;MoveTo(a,60,{0,0,4.28571415F});co_await a.At(60);Motion(a).clamped=true;
    co_await a.At(1000000);
}
Routine PatchouliBoss::Behaviors::FirstNonspell(Actor& a) {co_await Nonspell(a,false);}
Routine PatchouliBoss::Behaviors::SecondNonspell(Actor& a) {co_await Nonspell(a,true);}
namespace {
Routine PrepareSpell(Actor& a) {
    a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.SetEmitterSound(-1);
    a.damageable=false;a.skills.offset={};MoveTo(a,120,{0,0,6.42857143F});
    co_await a.At(120);a.damageable=true;a.Animate(66);a.SetPoses({66,66,66,67,68});
}
/// @brief 火属性的三种攻击复用轮次结构；kind=0/1/2 为普通/上级/Radiance，不代替难度身份。
Routine FireRings(Actor& a,int kind) {
    const int d=native_combat::DifficultyIndex(a.difficulty);const bool advanced=kind==1,radiance=kind==2;
    for(int cycle=0;;++cycle) {
        const int count=radiance?cycle/5+7:cycle/3+(advanced ? (d>=2?d+7:8) : d==0?7:11);
        for(int burst=0;burst<(radiance?8:5);++burst) {
            co_await a.At(burst*8);
            for(int ring=0;ring<(advanced||radiance ? 2 : 3);++ring) {
                a.skills.projectile={.angularVelocity=ring==1 ? -1.47262156F : 1.47262156F,.angularFrames=128};
                const float speed=radiance?(d==3?5.35714293F:4.01785707F):advanced?(d==3?6.69642878F:5.35714293F):ring==0?5.89285707F:ring==1?4.01785707F:1.875F;
                Fire(a,{Formation::FixedRing,7,0,count,advanced||radiance ? 2 : 1,speed,
                    radiance?(d==3?2.67857143F:1.33928571F):advanced?3.21428585F:1.875F,a.Random(2*pi)-pi,0.130899698F,32});
            }
            if(radiance)Fire(a,{Formation::RandomAngleSpeed,9,0,d==3?6:2,1,d==3?10.7142859F:8.03571415F,1.875F,pi,-pi,0});
            a.soundEvents.push_back(9);
        }
        // 第五/八次计数在最后一发之后再等 8 帧，随后源脚本才到 128；实际轮长为 170/194 帧。
        const int moveAt=radiance?184:160;
        co_await a.At(moveAt);Wander(a,4.01785707F,90);co_await a.At(moveAt+10);a.Rewind(0);
    }
}
/// @brief 左右边缘交替读随机位置；奇数迭代的源条件跳转先将局部时钟推进两帧，因而两发同刻到期。
Routine LeafRain(Actor& a,bool advanced,bool forest) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    const int halfPeriod=forest ? 11 : 5;
    const Vector3 acceleration=advanced ? Vector3{1.36370504F,0,-1.36370504F} : Vector3{-1.36370504F,0,-1.36370504F};
    for(int iteration=0;;++iteration) {
        const int top=iteration/2*(halfPeriod*2)+(iteration%2 ? halfPeriod+1 : 0);
        co_await a.At(top);
        if(iteration%(forest ? 4 : 6)==0) {
            a.skills.offset={};a.skills.projectile={.accelerationVector=acceleration};
            Fire(a,{Formation::RandomAngleSpeed,forest ? 7 : 5,forest ? 0 : advanced ? 13 : 11,
                forest ? (d==0?12:d==2?32:22) : advanced ? 15 : d==0?4:10,1,forest&&d>=2?6.69642878F:5.35714293F,0.803571429F,pi,-pi,19});
            // 高难上级木符保留两次独立配方发射；合并数量会改变三维采样分组和后续随机读取约定。
            if(advanced&&!forest&&d>=2)Fire(a,{Formation::RandomAngleSpeed,5,13,d==2?20:12,1,5.35714293F,0.803571429F,pi,-pi,19});
            a.soundEvents.push_back(26);
        }
        a.skills.projectile={.accelerationVector=acceleration,.accelerationFrames=(advanced||forest) ? 60.0F : -1.0F};
        auto p=a.Position();a.skills.offset={a.Random(17.1428571F)-8.57142857F-p.x,0,8.57142857F-p.z};
        const float first=advanced ? 1.06465089F : 2.63544726F,last=advanced ? 0.506145477F : 2.07694173F;
        Fire(a,{Formation::RandomAngleSpeed,5,forest ? 2 : 10,1,1,3.21428585F,0.803571429F,first,last,19});
        co_await a.At(top+(iteration%2==0 ? 2 : 0));p=a.Position();
        const float z=10-a.Random(8.57142857F)-(iteration%2 ? 8.57142857F : 0);
        a.skills.offset={(forest ? 8.39285714F : advanced ? -9.10714286F : 9.10714286F)-p.x,0,z-p.z};
        Fire(a,{Formation::RandomAngleSpeed,5,forest ? 2 : 11,(advanced||forest)&&d==3?2:1,1,
            forest&&d>=2?4.28571415F:advanced&&!forest&&d==3?4.01785707F:3.21428585F,0.803571429F,first,last,19});
    }
}
Routine StormLeaves(Actor& a) {
    const int d=native_combat::DifficultyIndex(a.difficulty),period=d==2?8:5;
    for(int iteration=0;;++iteration) {
        co_await a.At(iteration*5);
        if(iteration%period==0) {
            a.skills.offset={};a.skills.projectile={.accelerationVector=Vector3{0,0,-1.28571435F}};
            Fire(a,{Formation::RandomAngleSpeed,5,11,30,1,5.35714293F,0.803571429F,pi,-pi,19});a.soundEvents.push_back(26);
        }
        const bool left=iteration%2!=0;const auto p=a.Position();
        a.skills.offset={(left?-9.10714286F:9.10714286F)-p.x,0,10-a.Random(17.1428571F)-p.z};
        a.skills.projectile={.accelerationVector=Vector3{left?0.909136713F:-0.909136713F,0,-0.909136713F}};
        Fire(a,{Formation::RandomAngleSpeed,5,10,1,1,d==2?3.75F:5.35714293F,0.803571429F,
            left?1.06465089F:2.63544726F,left?0.506145477F:2.07694173F,19});
    }
}
/// @brief 三种土符共享延迟转向；kind=0/1/2 为普通/上级/Shake，后者将玩家与 Boss 等高时也计入追加条件。
Routine TurningOrbs(Actor& a,int kind) {
    const int d=native_combat::DifficultyIndex(a.difficulty);const bool advanced=kind==1,shake=kind==2;
    for(int cycle=0;;++cycle) {
        a.skills.projectile={.delayFrames=shake?90.0F:advanced?50.0F:180.0F,.turnAngle=a.Random(pi)-pi/2,
            .turnSpeed=shake?4.8214283F:advanced?5.89285707F:3.75F,.turnCount=advanced?2:1};
        const int count=shake?(d==2?12:18):advanced?(d==1?10:d==2?12:15):d==0?7:10;
        Fire(a,{Formation::RandomAngleSpeed,1,advanced||shake?12:13,count,1,advanced?8.83928585F:6.4285717F,
            advanced?6.16071415F:2.67857143F,pi,0,67});a.soundEvents.push_back(22);
        // 源普通/上级仅在玩家 Y 更小时追加；Shake 包含相等边界，不能统一成同一比较。
        const bool additional=shake?a.PlayerPosition().z>=a.Position().z:a.PlayerPosition().z>a.Position().z;
        if(additional)Fire(a,{Formation::AimedFan,3,6,7,2,7.5F,3.21428585F,0,0.34906584F,4});
        if(shake&&cycle%3==0)Fire(a,{Formation::AimedRing,9,3,7,1,d==2?6.4285717F:9.1071434F,
            3.21428585F,0,d==2?0.448798954F:0.392699093F,4});
        co_await a.At(shake?15:advanced?9:12);a.Rewind(0);
    }
}
}
Routine PatchouliBoss::Behaviors::AgniShine(Actor& a) {co_await PrepareSpell(a);co_await FireRings(a,0);}
Routine PatchouliBoss::Behaviors::AgniShineAdvanced(Actor& a) {co_await PrepareSpell(a);co_await FireRings(a,1);}
Routine PatchouliBoss::Behaviors::AgniRadiance(Actor& a) {co_await PrepareSpell(a);co_await FireRings(a,2);}
Routine PatchouliBoss::Behaviors::SylphyHorn(Actor& a) {co_await PrepareSpell(a);co_await LeafRain(a,false,false);}
Routine PatchouliBoss::Behaviors::SylphyHornAdvanced(Actor& a) {co_await PrepareSpell(a);co_await LeafRain(a,true,false);}
Routine PatchouliBoss::Behaviors::GreenStorm(Actor& a) {co_await PrepareSpell(a);co_await StormLeaves(a);}
Routine PatchouliBoss::Behaviors::ForestBlaze(Actor& a) {co_await PrepareSpell(a);co_await LeafRain(a,false,true);}
Routine PatchouliBoss::Behaviors::LazyTrilithon(Actor& a) {co_await PrepareSpell(a);co_await TurningOrbs(a,0);}
Routine PatchouliBoss::Behaviors::LazyTrilithonAdvanced(Actor& a) {co_await PrepareSpell(a);co_await TurningOrbs(a,1);}
Routine PatchouliBoss::Behaviors::TrilithonShake(Actor& a) {co_await PrepareSpell(a);co_await TurningOrbs(a,2);}
namespace {
/// @brief 水符共用十二轮激光和八轮扇弹；普通版仅 Normal 追加递增扇弹并读取扇轴随机数。
Routine WaterCurtain(Actor& a,bool lake) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);const int d=native_combat::DifficultyIndex(a.difficulty);
    for(int cycle=0;;++cycle) {
        ConfigureEmitter(a,{.formation=lake?Formation::AimedRing:Formation::AimedFan,.type=3,.color=6,
            .count=lake?(d==2?24:30):16,.layers=2,.firstSpeed=lake?8.03571415F:9.375F,
            .lastSpeed=lake?4.8214283F:3.21428585F,.angle=lake?0:pi,.spacing=0.34906584F,.flags=4,.aimAxisOffset=lake?0:pi});
        a.SetEmitterSound(7);a.SetEmitterPeriod(Period(a,22));float spread=pi/8;
        for(int burst=0;burst<12;++burst) {
            co_await a.At(120+burst*10);
            const auto origin=a.Position(),player=a.PlayerPosition();
            const auto frame=spatial::Frame::FromAxis({player.x-origin.x,player.y-origin.y,player.z-origin.z});
            for(int ray=lake?1:0;ray<3;++ray) {
                skill::Laser laser{.color=6,.length=lake?28.5714283F:4.28571415F,.width=lake?0.357142866F:0.267857134F,
                    .grow=lake?20.0F:0.0F,.sustain=lake?50.0F:9999.0F,.fade=lake?10.0F:30.0F,
                    .warning=lake?20.0F:0.0F,.harmlessTail=lake?10.0F:30.0F,
                    .angle=ray==0?0:ray==1?spread:-spread,.speed=lake?0:10.7142859F,.initialLength=lake?-1.0F:0.0F};
                // 中心束始终瞄准；侧束直接采样球冠，跨轮改变方位而不消耗额外源随机数。
                if(a.skills.spatial)laser.direction=ray==0 ? frame.forward : frame.Direction(std::cos(spread),
                    2*pi*spatial::RadicalInverse(static_cast<unsigned>(cycle*12+burst+1),2)+(ray==2 ? pi : 0));
                a.Fire(laser);
            }
            spread-=0.0261799395F;a.soundEvents.push_back(17);
        }
        co_await a.At(240);a.SetEmitterPeriod(0);Wander(a,4.01785707F,90);float angle=0;
        for(int burst=0;burst<8;++burst) {
            co_await a.At(240+burst*10);
            Fire(a,{Formation::AimedFan,lake?9:6,lake?1:3,10,1,lake?(d==2?9.375F:10.1785717F):6.69642878F,
                1.875F,angle,lake?(d==2?0.392699093F:0.314159274F):0.224399477F,0});
            if(!lake&&d==1)Fire(a,{Formation::AimedFan,3,6,cycle+1,1,1.875F,1.875F,angle,0.34906584F,0});
            if(lake||d==1)angle-=a.Random(lake?0.17453292F:0.224399477F)-(lake?0.08726646F:0.112199739F);
            a.soundEvents.push_back(9);
        }
        co_await a.At(320);Wander(a,4.01785707F,90);co_await a.At(370);a.Rewind(120);
    }
}
}
Routine PatchouliBoss::Behaviors::PrincessUndine(Actor& a) {co_await WaterCurtain(a,false);}
Routine PatchouliBoss::Behaviors::BuryInLake(Actor& a) {co_await WaterCurtain(a,true);}
Routine PatchouliBoss::Behaviors::MetalFatigue(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);
    for(;;) {
        const float angle=a.Random(2*pi)-pi;float turn=a.Random(2*pi)-pi;
        for(int burst=0;burst<8;++burst) {
            a.skills.projectile={.delayFrames=60,.turnAngle=turn,.turnSpeed=5.35714293F};
            Fire(a,{Formation::FixedRing,6,6,8,1,10.7142859F,2.67857143F,angle,0,257});turn+=pi/4;
        }
        a.soundEvents.push_back(8);co_await a.At(160);a.Rewind(120);
    }
}
Routine PatchouliBoss::Behaviors::SilverDragon(Actor& a) {
    co_await PrepareSpell(a);const int d=native_combat::DifficultyIndex(a.difficulty);
    for(;;) {
        const float speed=a.Random(6.69642878F)+2.67857143F;
        const auto origin=a.Position(),player=a.PlayerPosition();
        // 绝对转向在发射时采样平面瞄准角；三维弹体沿通用契约保留各自发射高度带。
        a.skills.projectile={.delayFrames=60,.turnAngle=std::atan2(origin.z-player.z,player.x-origin.x),.turnSpeed=speed};
        Fire(a,{Formation::FixedRing,3,15,d==2?8:10,3,16.0714283F,1.33928571F,a.Random(2*pi)-pi,0,257});
        a.soundEvents.push_back(8);co_await a.At(50);a.Rewind(0);
    }
}
Routine PatchouliBoss::Behaviors::LavaCromlech(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);const int d=native_combat::DifficultyIndex(a.difficulty);
    for(int cycle=0;;) {
        for(int burst=0;burst<8;++burst) {
            co_await a.At(120+burst*8);Fire(a,{Formation::FixedRing,7,0,d==0?8:d==1?12:d==2?16:20,2,6.69642878F,1.875F,a.Random(2*pi)-pi,0.130899698F,0});a.soundEvents.push_back(9);
        }
        co_await a.At(244);Wander(a,4.01785707F,90);++cycle;
        for(int burst=0;burst<3;++burst) {
            co_await a.At(244+burst*10);a.skills.projectile={.delayFrames=130,.turnAngle=a.Random(pi)-pi/2,.turnSpeed=d==2?4.28571415F:d==3?5.35714293F:3.75F};
            Fire(a,{Formation::RandomAngleSpeed,1,13,cycle*2+(d==0?4:d==1?10:d==2?16:24),1,6.4285717F,2.67857143F,pi,0,67});a.soundEvents.push_back(22);
        }
        co_await a.At(274);a.Rewind(120);
    }
}
Routine PatchouliBoss::Behaviors::WaterElf(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);a.SetEmitterSound(7);
    const int d=native_combat::DifficultyIndex(a.difficulty),first=d==0?18:d==1?24:d==2?30:32,second=d==0?16:d==3?24:20;
    // 准备行为先开启周期，再由首轮两次显式发射覆盖配方；不能在每次直接发射时重置周期计数。
    ConfigureEmitter(a,{Formation::AimedRing,3,8,first,2,7.5F,3.21428585F,pi,0.34906584F,4});a.SetEmitterPeriod(Period(a,20));
    float angle=0;
    for(int cycle=0;;++cycle) {
        Fire(a,{Formation::AimedRing,3,8,first,2,7.5F,3.21428585F,pi,0.34906584F,4});
        Fire(a,{Formation::AimedFan,d>=2?9:6,3,d==0?8:10,1,4.8214283F,1.875F,angle,0.34906584F,0});angle-=a.Random(0.224399477F)-0.112199739F;
        co_await a.At(140);ConfigureEmitter(a,{Formation::AimedRing,3,10,second,2,7.5F,3.21428585F,pi,0.34906584F,4});
        a.SetEmitterSound(7);a.SetEmitterPeriod(Period(a,20));Wander(a,4.01785707F,90);if((cycle+1)%8==0)angle=0;
        co_await a.At(190);a.Rewind(120);
    }
}
Routine PatchouliBoss::Behaviors::MercuryPoison(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);const int d=native_combat::DifficultyIndex(a.difficulty);
    for(int cycle=0;;++cycle) {
        for(int pair=0;pair<8;++pair) {
            co_await a.At(120+pair*40);const float angle=a.Random(2*pi)-pi;
            // 每对正反旋转共用一次随机角；第二轮必须保存首轮的角度，而非再次读取随机数。
            for(int reverse=0;reverse<2;++reverse) {
                co_await a.At(120+pair*40+reverse*20);
                a.skills.projectile={.angularVelocity=reverse?-0.7363108F:0.7363108F,.angularFrames=90,.speedAcceleration=0.48214286F};
                Fire(a,{Formation::AimedRing,3,reverse?8:14,cycle+(d==0?8:d==1?15:d==2?20:28),2,4.01785707F,2.14285707F,
                    angle,reverse?-0.34906584F:0.34906584F,37});a.soundEvents.push_back(7);
            }
        }
        co_await a.At(440);Wander(a,4.01785707F,90);co_await a.At(490);a.Rewind(120);
    }
}
Routine PatchouliBoss::Behaviors::EmeraldMegalith(Actor& a) {
    co_await PrepareSpell(a);a.ElapseLocalFrames(120);const int d=native_combat::DifficultyIndex(a.difficulty);
    for(int cycle=0;;++cycle) {
        a.skills.offset={a.Random(12.8571429F)-6.42857143F,0,-(a.Random(5.71428571F)-2.85714286F)};
        Fire(a,{d==3?Formation::FixedFan:Formation::FixedRing,9,2,d==3?3:1,1,a.Random(5.35714293F)+4.28571415F,2.14285707F,pi/2,d==3?0.52359879F:0,4});a.soundEvents.push_back(7);
        if(cycle%18==7)Wander(a,4.01785707F,90);
        co_await a.At(121);a.skills.projectile={.delayFrames=180,.turnAngle=a.Random(pi)-pi/2,.turnSpeed=4.5535717F};
        Fire(a,{Formation::RandomAngleSpeed,1,11,d==0?4:d==1?12:d==2?18:24,1,d>=2?6.4285717F:4.8214283F,2.67857143F,pi,-pi,2});a.soundEvents.push_back(22);
        co_await a.At(140);a.Rewind(120);
    }
}
Routine PatchouliBoss::Behaviors::CombinationEntrance(Actor& a) {
    Initialize(a);a.SetEmitterPeriod(0);a.ClearBullets();a.defeatActors=true;a.dropEvents.push_back(8);a.SetEmitterSound(-1);
    a.damageable=false;a.skills.offset={0,0,0.535714269F};co_await a.At(120);
    // 组合符卡包装层在等待后重置阶段总时钟；自身的 120 帧准备期另计，不能合并两段等待。
    a.phaseFrames=0;
}
Routine PatchouliBoss::Behaviors::Combination(Actor& a,behavior::Entry entry,int timeout,bool transitionFlash) {
    if(transitionFlash) {a.EmitEffect(effects::Kind::Flash,1,{128,128,255,255});a.soundEvents.push_back(7);}
    co_await CombinationEntrance(a);a.timeoutFrames=timeout;co_await entry(a);
}
Routine PatchouliBoss::Behaviors::SkillComplete(Actor&) {co_return;}
Routine PatchouliBoss::Behaviors::Defeated(Actor& a) {
    a.SetEmitterPeriod(0);a.damageable=false;a.contactEnabled=false;a.interactable=false;
    Wander(a,1.60714293F,60);
    constexpr std::array<Color,5> colors{WHITE,Color{255,128,128,255},Color{128,255,128,255},
        Color{128,128,255,255},Color{255,255,128,255}};
    // 清弹发生在第六轮末，与最后一次爆散及两次音效同一步；退场期间保留既有弹幕。
    for(int i=0;i<6;++i) {
        for(int n=0;n<5;++n) {co_await a.At(i*10+(n+1)*2);a.EmitEffect(effects::Kind::Burst,2,colors[n]);}
        a.soundEvents.push_back(7);
    }
    a.soundEvents.push_back(18);a.ClearBullets();a.finished=true;
}

}

// 本文件以命名协程实现Extra普通演员，不解释源操作码或转换源坐标。
#include "stages/stage_extra/behaviors.hpp"
#include "stages/common/ids.hpp"
#include <algorithm>
#include <numbers>

namespace koumakyou::stage_extra {
namespace {
using behavior::Actor;
using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;

Routine FinishAt(Actor& actor,int frame) {
    co_await actor.At(std::max(0,frame-static_cast<int>(actor.phaseFrames)));
    actor.finished=true;
}

Routine Curve(Actor& actor,int count,float spacing,int basePeriod) {
    actor.skills.projectile.sampleAtStepStart=true;
    skill::Shot shot{skill::Formation::AimedFan,0,6,count,1,10.7142859F,0,0,spacing,4};
    if(actor.skills.spatial)shot.spatialCount=count*3;
    actor.ConfigureEmitter(shot);
    const int period=basePeriod+basePeriod/5-(basePeriod/5*2*actor.skills.rank)/32;
    actor.SetEmitterPeriod(period,static_cast<int>(actor.RandomIndex(period)));
    co_await actor.At(40);actor.motion.Apply({.angularVelocity=-1.47262156F});
    co_await actor.At(120);actor.motion.Apply({.angularVelocity=1.17809725F});
    co_await actor.At(220);actor.motion.Apply({.angularVelocity=0});
    // 源周期发射在转弯结束后仍继续，场外回收由Spawn边界决定；不能在220帧顺带停火。
    co_await FinishAt(actor,10000);
}

Routine FanTurn(Actor& actor,int variant) {
    actor.skills.offset={0.535714269F,0,0.535714269F};
    actor.skills.projectile.sampleAtStepStart=true;
    co_await actor.At(60);actor.Animate(5);actor.motion.Apply({.speed=0});
    co_await actor.At(70);
    skill::Shot shot{skill::Formation::AimedFan,1,2,11,3,10.7142859F,8.03571415F,0,0.448798954F,3};
    if(variant==1)shot={skill::Formation::AimedFan,1,2,3,4,16.0714283F,5.35714286F,0,0.0654498488F,3};
    if(variant==2)shot={skill::Formation::AimedFan,3,2,7,4,13.3928576F,2.67857143F,0,0.0654498488F,3};
    if(actor.skills.spatial)shot.spatialCount=shot.count*3;
    actor.Fire(shot);
    if(variant!=0) {
        --shot.count;--shot.layers;
        shot.firstSpeed=variant==1 ? 17.6785717F : 12.3214283F;
        shot.lastSpeed=variant==1 ? 5.625F : 2.94642854F;
        if(actor.skills.spatial)shot.spatialCount=shot.count*3;
        actor.Fire(shot);
    }
    actor.soundEvents.push_back(7);
    co_await actor.At(130);
    actor.motion.Apply({.angularVelocity=variant==0 ? pi : 0.589048624F,.acceleration=8.03571415F});
    co_await actor.At(190);actor.motion.Apply({.angularVelocity=0});
    if(variant!=0)actor.motion.Apply({.acceleration=0});
    co_await FinishAt(actor,variant==0 ? 1000 : 10000);
}

Routine SpiralRings(Actor& actor,int variant) {
    actor.skills.projectile.sampleAtStepStart=true;
    actor.damageable=false;
    co_await actor.At(40);actor.motion.Apply({.acceleration=-10.7142859F});
    co_await actor.At(70);actor.motion.Apply({.acceleration=0});actor.SetEmitterSound(22);
    if(variant!=3)actor.damageable=true;
    int frame=70,count=4;float angle=0;
    const int pairs=variant==0 ? 1 : variant==3 ? 6 : 2;
    for(int pair=0;pair<pairs;++pair) {
        // 扩张款外层回跳跳过初始随机角，沿用上一串末角；蓝/红配对款每轮重取角度。
        if(pair==0||variant!=3)angle=actor.Random(6.28318548F)-pi;
        float speed=1.33928571F;
        const int bursts=variant==3 ? 14 : 8,interval=variant==3 ? 3 : 4;
        for(int n=0;n<bursts;++n) {
            co_await actor.At(frame+n*interval);
            skill::Shot shot{skill::Formation::FixedRing,variant==3?2:3,variant==2||variant==3?2:6,
                variant==0?16:variant==3?count+2:13,1,speed,2.67857143F,angle,0,variant==0?516:513};
            if(actor.skills.spatial)shot.spatialCount=shot.count*3;
            actor.Fire(shot);
            angle+=variant==0 ? 0.0392699093F : variant==1 ? 0.0654498488F : variant==2 ? -0.0654498488F : -0.0981747732F;
            speed+=variant==0 ? 0.803571463F : variant==3 ? 0.535714269F : 0.482142866F;
        }
        frame+=bursts*interval;co_await actor.At(frame);
        angle=actor.Random(6.28318548F)-pi;speed=variant==3 ? 5.35714286F : 1.33928571F;
        for(int n=0;n<8;++n) {
            co_await actor.At(frame+n*4);
            skill::Shot shot{skill::Formation::FixedRing,variant==3?2:3,variant==2?2:6,
                variant==0?16:variant==3?count:13,1,variant==0?10.7142859F:speed,2.67857143F,angle,0,variant==0?516:513};
            if(actor.skills.spatial)shot.spatialCount=shot.count*3;
            actor.Fire(shot);
            angle+=variant==0 ? -0.0392699093F : variant==1 ? -0.0654498488F : variant==2 ? 0.0654498488F : 0.0314159282F;
            speed+=variant==0 ? 0.803571463F : variant==3 ? 0.401785731F : 0.482142866F;
        }
        frame+=32;co_await actor.At(frame);
        // 外层回跳位于第二串结束后4帧；最后一次也保留这段等待，不提前恢复移动/受伤。
        if(variant!=0) {frame+=4;co_await actor.At(frame);}
        if(variant==3)count+=3;
    }
    actor.damageable=true;actor.motion.Polar(pi/2,2.67857143F);
    co_await FinishAt(actor,10000);
}

Routine StationaryEmitter(Actor& actor,int variant) {
    actor.AnimateAuxiliary(0,16);
    actor.skills.projectile.sampleAtStepStart=true;actor.interactable=false;
    co_await actor.At(30);actor.interactable=true;
    const int rounds=variant==0?8:variant==1?7:variant==4?4:6;
    const int interval=variant==0?11:variant==1?12:variant==4?30:20;
    if(variant==2)actor.soundEvents.push_back(7);
    for(int n=0;n<rounds;++n) {
        co_await actor.At(30+n*interval);
        // 两种瞄准款仍保留源脚本的废弃随机角读取，不能因配方不使用它而跳过。
        const float angle=actor.Random(6.28318548F)-pi;
        skill::Shot shot{skill::Formation::FixedRing,0,2,16,1,3.21428585F,2.67857143F,angle,0,3};
        if(variant==1)shot={skill::Formation::RandomAngleSpeed,0,2,20,1,4.8214283F,1.60714293F,pi,-pi,3};
        if(variant==2)shot={skill::Formation::AimedFan,0,6,9,1,4.28571463F,1.60714293F,0,0.314159274F,3};
        if(variant==3)shot={skill::Formation::AimedFan,0,6,1,4,6.69642878F,2.67857143F,0,0,3};
        if(variant==4)shot={skill::Formation::FixedRing,0,2,10,3,5.89285755F,1.60714293F,angle,0,3};
        if(actor.skills.spatial) {
            shot.spatialCount=shot.count*3;
            if(variant==1) {shot.sampleIndex=static_cast<int>(actor.spatialIndex&0x7fffffffU);actor.spatialIndex+=shot.spatialCount*shot.layers;}
        }
        actor.Fire(shot);actor.soundEvents.push_back(7);
    }
    co_await actor.At(30+rounds*interval);actor.interactable=false;actor.InterruptAnimation(1);actor.InterruptAuxiliary(0,1);
    co_await actor.At(60+rounds*interval);actor.finished=true;
}
}

Routine SingleCurve(Actor& actor) {co_await Curve(actor,1,0,60);}
Routine TightCurve(Actor& actor) {co_await Curve(actor,3,0.196349546F,90);}
Routine WideCurve(Actor& actor) {co_await Curve(actor,3,0.392699093F,70);}
Routine LayeredFanTurn(Actor& actor) {co_await FanTurn(actor,0);}
Routine TwinFanTurn(Actor& actor) {co_await FanTurn(actor,1);}
Routine CrossingFanTurn(Actor& actor) {co_await FanTurn(actor,2);}
Routine BlueSpiralRings(Actor& actor) {co_await SpiralRings(actor,0);}
Routine PairedBlueSpiralRings(Actor& actor) {co_await SpiralRings(actor,1);}
Routine PairedRedSpiralRings(Actor& actor) {co_await SpiralRings(actor,2);}
Routine ExpandingCrystalRings(Actor& actor) {co_await SpiralRings(actor,3);}
Routine DropEight(Actor& actor) {actor.dropEvents.push_back(8);actor.finished=true;co_return;}
Routine RedRingEmitter(Actor& actor) {co_await StationaryEmitter(actor,0);}
Routine RandomRedEmitter(Actor& actor) {co_await StationaryEmitter(actor,1);}
Routine BlueFanEmitter(Actor& actor) {co_await StationaryEmitter(actor,2);}
Routine BlueLayerEmitter(Actor& actor) {co_await StationaryEmitter(actor,3);}
Routine LayeredRedEmitter(Actor& actor) {co_await StationaryEmitter(actor,4);}
Routine EmitterDefeated(Actor& actor) {
    actor.dropEvents.push_back(8);actor.InterruptAnimation(1);actor.InterruptAuxiliary(0,1);co_await actor.At(30);actor.finished=true;
}
void ConfigureBehavior(Actor& actor,bool spatial) {
    actor.skills.bulletBase=stage_assets::kNativeBulletBase;
    actor.skills.laserBase=stage_assets::kNativeLaserBase;
    actor.skills.fallback=stage_assets::kBlueBullet;
    actor.skills.spatial=spatial;
    if(!spatial)actor.liftedMotion.reset();
}
}

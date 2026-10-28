// 本文件实现 RumiaBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/rumia.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage1;
using behavior::Actor;using behavior::Routine;using skill::Formation;

namespace {
using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
void Initialize(Actor& a) { a.poses={128,131,132,129,130};a.deathEffect=2;a.skills.offset={0,0,0.535714269F}; }
/// @brief 所有移动指令作用于原有世界平面编排；真实三维位置仍由 Actor::motion 持有。
movement::State& Motion(Actor& a) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    return a.liftedMotion ? a.liftedMotion->planar : a.motion;
}
/// @brief 每次开始移动时只选择一个保守高度；等路程执行器不会增加速度、距离或等待时间。
void AimMovement(Actor& a) {
    if(!a.liftedMotion)return;
    const float height=kBossHeights.x+(kBossHeights.y-kBossHeights.x)*spatial::RadicalInverse(a.spatialIndex++,2);
    a.liftedMotion->Aim(a.motion,height);
}
/// @brief 沿用原插值目标、时长和缓动，只改变实际移动在 X/Y/Z 中的分配。
void MoveTo(Actor& a,int duration,Vector3 target,int power) {
    target.y=0;Motion(a).Interpolate(duration,target,power);AimMovement(a);
}
/// @brief 保留一次原随机读取、边界反射及 speed×duration/120 的减速距离，不追加路点移动。
void Wander(Actor& a,float speed,int duration) {
    auto& path=Motion(a);
    a.RandomDirection(-pi,pi);path.Apply({.speed=speed});path.Decelerate(duration);AimMovement(a);
}
/// @brief 按原图案规模确定总方向预算；每层直接等面积采样，rank 修正在总预算上应用一次。
void Fire(Actor& a,skill::Shot shot) {
    shot.spatialCount=shot.count>1 ? shot.count*3 : 1;
    if(shot.sampleIndex<0)shot.coneHalfAngle=std::clamp(shot.spacing*std::max(1,shot.count-1)/2,0.25F,1.1F);
    a.Fire(shot);
}
/// @brief 道中加速环：14 向原地起步，角相位由调用方传入；加速度上下文持续保留给后续技能。
void AcceleratingRing(Actor& a,int color,float phase) {
    a.skills.projectile={.acceleration=3.21428585F};
    Fire(a,skill::Shot{Formation::FixedRing,0,color,14,1,0.0F,0.0F,phase,0,21});a.soundEvents.push_back(9);
}
/// @brief 道中两轮随机速度环，四次随机读取顺序固定，不能合并成共用方向或速度。
void RandomRings(Actor& a,int color) {
    for(int type:{0,2}) {
        const float speed=a.Random(8.03571415F)+1.33928573F;
        const float angle=a.Random(6.2831855F)-3.1415927F;
        Fire(a,skill::Shot{Formation::FixedRing,type,color,8,1,speed,1.875F,angle,0,5});
    }
    a.soundEvents.push_back(9);
}
/// @brief 同一循环内逐发改变速度与角度；局部帧由调用方累计，不引入额外子行为时钟。
skill::Shot SweepShot(const Actor& a,float speed,float angle,int color,int count=1,float spacing=0.06544985F,int flags=4) {
    // 逐发覆盖球冠面积，独立序号来自内容阶段时钟，不按显示更新次数推进。
    return {Formation::AimedFan,1,color,count,1,speed,0,angle,spacing,flags,0,0.9F,static_cast<int>(a.phaseFrames/2)};
}
/// @brief 夜符准备与暗符准备共享插值与保护期；前者有宣告动画，后者使用独立 rank 弹数修正。
Routine PrepareSpell(Actor& a,bool demarcation) {
    a.ClearBullets();a.damageable=false;a.phaseFrames=0;
    MoveTo(a,120,{0.0F,0.0F,5.71428585F},2);
    if(demarcation)a.skills.rankInfluence={-2.67857146F,2.67857146F,-3,6,0,0};
    co_await a.At(120);a.damageable=true;
    if(!demarcation)a.Animate(135);
}
/// @brief 两组非符共享随机移动和四种攻击轮换；lasers 选择第二组配方，variant 为 0…3。
/// 每次只抽一次后继分支，递归调用保持原随机序列和父行为冻结的时钟语义。
Routine NonspellCycle(Actor& a,bool lasers,int variant) {
    if(a.finished)co_return;
    Wander(a,8.03571415F,60);
    int next=0;
    if(variant==0) {
        if(lasers) {
            co_await a.At(12);Fire(a,skill::Shot{Formation::AimedFan,1,10,2,8,8.03571415F,2.67857146F,0,0.09817477F,4});a.soundEvents.push_back(8);
            for(int frame=20;frame<=60;frame+=8) { co_await a.At(frame);a.Fire(skill::Laser{});a.soundEvents.push_back(8); }
            for(int frame=124;frame<=164;frame+=8) { co_await a.At(frame);a.soundEvents.push_back(16); }
            next=224;
        } else {
            for(int i=0;i<7;++i) {
                co_await a.At(12+i*8);
                Fire(a,skill::Shot{Formation::AimedFan,3,i%2 ? 1 : 2,1,10,10.7142859F,2.67857146F,0,0.09817477F,4});a.soundEvents.push_back(8);
            }
            next=180;
        }
    } else if(variant==1) {
        if(lasers) {
            for(int i=0;i<3;++i) {
                co_await a.At(60+i*30);
                Fire(a,skill::Shot{Formation::AimedRing,i%2 ? 0 : 2,10,i%2 ? 28 : 36,1,i%2 ? 6.96428585F : 5.35714293F,2.67857146F,0,0,i%2 ? 2 : 4});a.soundEvents.push_back(8);
            }
            next=240;
        } else {
            for(int i=0;i<7;++i) {
                co_await a.At(60+i*8);
                const float speed=i%2 ? 8.03571415F : (i%4==0 ? 10.7142859F : 5.35714293F);
                Fire(a,skill::Shot{Formation::AimedRing,i%2 ? 0 : 1,i%2 ? 5 : 6,i%2 ? 16 : 12,1,speed,2.67857146F,0,0,i%2 ? 2 : 4});a.soundEvents.push_back(8);
            }
            next=228;
        }
    } else if(variant==2) {
        if(lasers) {
            for(int i=0;i<3;++i) {
                co_await a.At(60+i*20);
                Fire(a,skill::Shot{Formation::AimedFan,2,13,8+i,2,8.03571415F,2.67857146F,0,0.2617994F,8});
                if(i==0)a.Animate(136);
                a.soundEvents.push_back(7);
            }
            next=220;
        } else {
            co_await a.At(80);Fire(a,skill::Shot{Formation::AimedFan,2,2,3,16,13.3928576F,2.67857146F,0,0.06544985F,8});a.soundEvents.push_back(7);
            co_await a.At(110);Fire(a,skill::Shot{Formation::AimedRing,1,6,16,1,5.35714293F,2.67857146F,0,0,4});next=200;
        }
    } else {
        // 原分支会把局部帧从 0 跳到 2；两种方向的首发均在真实第 2 帧，之后每 2 帧一次。
        const bool positive=static_cast<int>(std::floor(a.Random(2)))==0;
        float speed=2.67857146F,angle=positive ? (lasers ? 0.7139983F : 0.2617994F) : (lasers ? -0.7139983F : -0.2617994F);
        const float spacing=lasers ? 0.14279966F : 0.06544985F;
        for(int i=0;i<16;++i) {
            co_await a.At(2+i*2);Fire(a,SweepShot(a,speed,angle,10,lasers ? 2 : 1,spacing));a.soundEvents.push_back(8);
            speed+=0.669642866F;angle+=positive ? -spacing : spacing;
        }
        next=152;
    }
    co_await a.At(next);
    const int choice=static_cast<int>(std::floor(a.Random(3)));
    // 后继顺序来自原四条非符子程序；选择后先进入该行为，返回时才走默认分支。
    constexpr int targets[4][3]={{1,2,3},{0,2,3},{1,0,3},{1,2,0}};
    if(choice<2)co_await NonspellCycle(a,lasers,targets[variant][choice]);
    co_await NonspellCycle(a,lasers,targets[variant][2]);
}
/// @brief 四色逐发旋转瞄准流；每轮两组四色，每色 16 发，速度/角度用逐次加法保留浮点相位。
Routine BirdStream(Actor& a) {
    bool repeated=false;
    for(;;) {
        int frame=0;
        for(int group=0;group<2;++group) {
            for(int colorIndex=0;colorIndex<4;++colorIndex) {
                float speed=2.67857146F;
                float angle=colorIndex<2 ? (colorIndex%2 ? 0.57119864F : -0.57119864F) : (colorIndex%2 ? 0.7853982F : -0.7853982F);
                const float increment=colorIndex<2 ? 0.14279966F : 0.19634955F;
                for(int shot=0;shot<16;++shot) {
                    frame+=2;co_await a.At(frame);Fire(a,SweepShot(a,speed,angle,std::array{5,7,6,8}[colorIndex]));
                    // 颜色顺序是 5、7、6、8，保持原作两两对向，而非按色号顺序排列。
                    speed+=0.535714269F;angle+=colorIndex%2 ? -increment : increment;
                    if(colorIndex%2)a.soundEvents.push_back(8);
                }
                if(colorIndex==1&&group==1&&repeated)a.Animate(135);
            }
        }
        co_await a.At(frame+60);a.Animate(136);repeated=true;
        co_await a.At(frame+100);Wander(a,5.35714293F,120);a.Rewind(0);
    }
}
/// @brief 三色错相双环与双向弯曲流共享同一演员的延迟转向参数，切换参数不追溯修改已生成弹。
Routine DarkPattern(Actor& a) {
    for(;;) {
        a.Animate(137);
        for(int pair=0;pair<3;++pair) {
            co_await a.At(pair*60);
            const int color=pair==0 ? 6 : pair==1 ? 10 : 2;
            const float turn=pair==1 ? -1.5707964F : 1.5707964F;
            a.skills.projectile={.delayFrames=40,.turnAngle=turn,.turnSpeed=4.01785707F};
            Fire(a,skill::Shot{Formation::AimedRing,2,color,16,1,8.03571415F,0.0F,0,0,68});
            a.skills.projectile.turnAngle=-turn;
            Fire(a,skill::Shot{Formation::AimedRing,2,color,16,1,8.03571415F,0.0F,0.19634955F,0,68});a.soundEvents.push_back(9);
        }
        a.skills.projectile={.delayFrames=60,.turnSpeed=-2.67857146F};
        co_await a.At(180);Wander(a,5.35714293F,120);a.skills.projectile={.delayFrames=40,.turnSpeed=8.03571415F};
        int frame=180;
        for(int repeat=0;repeat<2;++repeat)for(int direction=0;direction<2;++direction) {
            float speed=2.67857146F,angle=direction ? 0.57119864F : -0.57119864F;
            for(int shot=0;shot<12;++shot) {
                frame+=2;co_await a.At(frame);Fire(a,SweepShot(a,speed,angle,6,1,0.09817477F,132));
                if(direction)a.soundEvents.push_back(8);
                speed+=0.535714269F;angle+=direction ? -0.14279966F : 0.14279966F;
            }
        }
        Wander(a,5.35714293F,60);co_await a.At(frame+60);a.Rewind(0);
    }
}
} // namespace
Routine RumiaBoss::Behaviors::Midboss(Actor& a) {
    a.damageable=false;Initialize(a);a.skills.offset={0,0,0.535714269F};
    a.motion.position=Motion(a).position={0.0F,0.0F,11.4285717F};MoveTo(a,60,{5.71428585F,0.0F,4.28571415F},2);
    co_await a.At(60);a.damageable=true;
    co_await a.At(130);a.soundEvents.push_back(6);
    co_await a.At(160);Fire(a,skill::Shot{Formation::AimedRing,1,6,16,3,5.35714293F,4.01785707F,0,0,9});a.Animate(133);
    co_await a.At(192);a.soundEvents.push_back(8);
    for(int repeat=0;repeat<2;++repeat) {
        co_await a.At(202);MoveTo(a,60,{0.0F,0.0F,7.14285707F},2);
        co_await a.At(262);AcceleratingRing(a,6,0);a.Animate(133);
        co_await a.At(270);AcceleratingRing(a,2,0.06544985F);
        co_await a.At(278);AcceleratingRing(a,10,0.1308997F);
        co_await a.At(286);AcceleratingRing(a,13,0.19634955F);
        co_await a.At(294);AcceleratingRing(a,14,0.2617994F);
        co_await a.At(324);a.Animate(134);
        co_await a.At(384);MoveTo(a,60,{-5.71428585F,0.0F,5.71428585F},2);
        co_await a.At(394);a.soundEvents.push_back(6);
        co_await a.At(414);Fire(a,skill::Shot{Formation::AimedRing,1,10,16,3,5.35714293F,4.01785707F,0,0,9});
        co_await a.At(444);Fire(a,skill::Shot{Formation::AimedRing,1,13,16,3,5.35714293F,4.01785707F,0,0,9});a.soundEvents.push_back(8);
        co_await a.At(476);a.soundEvents.push_back(8);
        co_await a.At(526);MoveTo(a,60,{0.0F,0.0F,6.42857122F},2);
        co_await a.At(586);RandomRings(a,6);a.Animate(133);
        co_await a.At(594);RandomRings(a,2);
        co_await a.At(602);RandomRings(a,10);
        co_await a.At(610);RandomRings(a,13);
        co_await a.At(618);RandomRings(a,14);
        co_await a.At(648);a.Animate(134);
        co_await a.At(708);MoveTo(a,60,{5.71428585F,0.0F,5.71428585F},2);
        co_await a.At(718);a.soundEvents.push_back(6);
        co_await a.At(738);Fire(a,skill::Shot{Formation::AimedRing,1,6,16,3,5.35714293F,4.01785707F,0,0,9});
        co_await a.At(768);Fire(a,skill::Shot{Formation::AimedRing,1,2,16,3,5.35714293F,4.01785707F,0,0,9});a.Animate(133);a.soundEvents.push_back(8);
        co_await a.At(800);a.soundEvents.push_back(8);
        co_await a.At(840);a.Animate(134);
        if(repeat==0)a.Rewind(192);
    }
    co_await a.At(850);MoveTo(a,60,{0.0F,0.0F,12.8571434F},2);
    co_await a.At(910);a.finished=true;
}
Routine RumiaBoss::Behaviors::MidbossDefeated(Actor& a) {
    Motion(a).Apply({.speed=0});a.ClearBullets();a.soundEvents.push_back(18);a.dropEvents.push_back(5);
    Motion(a).clamped=false;a.damageable=false;a.Animate(128);
    co_await a.At(40);MoveTo(a,120,{0.0F,0.0F,12.8571434F},-2);
    co_await a.At(160);a.finished=true;
}
Routine RumiaBoss::Behaviors::MidbossTimeout(Actor& a) {
    Motion(a).Apply({.speed=0});a.ClearBullets();Motion(a).clamped=false;a.Animate(128);
    MoveTo(a,60,{0.0F,0.0F,12.8571434F},-2);co_await a.At(60);a.finished=true;
}
Routine RumiaBoss::Behaviors::FirstNonspell(Actor& a) {
    a.damageable=true;a.deathEffect=2;a.Animate(134);
    Motion(a).bounds={{-7.14285707F,3.5714283F},{7.14285707F,7.85714293F}};
    Motion(a).clamped=true;
    co_await a.At(100);co_await NonspellCycle(a,false,0);
    co_await a.At(3100);a.finished=true;
}
Routine RumiaBoss::Behaviors::NightBird(Actor& a) { co_await PrepareSpell(a,false);co_await BirdStream(a); }
Routine RumiaBoss::Behaviors::SecondNonspell(Actor& a) {
    a.damageable=false;a.ClearBullets();a.deathEffect=2;a.Animate(134);
    a.dropEvents.push_back(5);
    co_await a.At(200);a.damageable=true;co_await NonspellCycle(a,true,0);
    co_await a.At(3200);a.finished=true;
}
Routine RumiaBoss::Behaviors::Demarcation(Actor& a) { co_await PrepareSpell(a,true);co_await DarkPattern(a); }
Routine RumiaBoss::Behaviors::Defeated(Actor& a) {
    Motion(a).Apply({.speed=0});a.damageable=false;Wander(a,1.60714293F,60);a.timeoutFrames=-1;a.lifeThreshold=-1;
    // 六次颜色爆散由现有表现层负责；这里只保留原脚本音效、移动、清弹和结束时点。
    for(int repeat=0;repeat<6;++repeat) {
        co_await a.At(10);a.soundEvents.push_back(7);
        if(repeat<5)a.Rewind(0);
    }
    a.soundEvents.push_back(18);a.ClearBullets();a.deathCallback=End;a.health=a.maxHealth=0;
    co_await a.At(3010);a.finished=true;
}
Routine RumiaBoss::Behaviors::End(Actor& a) { a.finished=true;co_return; }

}

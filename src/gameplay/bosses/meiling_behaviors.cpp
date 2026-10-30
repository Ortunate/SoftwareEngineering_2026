// 本文件实现 MeilingBoss 内部命名行为；移动、弹幕、演出和随机读取保持原内容时序。
#include "gameplay/bosses/meiling.hpp"
#include "stages/stage3/behaviors.hpp"
#include "stages/stage3/ids.hpp"
#include "gameplay/spatial.hpp"
#include <cmath>
#include <numbers>
namespace koumakyou::bosses {
using namespace stage3;
using behavior::Actor;using behavior::Routine;using skill::Formation;
namespace {

using behavior::Actor;using behavior::Routine;using skill::Formation;
constexpr float pi=std::numbers::pi_v<float>;
movement::State& Motion(Actor& a) {return a.liftedMotion ? a.liftedMotion->planar : a.motion;}
void Height(Actor& a) {
    if(a.liftedMotion)a.liftedMotion->Aim(a.motion,-1.25F+2.5F*spatial::RadicalInverse(a.spatialIndex++,2));
}
void MoveTo(Actor& a,int frames,Vector3 point,int power=2) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    Motion(a).Interpolate(frames,point,power);Height(a);
}
void Wander(Actor& a,float speed,int frames) {
    if(a.skills.spatial&&!a.liftedMotion)a.liftedMotion.emplace(a.motion);
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=speed});Motion(a).Decelerate(frames);Height(a);
}
void Fire(Actor& a,skill::Shot shot,std::optional<Vector3> position={},const skill::State* configuration=nullptr) {
    if(a.skills.spatial) {
        shot.spatialCount=shot.count>1||shot.formation==Formation::FixedFan||shot.formation==Formation::AimedFan ? shot.count*3 : 1;
        if(shot.formation==Formation::RandomAngle||shot.formation==Formation::RandomAngleSpeed) {
            // 随机单发也须跨轮推进高度序列；每轮从零采样会使所有雨弹集中在同一个切面。
            shot.sampleIndex=static_cast<int>(a.spatialIndex&0x7fffffffU);a.spatialIndex+=shot.spatialCount*shot.layers;
        }
        if(shot.count==1&&(shot.formation==Formation::FixedRing||shot.formation==Formation::AimedRing)) {
            shot.formation=Formation::AimedFan;shot.sampleIndex=static_cast<int>(a.spatialIndex++&0x7fffffffU);shot.coneHalfAngle=pi;
        }
    }
    if(position)a.FireAt(shot,*position,configuration);else a.Fire(shot);
}
float Normalize(float angle) {while(angle>pi)angle-=2*pi;while(angle<-pi)angle+=2*pi;return angle;}
/// @brief 星形顶点和起点快照随角色协程存活；计数恢复时保留已经推进的顶点，不能由计数反算几何。
struct StarWeave {
    std::array<float,6> angles{};
    Vector3 start{},player{};
    skill::State configuration; // 发射配置时锁定 rank；后续改变角色 rank 不追溯这轮持续发射。
    void Emit(Actor& a,int counter) {
        if(counter==0) {
            start=a.Position();player=a.PlayerPosition();angles[0]=a.Random(2*pi)-pi;
            angles[1]=Normalize(angles[0]+2.51327419F);
        }
        if(counter%30==0) {
            angles[0]=angles[1];
            for(int i=1;i<6;++i)angles[i]=Normalize(angles[i-1]+2.51327419F);
        }
        if(counter%6)return;
        const float progress=counter/160.0F,part=(counter%30)/30.0F,factor=progress+0.5F;
        const Vector3 center{start.x+(player.x-start.x)*progress*0.1F,start.y+(player.y-start.y)*progress*0.1F,start.z+(player.z-start.z)*progress*0.1F};
        float angle=1.04719758F*factor;
        const bool lunatic=a.difficulty==native_combat::Difficulty::Lunatic;
        for(int edge=0;edge<5;++edge) {
            const Vector3 from{5*std::cos(angles[edge]),0,-5*std::sin(angles[edge])};
            const Vector3 to{5*std::cos(angles[edge+1]),0,-5*std::sin(angles[edge+1])};
            const Vector3 origin{center.x+from.x+(to.x-from.x)*part,center.y,center.z+from.z+(to.z-from.z)*part};
            // 每条边只取一次速度随机数，所有扇形弹共享该值；rank 修正在实际发射时另行应用。
            const float influence=configuration.rankInfluence[0]+configuration.rank*(configuration.rankInfluence[1]-configuration.rankInfluence[0])/32;
            const float randomRange=std::max(0.803571429F,2.67857143F+influence/2);
            const float speed=(lunatic?3.21428585F:2.67857143F)+a.Random(randomRange);
            Fire(a,{Formation::AimedFan,3,6,lunatic?4:2,1,speed,2.67857143F,angle,2.24399471F,4},origin,&configuration);
            angle-=0.52359879F*factor;
        }
        a.soundEvents.push_back(22);
    }
};
void Initialize(Actor& a) {
    a.SetPoses({64,69,70,67,68});a.deathEffect=2;
    Motion(a).bounds={{-7.14285714F,3.57142857F},{7.14285714F,7.85714286F}};Motion(a).clamped=true;
}

}

namespace {
/// @brief 原准备演出按十帧采样；等待消耗本子行为时钟，不推进父行为的局部时间。
Routine Prepare(Actor& a,int frames) {
    a.soundEvents.push_back(25);
    // 十帧轮询在原准备子时钟上交付，末轮之后才换姿态；颜色原字节为ARGB 0x00000001。
    for(int frame=0;frame<frames;frame+=10) {co_await a.At(frame);a.EmitEffect(effects::Kind::Drift,2,{0,0,1,0});}
    // 源99(4,66)在准备结束时重启独立扩散圈；不能覆盖本体姿态或占用外部粒子池。
    co_await a.At(frames);a.AnimateAuxiliary(4,66);a.soundEvents.push_back(7);
}
Routine OffsetRain(Actor& a) {
    constexpr int counts[]{12,16,20,24};
    float x=a.Random(1.42857146F)-0.714285731F,z=a.Random(1.42857146F)-0.714285731F;
    for(int shot=0;shot<8;++shot) {
        co_await a.At(20+shot*8);a.skills.offset={x,0,-z};
        Fire(a,{Formation::RandomSpeedRing,3,6,counts[native_combat::DifficultyIndex(a.difficulty)],1,8.03571415F,4.5535717F,0,0,4});
        x-=a.Random(1.42857146F)-0.714285731F;z-=a.Random(1.07142854F)-0.714285731F;a.soundEvents.push_back(8);
    }
    co_await a.At(124);
}
Routine DenseRing(Actor& a) {
    constexpr int counts[]{64,76,88,96};
    a.skills.offset={};Fire(a,{Formation::FixedRing,1,2,counts[native_combat::DifficultyIndex(a.difficulty)],1,2.94642878F,1.60714281F,0,0,4});
    a.soundEvents.push_back(26);co_await a.At(10);
}
Routine MidbossRetreat(Actor& a,bool defeated) {
    a.damageable=false;Motion(a).clamped=false;Motion(a).Apply({.speed=0});a.SetPoses({64,69,70,67,68});a.skills.offset={};
    a.ClearBullets();
    if(defeated) {a.dropEvents.push_back(5);a.soundEvents.push_back(18);}
    co_await a.At(130);a.Animate(68);MoveTo(a,60,{0,0,12.8571424F});co_await Prepare(a,50);
    // Prepare冻结父时钟50帧，返回时墙钟已到180；不能再等待父时钟180而多延迟50帧。
    a.finished=true;
}
Routine FirstCycle(Actor& a,int cycle) {
    constexpr int adjustment[]{-1,0,2,5};
    float angle=a.Random(2*pi)-pi,speed=5.35714293F;
    const int count=std::min(cycle+5,10)+adjustment[native_combat::DifficultyIndex(a.difficulty)];
    for(int i=0;i<30;++i) {
        co_await a.At(i*4);Fire(a,{Formation::AimedRing,5,2,count,1,speed,1.60714281F,angle,0,4});a.soundEvents.push_back(7);
        angle+=cycle%2==0 ? 0.0981747732F : -0.0981747732F;speed+=0.133928567F;
    }
    co_await a.At(120);Wander(a,13.3928576F,40);a.soundEvents.push_back(25);float spacing=0.0392699093F;
    for(int i=0;i<16;++i) {
        co_await a.At(120+i*2);
        if(i%5==0)a.EmitEffect(effects::Kind::Drift,2,{0,0,1,0});
        Fire(a,{Formation::AimedRing,5,2,count,1,speed,1.60714281F,angle,0,4});
        angle+=cycle%2==0 ? spacing : -spacing;spacing+=0.0654498488F;speed+=0.160714284F;
    }
    co_await a.At(152);a.AnimateAuxiliary(4,66);a.soundEvents.push_back(7);
}
void Summon(Actor& a,float x,float health) {
    auto pattern=kSummonedStream;pattern.spatialShots=a.skills.spatial;
    a.spawnEvents.push_back({.position={x,a.Position().y,11.4285717F},.pattern=pattern,
        .sourceHealth=health,.sourceScore=2000,.seed=++a.spawnIndex,.sourceItem=1,
        .visual=kOrb,.hitRadius=0.625F,.bulletBase=kNativeBulletBase,.laserBase=kNativeLaserBase,
        .defeatBase=kNativeDefeat,.fallback=kBlueBullet,.retirementHeights=Vector2{-10.5F,10.5F}});
}
Routine SummonWave(Actor& a) {
    Summon(a,-7.14285707F,2000);Summon(a,7.14285707F,1000);
    co_await a.At(30);Summon(a,-4.28571415F,500);Summon(a,4.28571415F,2000);
    co_await a.At(60);Summon(a,-1.42857146F,1000);Summon(a,1.42857146F,500);
}
Routine AlternatingRings(Actor& a) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int counts[]{16,16,16,20},layers[]{1,2,3,3};
    float angle=a.Random(2*pi)-pi;
    for(int i=0;i<3;++i) {
        co_await a.At(i*70);
        // 中间一轮在四难度均为 16 向两层，不能沿用首尾两轮的难度参数。
        Fire(a,{Formation::FixedRing,3,i%2 ? 6 : 5,i==1?16:counts[d],i==1?2:layers[d],8.03571415F,5.35714293F,angle,0.261799395F,4});a.soundEvents.push_back(8);
        // 下一轮角度在上一轮发射后立即预取；阶段被中断时也必须保留这次随机读取。
        if(i<2)angle=a.Random(2*pi)-pi;
    }
}
Routine FallingRings(Actor& a) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int counts[]{8,14,18,20};
    a.skills.projectile.accelerationVector=Vector3{0,0,-4.01785707F};
    for(int i=0;i<3;++i) {
        co_await a.At(i*70);Fire(a,{Formation::RandomAngleSpeed,3,6,i==1 ? 16 : counts[d],1,5.35714293F,0.803571463F,0,-pi,25});
        a.soundEvents.push_back(7);
    }
}
Routine FastRandomRings(Actor& a) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int counts[]{12,24,28,32};
    // 源仅在进入本子行为时读取一次未用于发射的角度，后两轮不得额外推进随机流。
    const float unused=a.Random(2*pi)-pi;(void)unused;
    for(int i=0;i<3;++i) {
        co_await a.At(i*70);
        Fire(a,{Formation::RandomAngleSpeed,3,5,i==0?counts[d]:i==1?16:24,1,10.7142859F,3.48214269F,pi,-pi,2});a.soundEvents.push_back(9);
    }
}
Routine BlueRain(Actor& a) {
    constexpr int counts[]{1,1,2,3};
    a.skills.projectile.accelerationVector=Vector3{0,0,-4.33928585F};a.skills.rankInfluence={};
    for(int i=0;i<40;++i) {
        co_await a.At(i*2);Fire(a,{Formation::RandomAngleSpeed,5,6,counts[native_combat::DifficultyIndex(a.difficulty)],1,2.67857143F,0.803571463F,0,-pi,19});a.soundEvents.push_back(22);
    }
    co_await a.At(160);
}
Routine TurnedRings(Actor& a) {
    const int d=native_combat::DifficultyIndex(a.difficulty);
    constexpr int counts[]{16,24,35,42};
    constexpr float left[]{-2.8972466F,-2.8972466F,-2.72271371F,-2.61799383F};
    constexpr float right[]{2.8972466F,2.8972466F,2.72271371F,2.44346094F};
    a.skills.rankInfluence={-1.33928573F,2.14285707F,0,0,0,0};
    for(int side=0;side<2;++side) {
        a.skills.projectile={.delayFrames=60,.turnAngle=side ? right[d] : left[d],.turnSpeed=d<2?8.03571415F:10.7142859F};
        float speed=5.35714293F;
        for(int i=0;i<6;++i) {
            co_await a.At(side*76+i);Fire(a,{Formation::FixedRing,4,side ? 2 : 6,counts[d],1,speed,5.35714293F,0,0,66});speed+=1.60714281F;
        }
        co_await a.At(side*76+26);a.soundEvents.push_back(9);
    }
}
}
Routine MeilingBoss::Behaviors::Midboss(Actor& a) {
    a.damageable=false;a.contactEnabled=false;Initialize(a);
    Motion(a).clamped=false;MoveTo(a,100,{0,0,3.30357146F},-2);co_await Prepare(a,100);
    Motion(a).clamped=true;a.damageable=true;a.contactEnabled=true;
    // 入场准备返回后才安装四轨；等待仍属于原父时钟，不能推迟第30帧的攻击。
    a.AttachEffect({-.7F,.5F,.1F},2.14285714F,{224,224,255,255});
    co_await a.At(10);a.AttachEffect({-.1F,.5F,.7F},2.14285714F,{224,224,255,255});
    co_await a.At(20);a.AttachEffect({.7F,.5F,.1F},2.14285714F,{224,224,255,255});
    co_await a.At(30);a.AttachEffect({-.1F,.5F,1},2.14285714F,{224,224,255,255});
    for(int i=0;i<160;++i) {
        co_await a.At(30);if(i%2==0)co_await OffsetRain(a);else co_await DenseRing(a);
        Wander(a,13.3928576F,50);co_await Prepare(a,50);co_await a.At(90);if(i<159)a.Rewind(30);
    }
    a.finished=true;
}
Routine MeilingBoss::Behaviors::GorgeousSweetFlower(Actor& a) {
    a.damageable=false;Initialize(a);a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,3.57142854F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    float left=a.Random(2*pi)-pi,right=a.Random(2*pi)-pi;
    const bool easy=a.difficulty==native_combat::Difficulty::Easy;
    a.skills.rankInfluence={-1.33928573F,0,0,2,0,0};
    for(;;) {
        Fire(a,{Formation::FixedRing,5,13,easy?3:4,1,5.35714293F,1.60714281F,left,0,4});
        Fire(a,{Formation::FixedRing,5,13,easy?3:4,1,5.35714293F,1.60714281F,right,0,4});a.soundEvents.push_back(27);
        left=std::remainder(left+0.1134464F,2*pi);right=std::remainder(right-0.130899698F,2*pi);
        for(int i=0;i<6;++i) {
            if(static_cast<int>(a.phaseFrames)%(easy?160:80)==0)Fire(a,{Formation::AimedRing,5,2,easy?32:42,1,3.21428585F,1.60714281F,0,0,4});
            co_await a.At(i+1);
        }
        a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::Selaginella(Actor& a) {
    a.damageable=false;Initialize(a);a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,3.57142854F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    // 两个方向在源中从零开始，不读取随机数；弹幕快照分别保存不同的延迟转向。
    float left=0,right=0;const bool lunatic=a.difficulty==native_combat::Difficulty::Lunatic;
    for(;;) {
        a.skills.projectile={.delayFrames=60,.turnAngle=-2.61799383F,.turnSpeed=4.28571415F};
        Fire(a,{Formation::FixedRing,5,13,lunatic?6:5,1,10.7142859F,1.60714281F,left,0,68});
        a.skills.projectile={.delayFrames=60,.turnAngle=-3.66519141F,.turnSpeed=4.28571415F};
        Fire(a,{Formation::FixedRing,5,13,lunatic?6:5,1,10.7142859F,1.60714281F,right,0,68});
        left+=0.148352981F;right-=0.165806279F;
        // 源 Sub18 的第二条角度补偿实际写回左方向，且比较界限为 +π；保留该读取/写入顺序。
        // 这里不能改成各自 remainder，否则长循环的浮点求值与原指令不同。
        if(left>pi)left-=2*pi;
        if(right<pi)left+=2*pi;
        a.soundEvents.push_back(27);
        for(int i=0;i<6;++i) {
            if(static_cast<int>(a.phaseFrames)%120==0)
                Fire(a,{Formation::AimedRing,5,2,lunatic?52:48,1,lunatic?4.28571415F:3.48214269F,1.60714281F,0,0,4});
            co_await a.At(i+1);
        }
        a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::MidbossDefeated(Actor& a) {co_await MidbossRetreat(a,true);}
Routine MeilingBoss::Behaviors::MidbossTimeout(Actor& a) {co_await MidbossRetreat(a,false);}
Routine MeilingBoss::Behaviors::Entrance(Actor& a) {
    Initialize(a);a.damageable=false;a.contactEnabled=false;a.skills.offset={0,0,0.535714269F};
    // 战斗边界须在场外入场完成后启用；对白等待期的下界对应源 176，开战阶段再改为 144。
    Motion(a).clamped=false;MoveTo(a,100,{0,0,3.30357146F},-2);co_await Prepare(a,100);
    Motion(a).bounds.lower.y=2.14285714F;Motion(a).clamped=true;
    co_await a.At(1000000);
}
Routine MeilingBoss::Behaviors::FirstNonspell(Actor& a) {
    a.interactable=true;a.contactEnabled=true;
    Initialize(a);
    a.phaseFrames=0;a.damageable=true;
    a.AttachEffect({-.7F,.5F,.1F},2.14285714F,{255,224,224,255});
    co_await a.At(10);a.AttachEffect({-.1F,.5F,.7F},2.14285714F,{255,224,255,255});
    co_await a.At(20);a.AttachEffect({.7F,.5F,.1F},2.14285714F,{255,224,255,255});
    co_await a.At(30);a.AttachEffect({-.1F,.5F,1},2.14285714F,{255,224,255,255});
    for(int cycle=0;;++cycle) {co_await a.At(50);co_await FirstCycle(a,cycle);co_await a.At(170);a.Rewind(50);}
}
Routine MeilingBoss::Behaviors::RainbowWindChime(Actor& a) {
    a.defeatActors=true;a.damageable=false;a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,7.14285707F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    MoveTo(a,2000,{0,0,2.85714293F});float base=0.87266463F;
    constexpr std::array<int,7> colors{2,14,13,11,8,6,4},counts{2,3,4,2,4,3,2};
    constexpr std::array<int,7> easyCounts{2,1,1,2,1,3,2};
    for(int cycle=0;;++cycle) {
        float angle=base;
        const int d=native_combat::DifficultyIndex(a.difficulty);
        for(int i=0;i<7;++i) {
            const int count=d==0?easyCounts[i]:i==4&&d>=2?(d==2?6:8):counts[i];
            Fire(a,{Formation::FixedRing,5,colors[i],count,i==4&&d>=2?2:1,6.96428537F,1.60714281F,angle,0,2});
            if(i<6)angle+=0.1134464F;
        }
        if(d>=2) {
            angle=pi-angle;
            Fire(a,{Formation::FixedRing,5,8,4,1,6.96428537F,1.60714281F,angle,0,2});
            Fire(a,{Formation::FixedRing,5,10,4,1,6.96428537F,1.60714281F,angle-0.1134464F,0,2});
        }
        base+=cycle%96<48 ? 0.130899698F : -0.130899698F;if(base<-pi)base+=2*pi;
        a.soundEvents.push_back(24);co_await a.At(7);a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::SummoningNonspell(Actor& a) {
    a.ClearBullets();Initialize(a);a.phaseFrames=0;
    a.dropEvents.push_back(5);a.damageable=true;
    a.skills.rankInfluence={-2.67857143F,2.67857143F,-2,6,0,0};
    for(;;) {
        co_await a.At(160);co_await SummonWave(a);
        for(int i=0;i<4;++i) {
            const auto choice=a.RandomIndex(3);
            if(choice==0)co_await AlternatingRings(a);else if(choice==1)co_await FallingRings(a);else co_await FastRandomRings(a);
            Wander(a,5.35714293F,80);co_await Prepare(a,80);
        }
        co_await a.At(280);a.Rewind(160);
    }
}
Routine MeilingBoss::Behaviors::FlowerDream(Actor& a) {
    a.ClearBullets();a.defeatActors=true;a.damageable=false;Initialize(a);a.skills.offset={};a.phaseFrames=0;
    co_await a.At(1);a.phaseFrames=0;
    a.skills.rankInfluence={-1.33928573F,2.14285707F,0,0,0,0};
    MoveTo(a,120,{0,0,7.14285707F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    MoveTo(a,2000,{0,0,2.85714293F});StarWeave weave;weave.configuration=a.skills;a.Rewind(0);
    for(;;) {
        // 源调用复制整个 0x60 字节上下文；80 帧准备期间计数继续，返回时恢复父计数 80。
        // 因而前 160 帧运行计数 0…159，随后 80 帧重复 80…159，再等待到本轮 260 帧重启。
        for(int frame=0;frame<260;++frame) {
            co_await a.At(frame);
            if(frame==80) {Wander(a,8.03571415F,80);a.soundEvents.push_back(25);}
            if(frame>=80&&frame<160&&frame%10==0)a.EmitEffect(effects::Kind::Drift,2,{0,0,1,0});
            if(frame==160) {a.AnimateAuxiliary(4,66);a.soundEvents.push_back(7);}
            if(frame<240)weave.Emit(a,frame<160?frame:frame-80);
        }
        co_await a.At(260);a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::RainNonspell(Actor& a) {
    a.ClearBullets();a.defeatActors=true;Initialize(a);a.phaseFrames=0;
    a.dropEvents.push_back(5);a.damageable=true;
    for(;;) {
        co_await a.At(160);co_await TurnedRings(a);co_await BlueRain(a);
        Wander(a,10.7142859F,80);co_await Prepare(a,80);co_await a.At(280);a.Rewind(160);
    }
}
Routine MeilingBoss::Behaviors::ColorfulRain(Actor& a) {
    a.ClearBullets();a.defeatActors=true;a.damageable=false;a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,7.14285707F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    for(;;) {
        a.skills.projectile.accelerationVector=Vector3{0,0,-4.33928585F};
        for(int i=0;i<20;++i) {
            co_await a.At(i*4);for(int color:{6,10,11,8})Fire(a,{Formation::RandomAngleSpeed,5,color,1,1,2.67857143F,0.803571463F,pi,-pi,19});
            a.soundEvents.push_back(22);
        }
        co_await a.At(80);Wander(a,8.03571415F,80);co_await Prepare(a,80);
        for(int i=0;i<20;++i) {
            co_await a.At(100+i*3);
            for(int j=0;j<(a.difficulty==native_combat::Difficulty::Easy?2:4);++j) {
                const float angle=std::array{pi,0.0F,2.3561945F,0.785398185F}[j];
                a.skills.projectile.accelerationVector=Vector3{3.85714269F*std::cos(angle),0,-3.85714269F*std::sin(angle)};
                Fire(a,{Formation::RandomAngleSpeed,5,std::array{2,4,13,14}[j],1,1,2.67857143F,0.803571463F,pi,-pi,19});
            }
            a.soundEvents.push_back(22);
        }
        co_await a.At(160);a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::ColorLightDance(Actor& a) {
    a.ClearBullets();a.defeatActors=true;a.damageable=false;Initialize(a);a.skills.offset={};a.phaseFrames=0;
    MoveTo(a,120,{0,0,7.14285707F});a.SetPoses({71,71,71,67,68});co_await Prepare(a,120);a.damageable=true;
    const bool lunatic=a.difficulty==native_combat::Difficulty::Lunatic;
    for(;;) {
        a.skills.projectile.accelerationVector=Vector3{0,0,-4.01785707F};
        float angle=a.Random(2*pi)-pi;
        for(int i=0;i<40;++i) {
            co_await a.At(i);float direction=angle;
            for(int color:{6,10,11,8}) {Fire(a,{Formation::FixedFan,5,color,1,1,4.01785707F,0.803571463F,direction,0.52359879F,19});direction+=pi/2;}
            angle=Normalize(angle+0.17453292F);a.soundEvents.push_back(22);
        }
        co_await a.At(40);Wander(a,8.03571415F,80);co_await Prepare(a,80);
        const int count=lunatic?30:20;
        for(int i=0;i<count;++i) {
            co_await a.At(60+i*2);float direction=angle;
            for(int side=0;side<4;++side) {
                const float acceleration=std::array{pi,0.0F,2.3561945F,0.785398185F}[side];
                a.skills.projectile.accelerationVector=Vector3{3.85714269F*std::cos(acceleration),0,-3.85714269F*std::sin(acceleration)};
                Fire(a,{Formation::RandomAngleSpeed,5,std::array{2,4,13,14}[side],lunatic&&side>=2?2:1,1,3.21428585F,0.803571463F,direction,0.0981747732F,19});
                direction+=pi/2;
            }
            angle=Normalize(angle-0.52359879F);a.soundEvents.push_back(22);
        }
        co_await a.At(60+count*2);a.Rewind(0);
    }
}
Routine MeilingBoss::Behaviors::DazzlingColorTyphoon(Actor& a) {
    a.ClearBullets();a.defeatActors=true;a.damageable=false;a.phaseFrames=0;
    a.dropEvents.push_back(5);a.EmitEffect(effects::Kind::Flash,1,{128,128,255,255});a.soundEvents.push_back(7);
    co_await a.At(60);a.skills.offset={};MoveTo(a,120,{0,0,4.28571415F});a.SetPoses({71,71,71,67,68});
    co_await Prepare(a,120);a.damageable=true;
    float reverse=a.Random(2*pi)-pi,forward=pi/2;int count=2;
    const bool hard=native_combat::DifficultyIndex(a.difficulty)>=2,lunatic=a.difficulty==native_combat::Difficulty::Lunatic;
    for(;;) {
        const auto rain=[&](int color,float acceleration,float angle,int directions) {
            a.skills.projectile.accelerationVector=Vector3{acceleration*std::cos(angle),0,-acceleration*std::sin(angle)};
            Fire(a,{Formation::RandomAngleSpeed,5,color,directions,1,2.67857143F,0.803571463F,pi,-pi,19});
        };
        rain(6,2.57142878F,forward,count);co_await a.At(61);
        rain(10,2.89285707F,forward+pi/2,1);
        if(hard)rain(11,2.57142878F,forward+pi,1);
        co_await a.At(62);
        rain(8,2.89285707F,forward-pi/2,1);co_await a.At(63);
        rain(2,2.57142878F,reverse,count);
        if(hard)rain(2,2.89285707F,lunatic?reverse:reverse+pi/2,1);
        co_await a.At(64);
        rain(2,2.89285707F,reverse+pi,1);co_await a.At(65);
        rain(2,2.57142878F,lunatic?reverse:reverse-pi/2,lunatic?3:1);
        forward+=0.104719758F;if(forward>pi)forward-=2*pi;
        reverse-=0.104719758F;if(reverse<-pi)reverse+=2*pi;
        if(a.phaseFrames>1200)count=3;else if(a.phaseFrames>600)count=2;
        a.soundEvents.push_back(22);co_await a.At(66);a.Rewind(60);
    }
}
Routine MeilingBoss::Behaviors::Defeated(Actor& a) {
    a.damageable=false;a.lifeThreshold=-1;a.timeoutFrames=-1;a.deathCallback=End;
    a.RandomDirection(-pi,pi);Motion(a).Apply({.speed=1.60714281F});Motion(a).Decelerate(60);
    constexpr std::array<Color,5> colors{WHITE,Color{255,128,128,255},Color{128,255,128,255},Color{128,128,255,255},Color{255,255,128,255}};
    for(int i=0;i<6;++i) {
        for(int n=0;n<5;++n) {co_await a.At(i*10+(n+1)*2);a.EmitEffect(effects::Kind::Burst,2,colors[n]);}
        a.soundEvents.push_back(7);
    }
    a.soundEvents.push_back(18);a.ClearBullets();a.pendingCallback=End;
}
Routine MeilingBoss::Behaviors::End(Actor& a) {a.finished=true;co_return;}

}

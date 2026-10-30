// 本文件验证四面出生、普通演员、Boss 源时钟、四机体阶段与通用弹道；整关图形另有集成测试。
#include "gameplay/bosses/koakuma.hpp"
#include "stages/stage4/behaviors.hpp"
#include "stages/stage4/ids.hpp"
#include "stages/stage4/data.hpp"
#include "stage4_reference_program.hpp"
#include "stage4_combat_reference.hpp"
#include "stages/stage4/boss_config.hpp"
#include "stages/stage4/content.hpp"
#include "gameplay/bosses/patchouli.hpp"
#include "app/spell_practice.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>
#include <iostream>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>;
void Near(float a,float b,float tolerance=5e-4F) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {std::cerr<<"Stage 4 mismatch "<<a<<" vs "<<b<<'\n';std::abort();}
}
unsigned Next(unsigned& state) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
/// @brief 独立源请求按同帧顺序比较；只在参考侧换算源像素/XYZ/ARGB，附着中心不得延长演员寿命。
void CompareStage4Effects(const behavior::Actor& actor,const std::vector<stage4::reference::ExternalEffect>& expected) {
    assert(actor.effectEvents.size()==expected.size());constexpr float scale=20.0F/448;
    for(std::size_t n=0;n<expected.size();++n) {
        const auto& a=actor.effectEvents[n];const auto& e=expected[n];
        assert(e.type==3||e.type==12||e.type==13);
        assert(a.kind==(e.type==3?effects::Kind::Burst:e.type==12?effects::Kind::Flash:effects::Kind::Attached)&&a.count==e.count);
        assert(a.color.r==((e.color>>16)&255)&&a.color.g==((e.color>>8)&255)&&a.color.b==(e.color&255)&&a.color.a==(e.color>>24));
        Near(a.position.x,(e.position.x-192)*scale);Near(a.position.y,-e.position.z*scale);Near(a.position.z,10-e.position.y*scale);
        if(e.type==13) {
            assert(a.axis.x==e.axis.x&&a.axis.y==-e.axis.z&&a.axis.z==-e.axis.y);
            const auto anchor=a.attachment.lock();assert(anchor);Near(anchor->Radius(),e.radius*scale,1e-5F);
        }
    }
}
/// @brief 四机体/四难度实际包装入口与60帧退场独立对照；首段无闪光，后两段同刻交付音效。
void TestPatchouliEffectReference() {
    using P=bosses::PatchouliBoss;auto bullets=std::make_unique<BulletSystem>();constexpr float scale=20.0F/448;
    for(unsigned d=0;d<4;++d)for(unsigned shot=0;shot<4;++shot) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);
        const auto config=stage4::PatchouliFinalConfig(static_cast<ShotType>(shot),difficulty);P inspector;
        for(std::size_t segment=0;segment<config.phases[2].skills.size();++segment) {
            auto definition=inspector.DescribeSkill(config.phases[2].skills[segment].id,static_cast<ShotType>(shot),difficulty);
            behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},definition.NativeHealth(),7);
            actor.difficulty=difficulty;actor.SetRank(32);P::Behaviors::ConfigurePractice(actor,false);
            actor.Start(definition.behavior(actor,inspector));
            stage4::reference::CombatReference source(d,shot,39+int(segment),{192,112,0},7,32);
            int flashes=0;
            for(int frame=0;frame<500;++frame) {
                bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{2,0,-6},*bullets);
                CompareStage4Effects(actor,source.effects);assert(actor.soundEvents==source.sounds);
                flashes+=int(actor.effectEvents.size());
            }
            assert(flashes==(segment?1:0));
        }
    }
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},1,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);P::Behaviors::ConfigurePractice(actor,false);
        actor.Start(P::Behaviors::Defeated(actor));stage4::reference::CombatReference source(d,0,96,{192,112,0},seed,32);
        int requests=0,particles=0;
        for(int frame=0;frame<=60;++frame) {
            bullets->Reset();auto* retained=bullets->Spawn(BulletOwner::Enemy,{}, {},.1F,WHITE,stage4::kBlueBullet);
            assert(retained);source.Tick({245,360,0});actor.Update(1.0F/60,{},*bullets);CompareStage4Effects(actor,source.effects);
            assert(source.clearBullets==(frame==60)&&retained->active==!source.clearBullets);
            assert(actor.soundEvents==source.sounds);Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            for(const auto& e:actor.effectEvents) {++requests;particles+=e.count;}
        }
        assert(requests==30&&particles==60&&actor.finished&&actor.Random(1)==source.Random(1));
    }
}
/// @brief 60/120Hz分别保持五显示FPS请求顺序/时间和空间参数一致；不借用显示帧数作为内容时钟。
void TestPatchouliEffectDisplayRates() {
    using P=bosses::PatchouliBoss;
    for(const auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120})for(int route=0;route<4;++route) {
        std::vector<effects::Request> expected;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor(rate);actor.Reset({0,0,5},17000,7);P::Behaviors::ConfigurePractice(actor,false);
            if(route==0)actor.Start(P::Behaviors::FirstNonspell(actor));
            else if(route==3)actor.Start(P::Behaviors::Defeated(actor));
            else actor.Start(P::Behaviors::Combination(actor,P::Behaviors::LavaCromlech,2400,route==2));
            auto bullets=std::make_unique<BulletSystem>();std::vector<effects::Request> actual;
            for(int n=0;n<fps*3;++n) {
                bullets->Reset();actor.Update(1.0F/fps,{2,0,-6},*bullets);
                actual.insert(actual.end(),actor.effectEvents.begin(),actor.effectEvents.end());
            }
            constexpr std::array<std::size_t,4> counts{5,0,1,30};assert(actual.size()==counts[route]);
            if(route==0)for(int n=0;n<5;++n)assert(actual[n].contentFrame==n*10);
            if(route==3)for(int n=0;n<30;++n)assert(actual[n].contentFrame==n/5*10+(n%5+1)*2);
            if(fps==120)expected=actual;
            else for(std::size_t n=0;n<actual.size();++n) {
                const auto& a=actual[n];const auto& e=expected[n];assert(a.kind==e.kind&&a.count==e.count&&a.contentFrame==e.contentFrame);
                assert(a.color.r==e.color.r&&a.color.g==e.color.g&&a.color.b==e.color.b&&a.color.a==e.color.a);
                Near(a.position.x,e.position.x);Near(a.position.y,e.position.y);Near(a.position.z,e.position.z);
            }
        }
    }
}
behavior::Spawn Spawn(behavior::Pattern pattern) {
    pattern.spatialShots=false;
    return {.pattern=pattern,.sourceHealth=100,.sourceScore=100,.seed=49,.visual=stage4::kOrb,.retireOutside=false};
}
void TestBirthsAndSourceMovement() {
    std::vector<const native_combat::HealthRecord*> originals;
    for(const auto& r:native_combat::Stage(4)->health)
        if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=21&&r.targetSub!=26)originals.push_back(&r);
    assert(originals.size()==395);const auto events=stage4::Stage4Timeline();assert(events.size()==400);
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d) {
    std::size_t index=0;std::array<bool,19> covered{};
    constexpr float scale=20.0F/448;
    for(const auto& event:events) {
        const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
        const auto& source=*originals.at(index++);const int sub=source.targetSub;covered[sub]=true;
        assert(event.atFrame==source.frame&&request->count==1&&request->interval==0);
        auto spawn=request->actor;assert(behavior::Valid(spawn));
        assert(spawn.sourceHealth==*source.value&&spawn.sourceScore==source.score&&spawn.sourceItem==source.item);
        assert(spawn.mirrored==(source.opcode==2||source.opcode==6));
        spawn.difficulty=*native_combat::DifficultyFromIndex(d);spawn.retireOutside=false;spawn.pattern.spatialShots=false;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
        assert(spawn.randomX==(source.position.x==-999));
        float x=spawn.randomX ? static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F*384 : source.position.x;
        float y=source.position.y,vx=0,vy=0,angle=pi/2,speed=2,acceleration=0,angular=0;
        if(sub>=2&&sub<=9) {angle=sub==8 ? pi/2 : sub==3||sub==5||sub==7 ? -1.04719758F : 0;speed=sub==2||sub==3||sub==4 ? 4.19999981F : 4.5F;}
        if(sub==10||sub==13)angle=0;
        if(sub==14||sub==16||sub==17)speed=0;
        unsigned random=spawn.seed;
        for(int frame=0;frame<=130;++frame) {
            x+=vx*(spawn.mirrored ? -1 : 1);y+=vy;
            if(sub>=2&&sub<=9) {
                if(frame==30) {
                    if(sub==2||sub==4||sub==6)angular=-0.0654498488F;
                    if(sub==3||sub==5||sub==7)angular=0.0654498488F;
                }
                if(frame==70) {
                    if(sub==2||sub==4||sub==6)angular=0;
                    if(sub==3||sub==5)angular=-0.130899698F;
                }
                if(sub==7&&frame==110)angular=0;
            } else if(sub!=14&&sub!=16&&sub!=17) {
                if(frame==40)acceleration=-0.0666666701F;
                if(frame==70) {
                    acceleration=0;
                    if(sub==0||sub==10||sub==18) {angle=static_cast<float>(Next(random)>>8)/16777216.0F*(pi/2)+pi/4;speed=1.79999995F;}
                }
            }
            angle+=angular;speed+=acceleration;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            if(std::abs(actor.position.x-(x-192)*scale)>5e-4F||std::abs(actor.position.z-(10-y*scale))>5e-4F)
                std::cerr<<"Stage 4 source sub "<<sub<<" birth "<<source.frame<<" local frame "<<frame<<'\n';
            Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,spawn.position.y);
            const bool summons=(sub==0||sub==10||sub==18)&&frame==70;
            assert(actor.spawnEvents.size()==(summons ? 1U : 0U));
            if(summons)assert(actor.spawnEvents.front().pattern.routine==(sub==18 ? stage4::HiddenLaserEmitter : stage4::HiddenRandomEmitter));
        }
    }
    assert(index==395);int recipes=0;for(bool hit:covered)recipes+=hit;assert(recipes==16);
    }
}
void TestPeriodicCrystals() {
    struct Case {behavior::Pattern pattern;int interval,end,count;bool right;};
    // 独立源 2/3/4/5/8/9 的周期/终点/层数；取模初相不得由浮点比例生成。
    const std::array<Case,6> cases{{{stage4::kHookFanShooter,50,70,6,false},
        {stage4::kSwerveFanShooter,50,150,6,true},{stage4::kHookLayerShooter,128,70,3,false},
        {stage4::kSwerveLayerShooter,128,150,3,true},{stage4::kDescendingFanShooter,50,100,6,false},
        {stage4::kCrossingFanShooter,50,100,6,false}}};
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d)for(std::size_t n=0;n<cases.size();++n)for(int rank:{10,16,31,32}) {
        const auto& c=cases[n];constexpr std::array subs{2,3,4,5,8,9};
        const auto program=stage4::reference::ProgramsForDifficulty(d)[subs[n]];
        const auto shot=std::find_if(program.begin(),program.end(),[](const auto& op){return op.opcode==67;});
        assert(shot!=program.end());const auto count=static_cast<std::size_t>(shot->args[2]*shot->args[3]);
        auto spawn=Spawn(c.pattern);spawn.difficulty=*native_combat::DifficultyFromIndex(d);
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.skills.rank=rank;
        unsigned random=49;const int period=c.interval+c.interval/5-(c.interval/5*2*rank)/32;
        int next=period-static_cast<int>(Next(random)%period)-1;
        for(int frame=0;frame<=160;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            const bool fire=frame==next&&frame<c.end;
            assert(bullets->Pool().ActiveCount()==(fire ? count : 0));
            if(fire) {
                const int directions=int(shot->args[2]),layers=int(shot->args[3]);
                const float speedScale=60*(20.0F/448),influence=-0.5F+rank/32.0F;
                const float first=std::max(0.3F,shot->args[4]+influence)*speedScale;
                const float last=std::max(0.3F,shot->args[5]+influence/2)*speedScale;
                const float aim=std::atan2(actor.Position().z+6,2-actor.Position().x);std::size_t index=0;
                bullets->Pool().ForEachActive([&](const Bullet& b) {
                    const int layer=int(index)/directions,n=int(index++)%directions;
                    float fan=directions%2?(n+1)/2*shot->args[7]:n/2*shot->args[7]+shot->args[7]/2;
                    if(n%2)fan=-fan;
                    const float angle=aim+fan,speed=first-(first-last)*layer/layers;
                    assert(b.nativeType==int(shot->args[0])&&b.nativeColor==int(shot->args[1]));
                    Near(b.velocity.x,std::cos(angle)*speed,1e-4F);Near(b.velocity.z,-std::sin(angle)*speed,1e-4F);
                });
                next+=period;
            }
            assert(!actor.CanContactPlayer()&&actor.CanBeTargeted()&&actor.CanTakeDamage());
        }
        assert(actor.Random(1)==static_cast<float>(Next(random)>>8)/16777216.0F);
    }
}
/// @brief 四难度普通配方的首轮逐发方向、速度层与弹种直接读取独立源指令；位移由出生对照另测。
void TestDifficultyOrdinaryShots() {
    struct Case {int sub,frame;behavior::Pattern pattern;};
    const std::array cases{
        Case{1,30,stage4::kHiddenRandomEmitter},Case{11,70,stage4::kStrongSpiralCrystal},
        Case{13,70,stage4::kGentleSpiralCrystal},Case{14,30,stage4::kGreenRingEmitter},
        Case{16,30,stage4::kBlueRingEmitter},Case{17,30,stage4::kOffsetRingEmitter},
        Case{20,120,stage4::kHiddenLaserEmitter}};
    constexpr float scale=20.0F/448,speedScale=60*scale;const Vector3 player{2,0,-6};
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d) {
        const auto programs=stage4::reference::ProgramsForDifficulty(d);
        const std::array totals{2105U,2111U,2116U,2116U};std::size_t total=0;
        for(const auto program:programs)total+=program.size();assert(total==totals[d]);
        for(const auto& c:cases) {
            auto spawn=Spawn(c.pattern);spawn.difficulty=*native_combat::DifficultyFromIndex(d);
            behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
            for(int frame=0;frame<=c.frame;++frame) {bullets->Reset();actor.Update(1.0F/60,player,*bullets);}
            std::vector<stage4::reference::Instruction> shots;
            for(const auto& op:programs[c.sub])if(op.opcode>=67&&op.opcode<=75)shots.push_back(op);
            if(c.sub==14)shots={shots.at(d*2),shots.at(d*2+1)};
            if(c.sub==20&&d<2)assert(shots.empty());
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b) {
                if(!b.IsBeam())actual.push_back(b);
            });
            unsigned random=spawn.seed;
            const auto read=[&](float range) {return static_cast<float>(Next(random)>>8)/16777216.0F*range;};
            Vector3 origin=actor.Position();float base=0,first=0;
            if(c.sub==1) {origin.x+=read(80)*scale-40*scale;origin.z-=read(80)*scale-40*scale;base=read(2*pi)-pi;first=read(1)+1;}
            if(c.sub==11||c.sub==13||c.sub==14)base=read(2*pi)-pi;
            if(c.sub==11||c.sub==13)(void)Next(random); // 螺旋方向另取一次整数随机数。
            std::size_t index=0;
            for(std::size_t group=0;group<shots.size();++group) {
                const auto& source=shots[group];const auto& a=source.args;
                const int count=int(a[2]),layers=int(a[3]);
                float angle=base;
                if(c.sub==14&&group==1)angle+=d==0?0.448798954F:0.285599321F;
                if(c.sub==17)angle=0; // 此入口保留零初始化的第二方向寄存器。
                if(source.opcode==67||source.opcode==69||source.opcode==71)
                    angle+=std::atan2(origin.z-player.z,player.x-origin.x);
                if(source.opcode==71)angle+=pi/count;
                const float high=(c.sub==1?first:a[4])*speedScale,low=std::max(0.3F,a[5])*speedScale;
                for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
                    assert(index<actual.size());const auto& bullet=actual[index++];
                    const float direction=angle+n*2*pi/count+layer*a[7],speed=high-(high-low)*layer/layers;
                    assert(bullet.nativeType==int(a[0])&&bullet.nativeColor==int(a[1]));
                    Near(bullet.position.x,origin.x);Near(bullet.position.z,origin.z);
                    Near(bullet.velocity.x,std::cos(direction)*speed,1e-4F);Near(bullet.velocity.z,-std::sin(direction)*speed,1e-4F);
                }
            }
            assert(index==actual.size());
            if(c.sub!=20)assert(actor.Random(1)==read(1));
        }
    }
}
void TestSpawnerAndHiddenEmitter() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Activate(Spawn(stage4::kDescendingSpawner));behavior::Spawn child;
    for(int frame=0;frame<=70;++frame) {
        actor.Update(1.0F/60,{},*bullets);assert(actor.spawnEvents.size()==(frame==70 ? 1U : 0U));
        if(frame==70)child=actor.spawnEvents.front();
    }
    assert(behavior::Valid(child)&&child.sourceHealth==1000&&child.sourceScore==10&&child.sourceItem==0&&!child.damageable);
    Near(child.position.x,actor.position.x);Near(child.position.z,actor.position.z);
    actor.Activate(child);
    for(int frame=0;frame<=222;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{},*bullets);
        const bool fires=frame>=30&&frame<=96&&(frame-30)%6==0;
        assert(bullets->Pool().ActiveCount()==(fires ? 4U : 0U));
        assert(!actor.CanBeTargeted()&&!actor.CanTakeDamage()&&!actor.CanContactPlayer());
        assert(actor.visible==(frame<222));assert(actor.active==(frame<222));assert(actor.animationInterrupt.has_value()==(frame>=102));
    }
    actor.Activate(Spawn(stage4::kHiddenLaserEmitter));
    for(int frame=0;frame<=440;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==120 ? 1U : 0U));
        if(frame==120)bullets->Pool().ForEachActive([](const Bullet& beam) {
            Near(beam.beamLength,22.3214283F);Near(beam.radius,0.71428573F);Near(beam.warningSeconds,70.0F/60);
            Near(beam.beamDirection.z,-1);Near(beam.maxAge,226.0F/60);
        });
        assert(actor.visible==(frame<440)&&actor.active==(frame<440)&&!actor.CanBeTargeted());
        assert(actor.animationInterrupt.has_value()==(frame>=320));
    }
}
void TestSpiralAndRingEmitters() {
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto& pattern:{stage4::kStrongSpiralCrystal,stage4::kGentleSpiralCrystal}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(Spawn(pattern));
        for(int frame=0;frame<=582;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=frame>=70&&frame<=574&&(frame-70)%8==0;
            assert(bullets->Pool().ActiveCount()==(fires ? 6U : 0U));
        }
        Near(actor.motion.angle,-pi/2);Near(actor.motion.speed,5.35714293F);
        bullets->Spawn(BulletOwner::Enemy,{}, {},0.1F,WHITE,stage4::kBlueBullet);
        auto* player=bullets->Spawn(BulletOwner::Player,{}, {},0.1F,WHITE,stage4::kBlueBullet);
        assert(actor.Defeat()&&!actor.Defeat());actor.Update(1.0F/60,{},*bullets);
        assert(actor.finished&&actor.dropEvents.size()==1&&actor.dropEvents.front()==3&&player->active);
        assert(bullets->Pool().ActiveCount()==1);
    }
    struct Case {behavior::Pattern pattern;int count,rounds;};
    for(const auto& c:std::array<Case,3>{{{stage4::kGreenRingEmitter,22,60},{stage4::kBlueRingEmitter,16,10},{stage4::kOffsetRingEmitter,22,6}}}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(Spawn(c.pattern));const int hidden=30+c.rounds*50;
        for(int frame=0;frame<=hidden+30;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=frame>=30&&frame<hidden&&(frame-30)%50==0;
            assert(bullets->Pool().ActiveCount()==(fires ? static_cast<std::size_t>(c.count) : 0));
            assert(actor.visible==(frame<hidden+30));assert(actor.CanBeTargeted()==(frame>=30&&frame<hidden));
            assert(actor.animationInterrupt.has_value()==(frame>=hidden));
            assert(actor.active==(frame<hidden+30));
        }
    }
}
behavior::Routine BeamProbe(behavior::Actor& actor) {
    actor.FireBeam(0,skill::Laser{.length=5,.width=0.5F,.grow=0,.sustain=10,.startOffset=1,.direction=Vector3{1,1,0}});
    co_await actor.At(1);
    if(auto* beam=actor.Beam(0)) {beam->position.x=99;actor.soundEvents.push_back(1);}
    else actor.soundEvents.push_back(2);
    co_await actor.At(2);actor.ClearBeamSlots();assert(!actor.Beam(0));
    co_await actor.At(10);actor.finished=true;
}
behavior::Routine EmitterDelay(behavior::Actor& actor) {co_await actor.At(10);}
behavior::Routine EmitterProbe(behavior::Actor& actor) {
    actor.skills.rankInfluence={0,0,0,4,0,0};
    actor.ConfigureEmitter({skill::Formation::FixedRing,3,6,2,1,2});actor.SetEmitterPeriod(3);actor.SetEmitterSound(7);
    co_await EmitterDelay(actor);
    actor.ConfigureEmitter({skill::Formation::FixedRing,3,2,5,1,2});
    co_await actor.At(10);actor.SetEmitterPeriod(0);co_await actor.At(30);actor.finished=true;
}
void TestPersistentEmitter() {
    auto bullets=std::make_unique<BulletSystem>();
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        behavior::Actor actor(rate);actor.Reset({},100,7);actor.Start(EmitterProbe(actor));
        for(int frame=0;frame<25;++frame) {
            if(frame==4)actor.SetRank(32);
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=frame<20&&frame%3==2;
            assert(bullets->Pool().ActiveCount()==(fires ? (frame<10 ? 4U : 9U) : 0U));
            // 新配方明确没有 PlaySound，身份仍保留，但不能沿用旧配方的播放开关。
            assert(actor.soundEvents.size()==(fires&&frame<10 ? 1U : 0U));
        }
        actor.Reset({},100,7);actor.Update(1.0F/60,{},*bullets);assert(actor.soundEvents.empty());
    }
}
behavior::Routine SoundFlagProbe(behavior::Actor& actor) {
    const skill::Shot silent{skill::Formation::FixedRing,3,6,1,1,1};auto sounding=silent;sounding.flags=skill::PlaySound;
    actor.ConfigureEmitter(silent);actor.SetEmitterSound(7);actor.SetEmitterPeriod(2);actor.Fire(silent);
    co_await actor.At(2);actor.ConfigureEmitter(silent);actor.Fire(silent);
    co_await actor.At(4);actor.Fire(sounding);
    co_await actor.At(5);actor.SetEmitterSound(-1);actor.Fire(sounding);
    co_await actor.At(6);actor.SetEmitterSound(8);co_await actor.At(8);actor.finished=true;
}
void TestEmitterSoundFlags() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({},100,7);actor.Start(SoundFlagProbe(actor));
    // 新典 84 设置位 512，而下一次发射配置覆盖该位；负号关闭当前配方但保留音效身份。
    constexpr std::array<int,8> sounds{-1,7,-1,-1,7,7,-1,8};
    for(int frame=0;frame<8;++frame) {
        actor.Update(1.0F/60,{},*bullets);assert(actor.soundEvents.size()==(sounds[frame]<0 ? 0U : 1U));
        if(sounds[frame]>=0)assert(actor.soundEvents.front()==sounds[frame]);
    }
}
void TestBossPreludeWitnesses() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({},7000,7);stage4::ConfigureBehavior(actor,false);actor.Start(bosses::KoakumaBoss::Behaviors::Encounter(actor));
    for(int frame=0;frame<475;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const bool first=frame>=30&&frame<=170&&(frame-30)%20==0;
        const bool second=frame>=280&&frame<=420&&(frame-280)%20==0;
        const bool next=frame==471;
        assert(bullets->Pool().ActiveCount()==(first||second||next ? 8U : 0U));
        assert(actor.spawnEvents.size()==(frame==190||frame==220||frame==440||frame==470 ? 1U : 0U));
        assert(actor.CanBeTargeted()==(frame>=30));
    }
    for(bool second:{false,true}) {
        actor.Reset({},second ? 16000 : 17000,7);stage4::ConfigureBehavior(actor,false);bullets->Reset();
        actor.Start(second ? bosses::PatchouliBoss::Behaviors::SecondNonspell(actor) : bosses::PatchouliBoss::Behaviors::FirstNonspell(actor));
        for(int frame=0;frame<=800;++frame) {
            actor.Update(1.0F/60,{2,3,-6},*bullets);int shots=0,beams=0;
            bullets->Pool().ForEachActive([&](Bullet& bullet){if(bullet.beamLength>0)++beams;else {++shots;bullet.active=false;}});
            const int first=second ? 180 : 100;
            if(frame==first)assert(beams==4&&shots==0);
            if(frame==first+60)assert(beams==8&&shots==0);
            if(!second) {
                if(frame==318)assert(shots==0);
                if(frame==319)assert(shots==64);
                if(frame==520)assert(shots==40&&beams==4);
                if(frame==620)assert(shots==80&&beams==8);
                if(frame==670)assert(shots==40);
            } else {
                if(frame==373)assert(shots==0);
                if(frame==374)assert(shots==30);
                if(frame==599)assert(shots==12);
                if(frame==600)assert(shots==40&&beams==4);
                if(frame==659)assert(shots==10);
            }
            // 光束必须独立老化；只删除普通发射样本，不能每帧 Reset 掩盖槽位引用失效。
            bullets->Update(1.0F/60,{}, {2,3,-6});
        }
    }
}
/// @brief 道中、两段非符及已接通的元素符卡直接执行源四难度指令，检查调用/回拨期间的位置、逐发参数及光束引用。
void TestDifficultyCombatReference() {
    constexpr float scale=20.0F/448,speedScale=scale*60;
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(int sub:{21,27,37,42,45,48,51,54,57,60,63,66,69,72,75,78,81,84,87,90,93})for(float playerY:{360.0F,40.0F,80.0F}) {
        if((sub<66||sub>=75)&&playerY!=360)continue;
        // 只运行实际存在的难度/符卡组合；Easy 的上级准备表包含无效卡号，不能冒充可玩技能。
        if(((sub==42||sub==51||sub==57||sub==66)&&d>=2)||((sub==45||sub==60||sub==69)&&d==0)||((sub==48||sub==54||sub==63||sub==72||sub==78)&&d<2)||(sub==75&&d!=1)||(sub==90&&d==0))continue;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,10-128*scale},17000,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);actor.SetRank(rank);stage4::ConfigureBehavior(actor,false);
        // 符卡从前一非符进入，继承战斗边界；独立启动测试也须还原这项阶段契约。
        if(sub>=42) {actor.motion.bounds={{(32-192)*scale,10-144*scale},{(352-192)*scale,10-48*scale}};actor.motion.clamped=true;}
        const auto entry=sub==21?bosses::KoakumaBoss::Behaviors::Encounter:sub==27?
            bosses::PatchouliBoss::Behaviors::FirstNonspell:sub==37?bosses::PatchouliBoss::Behaviors::SecondNonspell:
            sub==42?bosses::PatchouliBoss::Behaviors::AgniShine:sub==45?bosses::PatchouliBoss::Behaviors::AgniShineAdvanced:
            sub==48?bosses::PatchouliBoss::Behaviors::AgniRadiance:sub==51?bosses::PatchouliBoss::Behaviors::PrincessUndine:
            sub==54?bosses::PatchouliBoss::Behaviors::BuryInLake:sub==57?bosses::PatchouliBoss::Behaviors::SylphyHorn:
            sub==60?bosses::PatchouliBoss::Behaviors::SylphyHornAdvanced:sub==63?bosses::PatchouliBoss::Behaviors::GreenStorm:
            sub==66?bosses::PatchouliBoss::Behaviors::LazyTrilithon:sub==69?bosses::PatchouliBoss::Behaviors::LazyTrilithonAdvanced:
            sub==72?bosses::PatchouliBoss::Behaviors::TrilithonShake:sub==75?bosses::PatchouliBoss::Behaviors::MetalFatigue:
            sub==78?bosses::PatchouliBoss::Behaviors::SilverDragon:sub==81?bosses::PatchouliBoss::Behaviors::LavaCromlech:
            sub==84?bosses::PatchouliBoss::Behaviors::ForestBlaze:sub==87?bosses::PatchouliBoss::Behaviors::WaterElf:
            sub==90?bosses::PatchouliBoss::Behaviors::MercuryPoison:bosses::PatchouliBoss::Behaviors::EmeraldMegalith;
        actor.Start(entry(actor));stage4::reference::CombatReference source(d,0,sub,{192,128,0},seed,rank);
        for(int frame=0;frame<1500;++frame) {
            source.Tick({245,playerY,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-playerY*scale},*bullets);
            CompareStage4Effects(actor,source.effects);
            std::vector<const Bullet*> shots;bullets->Pool().ForEachActive([&](const Bullet& b){if(!b.IsBeam())shots.push_back(&b);});
            if(shots.size()!=source.emissions.size())std::cerr<<"Stage 4 combat d "<<d<<" sub "<<sub<<" frame "<<frame<<" shots "<<shots.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(shots.size()==source.emissions.size());
            if(std::abs(actor.position.x-(source.position.x-192)*scale)>5e-4F||std::abs(actor.position.z-(10-source.position.y*scale))>5e-4F)
                std::cerr<<"Stage 4 position d "<<d<<" sub "<<sub<<" frame "<<frame<<'\n';
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            assert(actor.CanTakeDamage()==(source.damageable&&source.interactable));
            if(actor.soundEvents!=source.sounds) {std::cerr<<"Stage 4 sounds d "<<d<<" sub "<<sub<<" frame "<<frame<<" actual";
                for(int sound:actor.soundEvents)std::cerr<<' '<<sound;std::cerr<<" expected";
                for(int sound:source.sounds)std::cerr<<' '<<sound;std::cerr<<'\n';}
            assert(actor.soundEvents==source.sounds);
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& s=source.emissions[n];
                if(std::abs(b.velocity.x-s.velocity.x*speedScale)>1e-4F||std::abs(b.velocity.z+s.velocity.y*speedScale)>1e-4F)
                    std::cerr<<"Stage 4 velocity d "<<d<<" sub "<<sub<<" frame "<<frame<<" shot "<<n<<'\n';
                Near(b.position.x,(s.position.x-192)*scale);Near(b.position.y,-s.position.z*scale);Near(b.position.z,10-s.position.y*scale);
                Near(b.velocity.x,s.velocity.x*speedScale,1e-4F);Near(b.velocity.y,-s.velocity.z*speedScale,1e-4F);Near(b.velocity.z,-s.velocity.y*speedScale,1e-4F);
                assert(b.visual==stage4::kNativeBulletBase+s.type*16+s.color);
                Near(std::sqrt(b.acceleration.x*b.acceleration.x+b.acceleration.z*b.acceleration.z),s.acceleration*scale*3600,1e-4F);
                Near(b.acceleration.x,s.accelerationVector.x*scale*3600,1e-4F);Near(b.acceleration.z,-s.accelerationVector.y*scale*3600,1e-4F);
                if(s.acceleration!=0&&s.accelerationFrames>=0)Near(b.accelerationSeconds,s.accelerationFrames/60,1e-5F);
                if(s.flags&(64|128|256)) {
                    Near(b.turnDelaySeconds,s.turnDelay/60,1e-5F);Near(b.turnAngle,s.turnAngle,1e-5F);
                    Near(b.turnSpeed,s.turnSpeed*speedScale,1e-4F);assert(b.turnCount==s.turnCount);
                    assert(b.turnAimed==bool(s.flags&128)&&b.turnAbsolute==bool(s.flags&256));
                }
                Near(b.speedAcceleration,s.speedAcceleration*scale*3600,1e-4F);
                Near(b.angularVelocity,s.angularVelocity*60,1e-5F);
                if(s.angularVelocity!=0)Near(b.angularSeconds,s.angularFrames/60,1e-5F);
            }
            assert(actor.spawnEvents.size()==source.spawns.size());
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& s=source.spawns[n];const auto& p=actor.spawnEvents[n];assert(s.sub==1);
                assert(p.sourceHealth==s.health&&p.sourceItem==s.item&&p.sourceScore==s.score);
                Near(p.position.x,(s.position.x-192)*scale);Near(p.position.z,10-s.position.y*scale);
            }
            // 在源声明的寿命窗口内比对光束；本执行器不证明边界帧的原生回收条件。
            // 新生数量单独精确核对，清引用不删除光束，池复用也不保证槽序。
            std::vector<const Bullet*> beams;
            bullets->Pool().ForEachActive([&](const Bullet& b){if(b.IsBeam())beams.push_back(&b);});
            assert(std::count_if(beams.begin(),beams.end(),[](const Bullet* b){return b->age==0;})==
                std::count_if(source.beams.begin(),source.beams.end(),[](const auto& b){return b.active&&b.age==0;}));
            // 伸长光束仅观察出生后 30 内容帧；之后项目场外回收与原生边界未建立等价，不将其当作源寿命证明。
            for(const auto& expected:source.beams)if(expected.active&&(expected.speed==0||expected.age<=30)) {
                auto match=std::find_if(beams.begin(),beams.end(),[&](const Bullet* b) {
                    return std::abs(b->age-expected.age/60.0F)<1e-5F&&
                        std::abs(b->beamDirection.x-std::cos(expected.angle))<1e-5F&&
                        std::abs(b->beamDirection.z+std::sin(expected.angle))<1e-5F;
                });
                if(match==beams.end())std::cerr<<"Stage 4 beam d "<<d<<" sub "<<sub<<" frame "<<frame<<" slot "<<expected.slot<<'\n';
                assert(match!=beams.end());const auto* beam=*match;beams.erase(match);
                Near(beam->beamAnchor.x,(expected.anchor.x-192)*scale);Near(beam->beamAnchor.z,10-expected.anchor.y*scale);
                Near(beam->beamStartOffset,expected.start*scale);Near(beam->beamLength,(expected.end-expected.start)*scale);
                Near(beam->radius,expected.width*scale/2);Near(beam->beamGrowSeconds,expected.grow/60.0F,1e-5F);
                Near(beam->warningSeconds,expected.warning/60.0F,1e-5F);
                Near(beam->dangerEndSeconds,(expected.grow+expected.sustain-expected.tail)/60.0F,1e-5F);
                if(expected.speed>0) {
                    Near(beam->beamTravelSpeed,expected.speed*speedScale,1e-4F);
                    Near(beam->beamLengthLimit,expected.lengthLimit*scale);
                }
                Near(beam->maxAge,(expected.grow+expected.sustain+expected.fade)/60.0F,1e-5F);
            }
            bullets->Pool().ForEachActive([](Bullet& b){if(!b.IsBeam())b.active=false;});
            bullets->Update(1.0F/60);
        }
        assert(actor.Random(1)==source.Random(1));
        assert(source.visited[sub]);
        if(sub==21)assert(source.visited[22]&&source.visited[23]);
        else if(sub==27||sub==37)assert(source.visited[28+d]&&source.visited[32+d]&&source.visited[sub==27?36:38]);
        else assert(source.visited[sub+1]&&source.visited[sub+2]);
    }
}
void TestBeamHandlesAndAngularMotion() {
    auto bullets=std::make_unique<BulletSystem>();
    auto* original=bullets->SpawnBeam({}, {1,0,0},2,0.1F,0,10,WHITE);assert(original);
    const auto handle=bullets->MakeHandle(original);assert(bullets->Resolve(handle)==original);
    bullets->ClearEnemyBullets();assert(!bullets->Resolve(handle));
    auto* reused=bullets->SpawnBeam({}, {0,0,1},2,0.1F,0,10,WHITE);assert(reused==original&&!bullets->Resolve(handle));
    const auto second=bullets->MakeHandle(reused);bullets->Reset();
    assert(bullets->SpawnBeam({}, {0,0,1},2,0.1F,0,10,WHITE)==original&&!bullets->Resolve(second));
    auto other=std::make_unique<BulletSystem>();assert(!other->Resolve(handle));
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({},100,7);actor.Start(BeamProbe(actor));bullets->Reset();
    actor.Update(1.0F/60,{},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& beam){Near(beam.position.x,std::sqrt(0.5F));Near(beam.position.y,std::sqrt(0.5F));Near(beam.beamLength,5);});
    // 刻意在两次行为更新之间复用同一池槽；旧光束句柄不能移动新弹或将其视为有效光束。
    bullets->Reset();auto* replacement=bullets->Spawn(BulletOwner::Enemy,{3,2,1},{},0.1F,WHITE,stage4::kBlueBullet);
    actor.Update(1.0F/60,{},*bullets);assert(actor.soundEvents.size()==1&&actor.soundEvents.front()==2&&replacement->position.x==3);
    actor.Update(1.0F/60,{},*bullets);assert(replacement->active);
    // 金属性旋转弹在 128 内容帧后停止转向，半帧积分不会缩短或延长持续时间。
    for(int fps:{60,120}) {
        bullets->Reset();skill::State state;state.rankInfluence={};state.projectile.angularVelocity=1.47262156F;state.projectile.angularFrames=128;
        state.Fire({skill::Formation::FixedRing,7,0,1,1,2,0,0,0,32},{},{},*bullets);
        Bullet* shot=nullptr;bullets->Pool().ForEachActive([&](Bullet& bullet){shot=&bullet;});assert(shot);
        for(int frame=0;frame<fps*3;++frame)bullets->Update(1.0F/fps);
        Near(shot->angularSeconds,0);Near(shot->velocity.x,2*std::cos(1.47262156F*128/60));Near(shot->velocity.z,-2*std::sin(1.47262156F*128/60));
        const auto velocity=shot->velocity;bullets->Update(1.0F/fps);assert(shot->velocity.x==velocity.x&&shot->velocity.z==velocity.z);
    }
}
void TestExtendedProjectileMotion() {
    auto bullets=std::make_unique<BulletSystem>();
    // 原生显现模板先推进到时钟1；最后一帧显现位移与运动时钟0重叠，随后第17帧清启动项、第18帧才加速。
    for(bool acceleration:{false,true}) {
        bullets->Reset();skill::State state;state.rankInfluence={};
        state.projectile={.acceleration=0.192857146F,.angularVelocity=0.7363108F,.angularFrames=90,
            .speedAcceleration=0.48214286F,.accelerationFrames=60,.sampleAtStepStart=true};
        state.Fire({skill::Formation::FixedRing,3,14,1,1,1,0,0,0,acceleration ? 19 : 37},{},{},*bullets);
        Bullet* bullet=nullptr;bullets->Pool().ForEachActive([&](Bullet& b){bullet=&b;});assert(bullet);
        float angle=0,speed=1,vx=1,vz=0,x=0,z=0;
        const int spawn=acceleration ? 10 : 16;
        for(int frame=0;frame<130;++frame) {
            if(frame<spawn) {x+=1.0F/60*(acceleration ? 0.5F : 0.4F);}
            if(frame>=spawn-1) {
                const int timer=frame-(spawn-1);
                if(timer<=16) {vx=1+13.3928571F*(1-timer/16.0F);vz=0;}
                else if(timer>=18) {
                    if(acceleration) {if(timer<60)vx+=0.192857146F/60;}
                    else if(timer<90) {angle+=0.7363108F/60;speed+=0.48214286F/60;vx=speed*std::cos(angle);vz=-speed*std::sin(angle);}
                }
                x+=vx/60;z+=vz/60;
            }
            bullets->Update(1.0F/60);assert(bullet->active);Near(bullet->position.x,x);Near(bullet->position.z,z);
            if(frame>=spawn-1) {Near(bullet->velocity.x,vx);Near(bullet->velocity.z,vz);}
        }
    }
    for(bool absolute:{false,true})for(int fps:{60,120}) {
        bullets->Reset();skill::State state;state.rankInfluence={};state.spatial=true;
        state.projectile={.delayFrames=50,.turnAngle=pi/2,.turnSpeed=1,.turnCount=2,.sampleAtStepStart=true};
        state.Fire({skill::Formation::FixedRing,1,12,1,1,1,0,0,0,absolute ? 256 : 64},{},{},*bullets);
        Bullet* bullet=nullptr;bullets->Pool().ForEachActive([&](Bullet& b){bullet=&b;});assert(bullet);
        const auto initial=bullet->velocity;
        for(int step=0;step<=fps*2;++step) {
            bullets->Update(1.0F/fps);
            const int expected=step*60/fps>=100 ? 2 : step*60/fps>=50 ? 1 : 0;
            assert(bullet->turnCompleted==expected&&bullet->turnApplied==(expected==2));
            if(step*60==100*fps) {
                const auto direction=absolute ? bullet->turnFixedDirection : BulletSystem::RotateAroundAxis(initial,bullet->rotationAxis,pi);
                Near(bullet->velocity.x,direction.x);Near(bullet->velocity.y,direction.y);Near(bullet->velocity.z,direction.z);
            }
        }
    }
    bullets->Reset();skill::State state;state.rankInfluence={};state.spatial=true;
    state.Fire({.formation=skill::Formation::AimedFan,.count=1,.firstSpeed=1,.aimAxisOffset=pi},{},{0,0,-4},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& b){Near(b.velocity.z,1);Near(b.velocity.y,0);});
    for(int fps:{60,120}) {
        bullets->Reset();auto* beam=state.Fire(skill::Laser{.length=4.28571415F,.width=0.267857134F,.grow=0,.sustain=9999,
            .fade=30,.warning=0,.harmlessTail=30,.speed=10.7142859F,.direction=Vector3{0,0,-1},.initialLength=0},{},{},*bullets);
        assert(beam&&beam->IsBeam()&&beam->beamLength==0);
        for(int step=1;step<=fps;++step) {
            bullets->Update(1.0F/fps);const float end=10.7142859F*step/fps;
            Near(beam->beamLength,std::min(end,4.28571415F));Near(beam->position.z,-std::max(0.0F,end-4.28571415F));
            Near(beam->BeamEnd().z,-end);Near(beam->beamAnchor.z,0);assert(beam->active);
        }
    }
}
void TestSpellClockWitnesses() {

    const std::array<behavior::Entry,13> entries{bosses::PatchouliBoss::Behaviors::AgniShine,bosses::PatchouliBoss::Behaviors::AgniShineAdvanced,bosses::PatchouliBoss::Behaviors::PrincessUndine,bosses::PatchouliBoss::Behaviors::SylphyHorn,bosses::PatchouliBoss::Behaviors::SylphyHornAdvanced,
        bosses::PatchouliBoss::Behaviors::LazyTrilithon,bosses::PatchouliBoss::Behaviors::LazyTrilithonAdvanced,bosses::PatchouliBoss::Behaviors::MetalFatigue,bosses::PatchouliBoss::Behaviors::LavaCromlech,bosses::PatchouliBoss::Behaviors::ForestBlaze,bosses::PatchouliBoss::Behaviors::WaterElf,bosses::PatchouliBoss::Behaviors::MercuryPoison,bosses::PatchouliBoss::Behaviors::EmeraldMegalith};
    auto bullets=std::make_unique<BulletSystem>();
    for(int variant=0;variant<13;++variant)for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        behavior::Actor actor(rate);actor.Reset({0,0,4},1000,7);stage4::ConfigureBehavior(actor,false);actor.Start(entries[variant](actor));
        for(int frame=0;frame<700;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,0,-6},*bullets);int actual=0,beams=0;
            bullets->Pool().ForEachActive([&](const Bullet& bullet){if(bullet.IsBeam())++beams;else ++actual;});
            int expected=0,expectedBeams=0;const int time=frame-120;
            if(time>=0)switch(variant) {
                case 0:case 1: {const int cycle=time/170,t=time%170;
                    if(t<=32&&t%8==0)expected=(cycle/3+(variant==0 ? 11 : 8))*(variant==0 ? 3 : 4);break;}
                case 2: {const int cycle=time/250,t=time%250;
                    if(t<120) {if(t%10==0)expectedBeams=3;if((t+1)%22==0)expected=32;}
                    if(t>=120&&t<=190&&(t-120)%10==0)expected=11+cycle;break;}
                case 3:case 4:
                    expected=(time%10==0||time%10==6 ? 1 : 0)+(time%10==2||time%10==6 ? 1 : 0)+(time%30==0 ? (variant==3 ? 10 : 15) : 0);break;
                case 5:case 6:expected=time%(variant==5 ? 12 : 9)==0 ? 10 : 0;break;
                case 7:expected=time%40==0 ? 64 : 0;break;
                case 8: {const int cycle=time/154,t=time%154;
                    if(t<=56&&t%8==0)expected=24;
                    if(t>=124&&t<=144&&(t-124)%10==0)expected=12+cycle*2;break;}
                case 9:expected=(time%22==0||time%22==12 ? 1 : 0)+(time%22==2||time%22==12 ? 1 : 0)+(time%44==0 ? 22 : 0);break;
                case 10: {const int cycle=time/70,t=time%70;
                    if(t==0)expected=58;if(t==(cycle==0 ? 19 : 9))expected+=10;if(t==39||t==59)expected+=40;break;}
                case 11:if(time%370<320&&time%370%20==0)expected=(time/370+15)*2;break;
                case 12:expected=time%20==0 ? 1 : time%20==1 ? 12 : 0;break;
            }
            if(actual!=expected||beams!=expectedBeams)std::cerr<<"Stage 4 spell "<<variant<<" Hz "<<static_cast<int>(rate)<<" frame "<<frame<<" got "<<actual<<'/'<<beams<<" expected "<<expected<<'/'<<expectedBeams<<'\n';
            assert(actual==expected&&beams==expectedBeams);assert(actor.CanTakeDamage()==(frame>=120));
        }
    }
}
void TestBossBranchesAndLifecycle() {
    auto bullets=std::make_unique<BulletSystem>();
    constexpr std::array<std::array<int,4>,4> cards{{{39,58,67,78},{46,51,75,78},{50,40,71,82},{57,63,82,75}}};
    constexpr std::array<float,4> first{1600,1600,1600,1800},second{1500,1600,1700,1800};
    for(int shot=0;shot<4;++shot)for(bool timeout:{false,true}) {
        bosses::PatchouliBoss boss(behavior::UpdateRate::Hz60,false);boss.StartBoss(stage4::PatchouliFinalConfig(static_cast<ShotType>(shot)));
        assert(!boss.CanTakeDamage()&&!boss.SpellName()&&boss.attack==0);
        for(int frame=0;frame<100;++frame)assert(!boss.Update(1.0F/60,{2,0,-6},*bullets));
        boss.ResumeCombat();assert(boss.attack==1&&boss.health==17000&&!boss.CanTakeDamage());
        int card=0;
        for(int phase=1;phase<=6;++phase) {
            assert(boss.attack==phase);assert(boss.HealthBar().count==2);
            const int prepare=phase==1||phase==3 ? 0 : phase>=5 ? 240 : 120;
            for(int frame=0;frame<=prepare;++frame) {bullets->Reset();assert(!boss.Update(1.0F/60,{2,0,-6},*bullets));}
            assert(boss.CanTakeDamage());
            if(phase!=1&&phase!=3) {
                const int number=cards[shot][card++];assert(boss.SpellName()&&boss.SpellCardNumber()==number);
                bool found=false;for(const auto& entry:spell::Cards())if(entry.number==number) {found=true;assert(entry.stage==3&&entry.difficulty==1);}
                assert(found);
            }
            if(timeout) {
                bool ended=false;
                for(int frame=0;frame<2600&&!ended;++frame) {bullets->Reset();ended=boss.Update(1.0F/60,{2,0,-6},*bullets);}
                assert(ended&&!boss.SkillDefeated());
            } else {
                boss.health=phase==1 ? first[shot]-1 : phase==3 ? second[shot]-1 : phase==5 ? 1699 : 0;
                assert(boss.ShouldAdvanceFromDamage());assert(boss.SkillDefeated()==(phase!=1&&phase!=3));
            }
            assert(boss.AdvanceSkill());assert(!boss.CanTakeDamage());
        }
        assert(!boss.SpellName());bool ended=false;
        for(int frame=0;frame<70&&!ended;++frame)ended=boss.Update(1.0F/60,{2,0,-6},*bullets);
        assert(ended&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==!timeout);
    }
}
/// @brief 独立源时限/出生生命与四难度工厂贯通；击破/超时均须等到实际退场才结束。
void TestKoakumaDifficultyLifecycle() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot)for(bool timeout:{false,true}) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto programs=stage4::reference::ProgramsForDifficulty(d);
        const auto time=std::find_if(programs[21].begin(),programs[21].end(),[](const auto& i){return i.opcode==115;});
        assert(time!=programs[21].end());
        auto config=stage4::KoakumaMidbossConfig(static_cast<ShotType>(shot),difficulty);assert(config.difficulty==difficulty);
        bosses::KoakumaBoss boss;boss.StartBoss(config);assert(boss.health==7000&&boss.TimeLimit()==time->args[0]/60);
        for(int frame=0;frame<31;++frame) {bullets->Reset();assert(!boss.Update(1.0F/60,{2,3,-6},*bullets));}
        assert(boss.CanTakeDamage());
        if(timeout) {
            bool ended=false;for(int frame=31;frame<=time->args[0]+1&&!ended;++frame) {bullets->Reset();ended=boss.Update(1.0F/60,{2,3,-6},*bullets);}
            assert(ended&&!boss.SkillDefeated());
        } else {boss.health=0;assert(boss.ShouldAdvanceFromDamage());}
        assert(boss.AdvanceSkill());bullets->Reset();assert(boss.Update(1.0F/120,{},*bullets));
        assert(!boss.AdvanceSkill());
        assert(!boss.active&&boss.Defeated()==!timeout);
        assert(boss.ItemEvents().size()==(timeout?0U:1U));assert(boss.DropEvents().size()==(timeout?0U:1U));
        auto factory=stage4::kKoakumaMidboss.create(static_cast<ShotType>(shot),difficulty);
        assert(factory&&factory->health==7000);
    }
}
/// @brief 非符按源条件流取得四机体生命/阈值；单元素符卡逐难度验证宣告、120 帧保护和击破/超时完整退场。
void TestPatchouliDifficultySkills() {
    using P=bosses::PatchouliBoss;using B=P::Behavior;
    P inspector;auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        for(bool second:{false,true}) {
            stage4::reference::CombatReference source(d,shot,second?37:27,{192,128,0},7);source.Tick({245,360,0});
            const auto definition=inspector.DescribeSkill(second?B::SecondNonspell:B::FirstNonspell,type,difficulty);
            Near(definition.health,source.health-source.threshold);Near(definition.reserveHealth,source.threshold);
            assert(definition.timeoutFrames==source.timeout);
        }
        for(int sub:{42,45,48,51,54,57,60,63,66,69,72,75,78}) {
            const bool standard=sub==42||sub==51||sub==57||sub==66,advanced=sub==45||sub==60||sub==69,wood=sub>=57&&sub<=63,earth=sub>=66&&sub<=72,metal=sub>=75,water=sub==51||sub==54;
            const auto selected=sub==42?B::AgniShine:sub==45?B::AgniShineAdvanced:sub==48?B::AgniRadiance:
                sub==51?B::PrincessUndine:sub==54?B::BuryInLake:sub==57?B::SylphyHorn:sub==60?B::SylphyHornAdvanced:sub==63?B::GreenStorm:
                sub==66?B::LazyTrilithon:sub==69?B::LazyTrilithonAdvanced:sub==72?B::TrilithonShake:
                sub==75?B::MetalFatigue:B::SilverDragon;
            const bool available=sub==75?d==1:standard?d<2:advanced?d>0:d>=2;
            if(!available) {
                bool rejected=false;try {inspector.DescribeSkill(selected,type,difficulty);}catch(const std::invalid_argument&) {rejected=true;}
                assert(rejected);continue;
            }
            const int healthSub=water?(d==0?37:27):standard?(d==0?37:27):advanced?(d==1?37:27):37;
            const int sourceShot=water?1:metal?3:earth?(standard?3:advanced?(d==1?0:3):0):wood?(standard?2:advanced?(d==1?1:2):1):sub==48||(sub==45&&d==1)?2:0;
            stage4::reference::CombatReference health(d,sourceShot,healthSub,{192,128,0},7);health.Tick({245,360,0});
            stage4::reference::CombatReference source(d,sourceShot,sub,{192,128,0},7);source.Tick({245,360,0});
            const auto definition=inspector.DescribeSkill(selected,type,difficulty);
            Near(definition.health,health.threshold);assert(definition.reserveHealth==0&&definition.timeoutFrames==source.timeout);
            assert(definition.spellCard==source.card+1);
            const auto card=std::find_if(spell::Cards().begin(),spell::Cards().end(),[&](const auto& c){return c.number==definition.spellCard;});
            assert(card!=spell::Cards().end()&&card->difficulty==d&&card->stage==3);
            for(bool timeout:{false,true}) {
                auto config=stage4::PatchouliFinalConfig(type);config.difficulty=difficulty;config.intro={};
                config.position={0,0,4.28571415F};config.phases={{{selected},{{{0,1,WHITE}}}}};
                P boss;boss.StartBoss(config);Near(boss.health,health.threshold);
                for(int frame=0;frame<=120;++frame) {
                    bullets->Reset();assert(!boss.Update(1.0F/60,{2,3,-6},*bullets));
                    assert(boss.CanTakeDamage()==(frame>=120));
                }
                assert(boss.SpellCardNumber()==source.card+1);
                if(timeout) {
                    bool ended=false;for(int frame=121;frame<=source.timeout+1&&!ended;++frame) {bullets->Reset();ended=boss.Update(1.0F/60,{2,3,-6},*bullets);}
                    assert(ended&&!boss.SkillDefeated());
                } else {boss.health=0;assert(boss.ShouldAdvanceFromDamage()&&boss.SkillDefeated());}
                assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());
                bool ended=false;for(int frame=0;frame<200&&!ended;++frame) {bullets->Reset();ended=boss.Update(1.0F/60,{},*bullets);}
                assert(ended&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==!timeout);
            }
        }
    }
}
/// @brief 从源难度/机体分支及原生三列表建立完整阶段期望，覆盖默认 120 Hz 的正 HP 阈值、延迟宣告及两路退场。
void TestPatchouliDifficultyEncounters() {
    using P=bosses::PatchouliBoss;
    struct Expected {int health,threshold,timeout,card,prepare;};
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        std::vector<Expected> expected;
        const auto nonspell=[&](int sub) {
            stage4::reference::CombatReference source(d,shot,sub,{192,128,0},7);source.Tick({245,360,0});
            expected.push_back({source.health,source.threshold,source.timeout,0,0});return source.healthCallback;
        };
        const auto element=[&](int sub,int health) {
            stage4::reference::CombatReference source(d,shot,sub,{192,128,0},7);source.Tick({245,360,0});
            assert(source.card>=0);expected.push_back({health,0,source.timeout,source.card+1,120});
        };
        const int first=nonspell(27);if(d>0)element(first,expected.back().threshold);
        const int second=nonspell(37);element(second,expected.back().threshold);
        int carry=0;
        for(int sub=39;sub<39+(d==0?1:d==1?2:3);++sub) {
            stage4::reference::CombatReference source(d,shot,sub,{192,128,0},7);source.health=carry;
            // 各包装段先等 120 帧才进入准备表、宣告并重置阶段时钟，初始生命/阈值来自包装段。
            for(int frame=0;frame<=120;++frame)source.Tick({245,360,0});
            assert(source.card>=0&&source.timeout==2400);
            expected.push_back({source.health,source.threshold,source.timeout+120,source.card+1,240});carry=source.threshold;
        }
        assert(expected.size()==std::size_t(d==0?4:d==1?6:7));
        const auto config=stage4::PatchouliFinalConfig(type,difficulty);
        assert(config.phases.size()==3&&config.difficulty==difficulty);
        P inspector;std::size_t cursor=0;
        for(const auto& phase:config.phases)for(const auto& skill:phase.skills) {
            const auto& e=expected.at(cursor++);const auto definition=inspector.DescribeSkill(skill.id,type,difficulty);
            Near(definition.NativeHealth(),e.health);Near(definition.reserveHealth,e.threshold);
            assert(definition.timeoutFrames==e.timeout&&definition.spellCard==e.card);
            assert(definition.thresholdDefeatsSpell==(e.card>0&&e.threshold>0));
        }
        assert(cursor==expected.size());
        for(bool timeout:{false,true}) {
            P boss;boss.StartBoss(config);for(int frame=0;frame<100;++frame) {bullets->Reset();boss.Update(1.0F/60,{2,0,-6},*bullets);}
            boss.ResumeCombat();std::size_t n=0;
            for(const auto& phase:config.phases)for(const auto& skill:phase.skills) {
                const auto& e=expected[n++];assert(boss.attack==int(n));Near(boss.health,e.health);
                assert(boss.HealthBar().count==phase.healthBar.segments.size()&&!boss.CanTakeDamage());
                for(int frame=0;frame<=e.prepare&&!boss.CanTakeDamage();++frame) {
                    bullets->Reset();assert(!boss.Update(1.0F/60,{2,0,-6},*bullets));
                    // 上一阶段留下的半帧按契约交付新技能；显示更新序号可能提前一帧，内容时钟不能提前开放保护。
                    if(e.prepare>0&&boss.CanTakeDamage()) {
                        assert(frame>=e.prepare-1);assert(boss.skillTimer>=2&&boss.skillTimer<=2+1.0F/60+1e-5F);
                    }
                    if(e.prepare==240)assert(boss.SpellCardNumber()==(frame+1>=120?e.card:0));
                }
                assert(boss.CanTakeDamage()&&boss.SpellCardNumber()==e.card);
                if(timeout) {
                    bool ended=false;for(int frame=0;frame<e.timeout+1&&!ended;++frame) {bullets->Reset();ended=boss.Update(1.0F/60,{2,0,-6},*bullets);}
                    assert(ended&&!boss.SkillDefeated());
                } else {
                    boss.health=e.threshold>0?e.threshold-1:0;
                    assert(boss.ShouldAdvanceFromDamage()&&boss.SkillDefeated()==(e.threshold==0||e.card>0));
                }
                assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());
            }
            bool ended=false;for(int frame=0;frame<70&&!ended;++frame) {bullets->Reset();ended=boss.Update(1.0F/60,{},*bullets);}
            assert(ended&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==!timeout);
        }
    }
}
/// @brief 逐张验证四面 47 张符卡的命名练习工厂；四机体均须正确宣告、交付弹幕并释放单技能遭遇。
void TestPatchouliPracticeSkills() {
    auto bullets=std::make_unique<BulletSystem>();int count=0;
    for(const auto& card:spell::Cards())if(card.stage==3) {
        ++count;const auto difficulty=*native_combat::DifficultyFromIndex(card.difficulty);stage4::Stage4Content content(difficulty);
        for(int shot=0;shot<4;++shot) {
            auto boss=content.CreateBoss(content.PracticeBoss(card.number),static_cast<ShotType>(shot));
            assert(boss->attack==1&&!boss->CanTakeDamage()&&boss->HealthBar().count==1);
            std::size_t peak=0;int ticks=0;
            for(;ticks<720&&(!boss->CanTakeDamage()||peak==0);++ticks) {
                bullets->Reset();assert(!boss->Update(1.0F/120,{2,3,-6},*bullets));
                peak=std::max(peak,bullets->Pool().ActiveCount());
            }
            assert(ticks<720&&peak>0&&boss->SpellCardNumber()==card.number);
            assert(std::string_view(boss->SpellName())==card.name);
            boss->health=0;assert(boss->ShouldAdvanceFromDamage()&&boss->SkillDefeated());
            assert(boss->AdvanceSkill());bool ended=false;
            for(int tick=0;tick<200&&!ended;++tick) {bullets->Reset();ended=boss->Update(1.0F/120,{},*bullets);}
            assert(ended&&!boss->AdvanceSkill()&&!boss->active&&boss->Defeated());
        }
        for(int number:{0,37,85,131}) {
            bool rejected=false;try {(void)content.PracticeBoss(number);}catch(const std::invalid_argument&) {rejected=true;}
            assert(rejected);
        }
        const auto other=*native_combat::DifficultyFromIndex((card.difficulty+1)%4);stage4::Stage4Content wrong(other);
        bool rejected=false;try {(void)wrong.PracticeBoss(card.number);}catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    }
    assert(count==47);
}
void TestEntranceSourceBoundary() {
    auto bullets=std::make_unique<BulletSystem>();bosses::PatchouliBoss boss(behavior::UpdateRate::Hz60,false);boss.StartBoss(stage4::PatchouliFinalConfig());
    for(int frame=0;frame<100;++frame) {
        boss.Update(1.0F/60,{},*bullets);
        const float ratio=frame>=59 ? 1 : 1-std::pow(1-frame/60.0F,2.0F);
        Near(boss.position.x,(64+128*ratio-192)*20/448);Near(boss.position.z,10-(-32+160*ratio)*20/448);
        assert(!boss.CanTakeDamage()&&boss.attack==0);
    }
}
/// @brief 四难度全部普通配方按固定 120 Hz 分批交付，显示帧率不能改变轨迹、弹数或随机流。
void TestOrdinarySpatialDisplayRates() {
    const std::array patterns{stage4::kDescendingSpawner,stage4::kCrossingSpawner,stage4::kLaserSpawner,
        stage4::kHookFanShooter,stage4::kSwerveFanShooter,stage4::kHookLayerShooter,stage4::kSwerveLayerShooter,
        stage4::kDescendingFanShooter,stage4::kCrossingFanShooter,stage4::kPassingHookCrystal,stage4::kPassingRisingCrystal,
        stage4::kStrongSpiralCrystal,stage4::kGentleSpiralCrystal,stage4::kGreenRingEmitter,
        stage4::kBlueRingEmitter,stage4::kOffsetRingEmitter,stage4::kHiddenRandomEmitter,stage4::kHiddenLaserEmitter};
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d)for(const auto& pattern:patterns) {
        Vector3 position{};float random=0;std::vector<Bullet> expected;
        for(int fps:{120,30,60,144,240}) {
            auto spawn=Spawn(pattern);spawn.difficulty=*native_combat::DifficultyFromIndex(d);spawn.pattern.spatialShots=true;
            behavior::Actor actor;actor.Activate(spawn);bullets->Reset();
            for(int frame=0;frame<fps*4;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
            const float next=actor.Random(1);assert(!bullets->Pool().DroppedSpawns());
            if(fps==120) {position=actor.Position();random=next;expected=actual;}
            else {
                assert(next==random&&actual.size()==expected.size());
                Near(actor.Position().x,position.x,1e-5F);Near(actor.Position().y,position.y,1e-5F);Near(actor.Position().z,position.z,1e-5F);
                for(std::size_t i=0;i<actual.size();++i) {
                    assert(actual[i].nativeType==expected[i].nativeType&&actual[i].nativeColor==expected[i].nativeColor);
                    Near(actual[i].position.x,expected[i].position.x,1e-5F);Near(actual[i].position.y,expected[i].position.y,1e-5F);Near(actual[i].position.z,expected[i].position.z,1e-5F);
                    Near(actual[i].velocity.x,expected[i].velocity.x,1e-5F);Near(actual[i].velocity.y,expected[i].velocity.y,1e-5F);Near(actual[i].velocity.z,expected[i].velocity.z,1e-5F);
                }
            }
        }
    }
}
void TestSpatialDisplayFrameRates() {
    using P=bosses::PatchouliBoss::Behaviors;
    const std::array<behavior::Entry,21> entries{bosses::KoakumaBoss::Behaviors::Encounter,P::FirstNonspell,P::SecondNonspell,
        P::AgniShine,P::AgniShineAdvanced,P::AgniRadiance,P::GreenStorm,P::TrilithonShake,P::SilverDragon,P::BuryInLake,P::PrincessUndine,P::SylphyHorn,
        P::SylphyHornAdvanced,P::LazyTrilithon,P::LazyTrilithonAdvanced,P::MetalFatigue,P::LavaCromlech,
        P::ForestBlaze,P::WaterElf,P::MercuryPoison,P::EmeraldMegalith};
    struct Snapshot {
        Vector3 position,velocity,beamDirection,acceleration,turnDirection;
        int type,color,turns;bool aimed,absolute;
        float angularVelocity,angularSeconds,beamLength,maxAge;
        double accelerationSeconds;
        float turnDelay,turnAngle,turnSpeed,warning,dangerEnd;
    };
    for(int d=0;d<4;++d)for(auto entry:entries) {
        const bool nonspell=entry==P::FirstNonspell||entry==P::SecondNonspell;
        // 单元素只运行有效难度；组合攻击的原生差分已覆盖四难度，Easy 不存在 Mercury Poison。
        const bool standard=entry==P::AgniShine||entry==P::SylphyHorn||entry==P::LazyTrilithon||entry==P::PrincessUndine;
        const bool advanced=entry==P::AgniShineAdvanced||entry==P::SylphyHornAdvanced||entry==P::LazyTrilithonAdvanced;
        const bool high=entry==P::AgniRadiance||entry==P::GreenStorm||entry==P::TrilithonShake||entry==P::SilverDragon||entry==P::BuryInLake;
        if(entry==P::MetalFatigue&&d!=1)continue;
        if(entry==P::MercuryPoison&&d==0)continue;
        if((standard&&d>=2)||(advanced&&d==0)||(high&&d<2))continue;
        std::vector<Snapshot> expected;Vector3 position{};float random=0;int animation=-1;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,4},7000,7);
            actor.difficulty=*native_combat::DifficultyFromIndex(d);
            stage4::ConfigureBehavior(actor,true);actor.Start(entry(actor));
            // 非符覆盖反向旋转；组合覆盖游走、跨轮周期发射与计数复位。冻结积分只比较行为分批交付。
            const bool combination=entry==P::LavaCromlech||entry==P::ForestBlaze||entry==P::WaterElf||entry==P::MercuryPoison||entry==P::EmeraldMegalith;
            for(int frame=0;frame<fps*(nonspell||combination?12:4);++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Snapshot> actual;
            bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back({b.position,b.velocity,b.beamDirection,b.acceleration,b.turnFixedDirection,
                b.nativeType,b.nativeColor,b.turnCount,b.turnAimed,b.turnAbsolute,b.angularVelocity,b.angularSeconds,b.beamLength,b.maxAge,
                b.accelerationSeconds,b.turnDelaySeconds,b.turnAngle,b.turnSpeed,b.warningSeconds,b.dangerEndSeconds});});
            const float nextRandom=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=nextRandom;animation=actor.animationScript;}
            else {
                assert(actual.size()==expected.size()&&nextRandom==random&&animation==actor.animationScript);
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                for(std::size_t i=0;i<actual.size();++i) {
                    const auto& a=actual[i];const auto& b=expected[i];assert(a.type==b.type&&a.color==b.color&&a.turns==b.turns&&a.aimed==b.aimed&&a.absolute==b.absolute);
                    Near(a.position.x,b.position.x);Near(a.position.y,b.position.y);Near(a.position.z,b.position.z);
                    Near(a.velocity.x,b.velocity.x);Near(a.velocity.y,b.velocity.y);Near(a.velocity.z,b.velocity.z);
                    Near(a.beamDirection.x,b.beamDirection.x,1e-5F);Near(a.beamDirection.y,b.beamDirection.y,1e-5F);Near(a.beamDirection.z,b.beamDirection.z,1e-5F);
                    Near(a.angularVelocity,b.angularVelocity,1e-5F);Near(a.angularSeconds,b.angularSeconds,1e-5F);
                    Near(a.beamLength,b.beamLength);Near(a.maxAge,b.maxAge,1e-5F);
                    Near(a.acceleration.x,b.acceleration.x,1e-4F);Near(a.acceleration.y,b.acceleration.y,1e-4F);Near(a.acceleration.z,b.acceleration.z,1e-4F);
                    Near(a.turnDirection.x,b.turnDirection.x,1e-5F);Near(a.turnDirection.y,b.turnDirection.y,1e-5F);Near(a.turnDirection.z,b.turnDirection.z,1e-5F);
                    Near(a.accelerationSeconds,b.accelerationSeconds,1e-5F);Near(a.turnDelay,b.turnDelay,1e-5F);
                    Near(a.turnAngle,b.turnAngle,1e-5F);Near(a.turnSpeed,b.turnSpeed,1e-4F);
                    Near(a.warning,b.warning,1e-5F);Near(a.dangerEnd,b.dangerEnd,1e-5F);
                }
            }
            assert(bullets->Pool().DroppedSpawns()==0);
        }
    }
}
}
/// @brief 四面独立源参数/时钟、四机体生命周期与 120 Hz 显示帧率一致性；图形整关另由生命周期目标验证。
/// @brief 单独运行四难度完整阶段和符卡练习对照；全量四面测试也执行同一验收。
void RunStage4EncounterTests() {TestPatchouliDifficultyEncounters();TestPatchouliPracticeSkills();}
/// @brief 单独运行102/118的源请求、退场和显示时钟对照；全量四面回归也执行。
void RunStage4EffectTests() {TestPatchouliEffectReference();TestPatchouliEffectDisplayRates();}
void RunStage4Tests() {RunStage4EffectTests();TestBirthsAndSourceMovement();TestPeriodicCrystals();TestDifficultyOrdinaryShots();TestSpawnerAndHiddenEmitter();TestSpiralAndRingEmitters();TestBeamHandlesAndAngularMotion();TestPersistentEmitter();TestEmitterSoundFlags();TestDifficultyCombatReference();TestBossPreludeWitnesses();TestExtendedProjectileMotion();TestSpellClockWitnesses();TestBossBranchesAndLifecycle();TestKoakumaDifficultyLifecycle();TestPatchouliDifficultySkills();TestPatchouliDifficultyEncounters();TestPatchouliPracticeSkills();TestEntranceSourceBoundary();TestOrdinarySpatialDisplayRates();TestSpatialDisplayFrameRates();}

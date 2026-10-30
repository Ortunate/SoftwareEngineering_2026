// 本文件检查六面出生、独立像素轨迹、MSG 放行、Boss 阶段、星阵/地雷和空间回收；图形另由生命周期目标验证。
#include "boss_fixture.hpp"
#include "stage6_ordinary_reference.hpp"
#include "stage6_combat_reference.hpp"
#include "stages/stage6/data.hpp"
#include "stages/stage6/content.hpp"
#include "app/spell_practice.hpp"
#include "stages/stage6/behaviors.hpp"
#include "stages/stage6/boss_config.hpp"
#include "gameplay/bosses/sakuya.hpp"
#include "gameplay/bosses/remilia.hpp"
#include "stages/stage6/ids.hpp"
#include "render/dialogue_motion.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>;
void Near(float a,float b,float tolerance=5e-4F) {if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {std::cerr<<"Stage 6 mismatch "<<a<<" vs "<<b<<'\n';std::abort();}}
unsigned Next(unsigned& state) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
/// @brief 只在参考侧转换像素/帧量；逐步核对环境请求，不从生产发射器生成期望。
void CompareAmbient(const behavior::Actor& actor,const std::vector<reference::AmbientEffect>& expected) {
    std::vector<const effects::Request*> ambient;
    for(const auto& request:actor.effectEvents)if(request.kind==effects::Kind::Ambient)ambient.push_back(&request);
    if(ambient.size()!=expected.size()) {
        std::cerr<<"Stage6 ambient phase "<<actor.phaseFrames<<" count "<<ambient.size()<<'/'<<expected.size()<<'\n';std::abort();
    }
    constexpr float scale=20.0F/448;
    for(std::size_t n=0;n<expected.size();++n) {
        const auto& actual=*ambient[n];const auto& source=expected[n];
        assert(actual.kind==effects::Kind::Ambient&&actual.count==1&&actual.color.r==48&&actual.color.g==48&&actual.color.b==255&&actual.color.a==255);
        Near(actual.position.x,(source.position.x-192)*scale);Near(actual.position.y,-source.position.z*scale);Near(actual.position.z,10-source.position.y*scale);
        Near(actual.velocity.x,source.velocity.x*scale*60,1e-4F);Near(actual.velocity.y,-source.velocity.z*scale*60,1e-4F);Near(actual.velocity.z,-source.velocity.y*scale*60,1e-4F);
        Near(actual.acceleration.x,source.acceleration.x*scale*3600,1e-4F);Near(actual.acceleration.y,-source.acceleration.z*scale*3600,1e-4F);Near(actual.acceleration.z,-source.acceleration.y*scale*3600,1e-4F);
    }
}
/// @brief 同一步请求按源指令顺序比较，环境回调位于末尾；颜色按整数ARGB拆分，102另验轴与共享世界半径。
void CompareStage6Effects(const behavior::Actor& actor,const std::vector<stage6::reference::ExternalEffect>& expected,
    const std::vector<reference::AmbientEffect>& ambient={}) {
    assert(actor.effectEvents.size()==expected.size()+ambient.size());
    constexpr float scale=20.0F/448;
    for(std::size_t n=0;n<expected.size();++n) {
        const auto& actual=actor.effectEvents[n];const auto& source=expected[n];
        const auto kind=source.type==3?effects::Kind::Burst:source.type==12?effects::Kind::Flash:
            source.type==13?effects::Kind::Attached:source.type==17?effects::Kind::Charge:effects::Kind::SlowCharge;
        assert(source.type==3||source.type==12||source.type==13||source.type==17||source.type==18);
        assert(actual.kind==kind&&actual.count==source.count);
        assert(actual.color.r==((source.color>>16)&255)&&actual.color.g==((source.color>>8)&255)&&actual.color.b==(source.color&255)&&actual.color.a==(source.color>>24));
        Near(actual.position.x,(source.position.x-192)*scale);Near(actual.position.y,-source.position.z*scale);Near(actual.position.z,10-source.position.y*scale);
        if(source.type==13) {
            assert(actual.axis.x==source.axis.x&&actual.axis.y==-source.axis.z&&actual.axis.z==-source.axis.y);
            const auto anchor=actual.attachment.lock();assert(anchor);Near(anchor->Radius(),source.radius*scale,1e-5F);
        }
    }
    CompareAmbient(actor,ambient);
}
/// @brief 安装点来自独立源122，检查入场/两次对白放行、交接连续、隐藏、最终符生命保护及退场停用。
void TestRemiliaAmbientLifecycle() {
    using B=bosses::RemiliaBoss::Behaviors;
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=1;d<4;++d) {
        const auto programs=stage6::reference::ProgramsForDifficulty(d);
        for(const auto [sub,callback]:{std::pair{18,6},std::pair{d==1?61:64,10},std::pair{67,-1}}) {
            int count=0;for(const auto& instruction:programs[sub])if(instruction.opcode==122) {
                assert(instruction.frame==0&&instruction.args[0]==callback);++count;
            }
            assert(count==1);
        }
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},13000,7);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);stage6::ConfigureBehavior(actor,false);
        actor.Start(B::Entrance(actor));
        for(int frame=0;frame<140;++frame) {actor.Update(1.0F/60,{},*bullets);assert(actor.effectEvents.empty());}
        reference::AmbientReference source(7);
        actor.Start(B::Prelude(actor));
        const auto check=[&](int callback,bool visible=true) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            constexpr float scale=20.0F/448;
            const auto p=actor.Position();
            CompareAmbient(actor,source.Step(callback,static_cast<int>(actor.health),visible,{192+p.x/scale,(10-p.z)/scale,-p.y/scale}));
        };
        for(int frame=0;frame<32;++frame)check(6);
        actor.Start(B::FirstNonspell(actor));
        // Active在零生命也执行；此隔离演员没有注册死亡回调。
        actor.health=0;for(int frame=0;frame<20;++frame)check(6);
        actor.visible=false;for(int frame=0;frame<20;++frame)check(6,false);
        actor.visible=true;actor.health=13000;for(int frame=0;frame<20;++frame)check(6);
        actor.Start(d==1?B::RedMagic(actor):B::ScarletGensokyo(actor));
        for(int frame=0;frame<210;++frame) {
            actor.health=frame>=45&&frame<190?0:4700;
            check(10);
        }
        actor.Start(B::Defeated(actor));
        for(int frame=0;frame<=60;++frame)check(-1);
        assert(actor.finished&&!actor.active);
        actor.Reset({},1,7);actor.Start(B::Prelude(actor));
        for(int frame=0;frame<=8;++frame) {actor.Update(1.0F/60,{},*bullets);assert(actor.effectEvents.size()==(frame==8?2U:0U));}
    }
}
/// @brief 直接执行Sub67的六轮五色源退场；60帧末尾最后一组请求与结束音效必须在同一步交付。
void TestRemiliaDefeatedEffects() {
    using B=bosses::RemiliaBoss::Behaviors;
    auto bullets=std::make_unique<BulletSystem>();constexpr float scale=20.0F/448;
    for(unsigned d=1;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},1,seed);stage6::ConfigureBehavior(actor,false);
        actor.motion.bounds={{-7.14285714F,4.64285714F},{7.14285714F,7.85714286F}};actor.motion.clamped=true;
        actor.Start(B::Defeated(actor));stage6::reference::CombatReference source(d,67,{192,112,0},seed,32);
        int requests=0,particles=0;
        for(int frame=0;frame<=60;++frame) {
            source.Tick({245,360,0});actor.Update(1.0F/60,{},*bullets);
            CompareStage6Effects(actor,source.effects,source.ambientEffects);
            assert(actor.soundEvents==source.sounds&&actor.animationScript==source.animation);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            for(const auto& effect:actor.effectEvents) {assert(effect.kind==effects::Kind::Burst);++requests;particles+=effect.count;}
        }
        assert(requests==30&&particles==60&&actor.finished&&actor.Random(1)==source.Random(1));
    }
}
/// @brief 实际命名行为在60/120Hz各自保持五显示FPS相同的请求时刻/参数；两频率共用整数内容事件窗。
void TestRemiliaEffectDisplayRates() {
    using B=bosses::RemiliaBoss::Behaviors;
    for(const auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120})for(int route=0;route<8;++route) {
        std::vector<effects::Request> expected;
        for(int fps:{120,30,60,144,240}) {
            B::PhaseMemory memory;behavior::Actor actor(rate);actor.Reset({0,0,5},13000,7);stage6::ConfigureBehavior(actor,false);
            switch(route) {
                case 0:actor.Start(B::FirstNonspell(actor));break;
                case 1:actor.Start(B::SecondNonspell(actor,memory));break;
                case 2:actor.Start(B::BatNonspell(actor));break;
                case 3:actor.Start(B::StarOfVanities(actor,memory));break;
                case 4:actor.difficulty=native_combat::Difficulty::Hard;actor.Start(B::YoungDemonLord(actor,memory));break;
                case 5:actor.Start(B::ScarletShoot(actor));break;
                case 6:actor.difficulty=native_combat::Difficulty::Hard;actor.Start(B::ScarletMeister(actor));break;
                case 7:actor.Start(B::Defeated(actor));break;
            }
            auto bullets=std::make_unique<BulletSystem>();std::vector<effects::Request> actual;
            for(int frame=0;frame<fps*8;++frame) {
                bullets->Reset();actor.Update(1.0F/fps,{2,0,-6},*bullets);
                for(const auto& request:actor.effectEvents)if(request.kind!=effects::Kind::Ambient)actual.push_back(request);
            }
            assert(!actual.empty());
            if(route==0) {
                assert(actual.size()==5);for(int n=0;n<5;++n)assert(actual[n].contentFrame==n*10&&actual[n].kind==effects::Kind::Attached);
            } else if(route==7)assert(actual.size()==30);
            if(expected.empty())expected=actual;
            else {
                assert(actual.size()==expected.size());
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& e=expected[n];
                    assert(a.kind==e.kind&&a.count==e.count&&a.contentFrame==e.contentFrame&&a.color.r==e.color.r&&a.color.g==e.color.g&&a.color.b==e.color.b&&a.color.a==e.color.a);
                    Near(a.position.x,e.position.x);Near(a.position.y,e.position.y);Near(a.position.z,e.position.z);
                }
            }
        }
    }
}
void TestBirthsAndNeutralMovement() {
    std::vector<const native_combat::HealthRecord*> births;
    for(const auto& r:native_combat::Stage(6)->health)
        if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=8&&r.targetSub!=17)births.push_back(&r);
    assert(births.size()==124&&stage6::Stage6Timeline().size()==135);std::size_t index=0;
    auto bullets=std::make_unique<BulletSystem>();constexpr float scale=20.0F/448;
    for(const auto& event:stage6::Stage6Timeline()) {
        const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
        const auto& source=*births.at(index++);const int sub=source.targetSub;auto spawn=request->actor;
        assert(event.atFrame==source.frame&&request->count==1&&behavior::Valid(spawn));
        assert(spawn.sourceHealth==*source.value&&spawn.sourceScore==source.score&&spawn.sourceItem==source.item);
        assert(spawn.mirrored==(source.opcode==2||source.opcode==6));
        spawn.pattern.spatialShots=false;spawn.retireOutside=false;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
        float x=source.position.x,y=source.position.y,vx=0,vy=0;
        float angle=sub==0||sub==5 ? 0 : sub<=2 ? 0.523598790F : sub<=4 ? -1.04719758F : pi/2;
        float speed=sub==0 ? 0 : sub<=2 ? 4.5F : sub<=4 ? 4 : 2,angular=0;
        unsigned random=spawn.seed,ambientRandom=spawn.seed;int next=-1;
        if(sub>=1&&sub<=4)next=79-static_cast<int>(Next(random)%80);
        for(int frame=0;frame<160;++frame) {
            x+=vx*(spawn.mirrored ? -1 : 1);y+=vy;
            if(sub==0&&frame>0&&frame%40==0) {
                x=static_cast<float>(Next(ambientRandom)>>8)/16777216.0F*384;
                y=static_cast<float>(Next(ambientRandom)>>8)/16777216.0F*224+224;
            }
            if(sub>=1&&sub<=4) {
                if(frame==30)angular=sub<=2 ? -0.0654498488F : 0.0349065848F;
                if(frame==(sub<=2 ? 115 : 90))angular=0;
            }
            if((sub==5||sub==7)&&frame==40) {
                Next(random);angle=static_cast<float>(Next(random)>>8)/16777216.0F*(pi/2)+pi/4;speed=1.79999995F;
            }
            angle+=angular;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
            bullets->Reset();actor.Update(1.0F/60,{0,0,-6},*bullets);
            Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,spawn.position.y);
            int expected=0;
            if(sub>=1&&sub<=4&&frame==next&&frame<(sub<=2 ? 115 : 90)) {expected=sub<=2 ? 12 : 9;next+=80;}
            if((sub==5||sub==7)&&frame==40)expected=60;
            assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
            if(sub==0)assert(!actor.visible&&!actor.CanBeTargeted());
            if(sub==2||sub==4)assert(actor.animationScript==9);
        }
        if(sub==5||sub==7) {assert(actor.Defeat());actor.Update(1.0F/60,{},*bullets);assert(actor.finished&&actor.dropEvents==std::vector<int>{8});}
    }
    assert(index==124);
}
/// @brief 四难度七种普通入口直接对照独立夹具，覆盖两种子、四 rank、镜像、逐发参数和死亡掉落。
void TestDifficultyOrdinaryReference() {
    struct Case {int sub;behavior::Pattern pattern;};
    const std::array<Case,7> cases{{
        {0,stage6::kAmbientPositions},{1,stage6::kBlueHook},{2,stage6::kBlueHook},
        {3,stage6::kRedHook},{4,stage6::kRedHook},{5,stage6::kCrossingRing},{7,stage6::kDescendingRing},
    }};
    constexpr float scale=20.0F/448,speedScale=scale*60;auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(bool mirror:{false,true})for(const auto& c:cases) {
        auto pattern=c.pattern;pattern.spatialShots=false;if(c.sub==2||c.sub==4)pattern.initialAnimation=9;
        behavior::Spawn spawn{.position={0,0,10-32*scale},.pattern=pattern,.difficulty=*native_combat::DifficultyFromIndex(d),
            .mirrored=mirror,.sourceHealth=100,.seed=seed,.visual=stage6::kBlueBullet,.hitRadius=14*scale,.retireOutside=false};
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);stage6::ConfigureBehavior(actor,false);
        stage6::reference::OrdinaryReference source(d,c.sub,{192,32,0},seed,rank,mirror);
        const auto near=[&](float actual,float expected,float tolerance) {
            if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance) {
                std::cerr<<"Stage6 ordinary d "<<d<<" sub "<<c.sub<<" seed "<<seed<<" rank "<<rank<<" mismatch "<<actual<<" vs "<<expected<<'\n';std::abort();
            }
        };
        for(int frame=0;frame<(c.sub==0 ? 2161 : 400);++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            CompareStage6Effects(actor,source.effects);
            near(actor.position.x,(source.position.x-192)*scale,5e-4F);near(actor.position.z,10-source.position.y*scale,5e-4F);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
            assert(actor.visible==source.visible&&actor.interactable==source.interactable&&actor.finished==!source.active);
            if(c.sub!=0)near(actor.hitRadius,source.radius*scale,1e-5F);
            std::vector<const Bullet*> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(&b);});
            if(shots.size()!=source.emissions.size())std::cerr<<"Stage6 ordinary d "<<d<<" sub "<<c.sub<<" frame "<<frame<<" shots "<<shots.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(shots.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                near(b.position.x,(e.position.x-192)*scale,5e-4F);near(b.position.z,10-e.position.y*scale,5e-4F);
                near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);near(b.Velocity().y,0,1e-5F);near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
            }
        }
        assert(actor.Random(1)==source.Random(1));
        if(source.death>=0) {
            assert(actor.Defeat());source.Defeat();source.Tick({245,360,0});bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            assert(actor.finished&&!source.active&&actor.dropEvents==source.drops);
        }
    }
}
/// @brief 四难度普通配方默认 120 Hz 在五种显示帧率下交付一致；冻结弹体积分，只检查行为、空间发射及随机流。
void TestOrdinaryDisplayRates() {
    const std::array patterns{stage6::kAmbientPositions,stage6::kBlueHook,stage6::kRedHook,stage6::kCrossingRing,stage6::kDescendingRing};
    for(int d=0;d<4;++d)for(const auto& pattern:patterns) {
        std::vector<Bullet> expected;Vector3 position{};float random=0;int animation=-1;double phase=0;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;
            behavior::Spawn spawn{.position={0,0,8},.pattern=pattern,.difficulty=*native_combat::DifficultyFromIndex(d),
                .sourceHealth=100,.seed=7,.visual=stage6::kBlueBullet,.retireOutside=false};
            actor.Activate(spawn);actor.SetRank(32);stage6::ConfigureBehavior(actor,true);
            for(int frame=0;frame<fps*6;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
            const float nextRandom=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=nextRandom;animation=actor.animationScript;phase=actor.phaseFrames;}
            else {
                assert(actual.size()==expected.size()&&random==nextRandom&&animation==actor.animationScript&&phase==actor.phaseFrames);
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& b=expected[n];
                    assert(a.nativeType==b.nativeType&&a.nativeColor==b.nativeColor&&a.sampleMotionAtStart==b.sampleMotionAtStart);
                    Near(a.position.x,b.position.x);Near(a.position.y,b.position.y);Near(a.position.z,b.position.z);
                    Near(a.Velocity().x,b.Velocity().x,1e-4F);Near(a.Velocity().y,b.Velocity().y,1e-4F);Near(a.Velocity().z,b.Velocity().z,1e-4F);
                    Near(a.spawnVisualSeconds,b.spawnVisualSeconds,1e-5F);
                }
            }
            assert(!bullets->Pool().DroppedSpawns());
        }
    }
}
/// @brief 道中非符/符卡逐帧对照独立源调用栈，检查瞬移、移动、周期、动画、音效、延迟改向和随机读取。
void TestMidbossDifficultyReference() {
    constexpr float scale=20.0F/448,speedScale=scale*60;auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(int sub:{9,14}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},6000,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);actor.SetRank(rank);stage6::ConfigureBehavior(actor,false);
        actor.SetPoses({128,131,132,129,130}); // 恢复Sub8入场已建立的动画表。
        if(sub==14) {actor.motion.bounds={{-7.14285714F,4.28571415F},{7.14285714F,7.85714286F}};actor.motion.clamped=true;}
        actor.Start(sub==9?bosses::SakuyaBoss::Behaviors::EternalMeekNonspell(actor):bosses::SakuyaBoss::Behaviors::EternalMeek(actor));
        stage6::reference::CombatReference source(d,sub,{192,128,0},seed,rank);
        for(int frame=0;frame<1600;++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            CompareStage6Effects(actor,source.effects,source.ambientEffects);
            std::vector<const Bullet*> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(&b);});
            if(shots.size()!=source.emissions.size()||actor.animationScript!=source.animation||actor.soundEvents!=source.sounds)
                std::cerr<<"Stage6 mid d "<<d<<" sub "<<sub<<" frame "<<frame<<" count "<<shots.size()<<'/'<<source.emissions.size()<<" animation "<<actor.animationScript<<'/'<<source.animation<<'\n';
            assert(shots.size()==source.emissions.size()&&actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
            if(std::abs(actor.position.x-(source.position.x-192)*scale)>5e-4F||std::abs(actor.position.z-(10-source.position.y*scale))>5e-4F)
                std::cerr<<"Stage6 mid position d "<<d<<" sub "<<sub<<" frame "<<frame<<'\n';
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            assert(actor.CanTakeDamage()==(source.damageable&&source.interactable));
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=source.emissions[n];
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                if(std::abs(b.Velocity().x-e.velocity.x*speedScale)>1e-4F||std::abs(b.Velocity().z+e.velocity.y*speedScale)>1e-4F)
                    std::cerr<<"Stage6 mid velocity d "<<d<<" sub "<<sub<<" seed "<<seed<<" rank "<<rank<<" frame "<<frame<<" shot "<<n<<'\n';
                Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().y,0,1e-5F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                Near(b.turnDelaySeconds,e.delay/60,1e-5F);
                if(e.turns>0) {
                    assert(!b.turnAimed&&!b.turnAbsolute&&b.turnCount==e.turns);
                    Near(b.turnAngle,e.turn,1e-5F);Near(b.turnSpeed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,1e-4F);
                }
            }
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief N/H/L 道中工厂分别验证非符超时、整场击破和符卡超时，完整退场后才结束并按路径交付奖励。
void TestMidbossDifficultyLifecycle() {
    using S=bosses::SakuyaBoss;using B=S::Behavior;auto bullets=std::make_unique<BulletSystem>();S inspector;
    for(int d=1;d<4;++d)for(int shot=0;shot<4;++shot) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        stage6::reference::CombatReference nonspell(d,9,{192,128,0},7,16,shot),spell(d,14,{192,128,0},7,16,shot);
        nonspell.Tick({245,360,0});spell.Tick({245,360,0});
        const auto n=inspector.DescribeSkill(B::EternalMeekNonspell,type,difficulty),s=inspector.DescribeSkill(B::EternalMeek,type,difficulty);
        Near(n.NativeHealth(),6000);Near(n.reserveHealth,nonspell.threshold);Near(s.health,nonspell.threshold);
        assert(n.timeoutFrames==nonspell.timeout&&s.timeoutFrames==spell.timeout&&s.spellCard==spell.card+1);
        for(int route=0;route<3;++route) {
            S boss;boss.StartBoss(stage6::SakuyaMidbossConfig(type,difficulty));boss.SetChallengeRank(32);
            for(int tick=0;tick<140;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            boss.ResumeCombat();assert(boss.attack==1&&boss.HealthBar().count==2&&!boss.CanTakeDamage());
            bool ended=false;
            for(int tick=0;tick<500&&!boss.CanTakeDamage();++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            assert(boss.CanTakeDamage());
            if(route==0) {
                for(int tick=0;tick<5000&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                assert(ended&&!boss.SkillDefeated());assert(boss.AdvanceSkill()&&boss.attack==1);
            } else {
                boss.health=nonspell.threshold-1;assert(boss.ShouldAdvanceFromDamage()&&!boss.SkillDefeated());
                assert(boss.AdvanceSkill()&&boss.attack==2&&!boss.CanTakeDamage());Near(boss.health,nonspell.threshold);
                for(int tick=0;tick<500&&!boss.CanTakeDamage();++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
                assert(boss.CanTakeDamage()&&boss.SpellCardNumber()==spell.card+1);
                if(route==2) {
                    for(int tick=0;tick<4000&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                    assert(ended&&!boss.SkillDefeated());
                } else {boss.health=0;assert(boss.ShouldAdvanceFromDamage()&&boss.SkillDefeated());}
                assert(boss.AdvanceSkill());
            }
            ended=false;int drops=0,lives=0;
            for(int tick=0;tick<450&&!ended;++tick) {
                bullets->Reset();ended=boss.Update(1.0F/120,{},*bullets);assert(!boss.CanTakeDamage()&&!boss.WorldTimeStopped());
                for(int count:boss.DropEvents())drops+=count;for(int item:boss.ItemEvents())lives+=item==3;
            }
            assert(ended&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==(route==1));
            assert(drops==(route==1?20:0)&&lives==(route==1?1:0));
        }
    }
    bool rejected=false;
    try {stage6::SakuyaMidbossConfig(ShotType::HomingAmulet,native_combat::Difficulty::Easy);}
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);rejected=false;
    try {inspector.DescribeSkill(B::EternalMeek,ShotType::HomingAmulet,native_combat::Difficulty::Easy);}
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
/// @brief 每只化蝠独立核对源轨迹、半径、随机流与交互；关闭项目场外回收，以免回收策略遮住后续源运动。
void TestBatSource(const behavior::Spawn& request,const stage6::reference::CombatSpawn& source,int rank,BulletSystem& emptyBullets) {
    constexpr float scale=20.0F/448;
    assert(source.sub==31&&request.sourceHealth==source.health&&request.sourceScore==source.score&&request.sourceItem==source.item);
    auto spawn=request;spawn.pattern.spatialShots=false;spawn.retireOutside=false;
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);stage6::ConfigureBehavior(actor,false);
    const int d=native_combat::DifficultyIndex(request.difficulty);
    stage6::reference::CombatReference reference(d,31,source.position,request.seed,rank);
    for(int frame=0;frame<180;++frame) {
        reference.Tick({245,360,0});actor.Update(1.0F/60,{},emptyBullets);
        CompareStage6Effects(actor,reference.effects,reference.ambientEffects);
        Near(actor.position.x,(reference.position.x-192)*scale);Near(actor.position.z,10-reference.position.y*scale);
        Near(actor.hitRadius,reference.radius*scale,1e-5F);
        assert(actor.animationScript==reference.animation&&!actor.interactable&&!reference.interactable&&reference.emissions.empty());
    }
    assert(actor.Random(1)==reference.Random(1)&&!emptyBullets.Pool().ActiveCount());
}
/// @brief 蕾米莉亚四段非符逐帧对照独立源调用栈，检查移动、周期、动画、音效、rank、召唤和随机读取。
void TestRemiliaNonspellDifficultyReference() {
    constexpr float scale=20.0F/448,speedScale=scale*60;auto bullets=std::make_unique<BulletSystem>();auto childBullets=std::make_unique<BulletSystem>();
    for(unsigned d=1;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(int sub:{19,21,25,28})for(int carry:{0,7,14}) {
        if(sub!=21&&carry!=0)continue;
        bosses::RemiliaBoss::Behaviors::PhaseMemory memory;memory.chargingPulsesLeft=carry;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},13000,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);actor.SetRank(rank);stage6::ConfigureBehavior(actor,false);
        actor.SetPoses({160,163,164,161,162});
        if(sub==21)actor.Start(bosses::RemiliaBoss::Behaviors::SecondNonspell(actor,memory));
        else actor.Start(sub==19?bosses::RemiliaBoss::Behaviors::FirstNonspell(actor):
            sub==25?bosses::RemiliaBoss::Behaviors::ThirdNonspell(actor):bosses::RemiliaBoss::Behaviors::BatNonspell(actor));
        stage6::reference::CombatReference source(d,sub,{192,112,0},seed,rank,0,carry);
        unsigned spawnSeed=seed;
        for(int frame=0;frame<1600;++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            CompareStage6Effects(actor,source.effects,source.ambientEffects);
            std::vector<const Bullet*> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(&b);});
            if(shots.size()!=source.emissions.size()||actor.animationScript!=source.animation||actor.soundEvents!=source.sounds)
                std::cerr<<"Stage6 Remilia nonspell d "<<d<<" sub "<<sub<<" carry "<<carry<<" frame "<<frame<<" count "<<shots.size()<<'/'<<source.emissions.size()<<" animation "<<actor.animationScript<<'/'<<source.animation<<'\n';
            assert(shots.size()==source.emissions.size()&&actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.dropEvents==source.drops);
            if(std::abs(actor.position.x-(source.position.x-192)*scale)>5e-4F||std::abs(actor.position.z-(10-source.position.y*scale))>5e-4F)
                std::cerr<<"Stage6 Remilia nonspell position d "<<d<<" sub "<<sub<<" carry "<<carry<<" frame "<<frame<<'\n';
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            assert(actor.CanTakeDamage()==(source.damageable&&source.interactable));
            assert(actor.spawnEvents.size()==source.spawns.size());
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& request=actor.spawnEvents[n];const auto& expected=source.spawns[n];
                assert(request.difficulty==actor.difficulty&&request.seed==++spawnSeed);
                Near(request.position.x,(expected.position.x-192)*scale);Near(request.position.z,10-expected.position.y*scale);
                TestBatSource(request,expected,rank,*childBullets);
            }
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=source.emissions[n];
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                if(std::abs(b.Velocity().x-e.velocity.x*speedScale)>1e-4F||std::abs(b.Velocity().z+e.velocity.y*speedScale)>1e-4F)
                    std::cerr<<"Stage6 Remilia nonspell velocity d "<<d<<" sub "<<sub<<" seed "<<seed<<" rank "<<rank<<" carry "<<carry<<" frame "<<frame<<" shot "<<n<<'\n';
                Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().y,0,1e-5F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                Near(b.turnDelaySeconds,e.delay/60,1e-5F);
                Near(b.acceleration.x,e.acceleration.x*scale*3600,1e-5F);Near(b.acceleration.y,0,1e-5F);Near(b.acceleration.z,-e.acceleration.y*scale*3600,1e-5F);
                if(e.turns>0) {
                    assert(!b.turnAimed&&!b.turnAbsolute&&b.turnCount==e.turns);
                    Near(b.turnAngle,e.turn,1e-5F);Near(b.turnSpeed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,1e-4F);
                }
            }
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief 独立单非符配置验证N/H/L四段技能绑定及默认120 Hz阈值/超时交接，完整关底组合另验。
void TestRemiliaNonspellDifficultyLifecycle() {
    using S=bosses::RemiliaBoss;using B=S::Behavior;
    constexpr std::array entries{B::FirstNonspell,B::SecondNonspell,B::ThirdNonspell,B::BatNonspell};constexpr std::array subs{19,21,25,28};
    auto bullets=std::make_unique<BulletSystem>();S inspector;
    for(int d=1;d<4;++d)for(int shot=0;shot<4;++shot)for(int phase=0;phase<4;++phase) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        stage6::reference::CombatReference source(d,subs[phase],{192,128,0},7,16);source.Tick({245,360,0});
        const auto definition=inspector.DescribeSkill(entries[phase],type,difficulty);
        Near(definition.NativeHealth(),source.health);Near(definition.reserveHealth,source.threshold);assert(definition.timeoutFrames==source.timeout);
        for(bool timeout:{false,true}) {
            auto config=stage6::RemiliaFinalConfig(type);config.difficulty=difficulty;config.resumeSteps.clear();
            config.phases={{{entries[phase]},{{{0,1,WHITE}}}}};S boss;boss.StartBoss(config);boss.SetChallengeRank(32);
            for(int tick=0;tick<140;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            boss.ResumeCombat();assert(boss.attack==1&&boss.HealthBar().count==1);Near(boss.health,source.health);
            bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));assert(boss.CanTakeDamage());
            if(timeout) {
                bool ended=false;for(int tick=0;tick<source.timeout*2+2&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                assert(ended&&!boss.SkillDefeated());Near(boss.health,source.threshold);
            } else {boss.health=source.threshold-1;assert(boss.ShouldAdvanceFromDamage()&&!boss.SkillDefeated());}
            assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());bool ended=false;
            for(int tick=0;tick<150&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
            assert(ended&&!boss.AdvanceSkill()&&!boss.active&&!boss.WorldTimeStopped());
        }
    }
}
// 这里只列已接入并可独立执行的符卡入口；源Sub号不进入生产技能选择。
struct RemiliaSpellCase {bosses::RemiliaBoss::Behavior normal,high;int normalSub,highSub,health;};
constexpr std::array kRemiliaSpellCases{
    RemiliaSpellCase{bosses::RemiliaBoss::Behavior::StarOfVanities,bosses::RemiliaBoss::Behavior::YoungDemonLord,32,35,1200},
    RemiliaSpellCase{bosses::RemiliaBoss::Behavior::ScarletNetherworld,bosses::RemiliaBoss::Behavior::ThousandNeedles,38,41,1600},
    RemiliaSpellCase{bosses::RemiliaBoss::Behavior::ScarletShoot,bosses::RemiliaBoss::Behavior::ScarletMeister,52,55,1300},
    RemiliaSpellCase{bosses::RemiliaBoss::Behavior::VladTepesCurse,bosses::RemiliaBoss::Behavior::VampireIllusion,44,47,1600},
    RemiliaSpellCase{bosses::RemiliaBoss::Behavior::RedMagic,bosses::RemiliaBoss::Behavior::ScarletGensokyo,61,64,4700},
};
/// @brief 已接入符卡对照独立源调用栈及原生星阵展开，逐帧验证光束、加速弹、姿态、音效和随机流。
void TestRemiliaSpellDifficultyReference(int groups=kRemiliaSpellCases.size()) {
    using R=bosses::RemiliaBoss;constexpr float scale=20.0F/448,speedScale=scale*60;
    auto bullets=std::make_unique<BulletSystem>();
    for(int group=0;group<groups;++group)for(unsigned d=1;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(int environment=0;environment<(group>=3?3:1);++environment) {
        R::Behaviors::PhaseMemory memory;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},1200,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);actor.SetRank(rank);stage6::ConfigureBehavior(actor,false);
        actor.SetPoses({160,163,164,161,162});
        // 单符入口继承入场已建立的巡航边界；Prepare本身只改目标，不重新声明边界。
        actor.motion.bounds={{-7.14285714F,4.64285714F},{7.14285714F,7.85714286F}};actor.motion.clamped=true;
        const auto& spell=kRemiliaSpellCases.at(group);
        switch(d==1?spell.normal:spell.high) {
            case R::Behavior::StarOfVanities:actor.Start(R::Behaviors::StarOfVanities(actor,memory));break;
            case R::Behavior::YoungDemonLord:actor.Start(R::Behaviors::YoungDemonLord(actor,memory));break;
            case R::Behavior::ScarletNetherworld:actor.Start(R::Behaviors::ScarletNetherworld(actor));break;
            case R::Behavior::ThousandNeedles:actor.Start(R::Behaviors::ThousandNeedles(actor));break;
            case R::Behavior::ScarletShoot:actor.Start(R::Behaviors::ScarletShoot(actor));break;
            case R::Behavior::ScarletMeister:actor.Start(R::Behaviors::ScarletMeister(actor));break;
            case R::Behavior::VladTepesCurse:actor.Start(R::Behaviors::VladTepesCurse(actor));break;
            case R::Behavior::VampireIllusion:actor.Start(R::Behaviors::VampireIllusion(actor));break;
            case R::Behavior::RedMagic:actor.Start(R::Behaviors::RedMagic(actor));break;
            case R::Behavior::ScarletGensokyo:actor.Start(R::Behaviors::ScarletGensokyo(actor));break;
            default:std::abort();
        }
        stage6::reference::CombatReference source(d,d==1?spell.normalSub:spell.highSub,{192,112,0},seed,rank);
        for(int frame=0;frame<1600;++frame) {
            bullets->Reset();source.environment.clear();
            // 两侧独立接收同一组显式源单位环境，不读取生产池来生成期望；零/持续/间歇候选分别覆盖原生返回分支。
            if(group>=3&&environment>0&&frame>180) {
                if(environment==1||frame%300<80)source.environment.push_back({.position={245,190,0},.velocity={1,0,0},.type=6,.color=3});
                source.environment.push_back({.position={192,112,0},.type=3,.color=1});
                source.environment.push_back({.position={200,200,0},.type=1,.color=1});
                source.environment.push_back({.position={170,160,0},.velocity={0,1,0},.type=4,.color=1});
                source.environment.push_back({.position={160,140,0},.type=2,.color=1,.acceleration={0,0.01F,0},.accelerationFrames=120});
                for(const auto& e:source.environment) {
                    auto* b=bullets->Spawn(BulletOwner::Enemy,{(e.position.x-192)*scale,0,10-e.position.y*scale},
                        {e.velocity.x*speedScale,0,-e.velocity.y*speedScale},0.1F,WHITE,stage6::kBlueBullet);
                    assert(b);b->nativeType=e.type;b->nativeColor=e.color;b->sampleMotionAtStart=true;
                    b->acceleration={e.acceleration.x*scale*3600,0,-e.acceleration.y*scale*3600};
                    if(e.accelerationFrames>=0)b->accelerationSeconds=e.accelerationFrames/60;
                }
            }
            source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            CompareStage6Effects(actor,source.effects,source.ambientEffects);
            const auto& expected=group>=3?source.projectiles:source.emissions;
            std::vector<const Bullet*> shots,beams;bullets->Pool().ForEachActive([&](const Bullet& b){(b.IsBeam()?beams:shots).push_back(&b);});
            if(shots.size()!=expected.size()||beams.size()!=source.beams.size()||actor.animationScript!=source.animation||actor.soundEvents!=source.sounds)
                std::cerr<<"Stage6 spell group "<<group<<" d "<<d<<" seed "<<seed<<" rank "<<rank<<" frame "<<frame<<" shots "<<shots.size()<<'/'<<expected.size()<<" beams "<<beams.size()<<'/'<<source.beams.size()<<" animation "<<actor.animationScript<<'/'<<source.animation<<'\n';
            assert(shots.size()==expected.size()&&beams.size()==source.beams.size());
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.CanTakeDamage()==source.damageable);
            if(std::abs(actor.position.x-(source.position.x-192)*scale)>5e-4F||std::abs(actor.position.z-(10-source.position.y*scale))>5e-4F)
                std::cerr<<"Stage6 spell position group "<<group<<" d "<<d<<" seed "<<seed<<" rank "<<rank<<" frame "<<frame<<'\n';
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            for(std::size_t n=0;n<beams.size();++n) {
                const auto& b=*beams[n];const auto& e=source.beams[n];
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.Direction().x,e.direction.x,1e-5F);Near(b.Direction().y,0,1e-5F);Near(b.Direction().z,-e.direction.y,1e-5F);
                Near(b.beamLength,e.length*scale);Near(b.radius,14*scale);Near(b.beamGrowSeconds,e.grow/60.0F,1e-5F);
                Near(b.warningSeconds,50.0F/60,1e-5F);Near(b.dangerEndSeconds,134.0F/60,1e-5F);Near(b.maxAge,166.0F/60,1e-5F);
            }
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=expected[n];
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().y,0,1e-5F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
                assert(b.nativeType==e.type&&b.nativeColor==e.color);
                assert(b.sampleMotionAtStart==(e.flags!=8||e.accelerationFrames>=0));
                Near(b.acceleration.x,e.acceleration.x*scale*3600,1e-5F);Near(b.acceleration.y,0,1e-5F);Near(b.acceleration.z,-e.acceleration.y*scale*3600,1e-5F);
                if(e.accelerationFrames>=0)Near(b.accelerationSeconds,e.accelerationFrames/60,1e-5F);
                Near(b.spawnVisualSeconds,(e.flags&8?32.0F:e.flags&4?16.0F:e.flags&2?10.0F:0.0F)/60,1e-5F);
                assert(b.polarMotion==(e.angularFrames>0)&&b.bouncesRemaining==e.bounces);
                if(e.angularFrames>0) {
                    Near(b.angularVelocity,e.angularVelocity*60,1e-5F);Near(b.angularSeconds,e.angularFrames/60,1e-5F);
                    Near(b.speedAcceleration,e.speedAcceleration*scale*3600,1e-5F);
                }
                if(e.bounces>0) {
                    assert(b.reflectLowerZ==e.reflectLowerZ);Near(b.bounceSpeed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,1e-4F);
                    Near(b.bounceLower.x,-192*scale);Near(b.bounceUpper.x,192*scale);Near(b.bounceLower.z,-10);Near(b.bounceUpper.z,10);
                    // 源5号针形弹ANM逻辑全尺寸为14×16像素，9号大泡为64×64；半尺寸与碰撞半径分开核对。
                    Near(b.bounceHalfExtents.x,(e.type==5?7:32)*scale);Near(b.bounceHalfExtents.z,(e.type==5?8:32)*scale);
                }
            }
            assert(!bullets->Pool().DroppedSpawns());
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief 已接入符卡按N/H/L和四机体核对原始卡号/时限，并覆盖120 Hz准备保护、击破、超时和完整退场。
void TestRemiliaSpellDifficultyLifecycle(int groups=kRemiliaSpellCases.size()) {
    using R=bosses::RemiliaBoss;using B=R::Behavior;R inspector;auto bullets=std::make_unique<BulletSystem>();
    for(int group=0;group<groups;++group)for(int d=1;d<4;++d)for(int shot=0;shot<4;++shot) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        const auto& spell=kRemiliaSpellCases.at(group);const auto entry=d==1?spell.normal:spell.high;
        stage6::reference::CombatReference source(d,d==1?spell.normalSub:spell.highSub,{192,112,0},7,16,shot);
        for(int frame=0;frame<=(group==4?60:0);++frame)source.Tick({245,360,0});
        const auto definition=inspector.DescribeSkill(entry,type,difficulty);
        Near(definition.health,group==4&&d>=2?5300:spell.health);assert(definition.spellCard==source.card+1&&definition.timeoutFrames==source.timeout);
        for(bool timeout:{false,true}) {
            auto config=stage6::RemiliaFinalConfig(type);config.difficulty=difficulty;config.resumeSteps.clear();
            config.phases={{{entry},{{{0,1,WHITE}}}}};R boss;boss.StartBoss(config);boss.SetChallengeRank(32);
            for(int tick=0;tick<260;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            boss.ResumeCombat();assert(boss.attack==1&&!boss.CanTakeDamage());
            for(int tick=0;tick<(group==4?360:240);++tick) {bullets->Reset();assert(!boss.CanTakeDamage());assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets)&&boss.CanTakeDamage());
            assert(boss.SpellCardNumber()==source.card+1);
            if(timeout) {
                bool ended=false;for(int tick=0;tick<source.timeout*2&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                assert(ended&&!boss.SkillDefeated());
            } else {boss.health=0;assert(boss.ShouldAdvanceFromDamage()&&boss.SkillDefeated());}
            assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());bool ended=false;
            for(int tick=0;tick<150&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
            assert(ended&&!boss.AdvanceSkill()&&!boss.active);
        }
    }
    for(const auto [entry,difficulty]:std::array{std::pair{B::YoungDemonLord,native_combat::Difficulty::Normal},std::pair{B::StarOfVanities,native_combat::Difficulty::Hard},std::pair{B::ThousandNeedles,native_combat::Difficulty::Normal},std::pair{B::ScarletNetherworld,native_combat::Difficulty::Hard},std::pair{B::ScarletMeister,native_combat::Difficulty::Normal},std::pair{B::ScarletShoot,native_combat::Difficulty::Hard},std::pair{B::VampireIllusion,native_combat::Difficulty::Normal},std::pair{B::VladTepesCurse,native_combat::Difficulty::Hard},std::pair{B::ScarletGensokyo,native_combat::Difficulty::Normal},std::pair{B::RedMagic,native_combat::Difficulty::Hard}}) {
        bool rejected=false;try {inspector.DescribeSkill(entry,ShotType::HomingAmulet,difficulty);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
}
/// @brief 完整九技能配置按N/H/L及四机体运行击破/超时，核对显式血条、源生命、卡号、保护交接和真正退场。
void TestRemiliaFinalDifficultyLifecycle() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=1;d<4;++d)for(int shot=0;shot<4;++shot)for(bool timeout:{false,true}) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);
        bosses::RemiliaBoss boss;boss.StartBoss(stage6::RemiliaFinalConfig(static_cast<ShotType>(shot),difficulty));
        const std::array hp{13000,1200,15500,1600,11000,1600,13000,1300,d==1?4700:5300};
        const std::array limits{2700,2400,2700,2400,2700,2400,3600,1800,d==1?7200:8400};
        const std::array reserves{1200,0,1600,0,1600,0,1300,0,0};
        for(int tick=0;tick<260;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
        boss.ResumeCombat();assert(boss.attack==0);boss.Update(1.0F/120,{2,3,-6},*bullets);boss.ResumeCombat();
        for(int attack=1;attack<=9;++attack) {
            assert(boss.attack==attack&&!boss.CanTakeDamage()&&boss.HealthBar().count==(attack<9?2:1));Near(boss.health,hp[attack-1]);
            bool ended=false;int ticks=0;
            while(!boss.CanTakeDamage()&&ticks++<500) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            assert(ticks<500);if(attack%2==0||attack==9)assert(boss.SpellCardNumber()==103+d+(attack==9?4:attack/2-1)*3);
            if(timeout) {
                for(int tick=0;tick<limits[attack-1]*2+2&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                assert(ended&&!boss.SkillDefeated());
            } else {boss.health=reserves[attack-1]-1;assert(boss.ShouldAdvanceFromDamage());assert(boss.SkillDefeated()==(attack%2==0||attack==9));}
            assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());
        }
        bool ended=false;for(int tick=0;tick<150&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
        assert(ended&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==!timeout);
    }
    bool rejected=false;try {stage6::RemiliaFinalConfig(ShotType::HomingAmulet,native_combat::Difficulty::Easy);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
/// @brief 十八张单符卡按四机体验证声明、准备保护、实际发射和完整退场；错误卡号/难度不得回退Normal。
void TestStage6PracticeSkills() {
    auto bullets=std::make_unique<BulletSystem>();int count=0;
    for(const auto& card:spell::Cards())if(card.stage==5) {
        ++count;const auto difficulty=*native_combat::DifficultyFromIndex(card.difficulty);stage6::Stage6Content content(difficulty);
        assert(content.SpellPortraitVisual(card.number)==(card.group==0?stage6::kSakuyaPortraits[1]:content.BossPortraitVisual(2)));
        for(int shot=0;shot<4;++shot) {
            auto boss=content.CreateBoss(content.PracticeBoss(card.number),static_cast<ShotType>(shot));
            assert(boss->attack==1&&!boss->CanTakeDamage()&&boss->HealthBar().count==1);std::size_t peak=0;int ticks=0;
            for(;ticks<1440&&(!boss->CanTakeDamage()||peak==0);++ticks) {
                bullets->Reset();assert(!boss->Update(1.0F/120,{2,3,-6},*bullets));peak=std::max(peak,bullets->Pool().ActiveCount());
            }
            assert(ticks<1440&&peak>0&&boss->SpellCardNumber()==card.number&&std::string_view(boss->SpellName())==card.name);
            assert(boss->CanContactPlayer());
            boss->health=0;assert(boss->ShouldAdvanceFromDamage()&&boss->SkillDefeated());assert(boss->AdvanceSkill());bool ended=false;
            for(int tick=0;tick<500&&!ended;++tick) {bullets->Reset();ended=boss->Update(1.0F/120,{},*bullets);}
            assert(ended&&!boss->AdvanceSkill()&&!boss->active&&boss->Defeated());
        }
        for(int number:{0,100,119,131}) {bool rejected=false;try{(void)content.PracticeBoss(number);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
        for(int d=0;d<4;++d)if(d!=card.difficulty) {
            stage6::Stage6Content wrong(*native_combat::DifficultyFromIndex(d));bool rejected=false;
            try{(void)wrong.PracticeBoss(card.number);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
        }
    }
    assert(count==18);
}
/// @brief MSG1 的创建帧独立取自原分支展开；检查隐藏、30 帧入场、表情与退场，以及跳句后重建。
void TestDialogueMilestones() {
    for(bool marisa:{false,true}) {
        const auto cues=stage6::Stage6DialogueCues(DialogueScene::FinalBoss,false,marisa);
        std::vector<int> signals;for(const auto& cue:cues)if(cue.kind==DialogueCueKind::StageSignal) {assert(cue.value==static_cast<int>(signals.size()+1));signals.push_back(cue.frame);}
        assert((signals==std::vector<int>{861,marisa ? 7866 : 8666}));
        assert(stage6::Stage6Dialogue(DialogueScene::FinalBoss,true,marisa).empty());
        const auto lines=stage6::Stage6Dialogue(DialogueScene::FinalBoss,false,marisa);
        int duration=0;for(const auto& line:lines)duration+=std::lround(line.duration*60);assert(duration==(marisa ? 11106 : 10704));
        for(std::size_t index=1;index<lines.size();++index)assert(lines[index].showBoss);
        std::vector<std::array<int,3>> starts;
        for(const auto& cue:cues)if(cue.kind==DialogueCueKind::PortraitStart)starts.push_back({cue.frame,cue.slot,cue.value});
        assert((starts==std::vector<std::array<int,3>>{{0,0,0},{921,1,0}}));
        ui::DialogueMotion motion;
        const auto seek=[&](int frame) {motion.Seek(cues,frame/60.0F);return motion.Sample(true);};
        assert(seek(920).opacity==0);
        assert(seek(921).slide==1&&motion.Sample(true).opacity==0);
        const auto entering=seek(936);Near(entering.slide,.25F);assert(entering.opacity==1);
        assert(seek(951).slide==0&&motion.Sample(true).opacity==1);
        assert(seek(1071).focus==1&&motion.Sample(true).opacity==1);
        assert(DialogueCueExpressionsAt(cues,1042.0F/60,{0,0})[1]==2);
        assert(DialogueCueExpressionsAt(cues,1844.0F/60,{0,0})[1]==3);
        assert(seek(duration).opacity==0);
        assert(seek(1071).opacity==1); // 跳句或重开后的采样不能继承已经退场的状态。
        const auto mid=stage6::Stage6DialogueCues(DialogueScene::Midboss,false,marisa);
        motion.Seek(mid,30.0F/60);assert(motion.Sample(false).opacity>0&&motion.Sample(true).opacity>0);
    }
    // 测试接收端独立交付各次等待，目录不得把两次恢复或对白中的入场合并到最后一次 EndEvent。
    StageDirector director(stage6::Stage6Timeline());std::vector<int> resumed;std::vector<int> bosses;
    int midDialogue=-1,mainDialogue=-1,waitSignal=-1;bool midEnded=false;
    for(int frame=1;frame<22000;++frame) {
        director.Update(1.0F/60,[&](const StageEvent& event) {
            if(const auto* boss=std::get_if<StartBoss>(&event.action)) {bosses.push_back(frame);if(boss->battle==&stage6::kSakuyaMidboss)assert(frame==2493);return false;}
            if(const auto* dialogue=std::get_if<StartDialogue>(&event.action)) {
                if(dialogue->scene==DialogueScene::Midboss)midDialogue=frame;else if(!dialogue->afterBattle)mainDialogue=frame;return false;
            }
            if(const auto* wait=std::get_if<WaitDialogueSignal>(&event.action)) {waitSignal=wait->signal;return false;}
            if(std::holds_alternative<ResumeBoss>(event.action)) {resumed.push_back(frame);return true;}
            return true;
        });
        if(frame==midDialogue+240&&midDialogue>0)director.EndEvent(2);
        if(frame==4000) {assert(director.EndEvent(1));midEnded=true;}
        if(mainDialogue>0) {
            if(frame==mainDialogue+861) {assert(waitSignal==1);director.EndEvent(6);}
            if(frame==mainDialogue+8666) {assert(waitSignal==2);director.EndEvent(7);}
            if(frame==mainDialogue+10704) {assert(waitSignal==0);director.EndEvent(3);director.EndEvent(8);}
        }
    }
    assert(midEnded&&mainDialogue==4600&&bosses.size()==2&&bosses[1]==5462);
    assert((resumed==std::vector<int>{2734,13267,15305}));
}
void TestBossPhaseLifecycle() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int shot=0;shot<4;++shot)for(bool final:{false,true})for(bool timeout:{false,true}) {
        std::unique_ptr<bosses::ConfiguredBoss> ownedBoss=final ? std::unique_ptr<bosses::ConfiguredBoss>(new bosses::RemiliaBoss(behavior::UpdateRate::Hz60,false)) : std::unique_ptr<bosses::ConfiguredBoss>(new bosses::SakuyaBoss(behavior::UpdateRate::Hz60,false));
        auto& boss=*ownedBoss;boss.StartBoss(final ? stage6::RemiliaFinalConfig(static_cast<ShotType>(shot)) : stage6::SakuyaMidbossConfig(static_cast<ShotType>(shot)));
        for(int frame=0;frame<130;++frame) {boss.Update(1.0F/60,{},*bullets);assert(boss.attack==0&&!boss.CanTakeDamage()&&!boss.CanContactPlayer()&&boss.CanBeTargeted());}
        boss.ResumeCombat();if(final) {assert(boss.attack==0);boss.Update(1.0F/60,{},*bullets);boss.ResumeCombat();}
        const int phases=final ? 9 : timeout ? 1 : 2;
        for(int phase=1;phase<=phases;++phase) {
            assert(boss.attack==phase);bool ready=false;
            for(int frame=0;frame<10000&&!ready;++frame) {
                bullets->Reset();ready=boss.Update(1.0F/60,{},*bullets);
                if(!timeout&&boss.CanTakeDamage()) {boss.health=0;ready=boss.ShouldAdvanceFromDamage();}
            }
            assert(ready);if(!timeout&&(phase%2==0||phase==9))assert(boss.SkillDefeated());
            if(!final&&phase==2)Near(boss.maxHealth,std::array{750.0F,1000.0F,1100.0F,1100.0F}[shot]);
            assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());
        }
        bool ready=false;for(int frame=0;frame<130&&!ready;++frame)ready=boss.Update(1.0F/60,{},*bullets);
        assert(ready&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==!timeout);
    }
}
behavior::Routine StarProbe(behavior::Actor& actor) {bosses::RemiliaBoss::Behaviors::StarLattice(actor,false);co_await actor.At(100);}
void TestNativeStarGeometry() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4},13000,49);
    stage6::ConfigureBehavior(actor,false);actor.Start(StarProbe(actor));actor.Update(1.0F/60,{},*bullets);
    assert(bullets->Pool().ActiveCount()==48);unsigned random=49;const float seed=static_cast<float>(Next(random)>>8)/16777216.0F*2*pi;
    struct Expected {Vector3 position,direction;float length;int grow;};std::vector<Expected> expected;
    constexpr float scale=20.0F/448;
    for(int pass=0;pass<2;++pass) {
        const float base=pass ? -7*pi/8+seed : -pi+seed,delta=pass ? -pi/4 : pi/4;
        std::array<Vector2,8> positions{};for(int i=0;i<8;++i)positions[i]={32*std::cos(base+i*pi/4),32*std::sin(base+i*pi/4)};
        for(int ring=0;ring<3;++ring)for(int i=0;i<8;++i) {
            const float angle=base+ring*delta+i*pi/4,length=ring<2 ? 112 : 480;
            expected.push_back({{positions[i].x*scale,0,4-positions[i].y*scale},{std::cos(angle),0,-std::sin(angle)},length*scale,60+ring*16});
            positions[i].x+=std::cos(angle)*length;positions[i].y+=std::sin(angle)*length;
        }
    }
    std::size_t index=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
        const auto& e=expected[index++];assert(b.IsBeam());Near(b.position.x,e.position.x);Near(b.position.z,e.position.z);
        Near(b.Direction().x,e.direction.x);Near(b.Direction().z,e.direction.z);Near(b.beamLength,e.length);
        Near(b.radius,0.625F);Near(b.beamGrowSeconds,e.grow/60.0F);Near(b.dangerEndSeconds,134.0F/60);Near(b.maxAge,166.0F/60);
    });
    assert(actor.Random(1)==static_cast<float>(Next(random)>>8)/16777216.0F);
}
behavior::Routine MineProbe(behavior::Actor& actor) {assert(bosses::RemiliaBoss::Behaviors::SeedMines(actor)==4);bosses::RemiliaBoss::Behaviors::ReleaseMines(actor);co_await actor.At(100);}
void TestNativeMines() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int type=0;type<10;++type) {
        auto* b=bullets->Spawn(BulletOwner::Enemy,{static_cast<float>(type),0,4},{0,0,1},0.1F,WHITE,stage6::kBlueBullet);
        b->nativeType=type;b->nativeColor=3;
    }
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4},13000,49);stage6::ConfigureBehavior(actor,false);actor.Start(MineProbe(actor));
    actor.Update(1.0F/60,{},*bullets);assert(bullets->Pool().ActiveCount()==14);
    unsigned random=49;for(int i=0;i<4;++i)Next(random);const float phase=static_cast<float>(Next(random)>>8)/16777216.0F*2*pi-pi;
    int index=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
        if(index<10) {Near(b.Velocity().z,1);Near(b.accelerationSeconds,1e9F);}
        else {
            assert(b.nativeType==3&&b.nativeColor==2);Near(b.position.x,index-4.0F);Near(BulletSystem::Length(b.Velocity()),0);
            const float angle=b.position.x*pi/11.4285714F+phase;
            Near(b.acceleration.x,1.60714281F*std::cos(angle));Near(b.acceleration.z,-1.60714281F*std::sin(angle));
            Near(b.accelerationSeconds,2);Near(b.spawnVisualSeconds,32.0F/60);
        }
        ++index;
    });
    assert(actor.Random(1)==static_cast<float>(Next(random)>>8)/16777216.0F);
}

behavior::Routine StarsEntry(behavior::Actor& actor) {bosses::RemiliaBoss::Behaviors::PhaseMemory memory;co_await bosses::RemiliaBoss::Behaviors::StarOfVanities(actor,memory);}
behavior::Routine YoungDemonEntry(behavior::Actor& actor) {bosses::RemiliaBoss::Behaviors::PhaseMemory memory;co_await bosses::RemiliaBoss::Behaviors::YoungDemonLord(actor,memory);}
behavior::Routine SecondEntry(behavior::Actor& actor) {bosses::RemiliaBoss::Behaviors::PhaseMemory memory;co_await bosses::RemiliaBoss::Behaviors::SecondNonspell(actor,memory);}
/// @brief 控制流见证独立展开源调用/回拨；与旧图形、原子程序索引工厂或生产协程不共享时刻计算。
void TestBossSourceClocks() {
    using namespace stage6;auto bullets=std::make_unique<BulletSystem>();
    const std::array<behavior::Entry,8> entries{bosses::SakuyaBoss::Behaviors::EternalMeekNonspell,bosses::SakuyaBoss::Behaviors::EternalMeek,bosses::RemiliaBoss::Behaviors::FirstNonspell,StarsEntry,
        SecondEntry,bosses::RemiliaBoss::Behaviors::ScarletNetherworld,bosses::RemiliaBoss::Behaviors::ThirdNonspell,bosses::RemiliaBoss::Behaviors::ScarletShoot};
    for(int kind=0;kind<8;++kind) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},19000,49);ConfigureBehavior(actor,false);actor.Start(entries[kind](actor));
        for(int frame=0;frame<1200;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{0,0,-6},*bullets);int expected=0;
            if(kind==0) {
                const int t=frame<30 ? frame : (frame-30)%130+30;
                if((t>=30&&t<=74&&(t-30)%4==0)||(t>=95&&t<=139&&(t-95)%4==0))expected=8;
                if((t>=79&&t<=94)||(t>=144&&t<=159))expected=5;
            } else if(kind==1) {if(frame>=120&&(frame-120)%4==0)expected=16;}
            else if(kind==2) {
                const int t=frame<100 ? frame : (frame-100)%642+100;
                if((t>=100&&t<=348&&(t-100)%8==0)||(t>=366&&t<=614&&(t-366)%8==0))expected=11;
            } else if(kind==3) {
                if(frame>=184&&(frame-184)%184==0)expected=48;
                if(frame>=244&&(frame-244)%184==0)expected=((frame-244)/184)%2 ? 144 : 240;
            } else if(kind==4) {
                const int t=frame<60 ? frame : (frame-60)%853+60;
                if((t>=161&&t<=241&&(t-161)%20==0)||(t>=803&&t<=883&&(t-803)%20==0))expected=21;
                if((t>=362&&t<=467&&(t-362)%15==0)||(t>=583&&t<=688&&(t-583)%15==0))expected=19;
            } else if(kind==5) {
                if(frame>=120) {const int t=(frame-120)%216;for(int i=0;i<6;++i)for(int j=0;j<4;++j)if(t==i*21+j*4)expected=j<2 ? 24 : 16;}
            } else if(kind==6) {
                const int t=frame<60 ? frame : (frame-60)%478+60;
                if((t>=60&&t<=178&&(t-60)%2==0)||(t>=308&&t<=426&&(t-308)%2==0))expected=8;
            } else if(frame>=120) {
                const int t=(frame-120)%484;if(t==64||t==124||t==334||t==364)expected=120;if(t==214)expected=72;
            }
            if(bullets->Pool().ActiveCount()!=static_cast<std::size_t>(expected))std::cerr<<"Stage 6 source clock kind "<<kind<<" frame "<<frame<<" actual "<<bullets->Pool().ActiveCount()<<" expected "<<expected<<'\n';
            assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
        }
    }
}
void TestSpatialDisplayFrameRates() {
    using namespace stage6;
    const std::array<behavior::Entry,16> entries{bosses::SakuyaBoss::Behaviors::EternalMeekNonspell,bosses::SakuyaBoss::Behaviors::EternalMeek,bosses::RemiliaBoss::Behaviors::FirstNonspell,StarsEntry,YoungDemonEntry,bosses::RemiliaBoss::Behaviors::ThousandNeedles,bosses::RemiliaBoss::Behaviors::ScarletMeister,bosses::RemiliaBoss::Behaviors::VampireIllusion,bosses::RemiliaBoss::Behaviors::ScarletGensokyo,SecondEntry,
        bosses::RemiliaBoss::Behaviors::ScarletNetherworld,bosses::RemiliaBoss::Behaviors::ThirdNonspell,bosses::RemiliaBoss::Behaviors::VladTepesCurse,bosses::RemiliaBoss::Behaviors::BatNonspell,bosses::RemiliaBoss::Behaviors::ScarletShoot,bosses::RemiliaBoss::Behaviors::RedMagic};
    struct Snapshot {Vector3 position,velocity;int type,color,bounces;};
    for(auto entry:entries)for(int d=0;d<4;++d) {
        const bool mid=entry==bosses::SakuyaBoss::Behaviors::EternalMeekNonspell||entry==bosses::SakuyaBoss::Behaviors::EternalMeek;
        const bool nonspell=entry==bosses::RemiliaBoss::Behaviors::FirstNonspell||entry==SecondEntry||
            entry==bosses::RemiliaBoss::Behaviors::ThirdNonspell||entry==bosses::RemiliaBoss::Behaviors::BatNonspell;
        if((entry==YoungDemonEntry||entry==bosses::RemiliaBoss::Behaviors::ThousandNeedles||entry==bosses::RemiliaBoss::Behaviors::ScarletMeister||entry==bosses::RemiliaBoss::Behaviors::VampireIllusion||entry==bosses::RemiliaBoss::Behaviors::ScarletGensokyo) ? d<2 : ((!mid&&d==0)||(!mid&&!nonspell&&d!=1)))continue;
        std::vector<Snapshot> expected;Vector3 position{};float random=0;double phase=0;int animation=-1;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,4},19000,7);
            actor.difficulty=*native_combat::DifficultyFromIndex(d);ConfigureBehavior(actor,true);actor.Start(entry(actor));
            for(int frame=0;frame<fps*12;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Snapshot> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back({b.position,b.Velocity(),b.nativeType,b.nativeColor,b.bouncesRemaining});});
            const float nextRandom=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=nextRandom;phase=actor.phaseFrames;animation=actor.animationScript;}
            else {
                assert(actual.size()==expected.size()&&nextRandom==random&&phase==actor.phaseFrames&&animation==actor.animationScript);
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                for(std::size_t i=0;i<actual.size();++i) {
                    const auto& a=actual[i];const auto& b=expected[i];assert(a.type==b.type&&a.color==b.color&&a.bounces==b.bounces);
                    Near(a.position.x,b.position.x);Near(a.position.y,b.position.y);Near(a.position.z,b.position.z);
                    Near(a.velocity.x,b.velocity.x);Near(a.velocity.y,b.velocity.y);Near(a.velocity.z,b.velocity.z);
                }
            }
            assert(bullets->Pool().DroppedSpawns()==0);
        }
    }
}

/// @brief 长时间高速巡航后，空间本体不能漂离出生回收盒使化蝠永久占槽；按正式顺序处理全部子演员。
void TestBatFamiliarRetirement() {
    for(int d=1;d<4;++d) {
    auto bullets=std::make_unique<BulletSystem>();auto pool=std::make_unique<behavior::ActorPool<128>>();
    behavior::Actor boss;boss.Reset({0,0,5},13000,7);boss.difficulty=*native_combat::DifficultyFromIndex(d);
    stage6::ConfigureBehavior(boss,true);boss.Start(bosses::RemiliaBoss::Behaviors::BatNonspell(boss));
    std::size_t peak=0;
    for(int tick=0;tick<60*120;++tick) {
        bullets->Reset();pool->ForEachActive([&](behavior::Actor& actor){actor.Update(1.0F/120,{0,0,-6},*bullets);});
        boss.Update(1.0F/120,{0,0,-6},*bullets);
        for(const auto& spawn:boss.spawnEvents) {
            assert(spawn.retirementBounds.Contains(spawn.position)&&spawn.difficulty==boss.difficulty);
            auto* actor=pool->Spawn(spawn);assert(actor&&actor->difficulty==boss.difficulty);actor->Update(1.0F/120,{0,0,-6},*bullets);
        }
        std::size_t count=0;pool->ForEachActive([&](const behavior::Actor&){++count;});peak=std::max(peak,count);
        assert(pool->DroppedSpawns()==0);
    }
    assert(peak>20&&peak<128);
    }
}

void TestBombBatProtection() {
    for(int d=1;d<4;++d) {
    auto bullets=std::make_unique<BulletSystem>();bosses::RemiliaBoss boss(behavior::UpdateRate::Hz60,false);boss.StartBoss(stage6::RemiliaFinalConfig(ShotType::HomingAmulet,*native_combat::DifficultyFromIndex(d)));
    boss.ResumeCombat();boss.Update(1.0F/60,{},*bullets);boss.ResumeCombat();
    for(int phase=1;phase<9;++phase) {
        int frames=0;while(!boss.CanTakeDamage()&&frames++<700)boss.Update(1.0F/60,{},*bullets);
        assert(frames<700);boss.health=0;assert(boss.AdvanceSkill());
    }
    boss.SetPlayerBombActive(true);
    for(int frame=0;frame<240;++frame) {
        bullets->Reset();boss.Update(1.0F/60,{},*bullets);assert(!boss.CanBeTargeted()&&!boss.CanTakeDamage()&&boss.animationScript==165);
    }
    const float time=boss.skillTimer;boss.SetPlayerBombActive(false);
    for(int frame=0;frame<59;++frame) {boss.Update(1.0F/60,{},*bullets);assert(!boss.CanBeTargeted());}
    boss.Update(1.0F/60,{},*bullets);assert(boss.CanTakeDamage()&&boss.CanBeTargeted()&&boss.skillTimer>time);
    }
}

void TestCrossPhaseChargeMemory() {
    for(int d=1;d<4;++d) {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);bosses::RemiliaBoss::Behaviors::PhaseMemory memory;
    actor.Reset({0,0,5},1200,49);actor.difficulty=*native_combat::DifficultyFromIndex(d);stage6::ConfigureBehavior(actor,false);
    actor.Start(d==1?bosses::RemiliaBoss::Behaviors::StarOfVanities(actor,memory):bosses::RemiliaBoss::Behaviors::YoungDemonLord(actor,memory));
    for(int frame=0;frame<=128;++frame)actor.Update(1.0F/60,{},*bullets);
    assert(memory.chargingPulsesLeft==14);actor.pendingCallback=bosses::RemiliaBoss::Behaviors::SkillComplete;actor.Transition();
    actor.Start(bosses::RemiliaBoss::Behaviors::SecondNonspell(actor,memory));
    for(int frame=0;frame<=174;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==174 ? (d==3?33U:21U) : 0U));
    }
    assert(memory.chargingPulsesLeft==0);
    }
}

}
/// @brief 运行六面独立出生/攻击时钟、九阶段、扩展几何、化蝠保护与 120 Hz 显示帧率一致性。
void RunStage6Tests() {TestRemiliaAmbientLifecycle();TestRemiliaDefeatedEffects();TestRemiliaEffectDisplayRates();TestBirthsAndNeutralMovement();TestDifficultyOrdinaryReference();TestOrdinaryDisplayRates();TestMidbossDifficultyReference();TestMidbossDifficultyLifecycle();TestRemiliaNonspellDifficultyReference();TestRemiliaNonspellDifficultyLifecycle();TestRemiliaSpellDifficultyReference();TestRemiliaSpellDifficultyLifecycle();TestRemiliaFinalDifficultyLifecycle();TestStage6PracticeSkills();TestDialogueMilestones();TestBossPhaseLifecycle();TestNativeStarGeometry();TestNativeMines();TestBossSourceClocks();TestSpatialDisplayFrameRates();TestBatFamiliarRetirement();TestBombBatProtection();TestCrossPhaseChargeMemory();}

/// @brief 只运行六面普通演员的四难度源差分与默认120 Hz显示帧率一致性。
void RunStage6DifficultyTests() {TestDifficultyOrdinaryReference();TestOrdinaryDisplayRates();}

/// @brief 六面道中咲夜的逐发源差分与三种遭遇结束路线。
void RunStage6MidbossTests() {TestMidbossDifficultyReference();TestMidbossDifficultyLifecycle();}

/// @brief 六面蕾米莉亚已接入非符的N/H/L源差分。
void RunStage6NonspellTests() {TestRemiliaAmbientLifecycle();TestRemiliaDefeatedEffects();TestRemiliaEffectDisplayRates();TestRemiliaNonspellDifficultyReference();TestRemiliaNonspellDifficultyLifecycle();}
/// @brief 第一组符卡的独立源差分与默认120 Hz生命周期入口，便于定位首次不一致。
void RunStage6FirstSpellTests() {TestRemiliaSpellDifficultyReference(1);TestRemiliaSpellDifficultyLifecycle(1);}
/// @brief 全部已接入符卡的源差分和120 Hz角色生命周期；未接入入口不因数据存在而执行。
void RunStage6SpellTests() {TestRemiliaSpellDifficultyReference();TestRemiliaSpellDifficultyLifecycle();TestRemiliaFinalDifficultyLifecycle();}

/// @brief 仅运行六面十八张单符工厂的四机体回归。
void RunStage6PracticeTests() {TestStage6PracticeSkills();}

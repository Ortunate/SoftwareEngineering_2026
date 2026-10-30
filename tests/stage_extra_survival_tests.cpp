// 本文件独立核对生存符的源出生、逐轮边界弹幕、追踪使魔及超时/最终前奏交接；原坐标仅留在参考侧。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <numbers>
namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Ref=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448,speedScale=scale*60;
int frame=0,route=-1,rank=0;
void Near(float a,float b,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {
        std::cerr<<"Extra survival frame "<<frame<<" route "<<route<<" rank "<<rank<<' '<<field<<": "<<a<<" vs "<<b<<'\n';std::abort();
    }
}
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
Vector3 Player(int n) {return {240+30*std::sin(n*.01F),350+20*std::cos(n*.013F),0};}
std::vector<behavior::Spawn> children;
std::vector<int> shots,births;
struct Trace {Bullet bullet;Ref::Emission source;};
std::vector<Trace> traces;
void CompareShots(const BulletSystem& bullets,const Ref& source) {
    std::vector<const Bullet*> actual;bullets.Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
    if(actual.size()!=source.emissions.size())std::cerr<<"survival shot count frame "<<frame<<": "<<actual.size()<<" vs "<<source.emissions.size()<<'\n';
    assert(actual.size()==source.emissions.size()&&!bullets.Pool().DroppedSpawns());
    for(std::size_t n=0;n<actual.size();++n) {
        const auto& b=*actual[n];const auto& e=source.emissions[n];const auto p=World(e.position);
        assert(b.nativeType==e.type&&b.nativeColor==e.color);
        Near(b.position.x,p.x,"bullet.x");Near(b.position.z,p.z,"bullet.z");Near(b.position.y,p.y,"bullet.y");
        Near(b.Velocity().x,e.velocity.x*speedScale,"bullet.vx",1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,"bullet.vz",1e-4F);
        Near(b.spawnVisualSeconds,(e.flags&8)?32.0F/60:(e.flags&2)?10.0F/60:0,"appearance",1e-5F);
        assert(b.sampleMotionAtStart&&b.polarMotion==((e.flags&32)!=0));
        if(b.polarMotion) {Near(b.angularVelocity,e.angularVelocity*60,"angular",1e-5F);Near(b.angularSeconds,e.angularFrames/60,"angular.seconds",1e-5F);}
    }
}
void TestSource() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int r:{10,16,18,31,32}) {
        rank=r;route=-1;auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,96,0}),1000,seed);F::Behaviors::ConfigurePractice(actor,false);actor.SetRank(rank);
        actor.Start(F::Behaviors::AndThenWillThereBeNone(actor));Ref source(82,{192,96,0},seed,rank,1000);source.death=91;
        int total=0;std::vector<behavior::Spawn> spawned;std::vector<Ref::Spawn> originals;
        for(frame=0;frame<5160;++frame) {
            bullets->Reset();const auto player=Player(frame);source.Tick(player);actor.Update(1.0F/60,World(player),*bullets);
            CompareEffects(actor,source);
            const auto p=World(source.position);Near(actor.position.x,p.x,"parent.x");Near(actor.position.z,p.z,"parent.z");
            assert(actor.damageable==source.damageable&&actor.interactable==source.interactable&&actor.contactEnabled==source.contact&&actor.visible==source.visible);
            assert(actor.soundEvents==source.sounds&&actor.defeatActors==source.defeatActors&&actor.animationScript==source.animation);
            assert(source.card==130&&source.timeout==5160&&source.health==1000&&source.timeoutCallback==91&&source.survival==(frame>=120));
            assert(actor.spawnEvents.size()==source.spawns.size());
            if(seed==7&&rank==16) {shots.push_back(static_cast<int>(source.emissions.size()));for(const auto& e:source.spawns)births.push_back(frame);}
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];const auto p=World(e.position);
                assert(e.sub==90&&a.sourceHealth==e.health&&a.sourceScore==e.score&&a.sourceItem==e.item&&behavior::Valid(a));
                assert(!a.visible&&!a.damageable&&a.stageClearResponse==behavior::StageClearResponse::Remove&&a.seed==seed+spawned.size()+1);
                Near(a.position.x,p.x,"spawn.x");Near(a.position.z,p.z,"spawn.z");spawned.push_back(a);originals.push_back(e);
            }
            CompareShots(*bullets,source);
            if(seed==7&&rank==16&&(frame==2936||frame==3256)) {
                std::size_t index=0;bullets->Pool().ForEachActive([&](const Bullet& b){traces.push_back({b,source.emissions[index++]});});
            }
            total+=static_cast<int>(source.emissions.size());
        }
        assert(total==1092&&spawned.size()==6&&actor.Random(1)==source.Random(1));
        if(seed==7&&rank==16)children=spawned;
        for(route=0;route<6;++route) {
            behavior::Actor child(behavior::UpdateRate::Hz60);child.Activate(spawned[route]);child.SetRank(rank);
            Ref original(90,originals[route].position,spawned[route].seed,rank,originals[route].health);
            const int end=60+192*original.health;
            for(frame=0;frame<=end;++frame) {
                bullets->Reset();const auto player=Player(frame);original.Tick(player);child.Update(1.0F/60,World(player),*bullets);
                const auto p=World(original.position);Near(child.position.x,p.x,"child.x");Near(child.position.z,p.z,"child.z");
                assert(child.active==original.active&&!child.visible&&!child.interactable&&!child.damageable&&!child.contactEnabled);
                assert(child.animationScript==original.animation&&child.soundEvents==original.sounds);
                CompareShots(*bullets,original);
            }
            assert(!child.active&&child.finished&&child.Random(1)==original.Random(1));
        }
    }
}
void TestClocksAndSpatial() {
    assert(children.size()==6&&births==std::vector<int>({120,920,984,1048,1112,1176}));
    for(int factor:{1,2}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);
        actor.Reset(World({192,96,0}),1000,7);F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::AndThenWillThereBeNone(actor));std::size_t birth=0;
        for(int tick=0;tick<5160*factor;++tick) {
            bullets->Reset();actor.Update(1.0F/(60*factor),World(Player(tick/factor)),*bullets);
            assert(actor.visible==(tick<120*factor)&&!actor.damageable);
            const bool spawned=birth<births.size()&&tick==births[birth]*factor;
            assert(actor.spawnEvents.size()==(spawned?1:0));if(spawned)++birth;
            assert(bullets->Pool().ActiveCount()==(tick%factor==0?shots[tick/factor]:0));
        }
        assert(birth==6);
    }
    for(const auto& spawn:children)for(int factor:{1,2})for(bool spatial:{false,true}) {
        auto config=spawn;config.pattern.spatialShots=spatial;behavior::Actor actor(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);
        actor.Activate(config);auto bullets=std::make_unique<BulletSystem>();const Vector3 target{2,spatial?2.0F:0,-6};
        const int end=60+192*spawn.sourceHealth;int total=0;const int period=6;
        for(int tick=0;tick<=end*factor;++tick) {
            bullets->Reset();actor.Update(1.0F/(60*factor),target,*bullets);total+=static_cast<int>(bullets->Pool().ActiveCount());
            assert(actor.active==(tick<end*factor)&&!actor.visible);
            const float f=static_cast<float>(tick)/factor;
            if(f>=60&&f<252) {
                const float t=(f-60)/192;const float fraction=1-(1-t)*(1-t);
                Near(actor.position.x,spawn.position.x+(target.x-spawn.position.x)*fraction,"half.x");
                Near(actor.position.y,spawn.position.y+(target.y-spawn.position.y)*fraction,"half.y");
                Near(actor.position.z,spawn.position.z+(target.z-spawn.position.z)*fraction,"half.z");
            }
            if(f>=252) {Near(actor.position.x,target.x,"target.x");Near(actor.position.y,target.y,"target.y");Near(actor.position.z,target.z,"target.z");}
            bullets->Pool().ForEachActive([&](const Bullet& b){Near(BulletSystem::Length(b.Velocity()),1.33928573F,"spatial.speed",1e-4F);});
        }
        assert(total==3*(end/period));
        actor.Activate(config);actor.Update(61.0F/60,target,*bullets);actor.ClearFromStage();assert(!actor.active&&actor.finished);
        actor.Activate(config);actor.Update(1.0F/60,target,*bullets);assert(actor.active&&!actor.visible&&!actor.retiring);
    }
}
void TestProjectileTraces() {
    assert(traces.size()==120);auto bullets=std::make_unique<BulletSystem>();
    for(const auto& trace:traces) {
        bullets->Reset();auto* actual=bullets->Spawn(BulletOwner::Enemy,{},{},.1F,WHITE,0);*actual=trace.bullet;
        float x=trace.source.position.x,y=trace.source.position.y,vx=trace.source.velocity.x,vy=trace.source.velocity.y;
        const float baseX=vx,baseY=vy;int angular=0;
        for(frame=0;frame<300;++frame) {
            // Large显现32内容帧，每帧位移1/3；最后显现帧同时执行运动钟0。
            if(frame<32) {x+=baseX/3;y+=baseY/3;}
            if(frame>=31) {
                if((trace.source.flags&32)&&angular++<220) {
                    const float cosine=std::cos(trace.source.angularVelocity),sine=std::sin(trace.source.angularVelocity);
                    const float previous=vx;vx=previous*cosine-vy*sine;vy=previous*sine+vy*cosine;
                }
                x+=vx;y+=vy;
            }
            bullets->Update(1.0F/60);const auto p=World({x,y,0});
            Near(actual->position.x,p.x,"trace.x");Near(actual->position.z,p.z,"trace.z");
            Near(actual->Velocity().x,vx*speedScale,"trace.vx",1e-4F);Near(actual->Velocity().z,-vy*speedScale,"trace.vz",1e-4F);
            if(!actual->active)break;
        }
    }
}
void TestDisplayRates() {
    for(bool spatial:{false,true})for(int child=-1;child<6;++child) {
        Vector3 position{};std::vector<Bullet> expected;std::vector<behavior::Spawn> expectedSpawns;float random=0;
        const int seconds=child<0?86:4;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;auto bullets=std::make_unique<BulletSystem>();std::vector<behavior::Spawn> spawned;
            if(child<0) {actor.Reset(World({192,96,0}),1000,7);F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::AndThenWillThereBeNone(actor));}
            else {auto config=children[child];config.pattern.spatialShots=spatial;actor.Activate(config);}
            for(int update=0;update<seconds*fps;++update) {actor.Update(1.0F/fps,{2,3,-6},*bullets);spawned.insert(spawned.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());}
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const auto next=actor.Random(1);
            assert(actor.phaseFrames==seconds*60&&!bullets->Pool().DroppedSpawns());
            if(fps==120) {position=actor.position;expected=actual;expectedSpawns=spawned;random=next;}
            else {
                assert(expected.size()==actual.size()&&expectedSpawns.size()==spawned.size()&&random==next);
                Near(actor.position.x,position.x,"display.x");Near(actor.position.y,position.y,"display.y");Near(actor.position.z,position.z,"display.z");
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& b=expected[n];assert(a.nativeType==b.nativeType&&a.nativeColor==b.nativeColor&&a.polarMotion==b.polarMotion);
                    Near(a.position.x,b.position.x,"display.bullet.x");Near(a.position.y,b.position.y,"display.bullet.y");Near(a.position.z,b.position.z,"display.bullet.z");
                    Near(a.Velocity().x,b.Velocity().x,"display.vx",1e-4F);Near(a.Velocity().y,b.Velocity().y,"display.vy",1e-4F);Near(a.Velocity().z,b.Velocity().z,"display.vz",1e-4F);
                }
            }
        }
    }
}
void TestFinalPreludeAndHandoffs() {
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120})for(int shot=0;shot<4;++shot) {
        auto bullets=std::make_unique<BulletSystem>();F boss(rate,false);
        const auto def=boss.DescribeSkill(F::Behavior::AndThenWillThereBeNone,static_cast<ShotType>(shot));
        assert(def.health==1000&&def.timeoutFrames==5160&&def.spellCard==130&&def.survival&&def.reactToBomb);
        bosses::StartConfig config{.position=World({192,96,0}),.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::SurvivalPrelude,F::Behavior::AndThenWillThereBeNone,F::Behavior::FinalPrelude},{{{0,.1F,WHITE},{1,.4F,PINK},{2,.5F,WHITE}}}}}};
        boss.StartBoss(config);const int factor=rate==behavior::UpdateRate::Hz60?1:2;bool ready=false;
        for(int tick=0;tick<=90*factor;++tick) {bullets->Reset();ready=boss.Update(1.0F/(60*factor),{},*bullets);if(tick<90*factor)assert(!ready);}
        assert(ready&&boss.AdvanceSkill()&&boss.attack==2&&!boss.CanTakeDamage());
        ready=false;
        for(int tick=0;tick<=5160*factor;++tick) {
            bullets->Reset();boss.SetPlayerBombActive(tick<30||tick>=250*factor&&tick<280*factor);
            ready=boss.Update(1.0F/(60*factor),{2,3,-6},*bullets);
            assert(!boss.CanTakeDamage()&&!boss.ShouldAdvanceFromDamage());
            assert(!boss.CanBeTargeted()||tick<120*factor);
            if(tick<120*factor)assert(boss.animationScript!=165);
            if(tick>=120*factor)assert(!boss.BodyVisible()&&!boss.CanContactPlayer());
            if(tick==260*factor)assert(boss.animationScript==165);
            if(tick==400*factor)assert(boss.animationScript!=165&&!boss.CanBeTargeted());
            assert(ready==(tick==5160*factor));
            assert(!boss.SkillDefeated()&&boss.SpellSucceeded()==ready&&boss.ClearsBulletsOnSkillEnd()!=ready);
        }
        assert(boss.AdvanceSkill()&&boss.attack==3&&boss.health==6000&&!boss.CanTakeDamage()&&!boss.SpellName());
        const auto last=boss.DescribeSkill(F::Behavior::FinalPrelude);assert(last.health==6000&&last.reserveHealth==0&&last.timeoutFrames==1800);
        Ref original(91,{192,80,0},7,16,1000);
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset(World({192,80,0}),6000,7);F::Behaviors::ConfigurePractice(actor,false);
        actor.visible=false;actor.Start(F::Behaviors::FinalPrelude(actor));
        for(frame=0;frame<=90;++frame) {
            bullets->Reset();original.Tick({240,350,0});actor.Update(1.0F/60,{},*bullets);
            assert(!actor.damageable&&actor.visible&&original.visible&&actor.contactEnabled==original.contact&&actor.interactable==original.interactable);
            assert(actor.animationScript==original.animation&&actor.dropEvents==original.drops&&actor.defeatActors==original.defeatActors);
            assert(actor.pendingCallback==(frame==90?bosses::SkillComplete:nullptr)&&!bullets->Pool().ActiveCount()&&original.emissions.empty());
        }
        assert(original.handoff==92&&actor.Random(1)==original.Random(1));
        for(int tick=0;tick<=90*factor;++tick) {
            ready=boss.Update(1.0F/(60*factor),{},*bullets);
            assert(boss.BodyVisible()&&!boss.CanTakeDamage()&&!boss.ShouldAdvanceFromDamage()&&!boss.SpellSucceeded()&&boss.ClearsBulletsOnSkillEnd());
            assert(ready==(tick==90*factor));
        }
        boss.StartBoss(config);assert(boss.BodyVisible()&&!boss.CanTakeDamage()&&!boss.SpellSucceeded());
    }
}
}
/// @brief 第九符独立逻辑验收，包含源弹道、不同显示帧率与真实81/91交接；最终符和正式Extra另行验收。
void RunStageExtraSurvivalTests() {TestSource();TestClocksAndSpatial();TestProjectileTraces();TestDisplayRates();TestFinalPreludeAndHandoffs();std::cout<<"Validated Extra survival source parent, six pursuers, rank periods, half steps, curved trajectories, display rates and real prelude handoffs\n";}

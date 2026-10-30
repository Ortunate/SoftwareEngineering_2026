// 本文件以101…105独立源指令验证Fragile Wing的出生、扫描、零初速弹道及内容时钟；不替代正式Extra流程验收。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Ref=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448,speedScale=60*scale,accelerationScale=3600*scale;
int frame=0,route=-1;
void Near(float actual,float expected,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Fragile Wing route "<<route<<" frame "<<frame<<' '<<field<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
void Position(Vector3 actual,Vector3 p) {const auto e=World(p);Near(actual.x,e.x,"x");Near(actual.y,e.y,"y");Near(actual.z,e.z,"z");}
std::vector<behavior::Spawn> firstChildren;
struct Sample {Bullet actual;Ref::Emission source;};
std::vector<Sample> samples;
void CompareChild(const behavior::Spawn& spawn,const Ref::Spawn& raw,int rank) {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor child(behavior::UpdateRate::Hz60);child.Activate(spawn);child.SetRank(rank);
    Ref source(105,raw.position,spawn.seed,rank,raw.health);source.Inherit(raw.variables);int shots=0;
    for(frame=0;frame<205;++frame) {
        bullets->Reset();source.Tick({240,350,0});child.Update(1.0F/60,World({240,350,0}),*bullets);
        Position(child.position,source.position);
        if(child.active!=source.active)std::cerr<<"Fragile Wing retirement route "<<route<<" frame "<<frame<<" active "<<child.active<<" vs "<<source.active<<" at "<<child.position.x<<','<<child.position.z<<'\n';
        assert(child.active==source.active&&!child.visible&&!child.damageable&&!child.contactEnabled&&!child.interactable);
        assert(child.soundEvents==source.sounds&&child.animationScript==source.animation);
        assert(child.health==source.health&&source.specialActor&&source.death==60);
        Near(child.hitRadius,source.radius*scale,"radius",1e-5F);
        int n=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
            assert(n<static_cast<int>(source.emissions.size()));const auto& e=source.emissions[n++];
            Position(b.position,e.position);assert(b.nativeType==5&&b.nativeColor==e.color&&e.flags==530&&b.sampleMotionAtStart);
            assert(BulletSystem::Length(b.velocity)==0&&b.launchBoostSeconds==0&&b.spawnVisualSeconds==10.0F/60);
            Near(b.acceleration.x,e.accelerationVector.x*accelerationScale,"ax",1e-4F);
            Near(b.acceleration.y,0,"ay",1e-4F);Near(b.acceleration.z,-e.accelerationVector.y*accelerationScale,"az",1e-4F);
            Near(b.accelerationSeconds,5,"acceleration.seconds",1e-5F);
            if(rank==16&&(frame==0||frame==111||frame==112||frame==130))samples.push_back({b,e});
        });
        assert(n==static_cast<int>(source.emissions.size()));shots+=n;
        if(!source.active)break;
    }
    assert(shots>0&&shots<=spawn.pattern.polarSweep->emissions&&child.Random(1)==source.Random(1));
}
void TestSource() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,96,0}),3900,seed);actor.visual=stage_assets::kBoss;
        F::Behaviors::ConfigurePractice(actor,false);actor.SetRank(rank);actor.Start(F::Behaviors::FragileWing(actor));
        Ref source(101,{192,96,0},seed,rank,15000);
        std::vector<behavior::Spawn> children;std::vector<Ref::Spawn> originals;int effects=0;
        route=-1;
        for(frame=0;frame<7200;++frame) {
            source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);CompareEffects(actor,source);effects+=source.effectRequests;
            Position(actor.position,source.position);
            if(actor.animationScript!=source.animation)std::cerr<<"Fragile Wing animation frame "<<frame<<": "<<actor.animationScript<<" vs "<<source.animation<<'\n';
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.damageable==source.damageable);
            assert(actor.visible==source.visible&&actor.interactable==source.interactable&&actor.contactEnabled==source.contact&&actor.defeatActors==source.defeatActors);
            assert(actor.health==source.health&&actor.spawnEvents.size()==source.spawns.size()&&source.emissions.empty()&&!bullets->Pool().ActiveCount());
            assert(source.card==132&&source.timeout==7200&&source.death==106&&source.timeoutCallback==106&&source.reserve==0);
            assert(source.nativeCallback==(frame<120?6:17));
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];assert(e.sub==105&&behavior::Valid(a)&&a.pattern.polarSweep);
                assert(a.seed==seed+children.size()+1&&a.sourceHealth==e.health&&a.sourceItem==e.item&&a.sourceScore==e.score);
                assert(!a.visible&&!a.damageable&&a.stageClearResponse==behavior::StageClearResponse::Remove);
                Position(a.position,e.position);Near(a.pattern.movement.angle,e.variables[5],"spawn.angle",1e-5F);
                const auto& sweep=*a.pattern.polarSweep;
                Near(sweep.offsetAngleStep,e.variables[7],"offset.step",1e-5F);Near(sweep.shotAngleStep,e.variables[6],"shot.step",1e-5F);
                assert(sweep.emissions==e.variables[9]);children.push_back(a);originals.push_back(e);
            }
        }
        assert(actor.Random(1)==source.Random(1));
        if(seed==7&&rank==16) {firstChildren.assign(children.begin(),children.begin()+9);std::cout<<"Fragile Wing source: "<<children.size()<<" children, "<<effects<<" effect requests\n";}
        assert(children.size()==75&&effects==1680);
        // 首轮和末轮分别覆盖未饱和/饱和次数，全部出生参数仍逐帧核验。
        for(std::size_t n=0;n<children.size();++n)if(n<9||n>=children.size()-9) {route=static_cast<int>(n);CompareChild(children[n],originals[n],rank);}
    }
}
// 独立像素积分：初速为零，显现前9帧静止，第10帧开始以0.005像素/帧²更新速度后移动，共300个运动步。
void TestProjectiles() {
    assert(samples.size()>20);auto bullets=std::make_unique<BulletSystem>();route=-2;
    for(const auto& sample:samples) {
        bullets->Reset();auto* actual=bullets->Spawn(BulletOwner::Enemy,{},{},0.1F,WHITE,0);assert(actual);
        *actual=sample.actual;actual->position={};float x=0,y=0,vx=0,vy=0;
        for(frame=0;frame<320;++frame) {
            if(frame>=9) {
                if(frame-9<300) {vx+=sample.source.accelerationVector.x;vy+=sample.source.accelerationVector.y;}
                x+=vx;y+=vy;
            }
            bullets->Update(1.0F/60);Near(actual->position.x,x*scale,"bullet.x");Near(actual->position.z,-y*scale,"bullet.z");
            Near(actual->velocity.x,vx*speedScale,"bullet.vx",1e-4F);Near(actual->velocity.z,-vy*speedScale,"bullet.vz",1e-4F);
        }
        assert(actual->accelerationSeconds==0);
    }
}
void TestPersistenceAndConfiguration() {
    auto bullets=std::make_unique<BulletSystem>();assert(firstChildren.size()==9);auto spawn=firstChildren[0];
    // 独立构造位于扩展边界内的140帧终点，验证发射停止不等于删除；父参数在Activate后修改不能影响子体。
    spawn.position=World({192,120,0});spawn.pattern.movement.angle=std::numbers::pi_v<float>/3;
    spawn.pattern.polarSweep->emissions=140;const auto saved=spawn;
    behavior::Actor child(behavior::UpdateRate::Hz60);child.Activate(spawn);spawn.pattern.polarSweep->emissions=1;
    Ref::Spawn raw{105,1200,-2,10,{192,120,0},{}};raw.variables[5]=saved.pattern.movement.angle;
    raw.variables[6]=saved.pattern.polarSweep->shotAngleStep;raw.variables[7]=saved.pattern.polarSweep->offsetAngleStep;raw.variables[9]=140;
    route=100;CompareChild(saved,raw,16);
    int total=0;
    for(frame=0;frame<210;++frame) {
        bullets->Reset();child.Update(1.0F/60,{},*bullets);total+=static_cast<int>(bullets->Pool().ActiveCount());
        assert(child.active&&!child.finished&&bullets->Pool().ActiveCount()==(frame<140?1U:0U));
        if(frame==70)Position(child.position,{244.5F,120+420*std::sin(saved.pattern.movement.angle)*.25F,0});
    }
    assert(total==140&&child.polarSweep->emissions==140);child.ClearFromStage();assert(!child.active&&child.finished);
    behavior::ActorPool<1> pool;auto* first=pool.Spawn(saved);assert(first&&!pool.Spawn(saved)&&pool.DroppedSpawns()==1);
    first->Update(1.0F/120,{},*bullets);assert(first->polarSweep);pool.Clear();auto plain=saved;plain.pattern.polarSweep.reset();plain.pattern.routine=nullptr;
    auto* second=pool.Spawn(plain);assert(second&&!second->polarSweep);second->Update(1.0F/120,{},*bullets);assert(!second->polarSweep);
    auto invalid=saved.pattern;invalid.polarSweep->emissions=0;assert(!behavior::Valid(invalid));
    invalid=saved.pattern;invalid.polarSweep->offsetAngleStep=std::numeric_limits<float>::infinity();assert(!behavior::Valid(invalid));
    invalid=saved.pattern;invalid.polarSweep->shotAngleStep=std::numeric_limits<float>::quiet_NaN();assert(!behavior::Valid(invalid));
    invalid=saved.pattern;invalid.routine=nullptr;assert(!behavior::Valid(invalid));
}
void TestClocksAndSpatial() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int factor:{1,2}) {
        behavior::Actor actor(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);
        actor.Reset(World({192,96,0}),3900,7);F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::FragileWing(actor));int count=0;
        for(int tick=0;tick<1031*factor;++tick) {
            actor.Update(1.0F/(60*factor),World({240,350,0}),*bullets);count+=static_cast<int>(actor.spawnEvents.size());
            assert(actor.damageable==(tick>=120*factor));
            if(!actor.spawnEvents.empty())assert(tick==200*factor||tick==320*factor||tick==440*factor||tick==560*factor||tick==770*factor||tick==1030*factor);
        }
        assert(count==10);
        auto spawn=firstChildren[0];spawn.retireOutside=false;spawn.pattern.polarSweep->emissions=140;behavior::Actor child(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);child.Activate(spawn);
        count=0;for(int tick=0;tick<140*factor;++tick) {bullets->Reset();child.Update(1.0F/(60*factor),{},*bullets);count+=static_cast<int>(bullets->Pool().ActiveCount());}
        // 此路线自然越界会结束协程；完整140次扫描由上面的场内终点专项覆盖。
        assert(count>100&&count<=140);
    }
    for(bool spatial:{false,true}) {
        std::vector<behavior::Spawn> expected;Vector3 point{};float random=0;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;actor.Reset(World({192,96,0}),3900,7);F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::FragileWing(actor));std::vector<behavior::Spawn> children;
            for(int tick=0;tick<120*fps;++tick) {actor.Update(1.0F/fps,World({240,350,0}),*bullets);children.insert(children.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());}
            const auto next=actor.Random(1);assert(actor.phaseFrames==7200);
            if(fps==120) {expected=children;point=actor.position;random=next;}
            else {assert(children.size()==expected.size()&&random==next);Near(actor.position.x,point.x,"display.x");Near(actor.position.y,point.y,"display.y");Near(actor.position.z,point.z,"display.z");
                for(std::size_t n=0;n<children.size();++n) {Near(children[n].position.x,expected[n].position.x,"display.spawn.x");Near(children[n].position.y,expected[n].position.y,"display.spawn.y");Near(children[n].position.z,expected[n].position.z,"display.spawn.z");assert(children[n].pattern.polarSweep->emissions==expected[n].pattern.polarSweep->emissions);}}
        }
    }
    for(bool space:{false,true}) {
        std::vector<Bullet> expected;Vector3 point{};float random=0;
        for(int fps:{120,30,60,144,240}) {
            bullets->Reset();auto spawn=firstChildren[0];spawn.position=World({192,120,0});
            spawn.pattern.movement.angle=std::numbers::pi_v<float>/3;spawn.pattern.polarSweep->emissions=140;spawn.pattern.spatialShots=space;
            behavior::Actor child;child.Activate(spawn);
            for(int tick=0;tick<4*fps;++tick)child.Update(1.0F/fps,{},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const float next=child.Random(1);
            assert(actual.size()==140&&child.active&&!child.finished&&child.phaseFrames==240);
            if(fps==120) {expected=actual;point=child.position;random=next;}
            else {
                assert(random==next);Near(child.position.x,point.x,"child.display.x");Near(child.position.z,point.z,"child.display.z");
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& e=expected[n];assert(a.nativeType==e.nativeType&&a.nativeColor==e.nativeColor);
                    Near(a.position.x,e.position.x,"child.display.bullet.x");Near(a.position.y,e.position.y,"child.display.bullet.y");Near(a.position.z,e.position.z,"child.display.bullet.z");
                    Near(a.acceleration.x,e.acceleration.x,"child.display.ax",1e-4F);Near(a.acceleration.y,e.acceleration.y,"child.display.ay",1e-4F);Near(a.acceleration.z,e.acceleration.z,"child.display.az",1e-4F);
                }
            }
        }
    }
    // 相同平面路线和攻击随机流，空间采样仅使加速度离开平面，零初速时也必须保留其方向。
    auto spawn=firstChildren[0];spawn.pattern.spatialShots=true;behavior::Actor spatial(behavior::UpdateRate::Hz60);spatial.Activate(spawn);
    auto flatSpawn=spawn;flatSpawn.pattern.spatialShots=false;behavior::Actor flat(behavior::UpdateRate::Hz60);flat.Activate(flatSpawn);bool above=false,below=false;
    for(frame=0;frame<100;++frame) {
        bullets->Reset();spatial.Update(1.0F/60,{},*bullets);flat.Update(1.0F/60,{},*bullets);int n=0;
        bullets->Pool().ForEachActive([&](const Bullet& b) {if(n++==0) {Near(BulletSystem::Length(b.acceleration),.005F*accelerationScale,"spatial.acceleration",1e-5F);above|=b.acceleration.y>0;below|=b.acceleration.y<0;assert(BulletSystem::Length(b.velocity)==0);}});
    }
    assert(above&&below&&spatial.Random(1)==flat.Random(1));
}
void TestRoleContract() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int shot=0;shot<4;++shot) {
        F boss;auto definition=boss.DescribeSkill(F::Behavior::FragileWing,static_cast<ShotType>(shot));
        assert(definition.NativeHealth()==3900&&definition.timeoutFrames==7200&&definition.entranceHealth==15000&&definition.spellCard==132&&definition.reactToBomb);
        bosses::StartConfig config{.position={},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.intro=F::Behavior::EncoreEntrance,.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::FragileWing},{{{0,1,WHITE}}}}}};
        boss.StartBoss(config);for(int tick=0;tick<240;++tick)boss.Update(1.0F/120,{},*bullets);
        assert(!boss.CurrentSkill()&&boss.health==15000);boss.ResumeCombat();assert(boss.CurrentSkill()&&boss.health==3900&&!boss.CanTakeDamage());
        for(int tick=0;tick<242;++tick)boss.Update(1.0F/120,{},*bullets);assert(boss.CanTakeDamage());
        boss.SetPlayerBombActive(true);boss.Update(1.0F/120,{},*bullets);assert(boss.animationScript==165&&!boss.CanBeTargeted());
        boss.SetPlayerBombActive(false);for(int tick=0;tick<120;++tick)boss.Update(1.0F/120,{},*bullets);
        assert(boss.animationScript==160&&boss.CanBeTargeted());
    }
}
}
/// @brief 仅验收132独立行为与已接入入场协议；106之后的正式战斗接续另行验证。
void RunStageExtraFragileWingTests() {
    TestSource();TestProjectiles();TestPersistenceAndConfiguration();TestClocksAndSpatial();TestRoleContract();
    std::cout<<"Validated Extra Fragile Wing source, sweep ownership, persistence, projectiles, clocks and spatial geometry\n";
}

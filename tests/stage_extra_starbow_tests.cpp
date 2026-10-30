// 本文件独立对照Starbow Break源指令、全部首轮雨弹路线及flags19的离散运动，不以生产配方生成期望。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Reference=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448,speedScale=scale*60,accelerationScale=scale*3600;
int checkedFrame=0,checkedRoute=-1,checkedRank=0;unsigned checkedSeed=0;
void Near(float actual,float expected,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Starbow route "<<checkedRoute<<" frame "<<checkedFrame<<" rank "<<checkedRank<<" seed "<<checkedSeed<<' '<<field<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
struct Sample {Bullet actual;Reference::Emission source;};
std::vector<Sample> samples;
std::vector<behavior::Spawn> firstChildren;
std::vector<std::pair<int,int>> birthCounts;
void TestSource() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        checkedSeed=seed;checkedRank=rank;checkedRoute=-1;auto bullets=std::make_unique<BulletSystem>();
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5.71428585F},2200,seed);actor.SetRank(rank);actor.visual=stage_assets::kBoss;
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::StarbowBreak(actor));
        Reference source(66,{192,96,0},seed,rank,2200);source.death=71;
        std::vector<behavior::Spawn> children;std::vector<Reference::Spawn> originals;int effects=0;
        for(int frame=0;frame<4200;++frame) {
            checkedFrame=frame;bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,{2,0,-6},*bullets);CompareEffects(actor,source);effects+=source.effectRequests;
            Near(actor.position.x,(source.position.x-192)*scale,"x");Near(actor.position.z,10-source.position.y*scale,"z");
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.damageable==source.damageable);
            assert(actor.interactable==source.interactable&&actor.contactEnabled==source.contact&&actor.defeatActors==source.defeatActors);
            assert(actor.spawnEvents.size()==source.spawns.size()&&bullets->Pool().ActiveCount()==0&&source.emissions.empty());
            if(seed==7&&rank==16&&!source.spawns.empty())birthCounts.push_back({frame,static_cast<int>(source.spawns.size())});
            assert(source.card==127&&source.timeout==4200&&source.timeoutCallback==71&&source.nativeCallback==(frame>=120?10:-1));
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];
                assert((e.sub==69||e.sub==70)&&a.sourceHealth==e.health&&a.sourceItem==e.item&&a.sourceScore==e.score&&behavior::Valid(a));
                assert(!a.visible&&!a.damageable&&a.stageClearResponse==behavior::StageClearResponse::Remove&&a.seed==seed+children.size()+1);
                Near(a.position.x,(e.position.x-192)*scale,"spawn.x");Near(a.position.z,10-e.position.y*scale,"spawn.z");
                Near(a.pattern.movement.angle,e.position.z,"spawn.angle",1e-5F);children.push_back(a);originals.push_back(e);
            }
        }
        assert(children.size()==155&&effects==2160&&actor.Random(1)==source.Random(1));
        if(seed==7&&rank==16)firstChildren.assign(children.begin(),children.begin()+27);
        for(int route=0;route<27;++route) {
            checkedRoute=route;behavior::Actor child(behavior::UpdateRate::Hz60);child.Activate(children[route]);child.SetRank(rank);
            Reference original(originals[route].sub,originals[route].position,children[route].seed,rank,originals[route].health);
            int frame=0,shots=0;
            for(;frame<=200&&original.active;++frame) {
                checkedFrame=frame;bullets->Reset();original.Tick({240,350,0});child.Update(1.0F/60,{},*bullets);
                Near(child.position.x,(original.position.x-192)*scale,"child.x");Near(child.position.z,10-original.position.y*scale,"child.z");
                if(child.active!=original.active)std::cerr<<"Starbow route "<<route<<" retirement frame "<<frame<<'\n';
                assert(child.active==original.active&&!child.visible&&!child.damageable&&!child.contactEnabled&&!child.interactable);
                assert(child.animationScript==original.animation&&child.soundEvents==original.sounds&&original.specialActor&&original.deathMode==1);
                Near(child.hitRadius,original.radius*scale,"hitbox",1e-5F);
                std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
                assert(actual.size()==original.emissions.size());shots+=static_cast<int>(actual.size());
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& b=*actual[n];const auto& e=original.emissions[n];
                    assert(b.nativeType==3&&b.nativeColor==e.color&&e.flags==19&&b.sampleMotionAtStart);
                    assert(b.spawnVisualSeconds==10.0F/60&&b.launchBoostSeconds==16.0F/60);
                    Near(b.position.x,(e.position.x-192)*scale,"bullet.x");Near(b.position.z,10-e.position.y*scale,"bullet.z");
                    Near(b.Velocity().x,e.velocity.x*speedScale,"velocity.x",1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,"velocity.z",1e-4F);
                    Near(b.acceleration.x,e.accelerationVector.x*accelerationScale,"acceleration.x",1e-4F);
                    Near(b.acceleration.z,-e.accelerationVector.y*accelerationScale,"acceleration.z",1e-4F);
                    Near(b.accelerationSeconds,e.accelerationFrames/60,"acceleration.seconds",1e-5F);
                    if(seed==7&&rank==16&&(frame==0||frame==20||frame==80))samples.push_back({b,e});
                }
            }
            assert(!child.active&&frame<=201&&shots>0&&shots<=50&&!bullets->Pool().DroppedSpawns()&&child.Random(1)==original.Random(1));
        }
    }
}
void TestZeroSpeedAndLifetime() {
    assert(firstChildren.size()==27);auto bullets=std::make_unique<BulletSystem>();
    auto spawn=firstChildren[0];spawn.seed=4071982377U; // xorshift32首个输出为1，高24位为0，独立验证精确零速率特判。
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.Update(1.0F/60,{},*bullets);
    Reference source(69,{192,80,2.356194496F},spawn.seed,16,102);source.Tick({240,350,0});
    assert(source.emissions.size()==1&&source.emissions[0].velocity.x==0&&source.emissions[0].velocity.y==0);
    bullets->Pool().ForEachActive([&](const Bullet& b){assert(b.Velocity().x==0&&b.Velocity().z==0&&b.launchDirection.z>0.99F);samples.push_back({b,source.emissions[0]});});
    assert(actor.Random(1)==source.Random(1));
    // 冻结上一更新步位移，仅隔离验证逻辑寿命；真实路线的场外回收已经逐条与源执行器对照。
    for(int route:{0,7})for(int factor:{1,2}) {
        auto config=firstChildren[route];config.position={};config.retireOutside=false;
        behavior::Actor emitter(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);emitter.Activate(config);int count=0;
        for(int tick=0;tick<=200*factor;++tick) {
            bullets->Reset();emitter.Update(1.0F/(60*factor),{},*bullets);emitter.motion.velocity={};
            count+=static_cast<int>(bullets->Pool().ActiveCount());
            assert(bullets->Pool().ActiveCount()==(tick<200*factor&&tick%(4*factor)==0?1U:0U));
            assert(emitter.active==(tick<200*factor));
        }
        assert(count==50&&emitter.finished&&!emitter.visible);
    }
}
// 独立源像素运动：前10帧显现以半初速移动，第10帧同时启动运动；0…16帧加5→0速率，17帧清选项，18帧才加速。
// 加速上限使用同一运动时钟，120/180均包含前18帧；不从生产Bullet运动时钟或参数余量生成答案。
struct Projectile {
    float x=192,y=224,vx,vy,baseX,baseY,ax,ay,speed;int age=0,limit;
    explicit Projectile(const Reference::Emission& e):vx(e.velocity.x),vy(e.velocity.y),baseX(vx),baseY(vy),
        ax(e.accelerationVector.x),ay(e.accelerationVector.y),speed(std::hypot(vx,vy)),limit(static_cast<int>(e.accelerationFrames)) {}
    void Tick() {
        if(age<10) {x+=baseX*0.5F;y+=baseY*0.5F;}
        if(age>=9) {
            const int frame=age-9;
            if(frame<=16) {const float boosted=speed+5*(1-frame/16.0F);vx=std::cos(-1.5707963705062866F)*boosted;vy=std::sin(-1.5707963705062866F)*boosted;}
            else if(frame>=18&&frame<limit) {vx+=ax;vy+=ay;}
            x+=vx;y+=vy;
        }
        ++age;
    }
};
void TestProjectileMotion() {
    assert(samples.size()>50);auto bullets=std::make_unique<BulletSystem>();int checked=0;
    for(const auto& sample:samples) {
        bullets->Reset();auto* actual=bullets->Spawn(BulletOwner::Enemy,{},{},0.1F,WHITE,0);assert(actual);
        *actual=sample.actual;actual->position={};Projectile expected(sample.source);
        for(int frame=0;frame<200;++frame) {
            checkedFrame=frame;expected.Tick();bullets->Update(1.0F/60);
            Near(actual->position.x,(expected.x-192)*scale,"motion.x");Near(actual->position.z,10-expected.y*scale,"motion.z");
            Near(actual->Velocity().x,expected.vx*speedScale,"motion.vx",1e-4F);Near(actual->Velocity().z,-expected.vy*speedScale,"motion.vz",1e-4F);
            assert(actual->active);
        }
        assert(actual->accelerationSeconds==0);++checked;
    }
    std::cout<<"Starbow independent flags19 trajectories: "<<checked<<'\n';
}
void TestClocksAndDisplayRates() {
    for(int factor:{1,2}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);
        actor.Reset({0,0,5.71428585F},2200,7);F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::StarbowBreak(actor));
        std::size_t next=0;
        for(int tick=0;tick<4200*factor;++tick) {
            actor.Update(1.0F/(60*factor),{},*bullets);assert(actor.CanTakeDamage()==(tick>=120*factor));
            const auto count=next<birthCounts.size()&&tick==birthCounts[next].first*factor?birthCounts[next++].second:0;
            assert(actor.spawnEvents.size()==static_cast<std::size_t>(count));
        }
        assert(next==birthCounts.size());
    }
    for(bool spatial:{false,true})for(int route=-1;route<27;++route) {
        Vector3 expectedPosition{};double expectedFrames=0;float expectedRandom=0;std::vector<Bullet> expected;std::vector<behavior::Spawn> expectedSpawns;
        auto bullets=std::make_unique<BulletSystem>();
        for(int fps:{120,30,60,144,240}) {
            bullets->Reset();behavior::Actor actor;
            if(route<0) {actor.Reset({0,0,5.71428585F},2200,7);F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::StarbowBreak(actor));}
            else {auto config=firstChildren[route];config.pattern.spatialShots=spatial;actor.Activate(config);}
            std::vector<behavior::Spawn> spawns;
            for(int frame=0;frame<20*fps;++frame) {
                actor.Update(1.0F/fps,{2,3,-6},*bullets);spawns.insert(spawns.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());
            }
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const auto random=actor.Random(1);
            assert(!bullets->Pool().DroppedSpawns());
            if(fps==120) {expectedPosition=actor.position;expectedFrames=actor.phaseFrames;expectedRandom=random;expected=actual;expectedSpawns=spawns;}
            else {
                Near(actor.position.x,expectedPosition.x,"display.x");Near(actor.position.y,expectedPosition.y,"display.y");Near(actor.position.z,expectedPosition.z,"display.z");
                assert(actor.phaseFrames==expectedFrames&&random==expectedRandom&&actual.size()==expected.size()&&spawns.size()==expectedSpawns.size());
                for(std::size_t n=0;n<actual.size();++n) {
                    Near(actual[n].position.x,expected[n].position.x,"display.bullet.x");Near(actual[n].position.y,expected[n].position.y,"display.bullet.y");Near(actual[n].position.z,expected[n].position.z,"display.bullet.z");
                    Near(actual[n].Velocity().x,expected[n].Velocity().x,"display.vx",1e-4F);Near(actual[n].Velocity().y,expected[n].Velocity().y,"display.vy",1e-4F);Near(actual[n].Velocity().z,expected[n].Velocity().z,"display.vz",1e-4F);
                    Near(actual[n].acceleration.z,expected[n].acceleration.z,"display.az",1e-4F);assert(actual[n].nativeColor==expected[n].nativeColor);
                }
                for(std::size_t n=0;n<spawns.size();++n) {
                    assert(spawns[n].seed==expectedSpawns[n].seed&&spawns[n].sourceHealth==expectedSpawns[n].sourceHealth);
                    Near(spawns[n].position.x,expectedSpawns[n].position.x,"display.spawn.x");Near(spawns[n].position.y,expectedSpawns[n].position.y,"display.spawn.y");Near(spawns[n].position.z,expectedSpawns[n].position.z,"display.spawn.z");
                }
            }
            if(route<0)assert(spawns.size()==40);else assert(!actor.active&&actor.finished);
        }
    }
}
void TestRoleHandoffAndBomb() {
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        F boss;const auto skill=boss.DescribeSkill(F::Behavior::StarbowBreak,static_cast<ShotType>(shot));
        assert(skill.health==2200&&skill.reserveHealth==0&&skill.timeoutFrames==4200&&skill.spellCard==127&&skill.reactToBomb);
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::StarbowBreak,F::Behavior::SeventhNonspell},{{{0,0.5F,PINK},{1,0.5F,WHITE}}}}}};
        boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false,bat=false,recovered=false;
        for(int tick=0;tick<8500;++tick) {
            bullets->Reset();boss.SetPlayerBombActive(tick<30||tick>=250&&tick<280);const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(tick<240)assert(boss.animationScript!=165&&!boss.CanTakeDamage());
            if(tick==260) {assert(boss.animationScript==165&&!boss.CanBeTargeted());bat=true;}
            if(tick==400) {assert(boss.CanTakeDamage()&&boss.animationScript!=165);recovered=true;}
            if(damage&&tick>2400&&boss.CanTakeDamage())boss.health=0;
            if(ready||boss.ShouldAdvanceFromDamage()) {if(!damage)assert(tick==8400);assert(boss.AdvanceSkill());advanced=true;break;}
        }
        assert(advanced&&bat&&recovered&&boss.attack==2&&boss.health==10000&&!boss.CanTakeDamage());
        boss.Update(1.0F/120,{},*bullets);assert(boss.DefeatActorsRequested()&&!boss.CanTakeDamage());
        boss.SetPlayerBombActive(true);boss.StartBoss(config);boss.Update(1.0F/120,{},*bullets);assert(boss.animationScript!=165);
    }
}
void TestSpatialRain() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int route:{0,7}) {
        auto config=firstChildren[route];config.position={};config.retireOutside=false;config.pattern.spatialShots=true;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(config);
        Reference source(route==0?69:70,{192,224,0},config.seed,16,static_cast<int>(config.sourceHealth));
        int round=0;bool above=false,below=false;
        for(int frame=0;frame<=200;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);actor.motion.velocity={};
            bullets->Pool().ForEachActive([&](const Bullet& b) {
                const float rawSpeed=source.Random(1),rawAcceleration=source.Random(route==0?0.01F:0.013F)+(route==0?0.015F:0.012F);
                const float expectedSpeed=rawSpeed==0?0:std::max(0.3F,rawSpeed)*speedScale;
                Near(BulletSystem::Length(b.Velocity()),expectedSpeed,"spatial.speed",1e-5F);
                Near(b.acceleration.y,0,"spatial.fixedAcceleration.y",1e-5F);
                Near(b.acceleration.z,-rawAcceleration*accelerationScale,"spatial.fixedAcceleration.z",1e-4F);
                double fraction=0,weight=0.5;for(int index=round+1;index;index/=2,weight*=0.5)fraction+=(index%2)*weight;
                Near(b.launchDirection.z,static_cast<float>(1-(1-std::cos(0.5))*fraction),"spatial.solidAngle",1e-5F);
                Near(BulletSystem::Length(b.launchDirection),1,"spatial.unitDirection",1e-5F);
                above|=b.launchDirection.y>0;below|=b.launchDirection.y<0;++round;
            });
        }
        assert(round==50&&above&&below&&actor.Random(1)==source.Random(1));
    }
}
}
/// @brief 验证第六符已实现核心行为；完整Game与正式关卡入口由集成测试另行验收。
void RunStageExtraStarbowTests() {
    TestSource();TestZeroSpeedAndLifetime();TestProjectileMotion();TestClocksAndDisplayRates();TestSpatialRain();TestRoleHandoffAndBomb();
    std::cout<<"Validated Extra Starbow Break source diffs, rain projectile motion, clocks, display rates, handoffs and Bomb recovery\n";
}

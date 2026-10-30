// 本文件以106…111独立源指令验证额外第二符的真实过渡、两类血痕使魔及随机读取，不以生产配方生成期望。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>
namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Ref=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448,speedScale=60*scale,accelerationScale=3600*scale;
int frame=0,route=-1;
void Near(float actual,float expected,const char* name,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Bloodstain route "<<route<<" frame "<<frame<<' '<<name<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
void Position(Vector3 actual,Vector3 source) {const auto e=World(source);Near(actual.x,e.x,"x");Near(actual.y,e.y,"y");Near(actual.z,e.z,"z");}
struct Sample {Bullet actual;Ref::Emission source;};
std::vector<Sample> samples;
std::vector<behavior::Spawn> firstChildren;
void TestPrelude() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,120,0}),4000,seed);F::Behaviors::ConfigurePractice(actor,false);actor.SetRank(rank);
        actor.Start(F::Behaviors::CreepingBloodstainPrelude(actor));Ref source(106,{192,120,0},seed,rank,3900);
        for(frame=0;frame<=210;++frame) {
            source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);Position(actor.position,source.position);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.health==source.health);
            assert(actor.dropEvents==source.drops&&actor.itemEvents==source.items&&actor.defeatActors==source.defeatActors);
            assert(!actor.damageable&&source.damageable==false&&actor.contactEnabled&&actor.interactable&&actor.visible);
            assert(!bullets->Pool().ActiveCount()&&source.emissions.empty()&&actor.spawnEvents.empty()&&source.spawns.empty());
            assert(source.timeout==7200&&source.timeoutCallback==112&&source.reserve==-1&&source.nativeCallback==6);
            assert((actor.pendingCallback!=nullptr)==(frame==210)&&source.handoff==(frame==210?107:-1));
            if(frame==30)assert(actor.dropEvents==std::vector<int>{40}&&actor.itemEvents==std::vector<int>{3});
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
void CompareChild(const behavior::Spawn& spawn,const Ref::Spawn& raw,int rank) {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);
    Ref source(raw.sub,raw.position,spawn.seed,rank,raw.health);source.Inherit(raw.variables);int shots=0;
    for(frame=0;frame<400;++frame) {
        bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);Position(actor.position,source.position);
        assert(actor.active==source.active&&!actor.visible&&!actor.damageable&&!actor.interactable&&!actor.contactEnabled);
        assert(actor.soundEvents==source.sounds&&actor.animationScript==source.animation&&source.specialActor&&source.death==60);
        Near(actor.hitRadius,source.radius*scale,"radius",1e-5F);
        int n=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
            assert(n<static_cast<int>(source.emissions.size()));const auto& e=source.emissions[n++];Position(b.position,e.position);
            assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart&&b.spawnVisualSeconds==10.0F/60&&b.launchBoostSeconds==0);
            Near(b.Velocity().x,e.velocity.x*speedScale,"vx",1e-4F);Near(b.Velocity().y,0,"vy",1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,"vz",1e-4F);
            if(e.type==3) {
                assert(e.color==1&&e.flags==530&&BulletSystem::Length(b.Velocity())==0);
                Near(b.acceleration.x,e.accelerationVector.x*accelerationScale,"ax",1e-4F);Near(b.acceleration.z,-e.accelerationVector.y*accelerationScale,"az",1e-4F);
                Near(b.accelerationSeconds,2000.0F/60,"acceleration.seconds",1e-5F);
            } else assert(e.type==9&&e.flags==514);
            if(rank==16&&route<15&&(frame==0||frame==15||frame==60))samples.push_back({b,e});
        });
        assert(n==static_cast<int>(source.emissions.size()));shots+=n;if(!source.active)break;
    }
    assert(frame<400&&shots>1&&actor.Random(1)==source.Random(1));
}
void TestSource() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,192,0}),4000,seed);actor.visual=stage_assets::kBoss;F::Behaviors::ConfigurePractice(actor,false);actor.SetRank(rank);
        actor.motion.bounds={{-7.14285714F,1.42857146F},{7.14285714F,7.85714293F}};
        actor.Start(F::Behaviors::CreepingBloodstain(actor));Ref source(107,{192,192,0},seed,rank,4000);source.death=112;source.timeout=7200;source.reserve=-1;
        std::vector<behavior::Spawn> children;std::vector<Ref::Spawn> originals;int effects=0;
        route=-1;
        for(frame=0;frame<7200;++frame) {
            source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);CompareEffects(actor,source);effects+=source.effectRequests;Position(actor.position,source.position);
            if(actor.spawnEvents.size()!=source.spawns.size())std::cerr<<"Bloodstain birth frame "<<frame<<": "<<actor.spawnEvents.size()<<" vs "<<source.spawns.size()<<'\n';
            assert(actor.spawnEvents.size()==source.spawns.size()&&actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
            assert(actor.damageable&&source.damageable&&actor.contactEnabled&&source.contact&&actor.interactable&&source.interactable&&actor.defeatActors==source.defeatActors);
            assert(source.card==133&&source.nativeCallback==17&&source.timeoutCallback==112&&source.emissions.empty()&&!bullets->Pool().ActiveCount());
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];assert((e.sub==110||e.sub==111)&&behavior::Valid(a));
                assert(a.sourceHealth==e.health&&a.sourceScore==e.score&&a.sourceItem==e.item&&a.seed==seed+children.size()+1);
                assert(!a.visible&&!a.damageable&&a.stageClearResponse==behavior::StageClearResponse::Remove);Position(a.position,e.position);
                if(e.sub==111)Near(a.pattern.movement.angle,e.variables[6],"spawn.angle",1e-5F);
                children.push_back(a);originals.push_back(e);
            }
        }
        assert(actor.Random(1)==source.Random(1));
        if(seed==7&&rank==16) {firstChildren.assign(children.begin(),children.begin()+15);std::cout<<"Bloodstain source: "<<children.size()<<" children, "<<effects<<" effect requests\n";}
        assert(children.size()==175&&effects==2880);
        for(std::size_t n=0;n<children.size();++n) {route=static_cast<int>(n);CompareChild(children[n],originals[n],rank);}
    }
}
// 独立源像素积分；只推进可见寿命内的实际轨迹，避免项目场外回收后继续访问已归还的弹槽。
void TestProjectiles() {
    assert(samples.size()>30);auto bullets=std::make_unique<BulletSystem>();route=-2;
    for(const auto& sample:samples) {
        bullets->Reset();auto* actual=bullets->Spawn(BulletOwner::Enemy,{},{},.1F,WHITE,0);assert(actual);*actual=sample.actual;actual->position={};
        float x=0,y=0,vx=sample.source.velocity.x,vy=sample.source.velocity.y;
        for(frame=0;frame<500&&actual->active;++frame) {
            if(frame<10) {x+=sample.source.velocity.x*.5F;y+=sample.source.velocity.y*.5F;}
            if(frame>=9) {
                if(sample.source.flags&16) {vx+=sample.source.accelerationVector.x;vy+=sample.source.accelerationVector.y;}
                x+=vx;y+=vy;
            }
            bullets->Update(1.0F/60);Near(actual->position.x,x*scale,"bullet.x");Near(actual->position.z,-y*scale,"bullet.z");
            Near(actual->Velocity().x,vx*speedScale,"bullet.vx",1e-4F);Near(actual->Velocity().z,-vy*speedScale,"bullet.vz",1e-4F);
        }
    }
    // 单独固定位置和上一速度，仅隔离2000帧加速有效期；时钟不能因120Hz而减半或受显现9帧偏移干扰。
    for(int factor:{1,2}) {
        auto found=std::find_if(samples.begin(),samples.end(),[](const Sample& s){return s.source.type==3;});assert(found!=samples.end());
        bullets->Reset();auto* b=bullets->Spawn(BulletOwner::Enemy,{},{},.1F,WHITE,0);*b=found->actual;b->position={};b->maxAge=100;
        for(int tick=0;tick<2010*factor;++tick) {
            b->position={};b->SetVelocity({});bullets->Update(1.0F/(60*factor));
            const bool accelerating=tick>=9*factor&&tick<2009*factor;
            if((BulletSystem::Length(b->Velocity())>0)!=accelerating||!b->active)
                std::cerr<<"Bloodstain acceleration factor "<<factor<<" tick "<<tick<<" remaining "<<b->accelerationSeconds<<" active "<<b->active<<'\n';
            assert((BulletSystem::Length(b->Velocity())>0)==accelerating&&b->active);
        }
        assert(b->accelerationSeconds==0);
    }
}
void TestClocksSpatialAndPool() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int factor:{1,2}) {
        behavior::Actor prelude(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);prelude.Reset(World({192,120,0}),4000,7);
        F::Behaviors::ConfigurePractice(prelude,false);prelude.Start(F::Behaviors::CreepingBloodstainPrelude(prelude));int items=0;
        for(int tick=0;tick<=210*factor;++tick) {
            prelude.Update(1.0F/(60*factor),{},*bullets);items+=static_cast<int>(prelude.itemEvents.size());
            assert((prelude.pendingCallback!=nullptr)==(tick==210*factor));if(!prelude.itemEvents.empty())assert(tick==30*factor);
        }
        assert(items==1);
    }
    for(bool space:{false,true}) {
        std::vector<behavior::Spawn> expected;float random=0;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor a;a.Reset(World({192,120,0}),4000,7);a.visual=stage_assets::kBoss;F::Behaviors::ConfigurePractice(a,space);
            a.Start(F::Behaviors::CreepingBloodstain(a));std::vector<behavior::Spawn> children;
            for(int tick=0;tick<120*fps;++tick) {a.Update(1.0F/fps,World({240,350,0}),*bullets);children.insert(children.end(),a.spawnEvents.begin(),a.spawnEvents.end());}
            const auto next=a.Random(1);assert(a.phaseFrames==7200&&a.damageable);
            if(fps==120) {expected=children;random=next;}
            else {assert(children.size()==expected.size()&&random==next);for(std::size_t n=0;n<children.size();++n) {Near(children[n].position.x,expected[n].position.x,"display.x");Near(children[n].position.y,expected[n].position.y,"display.y");Near(children[n].position.z,expected[n].position.z,"display.z");Near(children[n].pattern.movement.angle,expected[n].pattern.movement.angle,"display.angle",1e-5F);}}
        }
        for(int route:{0,10}) {
            std::vector<Bullet> expectedBullets;Vector3 point{};float random=0;bool above=false,below=false;
            for(int fps:{120,30,60,144,240}) {
                bullets->Reset();auto spawn=firstChildren[route];spawn.pattern.spatialShots=space;behavior::Actor child;child.Activate(spawn);
                for(int tick=0;tick<fps;++tick)child.Update(1.0F/fps,World({240,350,0}),*bullets);
                const auto next=child.Random(1);std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});assert(actual.size()==21&&child.active);
                if(fps==120) {expectedBullets=actual;point=child.position;random=next;}
                else {
                    assert(random==next);Near(child.position.x,point.x,"child.display.x");Near(child.position.z,point.z,"child.display.z");
                    for(std::size_t n=0;n<actual.size();++n) {Position(actual[n].position,{192+expectedBullets[n].position.x/scale,(10-expectedBullets[n].position.z)/scale,-expectedBullets[n].position.y/scale});Near(actual[n].acceleration.y,expectedBullets[n].acceleration.y,"child.display.ay",1e-4F);}
                }
                for(const auto& b:actual)if(b.nativeType==3) {
                    const float magnitude=(route==10?.003F:.002F)*accelerationScale;
                    if(space)Near(BulletSystem::Length(b.acceleration),magnitude,"spatial.acceleration",1e-5F);
                    above|=b.acceleration.y>0;below|=b.acceleration.y<0;
                }
            }
            if(space)assert(above&&below);
        }
    }
    behavior::ActorPool<1> pool;auto* actor=pool.Spawn(firstChildren[0]);assert(actor&&!pool.Spawn(firstChildren[1])&&pool.DroppedSpawns()==1);
    actor->Update(1.0F/120,{},*bullets);actor->ClearFromStage();assert(!actor->active&&actor->finished);pool.Clear();
    auto plain=firstChildren[0];plain.pattern.routine=nullptr;plain.visible=plain.damageable=true;auto* reused=pool.Spawn(plain);
    assert(reused&&reused->visible&&reused->damageable&&reused->interactable&&!reused->finished&&!reused->polarSweep);
}
void TestRoleTransitions() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int shot=0;shot<4;++shot)for(int factor:{1,2}) {
        F boss;bosses::StartConfig config{.position=World({192,120,0}),.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::FragileWing,F::Behavior::CreepingBloodstainPrelude,F::Behavior::CreepingBloodstain},{{{0,.4F,PINK},{1,.2F,WHITE},{2,.4F,PINK}}}}}};
        // ConfiguredBoss的积分频率使用其默认120Hz；显示更新分别60/120Hz，交接须保留同一内容时长。
        boss.StartBoss(config);for(int tick=0;tick<240*factor;++tick)boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets);
        boss.health=0;assert(boss.AdvanceSkill()&&boss.attack==2&&boss.health==4000&&!boss.CanTakeDamage());int drops=0,items=0,tick=0;
        for(;tick<212*factor;++tick) {
            const bool finished=boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets);
            drops+=static_cast<int>(boss.DropEvents().size());items+=static_cast<int>(boss.ItemEvents().size());if(finished)break;
            assert(!boss.CanTakeDamage()&&boss.SpellCardNumber()==0);
        }
        assert(tick>=210*factor&&tick<=211*factor&&drops==1&&items==1);assert(boss.AdvanceSkill()&&boss.attack==3&&boss.health==4000&&!boss.CanTakeDamage());
        const auto* definition=boss.CurrentSkill();assert(definition&&definition->spellCard==133&&definition->timeoutFrames==7200&&definition->bombReactionFrame==0);
        boss.SetPlayerBombActive(true);boss.Update(1.0F/120,World({240,350,0}),*bullets);assert(boss.animationScript==165&&!boss.CanBeTargeted());
        boss.SetPlayerBombActive(false);for(int n=0;n<120;++n)boss.Update(1.0F/120,World({240,350,0}),*bullets);
        assert(boss.animationScript==160&&boss.CanTakeDamage());
        // 独立检查133在7200时限按超时交接，106的210帧准备不侵占此时限。
        // 60Hz显示更新在106回调时可留下一个120Hz固定步；按真实阶段余量计数，不能丢弃它来凑显示更新次数。
        const int remaining=static_cast<int>(std::lround((120.0-boss.skillTimer)*120));
        for(int n=0;n<remaining;++n)assert(!boss.Update(1.0F/120,World({240,350,0}),*bullets));
        assert(boss.Update(1.0F/120,World({240,350,0}),*bullets));
        assert(boss.AdvanceSkill()&&!boss.CurrentSkill()&&!boss.Defeated());
    }
}
}
/// @brief 验收真实132→106→133协议及133独立攻击；112与正式Extra入口仍由后续工作完成。
void RunStageExtraBloodstainTests() {
    TestPrelude();TestSource();TestProjectiles();TestClocksSpatialAndPool();TestRoleTransitions();
    std::cout<<"Validated Extra Creeping Bloodstain, prelude, source emitters, RNG, projectiles, clocks and handoffs\n";
}

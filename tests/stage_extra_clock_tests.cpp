// 本文件独立执行铭刻往昔之钟的本体及激光使魔源指令，验证时钟、运动、光束句柄和生命周期。
#include "stage_extra_flandre_reference.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <numbers>

namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Reference=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448,speedScale=scale*60;
int checkedFrame=0,checkedRoute=-1,checkedRank=0;unsigned checkedSeed=0;
void Near(float actual,float expected,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Extra clock route "<<checkedRoute<<" frame "<<checkedFrame<<" rank "<<checkedRank<<" seed "<<checkedSeed<<' '<<field<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
std::vector<behavior::Spawn> routes;
std::vector<int> birthFrames,shotCounts;
void TestSource() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        checkedSeed=seed;checkedRank=rank;checkedRoute=-1;auto bullets=std::make_unique<BulletSystem>();
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5.71428585F},1800,seed);actor.SetRank(rank);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::ClockOfThePast(actor));
        Reference source(77,{192,96,0},seed,rank,1800);source.death=81;
        std::vector<behavior::Spawn> children;std::vector<Reference::Spawn> originals;int emitted=0,rounds=0;
        for(int frame=0;frame<4200;++frame) {
            checkedFrame=frame;bullets->Reset();const Vector3 player{240+30*std::sin(frame*.01F),350,0};
            source.Tick(player);actor.Update(1.0F/60,{(player.x-192)*scale,0,10-player.y*scale},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale,"x");Near(actor.position.z,10-source.position.y*scale,"z");
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.damageable==source.damageable);
            assert(actor.contactEnabled==source.contact&&actor.interactable==source.interactable&&actor.defeatActors==source.defeatActors);
            assert(source.card==129&&source.timeout==4200&&source.timeoutCallback==81&&source.health==1800&&source.nativeCallback==(frame>=120?10:-1));
            assert(actor.spawnEvents.size()==source.spawns.size());
            if(seed==7&&rank==16)for(const auto& spawn:source.spawns)birthFrames.push_back(frame);
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];
                assert(e.sub==80&&a.sourceHealth==e.health&&a.sourceItem==e.item&&a.sourceScore==e.score&&behavior::Valid(a));
                assert(!a.damageable&&a.visible&&a.stageClearResponse==behavior::StageClearResponse::Remove&&a.seed==seed+children.size()+1);
                Near(a.position.x,(e.position.x-192)*scale,"spawn.x");Near(a.position.z,10-e.position.y*scale,"spawn.z");Near(a.position.y,0,"spawn.y");
                children.push_back(a);originals.push_back(e);
            }
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());emitted+=static_cast<int>(actual.size());
            if(seed==7&&rank==16)shotCounts.push_back(static_cast<int>(actual.size()));
            // 同一帧先32扇、再瞄准扇；将同步发射的两轮分开统计，避免把帧数误当轮数。
            if(actual.size()>=32)++rounds;if(!actual.empty()&&actual.size()!=32)++rounds;
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];assert(b.nativeType==e.type&&b.nativeColor==e.color&&e.flags==0);
                Near(b.position.x,(e.position.x-192)*scale,"bullet.x");Near(b.position.z,10-e.position.y*scale,"bullet.z");
                Near(b.Velocity().x,e.velocity.x*speedScale,"bullet.vx",1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,"bullet.vz",1e-4F);
            }
        }
        assert(children.size()==24&&emitted==7448&&rounds==324&&actor.Random(1)==source.Random(1));
        if(seed==7&&rank==16)routes.assign(children.begin(),children.begin()+6);
        for(int route=0;route<6;++route) {
            checkedRoute=route;behavior::Actor child(behavior::UpdateRate::Hz60);child.Activate(children[route]);child.SetRank(rank);bullets->Reset();
            Reference original(80,originals[route].position,children[route].seed,rank,10);
            for(int frame=0;frame<=390;++frame) {
                checkedFrame=frame;original.Tick({240,350,0});child.Update(1.0F/60,{},*bullets);
                Near(child.position.x,(original.position.x-192)*scale,"child.x");Near(child.position.z,10-original.position.y*scale,"child.z");
                assert(child.active==original.active&&child.animationScript==original.animation&&child.soundEvents==original.sounds);
                assert(child.visible&&!child.damageable&&!child.contactEnabled&&!child.interactable&&child.health==10);
                assert((child.animationInterrupt?child.animationInterrupt->label:-1)==original.interrupt);
                Near(child.hitRadius,original.radius*scale,"radius",1e-5F);
                std::vector<const Bullet*> beams;bullets->Pool().ForEachActive([&](const Bullet& b){assert(b.IsBeam());beams.push_back(&b);});
                std::vector<const Reference::Beam*> expected;for(const auto& b:original.beams)if(b.active)expected.push_back(&b);
                if(beams.size()!=expected.size())std::cerr<<"Clock route "<<route<<" frame "<<frame<<" beams "<<beams.size()<<" vs "<<expected.size()<<'\n';
                assert(beams.size()==expected.size());
                for(std::size_t n=0;n<beams.size();++n) {
                    const auto& a=*beams[n];const auto& e=*expected[n];
                    Near(a.beamAnchor.x,(e.anchor.x-192)*scale,"beam.x");Near(a.beamAnchor.z,10-e.anchor.y*scale,"beam.z");
                    Near(a.Direction().x,std::cos(e.angle),"beam.dx",1e-4F);Near(a.Direction().z,-std::sin(e.angle),"beam.dz",1e-4F);
                    Near(a.beamLength,(e.end-e.start)*scale,"beam.length",1e-5F);Near(a.radius,e.width*scale/2,"beam.radius",1e-5F);
                    Near(a.age,e.age/60.0F,"beam.age",1e-5F);assert(a.beamAnimationScript==1);
                    Near(a.beamGrowSeconds,e.grow/60.0F,"beam.grow",1e-5F);Near(a.warningSeconds,e.warning/60.0F,"beam.warning",1e-5F);
                    Near(a.beamFadeSeconds,e.fade/60.0F,"beam.fade",1e-5F);
                    if(e.forcedFade<0)Near(a.dangerEndSeconds,(e.grow+e.sustain-e.tail)/60.0F,"beam.danger",1e-5F);
                    assert(a.Dangerous()==(e.age>=e.warning&&(e.forcedFade<0?e.age<e.grow+e.sustain-e.tail:e.forcedFade<e.fade-e.tail)));
                }
                bullets->Update(1.0F/60);
            }
            assert(!child.active&&child.finished&&child.Random(1)==original.Random(1)&&!bullets->Pool().DroppedSpawns());
        }
    }
}
void TestClocksAndLifecycle() {
    assert(routes.size()==6);auto bullets=std::make_unique<BulletSystem>();
    for(int factor:{1,2})for(const auto& spawn:routes) {
        behavior::Actor child(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);child.Activate(spawn);bullets->Reset();
        for(int tick=0;tick<=390*factor;++tick) {
            child.Update(1.0F/(60*factor),{},*bullets);
            assert(child.active==(tick<390*factor));
            assert(child.animationInterrupt.has_value()==(tick>=300*factor));
            if(tick==29*factor)assert(bullets->Pool().ActiveCount()==0);
            if(tick==30*factor)assert(bullets->Pool().ActiveCount()==4);
            bullets->Update(1.0F/(60*factor));
        }
    }
    for(const auto& spawn:routes) {
        behavior::Actor child;child.Activate(spawn);bullets->Reset();child.Update(100.0F/60,{},*bullets);assert(child.active&&bullets->Pool().ActiveCount()==4);
        child.ClearFromStage();assert(!child.active&&child.finished&&!child.deathEffect);
        bullets->ClearEnemyBullets();child.Activate(spawn);child.Update(31.0F/60,{},*bullets);assert(child.active&&bullets->Pool().ActiveCount()==4);
        // 清池后复用相同槽，新弹不能被旧光束句柄旋转或移动。
        bullets->Reset();auto* replacement=bullets->Spawn(BulletOwner::Enemy,{3,2,1},{},.1F,WHITE,0);assert(replacement);
        child.Update(1.0F/120,{},*bullets);Near(replacement->position.x,3,"reuse.x");Near(replacement->position.y,2,"reuse.y");Near(replacement->position.z,1,"reuse.z");
    }
}
// 半帧位置来自独立线性插值公式，方向来自源角速度；不能只比较120 Hz实现与自身快照。
void TestHalfFrames() {
    for(const auto& spawn:routes)for(bool spatial:{false,true}) {
        auto config=spawn;config.pattern.spatialShots=spatial;behavior::Actor child;child.Activate(config);
        auto bullets=std::make_unique<BulletSystem>();const float rotation=spawn.position.x<0?0.019634954631328583F:-0.019634954631328583F;
        for(int tick=0;tick<=640;++tick) {
            const auto before=child.Position(),velocity=child.motion.velocity;
            child.Update(1.0F/120,{},*bullets);const double frame=tick*.5;
            if(!spatial&&frame>=80&&frame<299.5)Near(child.position.x,spawn.position.x*(1-static_cast<float>((frame-80)/110)),"half.x");
            int count=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
                const float angle=static_cast<float>(-std::numbers::pi/2+count*std::numbers::pi/2+rotation*(frame-30+.5));
                const float tilt=spatial?(spawn.position.x<0?.42F:-.42F):0;
                Near(b.Direction().x,std::cos(angle)*std::cos(tilt),"half.dx",1e-4F);
                Near(b.Direction().y,std::cos(angle)*std::sin(tilt),"half.dy",1e-4F);
                Near(b.Direction().z,-std::sin(angle),"half.dz",1e-4F);
                // Follow读取应用上一半步位移后的位置；插值末步在Follow之后才吸附终点。
                Near(b.beamAnchor.x,before.x+velocity.x/120,"half.anchor.x");
                Near(b.beamAnchor.y,before.y+velocity.y/120,"half.anchor.y");
                Near(b.beamAnchor.z,before.z+velocity.z/120,"half.anchor.z");
                Near(BulletSystem::Length(b.Direction()),1,"half.unit",1e-5F);++count;
            });
            assert(count==(frame>=30?4:0));bullets->Update(1.0F/120);
        }
    }
}
void TestParentClocksAndDisplayRates() {
    assert(shotCounts.size()==4200&&birthFrames.size()==24);
    for(int factor:{1,2}) {
        behavior::Actor actor(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);actor.Reset({0,0,5.71428585F},1800,7);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::ClockOfThePast(actor));auto bullets=std::make_unique<BulletSystem>();std::size_t next=0;
        for(int tick=0;tick<4200*factor;++tick) {
            bullets->Reset();actor.Update(1.0F/(60*factor),{2,0,-6},*bullets);
            assert(actor.CanTakeDamage()==(tick>=120*factor));
            int births=0;while(next<birthFrames.size()&&tick==birthFrames[next]*factor) {++next;++births;}
            assert(actor.spawnEvents.size()==static_cast<std::size_t>(births));
            assert(bullets->Pool().ActiveCount()==(tick%factor==0?shotCounts[tick/factor]:0));
        }
        assert(next==birthFrames.size());
    }
    for(bool spatial:{false,true})for(int route=-1;route<6;++route) {
        Vector3 position{};std::vector<Bullet> expected;std::vector<behavior::Spawn> births;float random=0;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;auto bullets=std::make_unique<BulletSystem>();
            if(route<0) {actor.Reset({0,0,5.71428585F},1800,7);F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::ClockOfThePast(actor));}
            else {auto config=routes[route];config.pattern.spatialShots=spatial;actor.Activate(config);}
            std::vector<behavior::Spawn> children;const int seconds=route<0?20:4;
            // 只汇总逻辑发射/可控光束几何，不用显示步长积分弹幕；实际Game固定步积分由图形专项检查。
            for(int frame=0;frame<seconds*fps;++frame) {actor.Update(1.0F/fps,{2,3,-6},*bullets);children.insert(children.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());}
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const float nextRandom=actor.Random(1);
            assert(actor.phaseFrames==seconds*60&&!bullets->Pool().DroppedSpawns());
            if(fps==120) {position=actor.position;expected=actual;births=children;random=nextRandom;}
            else {
                Near(actor.position.x,position.x,"display.x");Near(actor.position.y,position.y,"display.y");Near(actor.position.z,position.z,"display.z");
                assert(nextRandom==random&&actual.size()==expected.size()&&children.size()==births.size());
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& e=expected[n];
                    Near(a.position.x,e.position.x,"display.bx");Near(a.position.y,e.position.y,"display.by");Near(a.position.z,e.position.z,"display.bz");
                    Near(a.Velocity().x,e.Velocity().x,"display.vx",1e-4F);Near(a.Velocity().y,e.Velocity().y,"display.vy",1e-4F);Near(a.Velocity().z,e.Velocity().z,"display.vz",1e-4F);
                    Near(a.Direction().x,e.Direction().x,"display.dx",1e-5F);Near(a.Direction().y,e.Direction().y,"display.dy",1e-5F);Near(a.Direction().z,e.Direction().z,"display.dz",1e-5F);
                    assert(a.nativeColor==e.nativeColor&&a.nativeType==e.nativeType);
                }
                for(std::size_t n=0;n<children.size();++n)assert(children[n].seed==births[n].seed&&children[n].sourceHealth==births[n].sourceHealth);
            }
            assert(route<0?children.size()==6:actual.size()==4);
        }
    }
}
void TestBeamFailure() {
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto& spawn:routes) {
        bullets->Reset();while(bullets->Spawn(BulletOwner::Enemy,{}, {},.1F,WHITE,0)) {}
        const auto dropped=bullets->Pool().DroppedSpawns();const auto count=bullets->Pool().ActiveCount();behavior::Actor child;child.Activate(spawn);
        child.Update(391.0F/60,{},*bullets);assert(!child.active&&child.finished&&bullets->Pool().DroppedSpawns()==dropped+4&&bullets->Pool().ActiveCount()==count);
        bullets->Pool().ForEachActive([](const Bullet& b){assert(!b.IsBeam()&&b.position.x==0&&b.position.y==0&&b.position.z==0);});
    }
}
void TestPrelude() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,6.42857143F},1000,seed);actor.SetRank(rank);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::SurvivalPrelude(actor));Reference source(81,{192,80,0},seed,rank);
        auto bullets=std::make_unique<BulletSystem>();
        for(int frame=0;frame<=90;++frame) {
            checkedFrame=frame;source.Tick({240,350,0});actor.Update(1.0F/60,{},*bullets);
            assert(actor.health==source.health&&source.reserve==1000&&source.timeout==1800&&source.timeoutCallback==82&&source.healthCallback==82);
            assert(actor.animationScript==source.animation&&actor.dropEvents==source.drops&&actor.soundEvents==source.sounds&&actor.damageable==source.damageable);
            assert(actor.defeatActors==source.defeatActors&&source.emissions.empty()&&actor.spawnEvents.empty()&&bullets->Pool().ActiveCount()==0);
            Near(actor.position.x,(source.position.x-192)*scale,"prelude.x");Near(actor.position.z,10-source.position.y*scale,"prelude.z");
            assert(static_cast<bool>(actor.pendingCallback)==(frame==90));assert(source.handoff==(frame==90?82:-1));
        }
        assert(actor.Random(1)==source.Random(1)&&!actor.finished);
    }
    for(int factor:{1,2})for(bool damage:{false,true}) {
        F boss(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120,false);
        const bosses::StartConfig config{.position={0,0,6.42857143F},.seed=7,.configure=F::Behaviors::ConfigurePractice,
            .defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,.phases={{{F::Behavior::SurvivalPrelude},{{{0,1,WHITE}}}}}};
        boss.StartBoss(config);assert(boss.health==1000&&boss.maxHealth==1000&&boss.HealthBar().segments[0].remaining==0);
        assert(boss.DescribeSkill(F::Behavior::SurvivalPrelude).health==0);auto bullets=std::make_unique<BulletSystem>();bool completed=false;
        for(int tick=0;tick<=90*factor;++tick) {
            const bool ready=boss.Update(1.0F/(60*factor),{},*bullets);
            assert(boss.HealthBar().segments[0].remaining==0&&std::isfinite(boss.HealthBar().segments[0].remaining));
            assert(boss.CanTakeDamage()==(tick>=30*factor));
            if(damage&&tick==45*factor)boss.health=999;
            if(ready||boss.ShouldAdvanceFromDamage()) {assert(tick==(damage?45:90)*factor);completed=true;break;}
        }
        assert(completed&&!boss.SkillDefeated());
    }
}
/// @brief 等面积球冠保留源扇形角宽、每轮数目和速率；瞄准轴含玩家真实高度，固定轴保持世界方向。
void TestSpatialFans() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({0,0,5.71428585F},1800,7);actor.SetRank(32);F::Behaviors::ConfigurePractice(actor,true);actor.Start(F::Behaviors::ClockOfThePast(actor));
    const Vector3 player{2,3,-6};
    for(int frame=0;frame<=210;++frame) {bullets->Reset();actor.Update(1.0F/60,player,*bullets);}
    std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});assert(actual.size()==49);
    bool above=false,below=false;
    for(int group=0;group<2;++group) {
        const int count=group==0?32:17,offset=group==0?0:32;
        const auto axis=group==0?Vector3{0,0,1}:BulletSystem::DirectionTo(actor.position,player);
        const float halfAngle=group==0?31*.5F*0.13089969754219055F:16*.5F*0.15707963705062866F;
        const float sourceSpeed=(group==0?5.199999809265137F:1.7999999523162842F)+.5F;
        for(int n=0;n<count;++n) {
            const auto& b=*actual[offset+n];const float speed=BulletSystem::Length(b.Velocity());
            Near(speed,sourceSpeed*speedScale,"spatial.speed",1e-4F);
            const float cosine=1-(1-std::cos(halfAngle))*(n+.5F)/count;
            Near((b.Velocity().x*axis.x+b.Velocity().y*axis.y+b.Velocity().z*axis.z)/speed,cosine,"spatial.area",1e-5F);
            Near(b.position.x,actor.position.x,"spatial.origin.x");Near(b.position.y,actor.position.y,"spatial.origin.y");Near(b.position.z,actor.position.z,"spatial.origin.z");
            above|=b.Velocity().y>1e-5F;below|=b.Velocity().y<-1e-5F;assert(b.nativeType==3&&b.nativeColor==2);
        }
    }
    assert(above&&below);
}
void TestIncomingHandoff() {
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::EighthNonspell,F::Behavior::ClockOfThePast},{{{0,.5F,WHITE},{1,.5F,PINK}}}}}};
        F boss;boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false;
        for(int tick=0;tick<3700;++tick) {
            bullets->Reset();const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(damage&&tick>400&&boss.CanTakeDamage())boss.health=1799;
            if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.AdvanceSkill());advanced=true;break;}
        }
        assert(advanced&&boss.health==1800&&boss.attack==2&&!boss.CanTakeDamage());
        int children=0;
        for(int tick=0;tick<=420;++tick) {bullets->Reset();boss.Update(1.0F/120,{2,3,-6},*bullets);children+=static_cast<int>(boss.SpawnEvents().size());}
        assert(boss.CanTakeDamage()&&boss.SpellCardNumber()==129&&children==2&&bullets->Pool().ActiveCount()==49);
    }
}
void TestRole() {
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        F boss;const auto setting=boss.DescribeSkill(F::Behavior::ClockOfThePast,static_cast<ShotType>(shot));
        assert(setting.health==1800&&setting.timeoutFrames==4200&&setting.spellCard==129&&setting.reactToBomb);
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::ClockOfThePast,F::Behavior::SurvivalPrelude},{{{0,.5F,PINK},{1,.5F,WHITE}}}}}};
        boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool completed=false,bat=false,recovered=false;
        for(int tick=0;tick<8500;++tick) {
            bullets->Reset();boss.SetPlayerBombActive(tick<30||tick>=250&&tick<280);const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(tick<240)assert(boss.animationScript!=165&&!boss.CanTakeDamage());
            if(tick==260) {assert(boss.animationScript==165&&!boss.CanBeTargeted());bat=true;}
            if(tick==400) {assert(boss.CanTakeDamage()&&boss.animationScript!=165);recovered=true;}
            if(damage&&tick>2400&&boss.CanTakeDamage())boss.health=0;
            if(ready||boss.ShouldAdvanceFromDamage()) {if(!damage)assert(tick==8400);completed=true;break;}
        }
        // 接续真实81过渡，保留1000寄存器及零有效HP；后继130仍单独验收。
        assert(completed&&bat&&recovered&&boss.SkillDefeated()==damage);
        assert(boss.AdvanceSkill()&&boss.attack==2&&boss.health==1000&&!boss.CanTakeDamage());
        boss.Update(1.0F/120,{},*bullets);assert(boss.DefeatActorsRequested()&&boss.HealthBar().segments[1].remaining==0);
        boss.SetPlayerBombActive(true);boss.StartBoss(config);boss.Update(1.0F/120,{},*bullets);assert(boss.animationScript!=165);
    }
}
}
/// @brief 第八符及真实81过渡的独立逻辑专项；后继130和正式Extra流程须另行完成。
void RunStageExtraClockTests() {
    TestSource();TestClocksAndLifecycle();TestHalfFrames();TestParentClocksAndDisplayRates();TestBeamFailure();TestSpatialFans();TestPrelude();TestIncomingHandoff();TestRole();
    std::cout<<"Validated Extra Clock of the Past source diffs, six laser-orb routes, half steps, display rates, pool failure, real prelude handoff and Bomb recovery\n";
}

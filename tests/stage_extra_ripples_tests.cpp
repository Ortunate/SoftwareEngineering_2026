// 本文件以独立Extra源指令验证最终符的生命/时钟参数、随机偏移、反弹及最终退场，不从生产配方生成期望值。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>
namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Ref=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448,speedScale=scale*60;
int frame=0,rank=0;
void Near(float a,float b,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {
        std::cerr<<"Extra ripples frame "<<frame<<" rank "<<rank<<' '<<field<<": "<<a<<" vs "<<b<<'\n';std::abort();
    }
}
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
struct Trace {Bullet bullet;Ref::Emission source;};
std::vector<Trace> traces;
int Health(int profile,int n) {return profile==0?6000:n<1000?6000:n<3000?5751:n<5000?3001:25;}
void TestSource() {
    for(int profile:{0,1})for(unsigned seed:{7U,0x9e3779b9U})for(int r:{10,16,18,31,32}) {
        rank=r;auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,96,0}),6000,seed);F::Behaviors::ConfigurePractice(actor,false);actor.SetRank(rank);
        actor.Start(F::Behaviors::RipplesOf495Years(actor));Ref source(92,{192,96,0},seed,rank,6000);source.death=95;
        int last=-1,previousWait=0,effects=0,rounds=0;
        Vector3 nextPoint{};
        for(frame=0;frame<9000;++frame) {
            const int hp=Health(profile,frame);actor.health=source.health=hp;
            bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);
            const auto p=World(source.position);Near(actor.position.x,p.x,"parent.x");Near(actor.position.z,p.z,"parent.z");
            assert(actor.damageable==source.damageable&&actor.contactEnabled==source.contact&&actor.interactable==source.interactable);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.defeatActors==source.defeatActors);
            assert(actor.spawnEvents.empty()&&source.spawns.empty()&&actor.dropEvents==source.drops);
            assert(source.card==131&&source.timeout==9000&&source.timeoutCallback==95&&source.nativeCallback==(frame>=120?10:-1));
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            if(actual.size()!=source.emissions.size())std::cerr<<"ripples count "<<frame<<": "<<actual.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            if(!actual.empty()) {
                assert(actual.size()==88);++rounds;
                assert(frame==(last<0?180:last+previousWait+1));
                previousWait=40+(frame<7200?hp:0)*240/6000;last=frame;
                if(rounds==1) {Near(actual[0]->position.x,0,"initial.origin.x");Near(actual[0]->position.z,6.42857143F,"initial.origin.z");}
                else {Near(actual[0]->position.x,nextPoint.x,"previous.offset.x");Near(actual[0]->position.z,nextPoint.z,"previous.offset.z");}
                const float effective=static_cast<float>(frame<7200?hp:0);
                const float width=(320-effective*160/6000)*scale,depth=(128-effective*64/6000)*scale;
                nextPoint={actor.position.x+actor.skills.offset.x,0,actor.position.z+actor.skills.offset.z};
                assert(std::abs(nextPoint.x)<=width*.5F+1e-5F&&std::abs(nextPoint.z-(10-96*scale))<=depth*.5F+1e-5F);
            }
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];const auto origin=World(e.position);
                assert(e.type==2&&e.color==6&&e.flags==2560&&e.bounces==1&&b.bouncesRemaining==1&&!b.reflectLowerZ&&b.sampleMotionAtStart);
                assert(b.nativeType==2&&b.nativeColor==6&&b.spawnVisualSeconds==0);
                Near(b.position.x,origin.x,"bullet.x");Near(b.position.z,origin.z,"bullet.z");
                Near(b.velocity.x,e.velocity.x*speedScale,"bullet.vx",1e-4F);Near(b.velocity.z,-e.velocity.y*speedScale,"bullet.vz",1e-4F);
                Near(b.bounceSpeed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,"bounce.speed",1e-4F);
                Near(b.bounceHalfExtents.x,7*scale,"bounce.half.x",1e-5F);Near(b.bounceHalfExtents.z,8*scale,"bounce.half.z",1e-5F);
                if(profile==0&&seed==7&&rank==32&&(frame==180||frame==7205))traces.push_back({b,e});
            }
            CompareEffects(actor,source);effects+=source.effectRequests;
        }
        assert(rounds==(profile==0?69:117)&&effects==180&&actor.Random(1)==source.Random(1));
    }
    assert(traces.size()==176);
}
void TestProjectileTraces() {
    int reflected=0,consumed=0;auto bullets=std::make_unique<BulletSystem>();
    for(const auto& trace:traces) {
        bullets->Reset();auto* b=bullets->Spawn(BulletOwner::Enemy,{},{},.1F,WHITE,0);*b=trace.bullet;
        float x=trace.source.position.x,y=trace.source.position.y,vx=trace.source.velocity.x,vy=trace.source.velocity.y;
        const float speed=std::hypot(vx,vy);int remaining=1;
        for(frame=0;frame<700;++frame) {
            if(remaining&&(x+7<0||x-7>384||y+8<0||y-8>448)) {
                const bool flipX=x<0||x>=384,flipY=y<0;if(flipX)vx=-vx;if(flipY)vy=-vy;
                const float length=std::hypot(vx,vy);vx=vx/length*speed;vy=vy/length*speed;
                --remaining;++consumed;reflected+=flipX||flipY;
            }
            x+=vx;y+=vy;bullets->Update(1.0F/60);
            const auto p=World({x,y,0});Near(b->position.x,p.x,"trace.x");Near(b->position.z,p.z,"trace.z");
            Near(b->velocity.x,vx*speedScale,"trace.vx",1e-4F);Near(b->velocity.z,-vy*speedScale,"trace.vz",1e-4F);
            assert(b->bouncesRemaining==remaining);if(!b->active)break;
        }
    }
    assert(reflected>100&&consumed>reflected);
}
void TestClocksAndSpatial() {
    for(int factor:{1,2}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);
        actor.Reset(World({192,96,0}),6000,7);F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::RipplesOf495Years(actor));
        int next=180;
        for(int tick=0;tick<9000*factor;++tick) {
            bullets->Reset();actor.Update(1.0F/(60*factor),{},*bullets);assert(actor.damageable==(tick>=120*factor));
            const bool fire=tick==next*factor;assert(bullets->Pool().ActiveCount()==(fire?88:0));
            if(fire)next+=next<7200?281:41;
        }
    }
    // 三维环均匀覆盖球面；随机发射点增加高度但仍只消费源的角度/X/Z三次随机数。
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset(World({192,96,0}),6000,7);
    F::Behaviors::ConfigurePractice(actor,true);actor.Start(F::Behaviors::RipplesOf495Years(actor));Ref source(92,{192,96,0},7,16,6000);
    bool above=false,below=false;
    for(frame=0;frame<=461;++frame) {
        bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,{2,3,-6},*bullets);
        if(frame==180||frame==461) {
            int n=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
                const auto& e=source.emissions[n++];Near(BulletSystem::Length(b.velocity),std::hypot(e.velocity.x,e.velocity.y)*speedScale,"spatial.speed",1e-4F);
                Near(b.position.x,frame==180?actor.position.x:(e.position.x-192)*scale,"spatial.x");Near(b.position.z,frame==180?actor.position.z:10-e.position.y*scale,"spatial.z");
                assert(std::abs(b.position.y)<=1.25F&&b.bouncesRemaining==1);above|=b.velocity.y>0;below|=b.velocity.y<0;
            });assert(n==88);
        }
    }
    assert(above&&below&&actor.Random(1)==source.Random(1));
    for(bool spatial:{false,true}) {
        std::vector<Bullet> expected;Vector3 position{};float random=0;
        for(int fps:{120,30,60,144,240}) {
            bullets->Reset();behavior::Actor a;a.Reset(World({192,96,0}),6000,7);F::Behaviors::ConfigurePractice(a,spatial);a.Start(F::Behaviors::RipplesOf495Years(a));
            for(int update=0;update<150*fps;++update)a.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const float next=a.Random(1);
            assert(a.phaseFrames==9000&&!bullets->Pool().DroppedSpawns());
            if(fps==120) {expected=actual;position=a.position;random=next;}
            else {
                assert(expected.size()==actual.size()&&random==next);Near(a.position.x,position.x,"display.x");Near(a.position.y,position.y,"display.y");Near(a.position.z,position.z,"display.z");
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& b=actual[n];const auto& e=expected[n];assert(b.nativeType==e.nativeType&&b.nativeColor==e.nativeColor);
                    Near(b.position.x,e.position.x,"display.bullet.x");Near(b.position.y,e.position.y,"display.bullet.y");Near(b.position.z,e.position.z,"display.bullet.z");
                    Near(b.velocity.x,e.velocity.x,"display.vx",1e-4F);Near(b.velocity.y,e.velocity.y,"display.vy",1e-4F);Near(b.velocity.z,e.velocity.z,"display.vz",1e-4F);
                }
            }
        }
    }
}
void TestExit() {
    for(unsigned seed:{7U,0x9e3779b9U})for(bool damage:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset(World({192,80,0}),1,seed);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::FinalExit(actor,damage));Ref source(95,{192,80,0},seed,16,1);source.deathMode=damage?0:3;
        int effects=0;
        for(frame=0;frame<=60+(damage?0:1);++frame) {
            bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,{},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale,"exit.x");Near(actor.position.z,10-source.position.y*scale,"exit.z");
            assert(!actor.damageable&&!source.damageable&&!actor.contactEnabled&&!source.contact&&actor.interactable&&source.interactable);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.defeatActors==source.defeatActors&&actor.active==source.active&&actor.finished==(frame==60+(damage?0:1)));
            CompareEffects(actor,source);effects+=source.effectRequests;
        }
        assert(effects==60&&!source.active&&source.death==(damage?96:-1)&&actor.health==(damage?0:1)&&actor.Random(1)==source.Random(1));
    }
    for(int factor:{1,2})for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();F boss(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120,false);
        const auto def=boss.DescribeSkill(F::Behavior::RipplesOf495Years,static_cast<ShotType>(shot));
        assert(def.health==6000&&def.timeoutFrames==9000&&def.spellCard==131&&!def.survival&&def.reactToBomb);
        const bosses::StartConfig config{.position=World({192,96,0}),.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::FinalExit,.retreat=F::Behavior::FinalExit,
            .phases={{{F::Behavior::FinalPrelude,F::Behavior::RipplesOf495Years},{{{0,.1F,WHITE},{1,.9F,PINK}}}}}};
        boss.StartBoss(config);bool ready=false;
        for(int tick=0;tick<=90*factor;++tick)ready=boss.Update(1.0F/(60*factor),{},*bullets);
        assert(ready&&boss.AdvanceSkill()&&boss.attack==2&&!boss.CanTakeDamage());
        bool bat=false,recovered=false;
        for(int tick=0;tick<=9000*factor;++tick) {
            bullets->Reset();boss.SetPlayerBombActive(tick<20||tick>=250*factor&&tick<280*factor);
            ready=boss.Update(1.0F/(60*factor),{2,3,-6},*bullets);
            if(tick<120*factor)assert(!boss.CanTakeDamage()&&boss.animationScript!=165);
            if(tick==260*factor) {bat=true;assert(!boss.CanBeTargeted()&&boss.animationScript==165);}
            if(tick==400*factor) {recovered=true;assert(boss.CanTakeDamage()&&boss.animationScript==160);}
            if(damage&&tick==500*factor)boss.health=0;
            if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.SpellSucceeded()==damage&&boss.AdvanceSkill());break;}
        }
        assert(bat&&recovered&&boss.Defeated()==damage&&boss.active&&!boss.CanContactPlayer());
        for(int tick=0;tick<=60*factor+(damage?0:1);++tick) {
            ready=boss.Update(1.0F/(60*factor),{},*bullets);assert(ready==(tick==60*factor+(damage?0:1)));
            assert(boss.active&&!boss.CanTakeDamage()&&!boss.CanContactPlayer());
        }
        assert(!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==damage);
    }
}
}
/// @brief 最终符源差分、反弹积分、两逻辑频率、五显示帧率及真实前奏/最终退场专项。
void RunStageExtraRipplesTests() {TestSource();TestProjectileTraces();TestClocksAndSpatial();TestExit();std::cout<<"Validated Extra final spell source, 176 bounce traces, HP clocks, spatial/display rates, Bomb and final exit\n";}

// 本文件以独立Extra源指令和源像素反弹模型验证Catadioptric，不从生产行为生成期望值。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>

namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;
int checkedFrame=0,checkedRank=0;unsigned checkedSeed=0;
void Near(float actual,float expected,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Catadioptric source72 frame "<<checkedFrame<<" rank "<<checkedRank<<" seed "<<checkedSeed<<' '<<field<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
constexpr float scale=20.0F/448,speedScale=scale*60;
struct Trace {Bullet bullet;stage_extra::reference::FlandreReference::Emission source;};
std::vector<Trace> TestSource() {
    std::vector<Trace> traces;
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        checkedSeed=seed;checkedRank=rank;auto bullets=std::make_unique<BulletSystem>();
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5.71428585F},1520,seed);actor.SetRank(rank);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::Catadioptric(actor));
        stage_extra::reference::FlandreReference source(72,{192,96,0},seed,rank,1520);source.death=76;
        int emitted=0,effects=0;
        for(int frame=0;frame<4200;++frame) {
            checkedFrame=frame;bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,{2,0,-6},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale,"x");Near(actor.position.z,10-source.position.y*scale,"z");
            if(actor.animationScript!=source.animation||actor.soundEvents!=source.sounds||actor.dropEvents!=source.drops) {
                std::cerr<<"Catadioptric state frame "<<frame<<" animation "<<actor.animationScript<<" vs "<<source.animation<<" sounds ";
                for(int sound:actor.soundEvents)std::cerr<<sound<<',';std::cerr<<" vs ";for(int sound:source.sounds)std::cerr<<sound<<',';std::cerr<<'\n';std::abort();
            }
            assert(actor.damageable==source.damageable&&actor.contactEnabled==source.contact&&actor.interactable==source.interactable);
            assert(actor.visible==source.visible&&actor.defeatActors==source.defeatActors&&actor.spawnEvents.empty()&&source.spawns.empty());
            assert(source.card==128&&source.health==1520&&source.timeout==4200&&source.timeoutCallback==76&&source.nativeCallback==(frame>=120?10:-1));
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            if(actual.size()!=source.emissions.size())std::cerr<<"Catadioptric frame "<<frame<<" count "<<actual.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.bouncesRemaining==e.bounces&&!b.reflectLowerZ&&e.flags==2048);
                Near(b.position.x,(e.position.x-192)*scale,"bullet.x");Near(b.position.z,10-e.position.y*scale,"bullet.z");
                Near(b.velocity.x,e.velocity.x*speedScale,"velocity.x",1e-4F);Near(b.velocity.z,-e.velocity.y*speedScale,"velocity.z",1e-4F);
                Near(b.bounceSpeed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,"bounce.speed",1e-4F);
                const float half=e.type==9?32:e.type==6?16:8;
                Near(b.bounceHalfExtents.x,half*scale,"half.x");Near(b.bounceHalfExtents.z,half*scale,"half.z");
                Near(b.bounceLower.x,-192*scale,"lower.x");Near(b.bounceUpper.x,192*scale,"upper.x");
                Near(b.bounceLower.z,-10,"lower.z");Near(b.bounceUpper.z,10,"upper.z");
                if(seed==7&&rank==32&&(frame==180||frame==350))traces.push_back({b,e});
            }
            emitted+=static_cast<int>(actual.size());koumakyou::stage_extra::reference::CompareEffects(actor,source);effects+=source.effectRequests;
            // 第二轮118直接跳回最初五向簇；源表尾七向簇不可达，不能纳入周期或伪造额外发射。
            if(frame==180||frame==860||frame==1540)assert(actual.size()==130&&actual.front()->bouncesRemaining==2);
            if(frame==350||frame==1030)assert(actual.size()==130&&actual.front()->bouncesRemaining==1);
        }
        assert(emitted==3120&&effects==1080&&actor.Random(1)==source.Random(1));
    }
    assert(traces.size()==260);return traces;
}
/// @brief 按原生先判越界再位移独立积分源像素；只验证自然回收前，不宣称模拟原生完整弹池。
void TestProjectileTraces(const std::vector<Trace>& traces) {
    int reflected=0,consumed=0;auto bullets=std::make_unique<BulletSystem>();
    for(const auto& trace:traces) {
        bullets->Reset();auto* actual=bullets->Spawn(BulletOwner::Enemy,{},{},.1F,WHITE,0);assert(actual);*actual=trace.bullet;
        float x=trace.source.position.x,y=trace.source.position.y,vx=trace.source.velocity.x,vy=trace.source.velocity.y;
        const float speed=std::hypot(vx,vy),half=trace.source.type==9?32:trace.source.type==6?16:8;
        int remaining=trace.source.bounces;
        for(int frame=0;frame<300;++frame) {
            checkedFrame=frame;
            if(remaining>0&&(x+half<0||x-half>384||y+half<0||y-half>448)) {
                const bool flipX=x<0||x>=384,flipY=y<0;
                if(flipX)vx=-vx;if(flipY)vy=-vy;
                // 原生以保存的速率和新角度重建速度；角落只消费一次，下缘离场不反射但仍消费。
                const float length=std::hypot(vx,vy);vx=vx/length*speed;vy=vy/length*speed;
                --remaining;++consumed;reflected+=flipX||flipY;
            }
            x+=vx;y+=vy;bullets->Update(1.0F/60);
            Near(actual->position.x,(x-192)*scale,"trace.x");Near(actual->position.z,10-y*scale,"trace.z");
            Near(actual->velocity.x,vx*speedScale,"trace.vx",1e-4F);Near(actual->velocity.z,-vy*speedScale,"trace.vz",1e-4F);
            assert(actual->bouncesRemaining==remaining);if(!actual->active)break;
        }
    }
    assert(reflected>200&&consumed>reflected);
}
int ExpectedCount(int frame) {
    if(frame<180)return 0;const int local=(frame-180)%680;
    if(local==0||local==170||local==290)return 130;
    if(local>=410&&local<=506&&(local-410)%24==0)return 26;
    return 0;
}
void TestClocksAndDisplayRates() {
    auto bullets=std::make_unique<BulletSystem>();
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        const int factor=rate==behavior::UpdateRate::Hz120?2:1;
        behavior::Actor actor(rate);actor.Reset({0,0,5.71428585F},1520,7);F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::Catadioptric(actor));
        for(int tick=0;tick<4200*factor;++tick) {
            bullets->Reset();actor.Update(1.0F/(60*factor),{},*bullets);
            assert(actor.CanTakeDamage()==(tick>=120*factor));
            assert(bullets->Pool().ActiveCount()==(tick%factor==0?ExpectedCount(tick/factor):0));
        }
    }
    for(bool spatial:{false,true}) {
        std::vector<Bullet> expected;Vector3 position{};float random=0;
        for(int fps:{120,30,60,144,240}) {
            bullets->Reset();behavior::Actor actor;actor.Reset({0,0,5.71428585F},1520,7);F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::Catadioptric(actor));
            for(int frame=0;frame<20*fps;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const float next=actor.Random(1);
            assert(actor.phaseFrames==1200&&!bullets->Pool().DroppedSpawns());
            if(fps==120) {expected=actual;position=actor.position;random=next;}
            else {
                assert(actual.size()==expected.size()&&random==next);
                Near(actor.position.x,position.x,"display.x");Near(actor.position.y,position.y,"display.y");Near(actor.position.z,position.z,"display.z");
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& b=expected[n];assert(a.nativeType==b.nativeType&&a.nativeColor==b.nativeColor&&a.bouncesRemaining==b.bouncesRemaining);
                    Near(a.position.x,b.position.x,"display.bullet.x");Near(a.position.y,b.position.y,"display.bullet.y");Near(a.position.z,b.position.z,"display.bullet.z");
                    Near(a.velocity.x,b.velocity.x,"display.vx",1e-4F);Near(a.velocity.y,b.velocity.y,"display.vy",1e-4F);Near(a.velocity.z,b.velocity.z,"display.vz",1e-4F);
                }
            }
        }
    }
}
void TestHandoffsAndBomb() {
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        F boss;const auto setting=boss.DescribeSkill(F::Behavior::Catadioptric,static_cast<ShotType>(shot));
        assert(setting.health==1520&&setting.timeoutFrames==4200&&setting.spellCard==128&&setting.reactToBomb);
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::Catadioptric,F::Behavior::EighthNonspell},{{{0,.5F,PINK},{1,.5F,WHITE}}}}}};
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
        boss.Update(1.0F/120,{},*bullets);assert(boss.DefeatActorsRequested());
        boss.SetPlayerBombActive(true);boss.StartBoss(config);boss.Update(1.0F/120,{},*bullets);assert(boss.animationScript!=165);
    }
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::SeventhNonspell,F::Behavior::Catadioptric},{{{0,.5F,WHITE},{1,.5F,PINK}}}}}};
        F boss;boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false;
        for(int tick=0;tick<3700;++tick) {
            bullets->Reset();const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(damage&&tick>400&&boss.CanTakeDamage())boss.health=1519;
            if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.AdvanceSkill());advanced=true;break;}
        }
        assert(advanced&&boss.health==1520&&boss.attack==2&&!boss.CanTakeDamage());
        for(int tick=0;tick<=360;++tick) {bullets->Reset();boss.Update(1.0F/120,{},*bullets);}
        assert(boss.CanTakeDamage()&&boss.SpellCardNumber()==128&&bullets->Pool().ActiveCount()==130);
    }
}
/// @brief 源每发随机速率不变，窄角区间延拓为等面积球冠；面积序号独立于演员高度/种子，不增取随机流。
void TestSpatialGeometry() {
    for(unsigned seed:{7U,0x9e3779b9U}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},1520,seed);actor.SetRank(32);F::Behaviors::ConfigurePractice(actor,true);actor.Start(F::Behaviors::Catadioptric(actor));
        stage_extra::reference::FlandreReference source(72,{192,96,0},seed,32,1520);
        for(int frame=0;frame<=180;++frame) {bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,{2,3,-6},*bullets);}
        std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});assert(actual.size()==130);
        const float centers[]{-2.356194496154785F,-1.9634953737258911F,-1.5707963705062866F,-1.1780972480773926F,-0.7853981852531433F};
        const int counts[]{9,3,5,8};const float halfAngles[]{0.02454369328916073F,0.06544984877109528F,0.07853981852531433F,0.09817477315664291F};
        int sample=0;bool above=false,below=false;
        for(int cluster=0;cluster<5;++cluster) {
            const int start=cluster*26;const float x=std::cos(centers[cluster]),z=-std::sin(centers[cluster]);
            const auto& center=*actual[start];const float centerSpeed=BulletSystem::Length(center.velocity);
            Near(center.velocity.x/centerSpeed,x,"spatial.center.x",1e-5F);Near(center.velocity.y,0,"spatial.center.y",1e-5F);Near(center.velocity.z/centerSpeed,z,"spatial.center.z",1e-5F);
            int index=start+1;
            for(int group=0;group<4;++group)for(int n=0;n<counts[group];++n,++index,++sample) {
                const auto& b=*actual[index];const auto& e=source.emissions[index];const float speed=BulletSystem::Length(b.velocity);
                Near(speed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,"spatial.speed",1e-4F);
                const float expected=1-(1-std::cos(halfAngles[group]))*spatial::RadicalInverse(sample+1,2);
                Near((b.velocity.x*x+b.velocity.z*z)/speed,expected,"spatial.area",1e-5F);
                above|=b.velocity.y>1e-5F;below|=b.velocity.y<-1e-5F;
                Near(b.position.x,actor.position.x,"spatial.origin.x");Near(b.position.y,actor.position.y,"spatial.origin.y");Near(b.position.z,actor.position.z,"spatial.origin.z");
            }
        }
        assert(above&&below&&actor.Random(1)==source.Random(1));
    }
}
}
/// @brief 第七符源行为、反弹积分、频率及角色交接专项；正式Extra流程另行验收。
void RunStageExtraCatadioptricTests() {
    const auto traces=TestSource();TestProjectileTraces(traces);TestClocksAndDisplayRates();TestSpatialGeometry();TestHandoffsAndBomb();
    std::cout<<"Validated Extra Catadioptric source diffs, 260 bounce traces, clocks, display rates, handoffs and Bomb recovery\n";
}

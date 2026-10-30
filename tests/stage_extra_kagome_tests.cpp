// 本文件以独立源像素弹池验证Kagome Kagome的邻域解放；期望不调用生产弹道、素材目录或配方。
#include "gameplay/bosses/flandre.hpp"
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace {
using namespace koumakyou;
constexpr float scale=20.0F/448,speedScale=scale*60,accelerationScale=scale*3600;
// width为源ANM主体逻辑高；state=0/5及缺失ANM不能参与，accel保存原生独立标量而非向量模长。
// 这里只覆盖本符普通弹及明确注入的边界，不模拟完整原生640槽分配、外部效果和全局随机流。
struct SourceBullet {
    float x,y,width;int color=9,state=1;bool anm=true;
    float accel=0,ax=0,ay=0,vx=0,vy=0;int accelerationLeft=0,spawnLeft=0;
};
int Release(std::vector<SourceBullet>& pool,float bossX,float bossY) {
    int count=0;
    for(const auto& big:pool) {
        if(big.state==0||big.state==5||!big.anm||big.width<30)continue;
        ++count;const float beta=std::atan2(big.y-bossY,big.x-bossX);
        for(auto& small:pool) {
            if(small.state==0||small.state==5||!small.anm||small.width>=30||small.accel!=0)continue;
            const float dx=small.x-big.x,dy=small.y-big.y;
            if(!(std::sqrt(dx*dx+dy*dy)<64))continue;
            const float theta=std::atan2(small.y-bossY,small.x-bossX);
            const float angle=beta+(theta-beta)*2.200000047683716F;
            small.accel=0.009999999776482582F;small.ax=std::cos(angle)*small.accel;small.ay=std::sin(angle)*small.accel;
            small.accelerationLeft=120;++small.color;
        }
    }
    return count;
}
// 独立离散积分：显现结束帧落入运动分支并消费首个加速时钟，此后共120次；视觉时钟不重置。
void Tick(SourceBullet& b) {
    if(b.spawnLeft>0&&--b.spawnLeft>0)return;
    if(b.accelerationLeft>0) {b.vx+=b.ax;b.vy+=b.ay;--b.accelerationLeft;}
    b.x+=b.vx;b.y+=b.vy;
}
void Near(float actual,float expected,float tolerance=1e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Kagome projectile "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
int releasedCount=0;
behavior::Routine ReleaseOnce(behavior::Actor& actor) {
    releasedCount=bosses::FlandreBoss::Behaviors::ReleaseCageBullets(actor);co_return;
}
Bullet* Make(BulletSystem& bullets,SourceBullet b,int type=1) {
    auto* result=bullets.Spawn(BulletOwner::Enemy,{(b.x-192)*scale,0,10-b.y*scale},
                              {b.vx*speedScale,0,-b.vy*speedScale},0.1F,WHITE,0);
    assert(result);result->nativeType=b.anm?type:-1;result->nativeColor=b.color;result->pendingExpiration=b.state==5;
    result->active=b.state!=0;result->acceleration={b.ax*accelerationScale,0,-b.ay*accelerationScale};
    // 非零/NaN标量沿X注入，确保生产零向量检查也拒绝无效加速度；本符解放前只有静止小弹。
    if(b.accel!=0&&b.ax==0&&b.ay==0)result->acceleration.x=b.accel*accelerationScale;
    return result;
}
void TestSlotOrderAndFilters() {
    for(bool reverse:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();
        // 两颗大弹同时接近第一颗小弹，反转它们的槽序必须改变小弹方向。
        std::vector<SourceBullet> source{{170,230,16},{150,224,64,3},{192,244,30,0},
            {256,244,16},{256.001F,244,16},{255.99F,244,16},
            {170,230,16,9,5},{170,230,16,9,1,false},
            {170,230,16,9,1,true,0.01F},{170,230,16,9,1,true,std::numeric_limits<float>::quiet_NaN()},
            {150,224,64,3,5},{150,224,64,3,1,false},{150,224,16,9,0}};
        if(reverse)std::swap(source[1],source[2]);
        std::vector<Bullet*> actual;
        // 失活槽安排在末尾，后续生成不会覆盖测试持有的指针。
        for(const auto& b:source)actual.push_back(Make(*bullets,b,b.width>=64?9:b.width>=30?7:1));
        auto* player=Make(*bullets,{170,230,16});player->owner=BulletOwner::Player;
        auto* beam=Make(*bullets,{170,230,16});beam->beamLength=2;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,0},2400,7);actor.skills.bulletBase=stage_assets::kNativeBulletBase;
        actor.Start(ReleaseOnce(actor));actor.Update(1.0F/60,{},*bullets);
        const int expected=Release(source,192,224);assert(releasedCount==expected&&expected==2);
        for(std::size_t n=0;n<source.size()-1;++n) {
            const auto& b=*actual[n];const auto& e=source[n];assert(b.nativeColor==e.color);
            if(std::isnan(e.accel)) {assert(std::isnan(b.acceleration.x));continue;}
            if(n==8)continue; // 注入非零加速度只用于验证不被重新解放，不是本符发射状态。
            Near(b.acceleration.x,e.ax*accelerationScale);Near(b.acceleration.z,-e.ay*accelerationScale);
            if(e.accelerationLeft>0) {
                assert(b.motionClock==0&&b.accelerationSeconds==2&&b.sampleMotionAtStart);
                assert(b.visual==stage_assets::kNativeBulletBase+b.nativeType*16+b.nativeColor);
            }
        }
        assert(player->nativeColor==9&&beam->nativeColor==9);
        assert(actual[3]->nativeColor==9&&actual[4]->nativeColor==9&&actual[5]->nativeColor==10); // 严格64像素边界。
        behavior::Actor untouched;untouched.Reset({},1,7);assert(actor.Random(1)==untouched.Random(1));
        // 重复调用不能转向或重启已经加速的小弹。
        const auto before=actual[0]->acceleration;actual[0]->accelerationSeconds=0.75F;actual[0]->motionClock=1.25;
        actor.Start(ReleaseOnce(actor));actor.Update(1.0F/60,{},*bullets);
        assert(actual[0]->nativeColor==10&&actual[0]->accelerationSeconds==0.75F&&actual[0]->motionClock==1.25);
        Near(actual[0]->acceleration.x,before.x);Near(actual[0]->acceleration.z,before.z);
        bullets->ClearEnemyBullets();auto* reused=Make(*bullets,{170,230,16});assert(reused->acceleration.x==0);
        Make(*bullets,{150,224,64,3},9);actor.Start(ReleaseOnce(actor));actor.Update(1.0F/60,{},*bullets);
        assert(reused->nativeColor==10&&reused->accelerationSeconds==2);
    }
}
void TestUnwrappedAngleAndIntegration() {
    for(int spawnLeft:{0,13})for(bool acrossBranch:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();
        std::vector<SourceBullet> source{{132,224+(acrossBranch?-0.125F:12),16},
                                       {132,224+(acrossBranch?0.125F:0),64,3}};
        source[0].spawnLeft=spawnLeft;
        auto* small=Make(*bullets,source[0]);auto* big=Make(*bullets,source[1],9);
        small->spawnVisualSeconds=16.0F/60;small->spawnMoveScale=0.4F;small->age=(16-spawnLeft)/60.0F;
        small->maxAge=30;big->maxAge=30;
        const float oldAge=small->age;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({},2400,7);
        actor.Start(ReleaseOnce(actor));actor.Update(1.0F/60,{},*bullets);assert(Release(source,192,224)==releasedCount);
        assert(small->age==oldAge&&small->spawnVisualSeconds==16.0F/60&&small->nativeColor==10);
        if(acrossBranch) {
            const float beta=std::atan2(source[1].y-224,source[1].x-192),theta=std::atan2(source[0].y-224,source[0].x-192);
            const float wrapped=beta+2.2F*std::remainder(theta-beta,2*3.14159274F);
            assert(std::abs(small->acceleration.x-std::cos(wrapped)*0.01F*accelerationScale)>0.5F);
        }
        for(int frame=0;frame<160;++frame) {
            Tick(source[0]);bullets->Update(1.0F/60);
            Near(small->position.x,(source[0].x-192)*scale,5e-4F);Near(small->position.z,10-source[0].y*scale,5e-4F);
            Near(small->velocity.x,source[0].vx*speedScale);Near(small->velocity.z,-source[0].vy*speedScale);
            assert(small->active&&small->nativeColor==10);
        }
        assert(small->accelerationSeconds==0&&source[0].accelerationLeft==0);
        actor.Start(ReleaseOnce(actor));actor.Update(1.0F/60,{},*bullets);assert(small->nativeColor==10&&small->accelerationSeconds==0);
    }
}
void TestSpatialAdaptation() {
    for(bool spatial:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();auto* small=Make(*bullets,{192,224,16});small->position={2,2,0};small->velocity={0.2F,0.3F,0.4F};
        auto* outside=Make(*bullets,{192,224,16});outside->position={2,2.1F,0};
        Make(*bullets,{192,224,64,3},9);
        behavior::Actor actor;actor.Reset({},2400,7);actor.skills.spatial=spatial;actor.Start(ReleaseOnce(actor));actor.Update(1.0F/120,{},*bullets);
        assert(releasedCount==1&&small->nativeColor==10&&outside->nativeColor==(spatial?9:10));
        Near(BulletSystem::Length(small->acceleration),0.01F*accelerationScale,1e-5F);
        Near(small->acceleration.y,spatial?0.01F*accelerationScale/std::sqrt(2.0F):0,1e-5F);
        assert(small->velocity.x==0.2F&&small->velocity.y==0.3F&&small->velocity.z==0.4F);
    }
}
void TestSpellSource() {
    using F=bosses::FlandreBoss;using Reference=stage_extra::reference::FlandreReference;
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},2400,seed);actor.visual=stage_assets::kBoss;actor.SetRank(rank);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::KagomeKagome(actor));
        Reference reference(56,{192,96,0},seed,rank,2400);reference.death=61;
        std::vector<behavior::Spawn> children;std::vector<Reference::Spawn> originals;int effects=0;
        const auto compare=[&](behavior::Actor& actual,Reference& expected,int frame,int route) {
            bullets->Reset();const Vector3 player{220+70*std::sin(frame*0.009F),360+20*std::cos(frame*0.013F),0};
            expected.Tick(player);actual.Update(1.0F/60,{(player.x-192)*scale,0,10-player.y*scale},*bullets);
            koumakyou::stage_extra::reference::CompareEffects(actual,expected);
            Near(actual.position.x,(expected.position.x-192)*scale,5e-4F);Near(actual.position.z,10-expected.position.y*scale,5e-4F);
            if(actual.animationScript!=expected.animation||actual.soundEvents!=expected.sounds||actual.active!=expected.active)
                std::cerr<<"Kagome route "<<route<<" frame "<<frame<<" animation "<<actual.animationScript<<'/'<<expected.animation
                         <<" active "<<actual.active<<'/'<<expected.active<<" sounds "<<actual.soundEvents.size()<<'/'<<expected.sounds.size()<<'\n';
            assert(actual.animationScript==expected.animation&&actual.soundEvents==expected.sounds&&actual.active==expected.active);
            assert(actual.damageable==expected.damageable&&actual.contactEnabled==expected.contact&&actual.interactable==expected.interactable&&actual.visible==expected.visible);
            std::vector<const Bullet*> emitted;bullets->Pool().ForEachActive([&](const Bullet& b){emitted.push_back(&b);});
            if(emitted.size()!=expected.emissions.size())std::cerr<<"Kagome route "<<route<<" frame "<<frame<<" shots "<<emitted.size()<<'/'<<expected.emissions.size()<<'\n';
            assert(emitted.size()==expected.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<emitted.size();++n) {
                const auto& b=*emitted[n];const auto& e=expected.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.spawnVisualSeconds==16.0F/60&&e.flags==516);
                Near(b.position.x,(e.position.x-192)*scale,5e-4F);Near(b.position.z,10-e.position.y*scale,5e-4F);
                Near(b.velocity.x,e.velocity.x*speedScale);Near(b.velocity.z,-e.velocity.y*speedScale);
            }
        };
        for(int frame=0;frame<3900;++frame) {
            compare(actor,reference,frame,-1);effects+=reference.effectRequests;
            assert(reference.card==125&&reference.timeout==3900&&reference.timeoutCallback==61&&reference.nativeCallback==(frame>=120?15:-1));
            assert(actor.spawnEvents.size()==reference.spawns.size());
            for(std::size_t n=0;n<reference.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=reference.spawns[n];
                assert(e.sub==59&&a.sourceHealth==e.health&&a.sourceScore==e.score&&a.sourceItem==e.item&&behavior::Valid(a));
                assert(a.seed==seed+children.size()+1&&a.visual==stage_assets::kBoss&&!a.damageable&&a.retireOutside&&!a.visible);
                assert(a.stageClearResponse==behavior::StageClearResponse::Remove);
                Near(a.position.x,(e.position.x-192)*scale);Near(a.position.z,10-e.position.y*scale);
                children.push_back(a);originals.push_back(e);
            }
        }
        assert(children.size()==90&&effects==192&&actor.Random(1)==reference.Random(1));
        for(std::size_t n=0;n<16;++n) {
            behavior::Actor emitter(behavior::UpdateRate::Hz60);emitter.Activate(children[n]);emitter.SetRank(rank);
            Reference child(59,originals[n].position,children[n].seed,rank,1200);
            int frame=0;
            for(;frame<500&&child.active;++frame) {
                compare(emitter,child,frame,static_cast<int>(n));
                assert(child.specialActor&&child.deathMode==1&&child.death==60&&!emitter.visible);
                Near(emitter.hitRadius,child.radius*scale,1e-5F);
            }
            assert(frame<500&&!emitter.active&&emitter.Random(1)==child.Random(1));
        }
    }
}
void TestClocksHandoffsAndDisplayRates() {
    using F=bosses::FlandreBoss;
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        const int factor=rate==behavior::UpdateRate::Hz120?2:1;auto bullets=std::make_unique<BulletSystem>();
        behavior::Actor actor(rate);actor.Reset({0,0,5.71428585F},2400,7);F::Behaviors::ConfigurePractice(actor,false);
        actor.Start(F::Behaviors::KagomeKagome(actor));
        for(int tick=0;tick<=216*factor;++tick) {
            bullets->Reset();actor.Update(1.0F/(60*factor),{},*bullets);
            assert(actor.CanTakeDamage()==(tick>=120*factor));assert(actor.spawnEvents.size()==(tick==216*factor?1U:0U));
            if(factor==2&&tick==240) {
                // 在120.5帧前注入邻近小弹；下半步必须执行解放，不能等到下一整数帧。
                Make(*bullets,{170,230,16});Make(*bullets,{150,224,64,3},9);
                actor.Update(1.0F/120,{},*bullets);bool released=false;
                bullets->Pool().ForEachActive([&](const Bullet& b){if(b.nativeType==1) {assert(b.nativeColor==10);released=true;}});
                assert(released);++tick;
            }
        }
    }
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        F boss;const auto skill=boss.DescribeSkill(F::Behavior::KagomeKagome,static_cast<ShotType>(shot));
        assert(skill.health==2400&&skill.reserveHealth==0&&skill.timeoutFrames==3900&&skill.spellCard==125&&skill.reactToBomb);
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::KagomeKagome,F::Behavior::FifthNonspell},{{{0,0.5F,PINK},{1,0.5F,WHITE}}}}}};
        boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false,bat=false,recovered=false;
        for(int tick=0;tick<7900;++tick) {
            bullets->Reset();boss.SetPlayerBombActive(tick<30||tick>=250&&tick<280);
            const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(tick<240)assert(boss.animationScript!=165&&!boss.CanTakeDamage());
            if(tick==260) {assert(boss.animationScript==165&&!boss.CanBeTargeted());bat=true;}
            if(tick==400) {assert(boss.CanTakeDamage()&&boss.animationScript!=165);recovered=true;}
            if(damage&&tick>2400&&boss.CanTakeDamage())boss.health=0;
            if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.AdvanceSkill());advanced=true;break;}
        }
        assert(advanced&&bat&&recovered&&boss.attack==2&&boss.health==10000&&!boss.CanTakeDamage());
        boss.Update(1.0F/120,{},*bullets);assert(boss.DefeatActorsRequested()&&!boss.CanTakeDamage());
        // 重启不得带入上一符的化蝠恢复时钟。
        boss.SetPlayerBombActive(true);boss.StartBoss(config);boss.Update(1.0F/120,{},*bullets);assert(boss.animationScript!=165);
    }
    for(bool spatial:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor parent;
        parent.Reset({0,0,5.71428585F},2400,7);parent.visual=stage_assets::kBoss;F::Behaviors::ConfigurePractice(parent,spatial);
        parent.Start(F::Behaviors::KagomeKagome(parent));std::vector<behavior::Spawn> children;
        for(int tick=0;tick<1700;++tick) {parent.Update(1.0F/120,{},*bullets);children.insert(children.end(),parent.spawnEvents.begin(),parent.spawnEvents.end());}
        assert(children.size()==16);
        for(int index=-1;index<16;++index) {
            Vector3 expectedPosition{};double expectedTime=0;float expectedRandom=0;std::vector<Bullet> expected;
            for(int fps:{120,30,60,144,240}) {
                bullets->Reset();behavior::Actor actor;
                if(index<0) {
                    actor.Reset({0,0,5.71428585F},2400,7);F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::KagomeKagome(actor));
                } else actor.Activate(children[index]);
                for(int frame=0;frame<20*fps;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
                std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const auto random=actor.Random(1);
                assert(!bullets->Pool().DroppedSpawns());
                if(fps==120) {expectedPosition=actor.position;expectedTime=actor.phaseFrames;expectedRandom=random;expected=actual;}
                else {
                    Near(actor.position.x,expectedPosition.x,5e-4F);Near(actor.position.y,expectedPosition.y,5e-4F);Near(actor.position.z,expectedPosition.z,5e-4F);
                    assert(actor.phaseFrames==expectedTime&&random==expectedRandom&&actual.size()==expected.size());
                    for(std::size_t n=0;n<actual.size();++n) {
                        Near(actual[n].position.x,expected[n].position.x,5e-4F);Near(actual[n].position.y,expected[n].position.y,5e-4F);Near(actual[n].position.z,expected[n].position.z,5e-4F);
                        Near(actual[n].velocity.x,expected[n].velocity.x);Near(actual[n].velocity.y,expected[n].velocity.y);Near(actual[n].velocity.z,expected[n].velocity.z);
                    }
                }
                if(index>=0)assert(!actor.active&&actor.finished);
            }
        }
    }
}
}
/// @brief 独立执行第四符源时钟、大小弹解放及120 Hz编排；正式关卡/完整原生弹池仍需另行验收。
void RunStageExtraKagomeTests() {
    TestSlotOrderAndFilters();TestUnwrappedAngleAndIntegration();TestSpatialAdaptation();TestSpellSource();TestClocksHandoffsAndDisplayRates();
    std::cout<<"Validated Extra Kagome source diffs, 16 emitter routes, projectile release/integration, clocks, handoffs and Bomb recovery\n";
}

// 本文件独立执行112…119源指令，验证最终额外符的生命快照、两层使魔、扫描弹与模式3退场。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Ref=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448,speedScale=60*scale,accelerationScale=3600*scale;
int frame=0,route=-1;
void Near(float actual,float expected,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Danmaku Heart route "<<route<<" frame "<<frame<<' '<<field<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
void Position(Vector3 actual,Vector3 source) {const auto p=World(source);Near(actual.x,p.x,"x");Near(actual.y,p.y,"y");Near(actual.z,p.z,"z");}
void Configure(behavior::Actor& actor,bool spatial=false) {
    F::Behaviors::ConfigurePractice(actor,spatial);
    (actor.liftedMotion?actor.liftedMotion->planar:actor.motion).bounds={{-7.14285714F,0},{7.14285714F,7.85714293F}};
}
void Snapshot(const behavior::Spawn& actual,const Ref::Spawn& expected) {
    assert(behavior::Valid(actual)&&actual.pattern.spiralTrail&&actual.sourceHealth==1200&&expected.health==1200);
    assert(actual.sourceScore==expected.score&&actual.sourceItem==expected.item&&!actual.visible&&!actual.damageable);
    assert(actual.stageClearResponse==behavior::StageClearResponse::Remove);Position(actual.position,expected.position);
    const auto& p=*actual.pattern.spiralTrail;
    Near(p.acceleration,expected.variables[4]*accelerationScale,"snapshot.acceleration",1e-5F);
    assert(p.accelerationFrames==expected.variables[3]&&p.emitterInterval==expected.variables[1]);
    if(expected.sub==118) {
        Near(actual.pattern.movement.angle,expected.variables[6],"travel.angle",1e-5F);
        Near(p.axisAngle,expected.variables[5],"axis.angle",1e-5F);Near(p.angleStep,expected.storedAngle,"angle.step",1e-5F);
    } else Near(p.aimOffset,expected.variables[7],"aim.offset",1e-5F);
}
struct Sample {Bullet actual;Ref::Emission source;};
std::vector<Sample> samples;
std::vector<behavior::Spawn> firstArms,firstEmitters;
std::vector<Ref::Spawn> firstRawEmitters;
void CompareEmitter(const behavior::Spawn& spawn,const Ref::Spawn& original,int rank,bool collect) {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);
    Ref source(118,original.position,spawn.seed,rank,1200);source.Inherit(original.variables,original.storedAngle);int shots=0;
    for(frame=0;frame<600;++frame) {
        bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);Position(actor.position,source.position);
        if(actor.active!=source.active)std::cerr<<std::setprecision(10)<<"Heart retirement route "<<route<<" frame "<<frame<<" active "<<actor.active<<" vs "<<source.active<<" at "<<actor.position.x<<','<<actor.position.z<<" source "<<source.position.x<<','<<source.position.y<<" angle "<<spawn.pattern.movement.angle<<" source "<<original.variables[6]<<" origin "<<spawn.position.x<<" analytic "<<spawn.position.x+frame*static_cast<double>(std::cos(spawn.pattern.movement.angle))*speedScale/60<<" source analytic "<<(original.position.x+frame*static_cast<double>(std::cos(original.variables[6]))-192)*scale<<'\n';
        assert(actor.active==source.active&&!actor.visible&&!actor.interactable&&!actor.damageable&&!actor.contactEnabled);
        assert(actor.animationScript==167&&source.animation==167&&actor.soundEvents==source.sounds&&source.specialActor);
        Near(actor.hitRadius,source.radius*scale,"emitter.radius",1e-5F);int n=0;
        bullets->Pool().ForEachActive([&](const Bullet& b) {
            assert(n<static_cast<int>(source.emissions.size()));const auto& e=source.emissions[n++];Position(b.position,e.position);
            assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart&&b.spawnVisualSeconds==10.0F/60&&b.launchBoostSeconds==0);
            Near(b.Velocity().x,e.velocity.x*speedScale,"vx",1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,"vz",1e-4F);
            if(e.type==5) {
                assert((e.color==2||e.color==3)&&e.flags==530&&BulletSystem::Length(b.Velocity())==0);
                Near(b.acceleration.x,e.accelerationVector.x*accelerationScale,"ax",1e-4F);Near(b.acceleration.z,-e.accelerationVector.y*accelerationScale,"az",1e-4F);
                Near(b.accelerationSeconds,e.accelerationFrames/60,"acceleration.seconds",1e-5F);
            } else assert(e.type==6&&e.color==1&&e.flags==514);
            if(collect&&(frame==0||frame==1||frame==61||frame==121))samples.push_back({b,e});
        });
        assert(n==static_cast<int>(source.emissions.size()));shots+=n;if(!source.active)break;
    }
    assert(frame<600&&shots>1&&actor.Random(1)==source.Random(1));
}
void CompareArm(const behavior::Spawn& spawn,const Ref::Spawn& original,int rank,bool collect) {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);
    Ref source(original.sub,original.position,spawn.seed,rank,1200);source.Inherit(original.variables,original.storedAngle);
    std::vector<behavior::Spawn> children;std::vector<Ref::Spawn> originals;
    for(frame=0;frame<400;++frame) {
        source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);Position(actor.position,source.position);
        assert(actor.active&&source.active&&!actor.visible&&!actor.damageable&&!actor.contactEnabled&&!actor.interactable);
        assert(actor.spawnEvents.size()==source.spawns.size()&&actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
        assert(source.emissions.empty()&&!bullets->Pool().ActiveCount());
        for(std::size_t n=0;n<source.spawns.size();++n) {
            const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];assert(e.sub==118);Snapshot(a,e);
            assert(a.seed==spawn.seed+children.size()+1&&frame==static_cast<int>(children.size())*static_cast<int>(original.variables[1]));
            children.push_back(a);originals.push_back(e);
        }
    }
    assert(children.size()==6&&actor.Random(1)==source.Random(1));
    if(collect) {firstEmitters.insert(firstEmitters.end(),children.begin(),children.end());firstRawEmitters.insert(firstRawEmitters.end(),originals.begin(),originals.end());}
    for(std::size_t n=0;n<children.size();++n)CompareEmitter(children[n],originals[n],rank,collect);
}
int Health(int profile,int tick) {return profile==0?6000:tick<1500?6000:tick<4000?3001:25;}
void TestSource() {
    for(int profile:{0,1})for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,170,0}),6000,seed);actor.visual=stage_assets::kBoss;Configure(actor);actor.SetRank(rank);
        actor.Start(F::Behaviors::DanmakuHeart(actor));Ref source(113,{192,170,0},seed,rank,6000);source.death=119;source.timeout=9000;source.reserve=-1;
        std::vector<behavior::Spawn> arms;std::vector<Ref::Spawn> originals;int effects=0;route=-1;
        for(frame=0;frame<9000;++frame) {
            actor.health=source.health=Health(profile,frame);source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);
            Position(actor.position,source.position);CompareEffects(actor,source);effects+=source.effectRequests;
            if(actor.spawnEvents.size()!=source.spawns.size())std::cerr<<"Heart birth frame "<<frame<<": "<<actor.spawnEvents.size()<<" vs "<<source.spawns.size()<<'\n';
            assert(actor.spawnEvents.size()==source.spawns.size()&&actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
            assert(actor.damageable&&source.damageable&&actor.interactable&&source.interactable&&actor.contactEnabled==source.contact&&actor.defeatActors==source.defeatActors);
            assert(source.card==134&&source.timeoutCallback==119&&source.nativeCallback==(frame<120?-1:19)&&source.emissions.empty()&&!bullets->Pool().ActiveCount());
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];assert(e.sub==116||e.sub==117);Snapshot(a,e);
                assert(a.seed==seed+arms.size()+1);arms.push_back(a);originals.push_back(e);
            }
        }
        assert(actor.Random(1)==source.Random(1));
        if(seed==7&&rank==16)std::cout<<"Heart profile "<<profile<<": "<<arms.size()<<" arms, "<<effects<<" effects\n";
        assert(arms.size()==(profile==0?30:40)&&effects==(profile==0?2760:3660));
        if(profile==0&&seed==7&&rank==16)firstArms.assign(arms.begin(),arms.begin()+2);
        // 全部控制器与发射体独立运行至停火/退场；不同HP与7200帧后的快照均不能被父体后续写入污染。
        for(std::size_t n=0;n<arms.size();++n) {route=static_cast<int>(n);CompareArm(arms[n],originals[n],rank,profile==0&&seed==7&&rank==16&&n<2);}
    }
}
void TestPreludeAndExit() {
    for(unsigned seed:{7U,0x9e3779b9U}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,192,0}),6000,seed);Configure(actor);actor.Start(F::Behaviors::DanmakuHeartPrelude(actor));
        Ref source(112,{192,192,0},seed,16,4000);
        for(frame=0;frame<=210;++frame) {
            source.Tick({240,350,0});actor.Update(1.0F/60,{},*bullets);Position(actor.position,source.position);
            assert(actor.health==source.health&&actor.animationScript==source.animation&&actor.dropEvents==source.drops&&actor.itemEvents.empty()&&source.items.empty());
            assert(!actor.damageable&&!source.damageable&&actor.contactEnabled&&source.contact&&source.deathMode==3);
            assert((actor.pendingCallback!=nullptr)==(frame==210)&&source.handoff==(frame==210?113:-1));
        }
        assert(actor.Random(1)==source.Random(1));
        for(bool damage:{false,true}) {
            actor.Reset(World({192,170,0}),1,seed);Configure(actor);actor.SetPoses({160,166,166,161,162});actor.Start(F::Behaviors::EncoreFinalExit(actor,damage));
            // 原生伤害检查消费模式3并写HP1；超时不消费该位，且清敌跳过带Boss标记的本体。
            Ref exit(119,{192,170,0},seed,16,1);exit.deathMode=damage?0:3;int effects=0;
            const int last=damage?60:61;
            for(frame=0;frame<=last;++frame) {
                exit.Tick({240,350,0});actor.Update(1.0F/60,{},*bullets);Position(actor.position,exit.position);CompareEffects(actor,exit);effects+=exit.effectRequests;
                assert(actor.soundEvents==exit.sounds&&actor.health==exit.health&&actor.animationScript==exit.animation&&actor.active==exit.active);
                assert(!actor.damageable&&!exit.damageable&&!actor.contactEnabled&&!exit.contact&&actor.interactable&&exit.interactable);
                assert(actor.finished==(frame==last)&&actor.defeatActors==exit.defeatActors);
            }
            assert(effects==60&&actor.health==(damage?0:1)&&!actor.active&&actor.Random(1)==exit.Random(1));
        }
    }
    for(int factor:{1,2})for(bool damage:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120);
        actor.Reset(World({192,170,0}),1,7);Configure(actor);actor.Start(F::Behaviors::EncoreFinalExit(actor,damage));
        const int last=60*factor+(damage?0:1);
        for(int tick=0;tick<=last;++tick) {actor.Update(1.0F/(60*factor),{},*bullets);assert(actor.finished==(tick==last));}
    }
}
void TestProjectilesAndOwnership() {
    assert(samples.size()>20&&firstArms.size()==2&&firstEmitters.size()==12);auto bullets=std::make_unique<BulletSystem>();route=-2;
    for(const auto& sample:samples) {
        bullets->Reset();auto* b=bullets->Spawn(BulletOwner::Enemy,{},{},.1F,WHITE,0);*b=sample.actual;b->position={};
        float x=0,y=0,vx=sample.source.velocity.x,vy=sample.source.velocity.y;
        for(frame=0;frame<500&&b->active;++frame) {
            if(frame<10) {x+=sample.source.velocity.x*.5F;y+=sample.source.velocity.y*.5F;}
            if(frame>=9) {
                if(sample.source.flags&16&&frame-9<sample.source.accelerationFrames) {vx+=sample.source.accelerationVector.x;vy+=sample.source.accelerationVector.y;}
                x+=vx;y+=vy;
            }
            bullets->Update(1.0F/60);Near(b->position.x,x*scale,"bullet.x");Near(b->position.z,-y*scale,"bullet.z");
            Near(b->Velocity().x,vx*speedScale,"bullet.vx",1e-4F);Near(b->Velocity().z,-vy*speedScale,"bullet.vz",1e-4F);
        }
    }
    auto spawn=firstArms[0];behavior::Actor actor;actor.Activate(spawn);spawn.pattern.spiralTrail->emitterInterval=1;
    for(int tick=0;tick<700;++tick)actor.Update(1.0F/120,{},*bullets);
    assert(actor.active&&!actor.finished&&actor.spiralTrail->emitterInterval==47);
    behavior::ActorPool<1> pool;auto* a=pool.Spawn(firstArms[0]);assert(a&&!pool.Spawn(firstArms[1])&&pool.DroppedSpawns()==1);
    a->Update(1.0F/120,{},*bullets);a->ClearFromStage();assert(!a->active&&a->finished);pool.Clear();
    auto plain=firstArms[0];plain.pattern.spiralTrail.reset();plain.pattern.routine=nullptr;auto* reused=pool.Spawn(plain);assert(reused&&!reused->spiralTrail);
    for(int field=0;field<6;++field) {
        auto p=firstArms[0].pattern;auto& s=*p.spiralTrail;
        if(field==0)s.emitterInterval=0;if(field==1)s.accelerationFrames=0;if(field==2)s.acceleration=-1;
        if(field==3)s.axisAngle=std::numeric_limits<float>::quiet_NaN();if(field==4)s.angleStep=std::numeric_limits<float>::infinity();if(field==5)p.routine=nullptr;
        assert(!behavior::Valid(p));
    }
}
void TestRatesAndSpatial() {
    auto bullets=std::make_unique<BulletSystem>();
    for(bool space:{false,true}) {
        std::vector<behavior::Spawn> expected;float random=0;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;actor.Reset(World({192,170,0}),6000,7);Configure(actor,space);actor.Start(F::Behaviors::DanmakuHeart(actor));
            std::vector<behavior::Spawn> actual;
            for(int n=0;n<150*fps;++n) {actor.Update(1.0F/fps,World({240,350,0}),*bullets);actual.insert(actual.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());}
            const float next=actor.Random(1);assert(actor.phaseFrames==9000);
            if(fps==120) {expected=actual;random=next;}
            else {assert(actual.size()==expected.size()&&random==next);for(std::size_t n=0;n<actual.size();++n) {Near(actual[n].position.x,expected[n].position.x,"display.x");Near(actual[n].position.z,expected[n].position.z,"display.z");assert(actual[n].pattern.spiralTrail->emitterInterval==expected[n].pattern.spiralTrail->emitterInterval);}}
        }
        for(int side:{0,6}) {
            std::vector<Bullet> expected;Vector3 point{};float random=0;bool above=false,below=false;
            for(int fps:{120,30,60,144,240}) {
                bullets->Reset();auto spawn=firstEmitters[side];spawn.pattern.spatialShots=space;behavior::Actor child;child.Activate(spawn);
                for(int n=0;n<2*fps;++n)child.Update(1.0F/fps,World({240,350,0}),*bullets);
                std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const float next=child.Random(1);
                assert(actual.size()==21&&child.active);
                if(fps==120) {expected=actual;point=child.position;random=next;}
                else {assert(random==next);Near(child.position.x,point.x,"child.display.x");Near(child.position.z,point.z,"child.display.z");for(std::size_t n=0;n<actual.size();++n) {Near(actual[n].position.x,expected[n].position.x,"child.bullet.x");Near(actual[n].position.y,expected[n].position.y,"child.bullet.y");Near(actual[n].position.z,expected[n].position.z,"child.bullet.z");}}
                for(const auto& b:actual)if(b.nativeType==5&&space) {Near(BulletSystem::Length(b.acceleration),spawn.pattern.spiralTrail->acceleration,"spatial.acceleration",1e-5F);above|=b.acceleration.y>0;below|=b.acceleration.y<0;}
            }
            if(space)assert(above&&below);
        }
    }
}
void TestFloatIntegrationContract() {
    const Vector3 origin{-8.5714283F,0,10},unit{scale,scale,-scale};
    movement::State motion;motion.position=World({192,202,0});motion.EnableFloatIntegration(origin,unit);motion.Polar(.64F,speedScale);
    float x=192,y=202;
    for(frame=0;frame<300;++frame) {
        if(frame>0) {x+=std::cos(.64F);y+=std::sin(.64F);}
        motion.BeginFrame(60);Position(motion.position,{x,y,0});
        Near(motion.AngleTo(World({240,350,0})),std::atan2(350-y,240-x),"precision.aim",1e-5F);motion.EndFrame(60);
    }
    motion.position={.125F,1,5};motion.velocity={};motion.BeginFrame(60);
    Near(motion.position.x,.125F,"precision.rebase.x",1e-5F);Near(motion.position.y,1,"precision.rebase.y",1e-5F);Near(motion.position.z,5,"precision.rebase.z",1e-5F);
    motion.Interpolate(1,{2,3,4},1);motion.EndFrame(60);assert(motion.position.x==2&&motion.position.y==3&&motion.position.z==4);
    movement::State mirror;mirror.position=World({192,202,0});mirror.EnableFloatIntegration(origin,unit);mirror.Polar(0,speedScale);mirror.mirrored=true;
    mirror.EndFrame(60);mirror.BeginFrame(60);Position(mirror.position,{191,202,0});
    for(int kind=0;kind<3;++kind) {
        bool rejected=false;try {motion.EnableFloatIntegration(kind==0?Vector3{std::numeric_limits<float>::infinity(),0,0}:origin,
            kind==1?Vector3{0,1,1}:kind==2?Vector3{1,1,2}:unit);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
}
void TestRole() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int shot=0;shot<4;++shot)for(int factor:{1,2})for(bool damage:{false,true}) {
        F boss(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120,false);
        bosses::StartConfig config{.position=World({192,120,0}),.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::EncoreFinalExit,.retreat=F::Behavior::EncoreFinalExit,
            .phases={{{F::Behavior::CreepingBloodstain,F::Behavior::DanmakuHeartPrelude,F::Behavior::DanmakuHeart},{{{0,.4F,PINK},{1,.2F,WHITE},{2,.4F,PINK}}}}}};
        boss.StartBoss(config);boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets);boss.health=0;boss.AdvanceSkill();assert(boss.attack==2&&boss.health==6000);
        int tick=0;for(;tick<212*factor;++tick)if(boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets))break;
        assert(tick==210*factor&&boss.AdvanceSkill()&&boss.attack==3&&boss.health==6000);
        const auto* p=boss.CurrentSkill();assert(p&&p->spellCard==134&&p->timeoutFrames==9000&&p->bombReactionFrame==120&&(*p->bombRecoveryPoses)[0]==176);
        boss.SetPlayerBombActive(true);
        for(tick=0;tick<=120*factor;++tick) {
            boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets);
            assert((boss.animationScript==165)==(tick>=120*factor));
        }
        boss.SetPlayerBombActive(false);for(int n=0;n<60*factor;++n)boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets);
        assert(boss.CanTakeDamage()&&boss.animationScript==176);
        if(damage) {boss.health=0;assert(boss.AdvanceSkill());}
        else {while(boss.skillTimer*60<9000-.01F)assert(!boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets));assert(boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets)&&boss.AdvanceSkill());}
        assert(!boss.CurrentSkill()&&boss.Defeated()==damage);int exit=0;
        for(;exit<65*factor;++exit)if(boss.Update(1.0F/(60*factor),World({240,350,0}),*bullets))break;
        assert(exit==60*factor+(damage?0:1)&&boss.health==(damage?0:1)&&!boss.AdvanceSkill()&&!boss.active);
    }
}
}
/// @brief 验收134的独立源行为与真实112/119衔接；正式Extra素材/对白/时间轴和rank链另行验收。
void RunStageExtraHeartTests() {
    TestSource();TestPreludeAndExit();TestProjectilesAndOwnership();TestRatesAndSpatial();TestFloatIntegrationContract();TestRole();
    std::cout<<"Validated Extra Danmaku Heart snapshots, nested emitters, RNG, projectiles, clocks and death-mode-3 exit\n";
}

/// @brief 隔离过渡/退场及角色协议，便于在嵌套路线首次分歧时继续核验独立原生语义；不替代完整134差分。
void RunStageExtraHeartProtocolTests() {TestPreludeAndExit();TestRole();}

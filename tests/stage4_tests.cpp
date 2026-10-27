// 本文件验证四面出生、普通演员、Boss 源时钟、四机体阶段与通用弹道；整关图形另有集成测试。
#include "stages/stage4/behaviors.hpp"
#include "stages/stage4/ids.hpp"
#include "stages/stage4/data.hpp"
#include "stages/stage4/boss.hpp"
#include "app/spell_practice.hpp"
#include <cassert>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>
#include <iostream>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>;
void Near(float a,float b) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>5e-4F) {std::cerr<<"Stage 4 mismatch "<<a<<" vs "<<b<<'\n';std::abort();}
}
unsigned Next(unsigned& state) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
behavior::Spawn Spawn(behavior::Pattern pattern) {
    pattern.spatialShots=false;
    return {.pattern=pattern,.sourceHealth=100,.sourceScore=100,.seed=49,.visual=stage4::kOrb,.retireOutside=false};
}
void TestBirthsAndSourceMovement() {
    std::vector<const native_combat::HealthRecord*> originals;
    for(const auto& r:native_combat::Stage(4)->health)
        if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=21&&r.targetSub!=26)originals.push_back(&r);
    assert(originals.size()==395);const auto events=stage4::Stage4Timeline();assert(events.size()==400);
    auto bullets=std::make_unique<BulletSystem>();std::size_t index=0;std::array<bool,19> covered{};
    constexpr float scale=20.0F/448;
    for(const auto& event:events) {
        const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
        const auto& source=*originals.at(index++);const int sub=source.targetSub;covered[sub]=true;
        assert(event.atFrame==source.frame&&request->count==1&&request->interval==0);
        auto spawn=request->actor;assert(behavior::Valid(spawn));
        assert(spawn.sourceHealth==*source.value&&spawn.sourceScore==source.score&&spawn.sourceItem==source.item);
        assert(spawn.mirrored==(source.opcode==2||source.opcode==6));
        spawn.retireOutside=false;spawn.pattern.spatialShots=false;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
        assert(spawn.randomX==(source.position.x==-999));
        float x=spawn.randomX ? static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F*384 : source.position.x;
        float y=source.position.y,vx=0,vy=0,angle=pi/2,speed=2,acceleration=0,angular=0;
        if(sub>=2&&sub<=9) {angle=sub==8 ? pi/2 : sub==3||sub==5||sub==7 ? -1.04719758F : 0;speed=sub==2||sub==3||sub==4 ? 4.19999981F : 4.5F;}
        if(sub==10||sub==13)angle=0;
        if(sub==14||sub==16||sub==17)speed=0;
        unsigned random=spawn.seed;
        for(int frame=0;frame<=130;++frame) {
            x+=vx*(spawn.mirrored ? -1 : 1);y+=vy;
            if(sub>=2&&sub<=9) {
                if(frame==30) {
                    if(sub==2||sub==4||sub==6)angular=-0.0654498488F;
                    if(sub==3||sub==5||sub==7)angular=0.0654498488F;
                }
                if(frame==70) {
                    if(sub==2||sub==4||sub==6)angular=0;
                    if(sub==3||sub==5)angular=-0.130899698F;
                }
                if(sub==7&&frame==110)angular=0;
            } else if(sub!=14&&sub!=16&&sub!=17) {
                if(frame==40)acceleration=-0.0666666701F;
                if(frame==70) {
                    acceleration=0;
                    if(sub==0||sub==10||sub==18) {angle=static_cast<float>(Next(random)>>8)/16777216.0F*(pi/2)+pi/4;speed=1.79999995F;}
                }
            }
            angle+=angular;speed+=acceleration;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            if(std::abs(actor.position.x-(x-192)*scale)>5e-4F||std::abs(actor.position.z-(10-y*scale))>5e-4F)
                std::cerr<<"Stage 4 source sub "<<sub<<" birth "<<source.frame<<" local frame "<<frame<<'\n';
            Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,spawn.position.y);
            const bool summons=(sub==0||sub==10||sub==18)&&frame==70;
            assert(actor.spawnEvents.size()==(summons ? 1U : 0U));
            if(summons)assert(actor.spawnEvents.front().pattern.routine==(sub==18 ? stage4::HiddenLaserEmitter : stage4::HiddenRandomEmitter));
        }
    }
    assert(index==395);int recipes=0;for(bool hit:covered)recipes+=hit;assert(recipes==16);
}
void TestPeriodicCrystals() {
    struct Case {behavior::Pattern pattern;int interval,end,count;bool right;};
    // 独立源 2/3/4/5/8/9 的周期/终点/层数；取模初相不得由浮点比例生成。
    const std::array<Case,6> cases{{{stage4::kHookFanShooter,50,70,6,false},
        {stage4::kSwerveFanShooter,50,150,6,true},{stage4::kHookLayerShooter,128,70,3,false},
        {stage4::kSwerveLayerShooter,128,150,3,true},{stage4::kDescendingFanShooter,50,100,6,false},
        {stage4::kCrossingFanShooter,50,100,6,false}}};
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto& c:cases)for(int rank:{10,16,31,32}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(Spawn(c.pattern));actor.skills.rank=rank;
        unsigned random=49;const int period=c.interval+c.interval/5-(c.interval/5*2*rank)/32;
        int next=period-static_cast<int>(Next(random)%period)-1;
        for(int frame=0;frame<=160;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            const bool fire=frame==next&&frame<c.end;
            assert(bullets->Pool().ActiveCount()==(fire ? static_cast<std::size_t>(c.count) : 0));if(fire)next+=period;
            assert(!actor.CanContactPlayer()&&actor.CanBeTargeted()&&actor.CanTakeDamage());
        }
    }
}
void TestSpawnerAndHiddenEmitter() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Activate(Spawn(stage4::kDescendingSpawner));behavior::Spawn child;
    for(int frame=0;frame<=70;++frame) {
        actor.Update(1.0F/60,{},*bullets);assert(actor.spawnEvents.size()==(frame==70 ? 1U : 0U));
        if(frame==70)child=actor.spawnEvents.front();
    }
    assert(behavior::Valid(child)&&child.sourceHealth==1000&&child.sourceScore==10&&child.sourceItem==0&&!child.damageable);
    Near(child.position.x,actor.position.x);Near(child.position.z,actor.position.z);
    actor.Activate(child);
    for(int frame=0;frame<=222;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{},*bullets);
        const bool fires=frame>=30&&frame<=96&&(frame-30)%6==0;
        assert(bullets->Pool().ActiveCount()==(fires ? 4U : 0U));
        assert(!actor.CanBeTargeted()&&!actor.CanTakeDamage()&&!actor.CanContactPlayer());
        assert(actor.visible==(frame<222));assert(actor.active==(frame<222));assert(actor.animationInterrupt.has_value()==(frame>=102));
    }
    actor.Activate(Spawn(stage4::kHiddenLaserEmitter));
    for(int frame=0;frame<=440;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==120 ? 1U : 0U));
        if(frame==120)bullets->Pool().ForEachActive([](const Bullet& beam) {
            Near(beam.beamLength,22.3214283F);Near(beam.radius,0.71428573F);Near(beam.warningSeconds,70.0F/60);
            Near(beam.beamDirection.z,-1);Near(beam.maxAge,226.0F/60);
        });
        assert(actor.visible==(frame<440)&&actor.active==(frame<440)&&!actor.CanBeTargeted());
        assert(actor.animationInterrupt.has_value()==(frame>=320));
    }
}
void TestSpiralAndRingEmitters() {
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto& pattern:{stage4::kStrongSpiralCrystal,stage4::kGentleSpiralCrystal}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(Spawn(pattern));
        for(int frame=0;frame<=582;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=frame>=70&&frame<=574&&(frame-70)%8==0;
            assert(bullets->Pool().ActiveCount()==(fires ? 6U : 0U));
        }
        Near(actor.motion.angle,-pi/2);Near(actor.motion.speed,5.35714293F);
        bullets->Spawn(BulletOwner::Enemy,{}, {},0.1F,WHITE,stage4::kBlueBullet);
        auto* player=bullets->Spawn(BulletOwner::Player,{}, {},0.1F,WHITE,stage4::kBlueBullet);
        assert(actor.Defeat()&&!actor.Defeat());actor.Update(1.0F/60,{},*bullets);
        assert(actor.finished&&actor.dropEvents.size()==1&&actor.dropEvents.front()==3&&player->active);
        assert(bullets->Pool().ActiveCount()==1);
    }
    struct Case {behavior::Pattern pattern;int count,rounds;};
    for(const auto& c:std::array<Case,3>{{{stage4::kGreenRingEmitter,22,60},{stage4::kBlueRingEmitter,16,10},{stage4::kOffsetRingEmitter,22,6}}}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(Spawn(c.pattern));const int hidden=30+c.rounds*50;
        for(int frame=0;frame<=hidden+30;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=frame>=30&&frame<hidden&&(frame-30)%50==0;
            assert(bullets->Pool().ActiveCount()==(fires ? static_cast<std::size_t>(c.count) : 0));
            assert(actor.visible==(frame<hidden+30));assert(actor.CanBeTargeted()==(frame>=30&&frame<hidden));
            assert(actor.animationInterrupt.has_value()==(frame>=hidden));
            assert(actor.active==(frame<hidden+30));
        }
    }
}
behavior::Routine BeamProbe(behavior::Actor& actor) {
    actor.FireBeam(0,skill::Laser{.length=5,.width=0.5F,.grow=0,.sustain=10,.startOffset=1,.direction=Vector3{1,1,0}});
    co_await actor.At(1);
    if(auto* beam=actor.Beam(0)) {beam->position.x=99;actor.soundEvents.push_back(1);}
    else actor.soundEvents.push_back(2);
    co_await actor.At(2);actor.ClearBeamSlots();assert(!actor.Beam(0));
    co_await actor.At(10);actor.finished=true;
}
behavior::Routine EmitterDelay(behavior::Actor& actor) {co_await actor.At(10);}
behavior::Routine EmitterProbe(behavior::Actor& actor) {
    actor.skills.rankInfluence={0,0,0,4,0,0};
    actor.ConfigureEmitter({skill::Formation::FixedRing,3,6,2,1,2});actor.SetEmitterPeriod(3);actor.SetEmitterSound(7);
    co_await EmitterDelay(actor);
    actor.ConfigureEmitter({skill::Formation::FixedRing,3,2,5,1,2});
    co_await actor.At(10);actor.SetEmitterPeriod(0);co_await actor.At(30);actor.finished=true;
}
void TestPersistentEmitter() {
    auto bullets=std::make_unique<BulletSystem>();
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        behavior::Actor actor(rate);actor.Reset({},100,7);actor.Start(EmitterProbe(actor));
        for(int frame=0;frame<25;++frame) {
            if(frame==4)actor.SetRank(32);
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=frame<20&&frame%3==2;
            assert(bullets->Pool().ActiveCount()==(fires ? (frame<10 ? 4U : 9U) : 0U));
            // 新配方明确没有 PlaySound，身份仍保留，但不能沿用旧配方的播放开关。
            assert(actor.soundEvents.size()==(fires&&frame<10 ? 1U : 0U));
        }
        actor.Reset({},100,7);actor.Update(1.0F/60,{},*bullets);assert(actor.soundEvents.empty());
    }
}
behavior::Routine SoundFlagProbe(behavior::Actor& actor) {
    const skill::Shot silent{skill::Formation::FixedRing,3,6,1,1,1};auto sounding=silent;sounding.flags=skill::PlaySound;
    actor.ConfigureEmitter(silent);actor.SetEmitterSound(7);actor.SetEmitterPeriod(2);actor.Fire(silent);
    co_await actor.At(2);actor.ConfigureEmitter(silent);actor.Fire(silent);
    co_await actor.At(4);actor.Fire(sounding);
    co_await actor.At(5);actor.SetEmitterSound(-1);actor.Fire(sounding);
    co_await actor.At(6);actor.SetEmitterSound(8);co_await actor.At(8);actor.finished=true;
}
void TestEmitterSoundFlags() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({},100,7);actor.Start(SoundFlagProbe(actor));
    // 新典 84 设置位 512，而下一次发射配置覆盖该位；负号关闭当前配方但保留音效身份。
    constexpr std::array<int,8> sounds{-1,7,-1,-1,7,7,-1,8};
    for(int frame=0;frame<8;++frame) {
        actor.Update(1.0F/60,{},*bullets);assert(actor.soundEvents.size()==(sounds[frame]<0 ? 0U : 1U));
        if(sounds[frame]>=0)assert(actor.soundEvents.front()==sounds[frame]);
    }
}
void TestBossPreludeWitnesses() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({},7000,7);stage4::ConfigureBehavior(actor,false);actor.Start(stage4::koakuma::Encounter(actor));
    for(int frame=0;frame<475;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const bool first=frame>=30&&frame<=170&&(frame-30)%20==0;
        const bool second=frame>=280&&frame<=420&&(frame-280)%20==0;
        const bool next=frame==471;
        assert(bullets->Pool().ActiveCount()==(first||second||next ? 8U : 0U));
        assert(actor.spawnEvents.size()==(frame==190||frame==220||frame==440||frame==470 ? 1U : 0U));
        assert(actor.CanBeTargeted()==(frame>=30));
    }
    for(bool second:{false,true}) {
        actor.Reset({},second ? 16000 : 17000,7);stage4::ConfigureBehavior(actor,false);bullets->Reset();
        actor.Start(second ? stage4::patchouli::SecondNonspell(actor) : stage4::patchouli::FirstNonspell(actor));
        for(int frame=0;frame<=800;++frame) {
            actor.Update(1.0F/60,{2,3,-6},*bullets);int shots=0,beams=0;
            bullets->Pool().ForEachActive([&](Bullet& bullet){if(bullet.beamLength>0)++beams;else {++shots;bullet.active=false;}});
            const int first=second ? 180 : 100;
            if(frame==first)assert(beams==4&&shots==0);
            if(frame==first+60)assert(beams==8&&shots==0);
            if(!second) {
                if(frame==318)assert(shots==0);
                if(frame==319)assert(shots==64);
                if(frame==520)assert(shots==40&&beams==4);
                if(frame==620)assert(shots==80&&beams==8);
                if(frame==670)assert(shots==40);
            } else {
                if(frame==373)assert(shots==0);
                if(frame==374)assert(shots==30);
                if(frame==599)assert(shots==12);
                if(frame==600)assert(shots==40&&beams==4);
                if(frame==659)assert(shots==10);
            }
            // 光束必须独立老化；只删除普通发射样本，不能每帧 Reset 掩盖槽位引用失效。
            bullets->Update(1.0F/60,{}, {2,3,-6});
        }
    }
}
void TestBeamHandlesAndAngularMotion() {
    auto bullets=std::make_unique<BulletSystem>();
    auto* original=bullets->SpawnBeam({}, {1,0,0},2,0.1F,0,10,WHITE);assert(original);
    const auto handle=bullets->MakeHandle(original);assert(bullets->Resolve(handle)==original);
    bullets->ClearEnemyBullets();assert(!bullets->Resolve(handle));
    auto* reused=bullets->SpawnBeam({}, {0,0,1},2,0.1F,0,10,WHITE);assert(reused==original&&!bullets->Resolve(handle));
    const auto second=bullets->MakeHandle(reused);bullets->Reset();
    assert(bullets->SpawnBeam({}, {0,0,1},2,0.1F,0,10,WHITE)==original&&!bullets->Resolve(second));
    auto other=std::make_unique<BulletSystem>();assert(!other->Resolve(handle));
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({},100,7);actor.Start(BeamProbe(actor));bullets->Reset();
    actor.Update(1.0F/60,{},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& beam){Near(beam.position.x,std::sqrt(0.5F));Near(beam.position.y,std::sqrt(0.5F));Near(beam.beamLength,5);});
    // 刻意在两次行为更新之间复用同一池槽；旧光束句柄不能移动新弹或将其视为有效光束。
    bullets->Reset();auto* replacement=bullets->Spawn(BulletOwner::Enemy,{3,2,1},{},0.1F,WHITE,stage4::kBlueBullet);
    actor.Update(1.0F/60,{},*bullets);assert(actor.soundEvents.size()==1&&actor.soundEvents.front()==2&&replacement->position.x==3);
    actor.Update(1.0F/60,{},*bullets);assert(replacement->active);
    // 金属性旋转弹在 128 内容帧后停止转向，半帧积分不会缩短或延长持续时间。
    for(int fps:{60,120}) {
        bullets->Reset();skill::State state;state.rankInfluence={};state.projectile.angularVelocity=1.47262156F;state.projectile.angularFrames=128;
        state.Fire({skill::Formation::FixedRing,7,0,1,1,2,0,0,0,32},{},{},*bullets);
        Bullet* shot=nullptr;bullets->Pool().ForEachActive([&](Bullet& bullet){shot=&bullet;});assert(shot);
        for(int frame=0;frame<fps*3;++frame)bullets->Update(1.0F/fps);
        Near(shot->angularSeconds,0);Near(shot->velocity.x,2*std::cos(1.47262156F*128/60));Near(shot->velocity.z,-2*std::sin(1.47262156F*128/60));
        const auto velocity=shot->velocity;bullets->Update(1.0F/fps);assert(shot->velocity.x==velocity.x&&shot->velocity.z==velocity.z);
    }
}
void TestExtendedProjectileMotion() {
    auto bullets=std::make_unique<BulletSystem>();
    // 独立的 60 Hz 源更新顺序：显现、启动加速、取消启动选项、极坐标运动；持续期从显现结束计数。
    for(bool acceleration:{false,true}) {
        bullets->Reset();skill::State state;state.rankInfluence={};
        state.projectile={.acceleration=0.192857146F,.angularVelocity=0.7363108F,.angularFrames=90,
            .speedAcceleration=0.48214286F,.accelerationFrames=60,.sampleAtStepStart=true};
        state.Fire({skill::Formation::FixedRing,3,14,1,1,1,0,0,0,acceleration ? 19 : 37},{},{},*bullets);
        Bullet* bullet=nullptr;bullets->Pool().ForEachActive([&](Bullet& b){bullet=&b;});assert(bullet);
        float angle=0,speed=1,vx=1,vz=0,x=0,z=0;
        const int spawn=acceleration ? 10 : 16;
        for(int frame=0;frame<130;++frame) {
            if(frame<spawn) {x+=1.0F/60*(acceleration ? 0.5F : 0.4F);}
            else {
                const int timer=frame-spawn;
                if(timer<=16) {vx=1+13.3928571F*(1-timer/16.0F);vz=0;}
                else if(timer>=18) {
                    if(acceleration) {if(timer<60)vx+=0.192857146F/60;}
                    else if(timer<90) {angle+=0.7363108F/60;speed+=0.48214286F/60;vx=speed*std::cos(angle);vz=-speed*std::sin(angle);}
                }
                x+=vx/60;z+=vz/60;
            }
            bullets->Update(1.0F/60);assert(bullet->active);Near(bullet->position.x,x);Near(bullet->position.z,z);
            if(frame>=spawn) {Near(bullet->velocity.x,vx);Near(bullet->velocity.z,vz);}
        }
    }
    for(bool absolute:{false,true})for(int fps:{60,120}) {
        bullets->Reset();skill::State state;state.rankInfluence={};state.spatial=true;
        state.projectile={.delayFrames=50,.turnAngle=pi/2,.turnSpeed=1,.turnCount=2,.sampleAtStepStart=true};
        state.Fire({skill::Formation::FixedRing,1,12,1,1,1,0,0,0,absolute ? 256 : 64},{},{},*bullets);
        Bullet* bullet=nullptr;bullets->Pool().ForEachActive([&](Bullet& b){bullet=&b;});assert(bullet);
        const auto initial=bullet->velocity;
        for(int step=0;step<=fps*2;++step) {
            bullets->Update(1.0F/fps);
            const int expected=step*60/fps>=100 ? 2 : step*60/fps>=50 ? 1 : 0;
            assert(bullet->turnCompleted==expected&&bullet->turnApplied==(expected==2));
            if(step*60==100*fps) {
                const auto direction=absolute ? bullet->turnFixedDirection : BulletSystem::RotateAroundAxis(initial,bullet->rotationAxis,pi);
                Near(bullet->velocity.x,direction.x);Near(bullet->velocity.y,direction.y);Near(bullet->velocity.z,direction.z);
            }
        }
    }
    bullets->Reset();skill::State state;state.rankInfluence={};state.spatial=true;
    state.Fire({.formation=skill::Formation::AimedFan,.count=1,.firstSpeed=1,.aimAxisOffset=pi},{},{0,0,-4},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& b){Near(b.velocity.z,1);Near(b.velocity.y,0);});
    for(int fps:{60,120}) {
        bullets->Reset();auto* beam=state.Fire(skill::Laser{.length=4.28571415F,.width=0.267857134F,.grow=0,.sustain=9999,
            .fade=30,.warning=0,.harmlessTail=30,.speed=10.7142859F,.direction=Vector3{0,0,-1},.initialLength=0},{},{},*bullets);
        assert(beam&&beam->IsBeam()&&beam->beamLength==0);
        for(int step=1;step<=fps;++step) {
            bullets->Update(1.0F/fps);const float end=10.7142859F*step/fps;
            Near(beam->beamLength,std::min(end,4.28571415F));Near(beam->position.z,-std::max(0.0F,end-4.28571415F));
            Near(beam->BeamEnd().z,-end);Near(beam->beamAnchor.z,0);assert(beam->active);
        }
    }
}
void TestSpellClockWitnesses() {
    using namespace stage4::patchouli;
    const std::array<behavior::Entry,13> entries{AgniShine,AgniShineAdvanced,PrincessUndine,SylphyHorn,SylphyHornAdvanced,
        LazyTrilithon,LazyTrilithonAdvanced,MetalFatigue,LavaCromlech,ForestBlaze,WaterElf,MercuryPoison,EmeraldMegalith};
    auto bullets=std::make_unique<BulletSystem>();
    for(int variant=0;variant<13;++variant)for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        behavior::Actor actor(rate);actor.Reset({0,0,4},1000,7);stage4::ConfigureBehavior(actor,false);actor.Start(entries[variant](actor));
        for(int frame=0;frame<700;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,0,-6},*bullets);int actual=0,beams=0;
            bullets->Pool().ForEachActive([&](const Bullet& bullet){if(bullet.IsBeam())++beams;else ++actual;});
            int expected=0,expectedBeams=0;const int time=frame-120;
            if(time>=0)switch(variant) {
                case 0:case 1: {const int cycle=time/170,t=time%170;
                    if(t<=32&&t%8==0)expected=(cycle/3+(variant==0 ? 11 : 8))*(variant==0 ? 3 : 4);break;}
                case 2: {const int cycle=time/250,t=time%250;
                    if(t<120) {if(t%10==0)expectedBeams=3;if((t+1)%22==0)expected=32;}
                    if(t>=120&&t<=190&&(t-120)%10==0)expected=11+cycle;break;}
                case 3:case 4:
                    expected=(time%10==0||time%10==6 ? 1 : 0)+(time%10==2||time%10==6 ? 1 : 0)+(time%30==0 ? (variant==3 ? 10 : 15) : 0);break;
                case 5:case 6:expected=time%(variant==5 ? 12 : 9)==0 ? 10 : 0;break;
                case 7:expected=time%40==0 ? 64 : 0;break;
                case 8: {const int cycle=time/154,t=time%154;
                    if(t<=56&&t%8==0)expected=24;
                    if(t>=124&&t<=144&&(t-124)%10==0)expected=12+cycle*2;break;}
                case 9:expected=(time%22==0||time%22==12 ? 1 : 0)+(time%22==2||time%22==12 ? 1 : 0)+(time%44==0 ? 22 : 0);break;
                case 10: {const int cycle=time/70,t=time%70;
                    if(t==0)expected=58;if(t==(cycle==0 ? 19 : 9))expected+=10;if(t==39||t==59)expected+=40;break;}
                case 11:if(time%370<320&&time%370%20==0)expected=(time/370+15)*2;break;
                case 12:expected=time%20==0 ? 1 : time%20==1 ? 12 : 0;break;
            }
            if(actual!=expected||beams!=expectedBeams)std::cerr<<"Stage 4 spell "<<variant<<" Hz "<<static_cast<int>(rate)<<" frame "<<frame<<" got "<<actual<<'/'<<beams<<" expected "<<expected<<'/'<<expectedBeams<<'\n';
            assert(actual==expected&&beams==expectedBeams);assert(actor.CanTakeDamage()==(frame>=120));
        }
    }
}
void TestBossBranchesAndLifecycle() {
    auto bullets=std::make_unique<BulletSystem>();
    constexpr std::array<std::array<int,4>,4> cards{{{39,58,67,78},{46,51,75,78},{50,40,71,82},{57,63,82,75}}};
    constexpr std::array<float,4> first{1600,1600,1600,1800},second{1500,1600,1700,1800};
    for(int shot=0;shot<4;++shot)for(bool timeout:{false,true}) {
        stage4::Stage4Boss boss(true,behavior::UpdateRate::Hz60,false);boss.SetPlayerShot(static_cast<ShotType>(shot));
        assert(!boss.CanTakeDamage()&&!boss.SpellName()&&boss.phase==0);
        for(int frame=0;frame<100;++frame)assert(!boss.Update(1.0F/60,{2,0,-6},*bullets));
        boss.ResumeCombat();assert(boss.phase==1&&boss.health==17000&&!boss.CanTakeDamage());
        int card=0;
        for(int phase=1;phase<=6;++phase) {
            assert(boss.phase==phase);assert(boss.HealthBar().count==2);
            const int prepare=phase==1||phase==3 ? 0 : phase>=5 ? 240 : 120;
            for(int frame=0;frame<=prepare;++frame) {bullets->Reset();assert(!boss.Update(1.0F/60,{2,0,-6},*bullets));}
            assert(boss.CanTakeDamage());
            if(phase!=1&&phase!=3) {
                const int number=cards[shot][card++];assert(boss.SpellName()&&boss.SpellCardNumber()==number);
                bool found=false;for(const auto& entry:spell::Cards())if(entry.number==number) {found=true;assert(entry.stage==3&&entry.difficulty==1);}
                assert(found);
            }
            if(timeout) {
                bool ended=false;
                for(int frame=0;frame<2600&&!ended;++frame) {bullets->Reset();ended=boss.Update(1.0F/60,{2,0,-6},*bullets);}
                assert(ended&&!boss.PhaseDefeated());
            } else {
                boss.health=phase==1 ? first[shot]-1 : phase==3 ? second[shot]-1 : phase==5 ? 1699 : 0;
                assert(boss.ShouldAdvanceFromDamage());assert(boss.PhaseDefeated()==(phase!=1&&phase!=3));
            }
            assert(boss.AdvancePhase());assert(!boss.CanTakeDamage());
        }
        assert(!boss.SpellName());bool ended=false;
        for(int frame=0;frame<70&&!ended;++frame)ended=boss.Update(1.0F/60,{2,0,-6},*bullets);
        assert(ended&&!boss.AdvancePhase()&&!boss.active&&boss.Defeated()==!timeout);
    }
}
void TestEntranceSourceBoundary() {
    auto bullets=std::make_unique<BulletSystem>();stage4::Stage4Boss boss(true,behavior::UpdateRate::Hz60,false);
    for(int frame=0;frame<100;++frame) {
        boss.Update(1.0F/60,{},*bullets);
        const float ratio=frame>=59 ? 1 : 1-std::pow(1-frame/60.0F,2.0F);
        Near(boss.position.x,(64+128*ratio-192)*20/448);Near(boss.position.z,10-(-32+160*ratio)*20/448);
        assert(!boss.CanTakeDamage()&&boss.phase==0);
    }
}
void TestSpatialDisplayFrameRates() {
    using namespace stage4::patchouli;
    const std::array<behavior::Entry,14> entries{stage4::koakuma::Encounter,AgniShine,AgniShineAdvanced,PrincessUndine,SylphyHorn,
        SylphyHornAdvanced,LazyTrilithon,LazyTrilithonAdvanced,MetalFatigue,LavaCromlech,ForestBlaze,WaterElf,MercuryPoison,EmeraldMegalith};
    struct Snapshot {Vector3 position,velocity;int type,color,turns;};
    for(auto entry:entries) {
        std::vector<Snapshot> expected;Vector3 position{};float random=0;int animation=-1;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,4},7000,7);
            stage4::ConfigureBehavior(actor,true);actor.Start(entry(actor));
            for(int frame=0;frame<fps*4;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Snapshot> actual;
            bullets->Pool().ForEachActive([&](const Bullet& bullet){actual.push_back({bullet.position,bullet.velocity,bullet.nativeType,bullet.nativeColor,bullet.turnCount});});
            const float nextRandom=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=nextRandom;animation=actor.animationScript;}
            else {
                assert(actual.size()==expected.size()&&nextRandom==random&&animation==actor.animationScript);
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                for(std::size_t i=0;i<actual.size();++i) {
                    const auto& a=actual[i];const auto& b=expected[i];assert(a.type==b.type&&a.color==b.color&&a.turns==b.turns);
                    Near(a.position.x,b.position.x);Near(a.position.y,b.position.y);Near(a.position.z,b.position.z);
                    Near(a.velocity.x,b.velocity.x);Near(a.velocity.y,b.velocity.y);Near(a.velocity.z,b.velocity.z);
                }
            }
            assert(bullets->Pool().DroppedSpawns()==0);
        }
    }
}
}
/// @brief 四面独立源参数/时钟、四机体生命周期与 120 Hz 显示帧率一致性；图形整关另由生命周期目标验证。
void RunStage4Tests() {TestBirthsAndSourceMovement();TestPeriodicCrystals();TestSpawnerAndHiddenEmitter();TestSpiralAndRingEmitters();TestBeamHandlesAndAngularMotion();TestPersistentEmitter();TestEmitterSoundFlags();TestBossPreludeWitnesses();TestExtendedProjectileMotion();TestSpellClockWitnesses();TestBossBranchesAndLifecycle();TestEntranceSourceBoundary();TestSpatialDisplayFrameRates();}

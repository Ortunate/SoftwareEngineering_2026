// 本文件检查六面出生、独立像素轨迹、MSG 放行、Boss 阶段、星阵/地雷和空间回收；图形另由生命周期目标验证。
#include "boss_fixture.hpp"
#include "stages/stage6/data.hpp"
#include "stages/stage6/behaviors.hpp"
#include "stages/stage6/boss_config.hpp"
#include "gameplay/bosses/sakuya.hpp"
#include "gameplay/bosses/remilia.hpp"
#include "stages/stage6/ids.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>;
void Near(float a,float b) {if(!std::isfinite(a)||std::abs(a-b)>5e-4F) {std::cerr<<"Stage 6 mismatch "<<a<<" vs "<<b<<'\n';std::abort();}}
unsigned Next(unsigned& state) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
void TestBirthsAndNeutralMovement() {
    std::vector<const native_combat::HealthRecord*> births;
    for(const auto& r:native_combat::Stage(6)->health)
        if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=8&&r.targetSub!=17)births.push_back(&r);
    assert(births.size()==124&&stage6::Stage6Timeline().size()==135);std::size_t index=0;
    auto bullets=std::make_unique<BulletSystem>();constexpr float scale=20.0F/448;
    for(const auto& event:stage6::Stage6Timeline()) {
        const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
        const auto& source=*births.at(index++);const int sub=source.targetSub;auto spawn=request->actor;
        assert(event.atFrame==source.frame&&request->count==1&&behavior::Valid(spawn));
        assert(spawn.sourceHealth==*source.value&&spawn.sourceScore==source.score&&spawn.sourceItem==source.item);
        assert(spawn.mirrored==(source.opcode==2||source.opcode==6));
        spawn.pattern.spatialShots=false;spawn.retireOutside=false;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
        float x=source.position.x,y=source.position.y,vx=0,vy=0;
        float angle=sub==0||sub==5 ? 0 : sub<=2 ? 0.523598790F : sub<=4 ? -1.04719758F : pi/2;
        float speed=sub==0 ? 0 : sub<=2 ? 4.5F : sub<=4 ? 4 : 2,angular=0;
        unsigned random=spawn.seed,ambientRandom=spawn.seed;int next=-1;
        if(sub>=1&&sub<=4)next=79-static_cast<int>(Next(random)%80);
        for(int frame=0;frame<160;++frame) {
            x+=vx*(spawn.mirrored ? -1 : 1);y+=vy;
            if(sub==0&&frame>0&&frame%40==0) {
                x=static_cast<float>(Next(ambientRandom)>>8)/16777216.0F*384;
                y=static_cast<float>(Next(ambientRandom)>>8)/16777216.0F*224+224;
            }
            if(sub>=1&&sub<=4) {
                if(frame==30)angular=sub<=2 ? -0.0654498488F : 0.0349065848F;
                if(frame==(sub<=2 ? 115 : 90))angular=0;
            }
            if((sub==5||sub==7)&&frame==40) {
                Next(random);angle=static_cast<float>(Next(random)>>8)/16777216.0F*(pi/2)+pi/4;speed=1.79999995F;
            }
            angle+=angular;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
            bullets->Reset();actor.Update(1.0F/60,{0,0,-6},*bullets);
            Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,spawn.position.y);
            int expected=0;
            if(sub>=1&&sub<=4&&frame==next&&frame<(sub<=2 ? 115 : 90)) {expected=sub<=2 ? 12 : 9;next+=80;}
            if((sub==5||sub==7)&&frame==40)expected=60;
            assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
            if(sub==0)assert(!actor.visible&&!actor.CanBeTargeted());
            if(sub==2||sub==4)assert(actor.animationScript==9);
        }
        if(sub==5||sub==7) {assert(actor.Defeat());actor.Update(1.0F/60,{},*bullets);assert(actor.finished&&actor.dropEvents==std::vector<int>{8});}
    }
    assert(index==124);
}
void TestDialogueMilestones() {
    for(bool marisa:{false,true}) {
        const auto cues=stage6::Stage6DialogueCues(DialogueScene::FinalBoss,false,marisa);
        std::vector<int> signals;for(const auto& cue:cues)if(cue.kind==DialogueCueKind::StageSignal) {assert(cue.value==static_cast<int>(signals.size()+1));signals.push_back(cue.frame);}
        assert((signals==std::vector<int>{861,marisa ? 7866 : 8666}));
        assert(stage6::Stage6Dialogue(DialogueScene::FinalBoss,true,marisa).empty());
        const auto lines=stage6::Stage6Dialogue(DialogueScene::FinalBoss,false,marisa);
        int duration=0;for(const auto& line:lines)duration+=std::lround(line.duration*60);assert(duration==(marisa ? 11106 : 10704));
    }
    // 测试接收端独立交付各次等待，目录不得把两次恢复或对白中的入场合并到最后一次 EndEvent。
    StageDirector director(stage6::Stage6Timeline());std::vector<int> resumed;std::vector<int> bosses;
    int midDialogue=-1,mainDialogue=-1,waitSignal=-1;bool midEnded=false;
    for(int frame=1;frame<22000;++frame) {
        director.Update(1.0F/60,[&](const StageEvent& event) {
            if(const auto* boss=std::get_if<StartBoss>(&event.action)) {bosses.push_back(frame);if(boss->battle==&stage6::kSakuyaMidboss)assert(frame==2493);return false;}
            if(const auto* dialogue=std::get_if<StartDialogue>(&event.action)) {
                if(dialogue->scene==DialogueScene::Midboss)midDialogue=frame;else if(!dialogue->afterBattle)mainDialogue=frame;return false;
            }
            if(const auto* wait=std::get_if<WaitDialogueSignal>(&event.action)) {waitSignal=wait->signal;return false;}
            if(std::holds_alternative<ResumeBoss>(event.action)) {resumed.push_back(frame);return true;}
            return true;
        });
        if(frame==midDialogue+240&&midDialogue>0)director.EndEvent(2);
        if(frame==4000) {assert(director.EndEvent(1));midEnded=true;}
        if(mainDialogue>0) {
            if(frame==mainDialogue+861) {assert(waitSignal==1);director.EndEvent(6);}
            if(frame==mainDialogue+8666) {assert(waitSignal==2);director.EndEvent(7);}
            if(frame==mainDialogue+10704) {assert(waitSignal==0);director.EndEvent(3);director.EndEvent(8);}
        }
    }
    assert(midEnded&&mainDialogue==4600&&bosses.size()==2&&bosses[1]==5462);
    assert((resumed==std::vector<int>{2734,13267,15305}));
}
void TestBossPhaseLifecycle() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int shot=0;shot<4;++shot)for(bool final:{false,true})for(bool timeout:{false,true}) {
        std::unique_ptr<bosses::ConfiguredBoss> ownedBoss=final ? std::unique_ptr<bosses::ConfiguredBoss>(new bosses::RemiliaBoss(behavior::UpdateRate::Hz60,false)) : std::unique_ptr<bosses::ConfiguredBoss>(new bosses::SakuyaBoss(behavior::UpdateRate::Hz60,false));
        auto& boss=*ownedBoss;boss.StartBoss(final ? stage6::RemiliaFinalConfig(static_cast<ShotType>(shot)) : stage6::SakuyaMidbossConfig(static_cast<ShotType>(shot)));
        for(int frame=0;frame<130;++frame) {boss.Update(1.0F/60,{},*bullets);assert(boss.attack==0&&!boss.CanTakeDamage()&&!boss.CanContactPlayer()&&boss.CanBeTargeted());}
        boss.ResumeCombat();if(final) {assert(boss.attack==0);boss.Update(1.0F/60,{},*bullets);boss.ResumeCombat();}
        const int phases=final ? 9 : timeout ? 1 : 2;
        for(int phase=1;phase<=phases;++phase) {
            assert(boss.attack==phase);bool ready=false;
            for(int frame=0;frame<10000&&!ready;++frame) {
                bullets->Reset();ready=boss.Update(1.0F/60,{},*bullets);
                if(!timeout&&boss.CanTakeDamage()) {boss.health=0;ready=boss.ShouldAdvanceFromDamage();}
            }
            assert(ready);if(!timeout&&(phase%2==0||phase==9))assert(boss.SkillDefeated());
            if(!final&&phase==2)Near(boss.maxHealth,std::array{750.0F,1000.0F,1100.0F,1100.0F}[shot]);
            assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());
        }
        bool ready=false;for(int frame=0;frame<130&&!ready;++frame)ready=boss.Update(1.0F/60,{},*bullets);
        assert(ready&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==!timeout);
    }
}
behavior::Routine StarProbe(behavior::Actor& actor) {bosses::RemiliaBoss::Behaviors::StarLattice(actor,false);co_await actor.At(100);}
void TestNativeStarGeometry() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4},13000,49);
    stage6::ConfigureBehavior(actor,false);actor.Start(StarProbe(actor));actor.Update(1.0F/60,{},*bullets);
    assert(bullets->Pool().ActiveCount()==48);unsigned random=49;const float seed=static_cast<float>(Next(random)>>8)/16777216.0F*2*pi;
    struct Expected {Vector3 position,direction;float length;int grow;};std::vector<Expected> expected;
    constexpr float scale=20.0F/448;
    for(int pass=0;pass<2;++pass) {
        const float base=pass ? -7*pi/8+seed : -pi+seed,delta=pass ? -pi/4 : pi/4;
        std::array<Vector2,8> positions{};for(int i=0;i<8;++i)positions[i]={32*std::cos(base+i*pi/4),32*std::sin(base+i*pi/4)};
        for(int ring=0;ring<3;++ring)for(int i=0;i<8;++i) {
            const float angle=base+ring*delta+i*pi/4,length=ring<2 ? 112 : 480;
            expected.push_back({{positions[i].x*scale,0,4-positions[i].y*scale},{std::cos(angle),0,-std::sin(angle)},length*scale,60+ring*16});
            positions[i].x+=std::cos(angle)*length;positions[i].y+=std::sin(angle)*length;
        }
    }
    std::size_t index=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
        const auto& e=expected[index++];assert(b.IsBeam());Near(b.position.x,e.position.x);Near(b.position.z,e.position.z);
        Near(b.beamDirection.x,e.direction.x);Near(b.beamDirection.z,e.direction.z);Near(b.beamLength,e.length);
        Near(b.radius,0.625F);Near(b.beamGrowSeconds,e.grow/60.0F);Near(b.dangerEndSeconds,134.0F/60);Near(b.maxAge,166.0F/60);
    });
    assert(actor.Random(1)==static_cast<float>(Next(random)>>8)/16777216.0F);
}
behavior::Routine MineProbe(behavior::Actor& actor) {assert(bosses::RemiliaBoss::Behaviors::SeedMines(actor)==4);bosses::RemiliaBoss::Behaviors::ReleaseMines(actor);co_await actor.At(100);}
void TestNativeMines() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int type=0;type<10;++type) {
        auto* b=bullets->Spawn(BulletOwner::Enemy,{static_cast<float>(type),0,4},{0,0,1},0.1F,WHITE,stage6::kBlueBullet);
        b->nativeType=type;b->nativeColor=3;
    }
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4},13000,49);stage6::ConfigureBehavior(actor,false);actor.Start(MineProbe(actor));
    actor.Update(1.0F/60,{},*bullets);assert(bullets->Pool().ActiveCount()==14);
    unsigned random=49;for(int i=0;i<4;++i)Next(random);const float phase=static_cast<float>(Next(random)>>8)/16777216.0F*2*pi-pi;
    int index=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
        if(index<10) {Near(b.velocity.z,1);Near(b.accelerationSeconds,1e9F);}
        else {
            assert(b.nativeType==3&&b.nativeColor==2);Near(b.position.x,index-4.0F);Near(BulletSystem::Length(b.velocity),0);
            const float angle=b.position.x*pi/11.4285714F+phase;
            Near(b.acceleration.x,1.60714281F*std::cos(angle));Near(b.acceleration.z,-1.60714281F*std::sin(angle));
            Near(b.accelerationSeconds,2);Near(b.spawnVisualSeconds,32.0F/60);
        }
        ++index;
    });
    assert(actor.Random(1)==static_cast<float>(Next(random)>>8)/16777216.0F);
}

behavior::Routine StarsEntry(behavior::Actor& actor) {bosses::RemiliaBoss::Behaviors::PhaseMemory memory;co_await bosses::RemiliaBoss::Behaviors::StarOfVanities(actor,memory);}
behavior::Routine SecondEntry(behavior::Actor& actor) {bosses::RemiliaBoss::Behaviors::PhaseMemory memory;co_await bosses::RemiliaBoss::Behaviors::SecondNonspell(actor,memory);}
/// @brief 控制流见证独立展开源调用/回拨；与旧图形、原子程序索引工厂或生产协程不共享时刻计算。
void TestBossSourceClocks() {
    using namespace stage6;auto bullets=std::make_unique<BulletSystem>();
    const std::array<behavior::Entry,8> entries{bosses::SakuyaBoss::Behaviors::EternalMeekNonspell,bosses::SakuyaBoss::Behaviors::EternalMeek,bosses::RemiliaBoss::Behaviors::FirstNonspell,StarsEntry,
        SecondEntry,bosses::RemiliaBoss::Behaviors::ScarletNetherworld,bosses::RemiliaBoss::Behaviors::ThirdNonspell,bosses::RemiliaBoss::Behaviors::ScarletShoot};
    for(int kind=0;kind<8;++kind) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},19000,49);ConfigureBehavior(actor,false);actor.Start(entries[kind](actor));
        for(int frame=0;frame<1200;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{0,0,-6},*bullets);int expected=0;
            if(kind==0) {
                const int t=frame<30 ? frame : (frame-30)%130+30;
                if((t>=30&&t<=74&&(t-30)%4==0)||(t>=95&&t<=139&&(t-95)%4==0))expected=8;
                if((t>=79&&t<=94)||(t>=144&&t<=159))expected=5;
            } else if(kind==1) {if(frame>=120&&(frame-120)%4==0)expected=16;}
            else if(kind==2) {
                const int t=frame<100 ? frame : (frame-100)%642+100;
                if((t>=100&&t<=348&&(t-100)%8==0)||(t>=366&&t<=614&&(t-366)%8==0))expected=11;
            } else if(kind==3) {
                if(frame>=184&&(frame-184)%184==0)expected=48;
                if(frame>=244&&(frame-244)%184==0)expected=((frame-244)/184)%2 ? 144 : 240;
            } else if(kind==4) {
                const int t=frame<60 ? frame : (frame-60)%853+60;
                if((t>=161&&t<=241&&(t-161)%20==0)||(t>=803&&t<=883&&(t-803)%20==0))expected=21;
                if((t>=362&&t<=467&&(t-362)%15==0)||(t>=583&&t<=688&&(t-583)%15==0))expected=19;
            } else if(kind==5) {
                if(frame>=120) {const int t=(frame-120)%216;for(int i=0;i<6;++i)for(int j=0;j<4;++j)if(t==i*21+j*4)expected=j<2 ? 24 : 16;}
            } else if(kind==6) {
                const int t=frame<60 ? frame : (frame-60)%478+60;
                if((t>=60&&t<=178&&(t-60)%2==0)||(t>=308&&t<=426&&(t-308)%2==0))expected=8;
            } else if(frame>=120) {
                const int t=(frame-120)%484;if(t==64||t==124||t==334||t==364)expected=120;if(t==214)expected=72;
            }
            if(bullets->Pool().ActiveCount()!=static_cast<std::size_t>(expected))std::cerr<<"Stage 6 source clock kind "<<kind<<" frame "<<frame<<" actual "<<bullets->Pool().ActiveCount()<<" expected "<<expected<<'\n';
            assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
        }
    }
}
void TestSpatialDisplayFrameRates() {
    using namespace stage6;
    const std::array<behavior::Entry,11> entries{bosses::SakuyaBoss::Behaviors::EternalMeekNonspell,bosses::SakuyaBoss::Behaviors::EternalMeek,bosses::RemiliaBoss::Behaviors::FirstNonspell,StarsEntry,SecondEntry,
        bosses::RemiliaBoss::Behaviors::ScarletNetherworld,bosses::RemiliaBoss::Behaviors::ThirdNonspell,bosses::RemiliaBoss::Behaviors::VladTepesCurse,bosses::RemiliaBoss::Behaviors::BatNonspell,bosses::RemiliaBoss::Behaviors::ScarletShoot,bosses::RemiliaBoss::Behaviors::RedMagic};
    struct Snapshot {Vector3 position,velocity;int type,color,bounces;};
    for(auto entry:entries) {
        std::vector<Snapshot> expected;Vector3 position{};float random=0;double phase=0;int animation=-1;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,4},19000,7);ConfigureBehavior(actor,true);actor.Start(entry(actor));
            for(int frame=0;frame<fps*12;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Snapshot> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back({b.position,b.velocity,b.nativeType,b.nativeColor,b.bouncesRemaining});});
            const float nextRandom=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=nextRandom;phase=actor.phaseFrames;animation=actor.animationScript;}
            else {
                assert(actual.size()==expected.size()&&nextRandom==random&&phase==actor.phaseFrames&&animation==actor.animationScript);
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                for(std::size_t i=0;i<actual.size();++i) {
                    const auto& a=actual[i];const auto& b=expected[i];assert(a.type==b.type&&a.color==b.color&&a.bounces==b.bounces);
                    Near(a.position.x,b.position.x);Near(a.position.y,b.position.y);Near(a.position.z,b.position.z);
                    Near(a.velocity.x,b.velocity.x);Near(a.velocity.y,b.velocity.y);Near(a.velocity.z,b.velocity.z);
                }
            }
            assert(bullets->Pool().DroppedSpawns()==0);
        }
    }
}

/// @brief 长时间高速巡航后，空间本体不能漂离出生回收盒使化蝠永久占槽；按正式顺序处理全部子演员。
void TestBatFamiliarRetirement() {
    auto bullets=std::make_unique<BulletSystem>();auto pool=std::make_unique<behavior::ActorPool<128>>();
    behavior::Actor boss;boss.Reset({0,0,5},13000,7);stage6::ConfigureBehavior(boss,true);boss.Start(bosses::RemiliaBoss::Behaviors::BatNonspell(boss));
    std::size_t peak=0;
    for(int tick=0;tick<60*120;++tick) {
        bullets->Reset();pool->ForEachActive([&](behavior::Actor& actor){actor.Update(1.0F/120,{0,0,-6},*bullets);});
        boss.Update(1.0F/120,{0,0,-6},*bullets);
        for(const auto& spawn:boss.spawnEvents) {
            assert(spawn.retirementBounds.Contains(spawn.position));auto* actor=pool->Spawn(spawn);assert(actor);actor->Update(1.0F/120,{0,0,-6},*bullets);
        }
        std::size_t count=0;pool->ForEachActive([&](const behavior::Actor&){++count;});peak=std::max(peak,count);
        assert(pool->DroppedSpawns()==0);
    }
    assert(peak>20&&peak<128);
}

void TestBombBatProtection() {
    auto bullets=std::make_unique<BulletSystem>();bosses::RemiliaBoss boss(behavior::UpdateRate::Hz60,false);boss.StartBoss(stage6::RemiliaFinalConfig());
    boss.ResumeCombat();boss.Update(1.0F/60,{},*bullets);boss.ResumeCombat();
    for(int phase=1;phase<9;++phase) {
        int frames=0;while(!boss.CanTakeDamage()&&frames++<700)boss.Update(1.0F/60,{},*bullets);
        assert(frames<700);boss.health=0;assert(boss.AdvanceSkill());
    }
    boss.SetPlayerBombActive(true);
    for(int frame=0;frame<240;++frame) {
        bullets->Reset();boss.Update(1.0F/60,{},*bullets);assert(!boss.CanBeTargeted()&&!boss.CanTakeDamage()&&boss.animationScript==165);
    }
    const float time=boss.skillTimer;boss.SetPlayerBombActive(false);
    for(int frame=0;frame<59;++frame) {boss.Update(1.0F/60,{},*bullets);assert(!boss.CanBeTargeted());}
    boss.Update(1.0F/60,{},*bullets);assert(boss.CanTakeDamage()&&boss.CanBeTargeted()&&boss.skillTimer>time);
}

void TestCrossPhaseChargeMemory() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);bosses::RemiliaBoss::Behaviors::PhaseMemory memory;
    actor.Reset({0,0,5},1200,49);stage6::ConfigureBehavior(actor,false);actor.Start(bosses::RemiliaBoss::Behaviors::StarOfVanities(actor,memory));
    for(int frame=0;frame<=128;++frame)actor.Update(1.0F/60,{},*bullets);
    assert(memory.chargingPulsesLeft==14);actor.pendingCallback=bosses::RemiliaBoss::Behaviors::SkillComplete;actor.Transition();
    actor.Start(bosses::RemiliaBoss::Behaviors::SecondNonspell(actor,memory));
    for(int frame=0;frame<=174;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==174 ? 21U : 0U));
    }
    assert(memory.chargingPulsesLeft==0);
}

}
/// @brief 运行六面独立出生/攻击时钟、九阶段、扩展几何、化蝠保护与 120 Hz 显示帧率一致性。
void RunStage6Tests() {TestBirthsAndNeutralMovement();TestDialogueMilestones();TestBossPhaseLifecycle();TestNativeStarGeometry();TestNativeMines();TestBossSourceClocks();TestSpatialDisplayFrameRates();TestBatFamiliarRetirement();TestBombBatProtection();TestCrossPhaseChargeMemory();}

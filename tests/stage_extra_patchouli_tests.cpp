// 本文件对照Extra帕秋莉独立源程序与命名技能，核对出生快照、移动、随机流和120 Hz内容时钟。
#include "stage_extra_patchouli_reference.hpp"
#include "gameplay/bosses/patchouli.hpp"
#include "stages/common/ids.hpp"
#include "stages/stage_extra/boss_config.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

namespace {
using namespace koumakyou;
using P=bosses::PatchouliBoss;
int subChecked=0,frameChecked=0,rankChecked=0;
void Near(float actual,float expected,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Extra Patchouli sub "<<subChecked<<" frame "<<frameChecked<<" rank "<<rankChecked
                 <<" mismatch "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
void TestSource() {
    constexpr float scale=20.0F/448,speedScale=scale*60,accelerationScale=scale*3600;
    auto bullets=std::make_unique<BulletSystem>();
    for(int sub:{17,18,19})for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        subChecked=sub;rankChecked=rank;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,10-120*scale},1800,seed);
        actor.SetRank(rank);P::Behaviors::ConfigureExtraPractice(actor,false);
        actor.Start(sub==17?P::Behaviors::SilentSelene(actor):sub==18?P::Behaviors::RoyalFlare(actor):P::Behaviors::PhilosophersStone(actor));
        stage_extra::reference::PatchouliReference source(sub,{192,120,0},seed,rank);
        for(int frame=0;frame<(sub==17?2000:sub==18?2800:2300);++frame) {
            frameChecked=frame;bullets->Reset();source.Tick({245,360,0});
            actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            if(actor.soundEvents!=source.sounds||actor.animationScript!=source.animation)
                std::cerr<<"Extra Patchouli state sub "<<sub<<" frame "<<frame<<" animation "<<actor.animationScript<<" vs "<<source.animation<<'\n';
            assert(actor.soundEvents==source.sounds&&actor.animationScript==source.animation);
            assert(actor.dropEvents==source.drops&&actor.defeatActors==source.defeatActors);
            assert(actor.auxiliaryAnimations[0].script==source.auxiliaryAnimation);
            assert(actor.CanTakeDamage()==(source.damageable&&source.interactable));
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            if(actual.size()!=source.emissions.size())std::cerr<<"Extra Patchouli emission count sub "<<sub<<" frame "<<frame<<" actual "<<actual.size()<<" expected "<<source.emissions.size()<<'\n';
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.velocity.x,e.velocity.x*speedScale,1e-4F);Near(b.velocity.z,-e.velocity.y*speedScale,1e-4F);
                Near(b.acceleration.x,e.acceleration.x*accelerationScale,1e-4F);Near(b.acceleration.z,-e.acceleration.y*accelerationScale,1e-4F);
                if(e.flags&16)Near(b.accelerationSeconds,e.accelerationFrames/60,1e-5F);
                Near(b.spawnVisualSeconds,e.flags&8?32.0F/60:0,1e-5F);
            }
            assert(source.card==sub+101&&source.health==(sub==17?1800:sub==18?1450:1100)&&source.timeout==(sub==17?2100:sub==18?2880:2400));
            assert(source.timeoutCallback==sub+1&&source.death==sub+1);
            assert(actor.spawnEvents.size()==source.spawns.size());
            constexpr std::array entries{P::Behaviors::FireStone,P::Behaviors::WaterStone,P::Behaviors::WoodStone,P::Behaviors::MetalStone,P::Behaviors::EarthStone};
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& expected=source.spawns[n];const auto& actual=actor.spawnEvents[n];
                assert(sub==19&&frame==120&&expected.sub==31+static_cast<int>(n)&&actual.pattern.routine==entries[n]);
                assert(actual.sourceHealth==expected.health&&expected.health==1&&actual.sourceItem==expected.item&&actual.sourceScore==expected.score);
                assert(expected.variables[4]==0&&actual.seed==29001+n&&behavior::Valid(actual));
                Near(actual.position.x,(expected.position.x-192)*scale);Near(actual.position.z,10-expected.position.y*scale);
            }
        }
        assert(actor.Random(1)==source.Random(1));
    }
}

void TestMetadataAndDisplayRates() {
    P inspector;
    for(bool moon:{true,false}) {
        const auto id=moon?P::Behavior::SilentSelene:P::Behavior::RoyalFlare;
        for(unsigned shot=0;shot<4;++shot) {
            const auto skill=inspector.DescribeSkill(id,static_cast<ShotType>(shot));
            assert(skill.health==(moon?1800:1450)&&skill.reserveHealth==0&&skill.timeoutFrames==(moon?2100:2880));
            assert(skill.spellCard==(moon?119:120)&&skill.spellRevealFrame==0);
        }
        for(auto difficulty:{native_combat::Difficulty::Easy,native_combat::Difficulty::Hard,native_combat::Difficulty::Lunatic}) {
            bool rejected=false;try {inspector.DescribeSkill(id,ShotType::HomingAmulet,difficulty);}catch(const std::invalid_argument&) {rejected=true;}
            assert(rejected);
        }
        float neutralRandom=0;
        for(bool spatial:{false,true}) {
            Vector3 expectedPosition{};std::vector<Bullet> expected;float nextRandom=0;double phase=0;
            for(int fps:{120,30,60,144,240}) {
                auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;
                actor.Reset({0,0,4.64285707F},1800,7);actor.SetRank(32);P::Behaviors::ConfigureExtraPractice(actor,spatial);
                actor.Start(moon?P::Behaviors::SilentSelene(actor):P::Behaviors::RoyalFlare(actor));
                for(int frame=0;frame<fps*8;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
                std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
                const float random=actor.Random(1);
                if(fps==120) {if(!spatial)neutralRandom=random;else assert(random==neutralRandom);}
                if(fps==120) {expectedPosition=actor.position;expected=actual;nextRandom=random;phase=actor.phaseFrames;}
                else {
                    assert(actual.size()==expected.size()&&random==nextRandom&&actor.phaseFrames==phase);
                    Near(actor.position.x,expectedPosition.x);Near(actor.position.y,expectedPosition.y);Near(actor.position.z,expectedPosition.z);
                    for(std::size_t n=0;n<actual.size();++n) {
                        Near(actual[n].position.x,expected[n].position.x);Near(actual[n].position.y,expected[n].position.y);Near(actual[n].position.z,expected[n].position.z);
                        Near(actual[n].velocity.x,expected[n].velocity.x);Near(actual[n].velocity.y,expected[n].velocity.y);Near(actual[n].velocity.z,expected[n].velocity.z);
                    }
                }
                assert(!bullets->Pool().DroppedSpawns()&&actor.phaseFrames==480&&actor.CanTakeDamage());
            }
        }
    }
}

void TestConfiguredSkills() {
    for(auto id:{P::Behavior::SilentSelene,P::Behavior::RoyalFlare,P::Behavior::PhilosophersStone})for(unsigned shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        bosses::StartConfig config{.position={0,0,4.64285707F},.seed=7,.visual=stage_assets::kOrb,
            .configure=P::Behaviors::ConfigureExtraPractice,.shot=static_cast<ShotType>(shot),
            .defeated=P::Behavior::ExtraRetreat,.retreat=P::Behavior::ExtraRetreat,
            .phases={{{id},{{{0,1,WHITE}}}}}};
        auto bullets=std::make_unique<BulletSystem>();P boss;boss.StartBoss(config);boss.SetChallengeRank(32);
        bool declared=false,emitted=false,summoned=false;int ticks=0;
        for(;ticks<60*120&&boss.active;++ticks) {
            bullets->Update(1.0F/120,{},Vector3{2,3,-6});
            const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(boss.SpellName()) {declared=true;assert(boss.SpellCardNumber()==(id==P::Behavior::SilentSelene?119:id==P::Behavior::RoyalFlare?120:121));}
            emitted|=bullets->Pool().ActiveCount()>0;summoned|=boss.SpawnEvents().size()==5;
            if(ticks<240)assert(!boss.CanTakeDamage());
            if(damage&&boss.CanTakeDamage()&&boss.skillTimer>8)boss.health=0;
            if(ready||boss.ShouldAdvanceFromDamage())boss.AdvanceSkill();
            assert(!bullets->Pool().DroppedSpawns());
        }
        assert(ticks<60*120&&!boss.active&&boss.Defeated()==damage&&declared&&(id==P::Behavior::PhilosophersStone?summoned:emitted));
        assert(!bullets->Pool().ActiveCount());
    }
}

void TestEmissionClocks() {
    // 独立展开等待/回跳和原生13计数，不从生产配方或参考执行器取期望。
    for(bool moon:{true,false})for(bool hz120:{false,true}) {
        auto bullets=std::make_unique<BulletSystem>();
        behavior::Actor actor(hz120?behavior::UpdateRate::Hz120:behavior::UpdateRate::Hz60);
        actor.Reset({0,0,4.64285707F},1800,7);P::Behaviors::ConfigureExtraPractice(actor,false);
        actor.Start(moon?P::Behaviors::SilentSelene(actor):P::Behaviors::RoyalFlare(actor));
        const int steps=hz120?4400:2200;
        for(int tick=0;tick<steps;++tick) {
            bullets->Reset();actor.Update(1.0F/(hz120?120:60),{2,0,-6},*bullets);
            const int frame=hz120?tick/2:tick;int expected=0;
            if(frame>=120&&(!hz120||tick%2==0)) {
                const int attack=frame-120;
                if(moon) {if(attack%6==0)expected=14+attack/6/5;}
                else {
                    const int cycle=attack/1000,local=attack%1000,calls=cycle*940+local;
                    if(local<940&&calls%6==0)expected=local<280?6:local<560?10:local<740?6:5;
                }
            }
            if(bullets->Pool().ActiveCount()!=static_cast<std::size_t>(expected))
                std::cerr<<"Extra Patchouli clock moon "<<moon<<" hz120 "<<hz120<<" tick "<<tick<<'\n';
            assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
            assert(actor.CanTakeDamage()==(frame>=120));
        }
    }
}

void TestStoneSourceAndDefeat() {
    constexpr std::array entries{P::Behaviors::FireStone,P::Behaviors::WaterStone,P::Behaviors::WoodStone,P::Behaviors::MetalStone,P::Behaviors::EarthStone};
    constexpr float scale=20.0F/448,speedScale=scale*60,accelerationScale=scale*3600;
    for(int kind=0;kind<5;++kind)for(unsigned seed:{7U,29001U})for(int rank:{10,16,18,31,32})for(float playerY:{40.0F,360.0F}) {
        subChecked=31+kind;rankChecked=rank;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,10-144*scale},1,seed);
        actor.SetRank(rank);actor.skills.bulletBase=stage_assets::kNativeBulletBase;actor.skills.laserBase=stage_assets::kNativeLaserBase;
        actor.SetDeathCallback(P::Behaviors::StoneDefeated);actor.Start(entries[kind](actor));
        stage_extra::reference::PatchouliReference source(31+kind,{192,144,0},seed,rank);
        auto bullets=std::make_unique<BulletSystem>();
        for(int frame=0;frame<650;++frame) {
            frameChecked=frame;bullets->Reset();source.Tick({245,playerY,0});
            actor.Update(1.0F/60,{(245-192)*scale,0,10-playerY*scale},*bullets);
            assert(actor.animationScript==source.animation&&actor.interactable==source.interactable&&actor.soundEvents==source.sounds);
            assert(actor.health==source.health&&source.health==1);
            assert(actor.auxiliaryAnimations[0].script==source.auxiliaryAnimation);
            assert(!actor.CanTakeDamage()&&!actor.CanContactPlayer());Near(actor.hitRadius,source.radius*scale);
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            if(actual.size()!=source.emissions.size())std::cerr<<"Extra stone count kind "<<kind<<" frame "<<frame<<'\n';
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.velocity.x,e.velocity.x*speedScale,1e-4F);Near(b.velocity.z,-e.velocity.y*speedScale,1e-4F);
                Near(b.acceleration.x,e.acceleration.x*accelerationScale,1e-4F);Near(b.acceleration.z,-e.acceleration.y*accelerationScale,1e-4F);
                if(e.flags&16)Near(b.accelerationSeconds,e.accelerationFrames/60,1e-5F);
                if(e.flags&(64|128|256)) {
                    assert(b.turnCount==e.turnCount&&b.turnAbsolute==bool(e.flags&256)&&b.turnAimed==bool(e.flags&128));
                    Near(b.turnDelaySeconds,e.turnDelay/60,1e-5F);Near(b.turnAngle,e.turnAngle,1e-5F);Near(b.turnSpeed,e.turnSpeed*speedScale,1e-4F);
                }
                Near(b.spawnVisualSeconds,e.flags&8?32.0F/60:e.flags&4?16.0F/60:e.flags&2?10.0F/60:0,1e-5F);
            }
        }
        assert(actor.Random(1)==source.Random(1));
        assert(actor.Defeat());source.Defeat();
        for(int frame=0;frame<=30;++frame) {
            bullets->Reset();source.Tick({245,playerY,0});actor.Update(1.0F/60,{},*bullets);
            assert(actor.dropEvents==source.drops&&actor.finished==!source.active);
            if(frame==0)assert(source.drops==std::vector<int>{8});
            assert(actor.animationInterrupt&&actor.animationInterrupt->label==source.interrupt);
            assert(actor.auxiliaryAnimations[0].interrupt&&actor.auxiliaryAnimations[0].interrupt->label==source.auxiliaryInterrupt);
            assert(!bullets->Pool().ActiveCount());
        }
    }
}

void TestStoneDisplayRates() {
    constexpr std::array entries{P::Behaviors::FireStone,P::Behaviors::WaterStone,P::Behaviors::WoodStone,P::Behaviors::MetalStone,P::Behaviors::EarthStone};
    for(auto entry:entries) {
        float neutralRandom=0;
        for(bool spatial:{false,true}) {
            std::vector<Bullet> expected;float nextRandom=0;
            for(int fps:{120,30,60,144,240}) {
                auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;
                actor.Reset({0,0,3.57142857F},1,7);actor.SetRank(32);actor.skills.spatial=spatial;
                actor.skills.bulletBase=stage_assets::kNativeBulletBase;actor.Start(entry(actor));
                for(int n=0;n<fps*8;++n)actor.Update(1.0F/fps,{2,3,-6},*bullets);
                const float random=actor.Random(1);std::vector<Bullet> actual;
                bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
                if(fps==120) {
                    if(spatial)assert(random==neutralRandom);else neutralRandom=random;
                    expected=actual;nextRandom=random;
                } else {
                    assert(actual.size()==expected.size()&&random==nextRandom);
                    for(std::size_t n=0;n<actual.size();++n) {
                        Near(actual[n].velocity.x,expected[n].velocity.x);Near(actual[n].velocity.y,expected[n].velocity.y);Near(actual[n].velocity.z,expected[n].velocity.z);
                        assert(actual[n].turnAbsolute==expected[n].turnAbsolute);Near(actual[n].turnAngle,expected[n].turnAngle);
                    }
                }
                assert(!bullets->Pool().DroppedSpawns()&&actor.phaseFrames==480&&!actor.CanTakeDamage());
            }
        }
    }
}

void TestFullEncounter() {
    for(unsigned shot=0;shot<4;++shot)for(int route=0;route<3;++route) {
        const auto config=stage_extra::PatchouliMidbossConfig(static_cast<ShotType>(shot));
        assert(config.phases.size()==3&&!config.initialContact);
        P boss;boss.StartBoss(config);boss.SetChallengeRank(32);assert(boss.health==6000);
        auto bullets=std::make_unique<BulletSystem>();behavior::ActorPool<8> children;
        for(int tick=0;tick<240;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));assert(!boss.CanTakeDamage()&&!boss.CanContactPlayer());}
        boss.ResumeCombat();assert(boss.health==1800&&!boss.CanTakeDamage());
        std::array<bool,3> seen{};int drops=0,items=0,spawned=0;bool ended=false;int tick=0;
        // 此处冻结弹体积分，隔离阶段和子演员生命周期；实际弹池/图形由Game专项独立覆盖。
        for(;tick<140*120;++tick) {
            bullets->Reset();
            children.ForEachActive([&](behavior::Actor& actor) {
                actor.Update(1.0F/120,{2,3,-6},*bullets);if(actor.pendingCallback)actor.Transition();
                for(int count:actor.dropEvents)drops+=count;
            });
            if(boss.active) {
                const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
                for(int count:boss.DropEvents())drops+=count;for(int item:boss.ItemEvents()) {assert(item==5);++items;}
                if(boss.DefeatActorsRequested())children.ForEachActive([](behavior::Actor& actor){actor.health=0;});
                for(const auto& request:boss.SpawnEvents()) {
                    assert(request.sourceHealth==1&&request.sourceScore==10&&request.sourceItem==-2);
                    auto* actor=children.Spawn(request);assert(actor);actor->SetRank(32);actor->Update(1.0F/120,{2,3,-6},*bullets);++spawned;
                }
                if(boss.SpellName()) {
                    const int card=boss.SpellCardNumber();assert(card>=119&&card<=121);seen[card-119]=true;
                    const bool damage=route==1||(route==2&&card==120);
                    if(damage&&boss.CanTakeDamage()&&boss.skillTimer>8)boss.health=0;
                }
                if(ready||boss.ShouldAdvanceFromDamage())boss.AdvanceSkill();
            }
            bool activeChild=false;children.ForEachActive([&](const behavior::Actor&){activeChild=true;});
            assert(!children.DroppedSpawns()&&!bullets->Pool().DroppedSpawns());
            if(!boss.active&&!activeChild) {ended=true;break;}
        }
        assert(ended&&tick<140*120&&spawned==5&&items==1&&drops==80);
        assert(std::all_of(seen.begin(),seen.end(),[](bool value){return value;}));
        assert(boss.Defeated()==(route==1)&&boss.CompletionScore()==200000);
        children.Clear();
    }
    P inspector;const auto skill=inspector.DescribeSkill(P::Behavior::PhilosophersStone);
    assert(skill.health==1100&&skill.timeoutFrames==2400&&skill.spellCard==121);
}
}
/// @brief Extra三符与五种使魔专项；正式关卡入口与辅助槽图形另行验收。
void RunStageExtraPatchouliTests() {TestSource();TestMetadataAndDisplayRates();TestConfiguredSkills();TestEmissionClocks();TestStoneSourceAndDefeat();TestStoneDisplayRates();TestFullEncounter();}

// 本文件对照芙兰朵露独立源指令，验证八段非符、前三符及使魔/激光/Bomb响应、入场和跨图集动画。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/stage_extra/boss_config.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <limits>
namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;
// 预期独立抄自源入口元数据，不读取生产配方；编号只用于测试侧选择原指令。
struct NonspellCase {int sub,health,reserve,death,callback,period,count;F::Behavior behavior;};
constexpr NonspellCase cases[]{
    {38,10000,1800,43,39,60,128,F::Behavior::FirstNonspell},
    {43,8000,1200,48,44,30,128,F::Behavior::SecondNonspell},
    {48,8000,1900,55,49,60,96,F::Behavior::ThirdNonspell},
    {55,8000,2400,61,56,40,64,F::Behavior::FourthNonspell},
    {61,10000,1400,65,62,30,64,F::Behavior::FifthNonspell},
    {65,10000,2200,71,66,30,64,F::Behavior::SixthNonspell},
    {71,10000,1520,76,72,30,96,F::Behavior::SeventhNonspell},
    {76,10000,1800,81,77,20,30,F::Behavior::EighthNonspell},
};
behavior::Routine StartNonspell(behavior::Actor& actor,F::Behavior id) {
    switch(id) {
        case F::Behavior::FirstNonspell:co_await F::Behaviors::FirstNonspell(actor);break;
        case F::Behavior::SecondNonspell:co_await F::Behaviors::SecondNonspell(actor);break;
        case F::Behavior::ThirdNonspell:co_await F::Behaviors::ThirdNonspell(actor);break;
        case F::Behavior::FourthNonspell:co_await F::Behaviors::FourthNonspell(actor);break;
        case F::Behavior::FifthNonspell:co_await F::Behaviors::FifthNonspell(actor);break;
        case F::Behavior::SixthNonspell:co_await F::Behaviors::SixthNonspell(actor);break;
        case F::Behavior::SeventhNonspell:co_await F::Behaviors::SeventhNonspell(actor);break;
        case F::Behavior::EighthNonspell:co_await F::Behaviors::EighthNonspell(actor);break;
        default:throw std::logic_error("Invalid test nonspell");
    }
}
int checkedSub=0,checkedFrame=0,checkedRank=0,checkedRoute=0;
void Near(float actual,float expected,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Flandre sub "<<checkedSub<<" frame "<<checkedFrame<<" rank "<<checkedRank<<" route "<<checkedRoute<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
void TestSource() {
    constexpr float scale=20.0F/448,speedScale=scale*60;
    auto bullets=std::make_unique<BulletSystem>();
    for(int sub:{37,38,43,48,55,61,65,71,76})for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        checkedSub=sub;checkedRank=rank;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5.71428585F},15000,seed);actor.SetRank(rank);
        F::Behaviors::ConfigurePractice(actor,false);
        const NonspellCase* spec=nullptr;for(const auto& entry:cases)if(entry.sub==sub)spec=&entry;
        actor.Start(sub==37?F::Behaviors::Entrance(actor):StartNonspell(actor,spec->behavior));
        stage_extra::reference::FlandreReference source(sub,{192,96,0},seed,rank);
        for(int frame=0;frame<1750;++frame) {
            checkedFrame=frame;bullets->Reset();
            // 移动自机可识别第八段是否每轮使用当前瞄准点，避免固定玩家掩盖配置时快照错误。
            const Vector3 player{192+80*std::sin(frame*0.007F),360+30*std::cos(frame*0.011F),0};
            source.Tick(player);actor.Update(1.0F/60,{(player.x-192)*scale,0,10-player.y*scale},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            if(actor.animationScript!=source.animation)std::cerr<<"Flandre animation "<<sub<<' '<<frame<<' '<<actor.animationScript<<" vs "<<source.animation<<'\n';
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.dropEvents==source.drops);
            assert(actor.defeatActors==source.defeatActors&&actor.damageable==source.damageable&&actor.interactable==source.interactable&&actor.contactEnabled==source.contact);
            assert(actor.auxiliaryAnimations[0].script==source.auxiliaryAnimation);
            if(sub==37)assert(actor.auxiliaryAnimations[0].visual==stage_assets::kOrb);
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            if(actual.size()!=source.emissions.size())std::cerr<<"Flandre count "<<sub<<' '<<frame<<' '<<rank<<' '<<actual.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&e.flags==513&&b.sampleMotionAtStart);
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
            }
            if(sub!=37)assert(source.health==spec->health&&source.reserve==spec->reserve&&source.timeout==1800&&source.death==spec->death&&source.healthCallback==spec->callback&&source.timeoutCallback==source.healthCallback);
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
void TestClocksAndDisplayRates() {
    for(const auto& spec:cases)for(bool hz120:{true,false}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(hz120?behavior::UpdateRate::Hz120:behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},10000,7);actor.SetRank(32);F::Behaviors::ConfigurePractice(actor,false);
        actor.Start(StartNonspell(actor,spec.behavior));
        const int factor=hz120?2:1,onset=spec.sub==38?70:90,period=spec.period+spec.period/5-(spec.period/5*2*32)/32;
        for(int tick=0;tick<factor*1000;++tick) {
            bullets->Reset();actor.Update(1.0F/(factor*60),{2,0,-6},*bullets);
            const bool fires=tick>=factor*onset&&(tick-factor*onset+1)%(factor*period)==0;
            assert(bullets->Pool().ActiveCount()==(fires?static_cast<unsigned>(spec.count):0U));
            assert(actor.CanTakeDamage()==(tick>=30*factor));
        }
    }
    for(const auto& spec:cases)for(bool spatial:{true,false}) {
        Vector3 position{};std::vector<Bullet> expected;float random=0;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,5.71428585F},10000,7);actor.SetRank(32);
            F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(StartNonspell(actor,spec.behavior));
            for(int frame=0;frame<8*fps;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
            const float next=actor.Random(1);
            assert(actor.phaseFrames==480&&!bullets->Pool().DroppedSpawns());
            if(fps==120) {position=actor.position;expected=actual;random=next;}
            else {
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                assert(actual.size()==expected.size()&&next==random);
                for(std::size_t n=0;n<actual.size();++n) {
                    Near(actual[n].position.x,expected[n].position.x);Near(actual[n].position.y,expected[n].position.y);Near(actual[n].position.z,expected[n].position.z);
                    Near(actual[n].Velocity().x,expected[n].Velocity().x);Near(actual[n].Velocity().y,expected[n].Velocity().y);Near(actual[n].Velocity().z,expected[n].Velocity().z);
                }
            }
        }
    }
}
// 子演员的出生配置取自真实父行为；源侧独立按42执行，覆盖全部16次请求中的12种路线。
void TestCranberrySource() {
    constexpr float scale=20.0F/448,speedScale=scale*60;
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},1800,seed);actor.SetRank(rank);F::Behaviors::ConfigurePractice(actor,false);
        actor.Start(F::Behaviors::CranberryTrap(actor));
        stage_extra::reference::FlandreReference source(39,{192,96,0},seed,rank,1800);source.death=43;
        std::vector<behavior::Spawn> children;std::vector<stage_extra::reference::FlandreReference::Spawn> original;
        checkedSub=39;checkedRank=rank;
        for(int frame=0;frame<3200;++frame) {
            checkedFrame=frame;bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.dropEvents==source.drops);
            assert(actor.CanTakeDamage()==source.damageable&&actor.defeatActors==source.defeatActors&&bullets->Pool().ActiveCount()==0);
            assert(source.card==122&&source.timeout==3300&&source.timeoutCallback==43);
            assert(actor.spawnEvents.size()==source.spawns.size()&&source.nativeCallback==(frame>=120?10:-1));
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];
                assert(e.sub==42&&a.sourceHealth==e.health&&a.sourceScore==e.score&&a.sourceItem==e.item&&behavior::Valid(a));
                assert(a.visual==stage_assets::kOrb&&!a.retireOutside&&!a.damageable&&a.seed==seed+children.size()+1);
                Near(a.position.x,(e.position.x-192)*scale);Near(a.position.z,10-e.position.y*scale);
                assert(e.variables[4]==1.3F);children.push_back(a);original.push_back(e);
            }
        }
        assert(children.size()==60&&actor.Random(1)==source.Random(1));
        for(std::size_t index=0;index<16;++index) {
            const auto& spawn=children[index];const auto& raw=original[index];
            behavior::Actor emitter(behavior::UpdateRate::Hz60);emitter.Activate(spawn);emitter.SetRank(rank);
            stage_extra::reference::FlandreReference child(42,raw.position,spawn.seed,rank,raw.health);
            checkedSub=42;checkedRoute=raw.health;int emitted=0;int frame=0;
            for(;frame<400;++frame) {
                checkedFrame=frame;bullets->Reset();
                const Vector3 player{210+40*std::sin(frame*0.02F),350,0};
                child.Tick(player);emitter.Update(1.0F/60,{(player.x-192)*scale,0,10-player.y*scale},*bullets);
                Near(emitter.position.x,(child.position.x-192)*scale);Near(emitter.position.z,10-child.position.y*scale);
                if(emitter.animationScript!=child.animation||emitter.soundEvents!=child.sounds||emitter.contactEnabled!=child.contact||emitter.damageable!=child.damageable)
                    std::cerr<<"Cranberry child "<<raw.health<<" frame "<<frame<<" animation "<<emitter.animationScript<<" vs "<<child.animation<<" sounds "<<emitter.soundEvents.size()<<" vs "<<child.sounds.size()<<" contact "<<emitter.contactEnabled<<" vs "<<child.contact<<" damage "<<emitter.damageable<<" vs "<<child.damageable<<'\n';
                assert(emitter.animationScript==child.animation&&emitter.soundEvents==child.sounds&&emitter.contactEnabled==child.contact&&emitter.damageable==child.damageable);
                Near(emitter.hitRadius,child.radius*scale,1e-5F);
                const int interrupt=emitter.animationInterrupt?emitter.animationInterrupt->label:-1;assert(interrupt==child.interrupt);
                std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
                assert(actual.size()==child.emissions.size());emitted+=static_cast<int>(actual.size());
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& b=*actual[n];const auto& e=child.emissions[n];
                    assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart&&e.flags==512);
                    Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                    Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
                }
                if(emitter.finished||!child.active) {assert(emitter.finished&&!child.active);break;}
            }
            const int edges=raw.health/10%10;assert(frame==90+edges*121&&emitted==edges*10&&!bullets->Pool().DroppedSpawns());
            assert(emitter.Random(1)==child.Random(1));
        }
    }
}
void TestCranberryEmitterDisplayRates() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor parent;
    parent.Reset({0,0,5.71428585F},1800,7);F::Behaviors::ConfigurePractice(parent,false);parent.Start(F::Behaviors::CranberryTrap(parent));
    std::vector<behavior::Spawn> children;
    for(int tick=0;tick<1900;++tick) {parent.Update(1.0F/120,{},*bullets);children.insert(children.end(),parent.spawnEvents.begin(),parent.spawnEvents.end());}
    assert(children.size()==18);
    for(std::size_t index:{0U,2U,8U,13U})for(bool spatial:{true,false}) {
        Vector3 expected{};std::vector<Bullet> snapshot;double phase=0;float random=0;
        for(int fps:{120,30,60,144,240}) {
            auto spawn=children[index];spawn.pattern.spatialShots=spatial;behavior::Actor actor;actor.Activate(spawn);actor.SetRank(32);bullets->Reset();
            for(int frame=0;frame<7*fps;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
            const auto next=actor.Random(1);assert(actor.finished&&!actor.active&&!bullets->Pool().DroppedSpawns());
            if(fps==120) {expected=actor.position;snapshot=actual;phase=actor.phaseFrames;random=next;}
            else {
                Near(actor.position.x,expected.x);Near(actor.position.z,expected.z);assert(actor.phaseFrames==phase&&actual.size()==snapshot.size()&&next==random);
                for(std::size_t n=0;n<actual.size();++n) {
                    Near(actual[n].position.x,snapshot[n].position.x);Near(actual[n].position.y,snapshot[n].position.y);Near(actual[n].position.z,snapshot[n].position.z);
                    Near(actual[n].Velocity().x,snapshot[n].Velocity().x,1e-4F);Near(actual[n].Velocity().y,snapshot[n].Velocity().y,1e-4F);Near(actual[n].Velocity().z,snapshot[n].Velocity().z,1e-4F);
                }
            }
        }
    }
}
void TestCranberryClocksAndBomb() {
    F inspector;const auto definition=inspector.DescribeSkill(F::Behavior::CranberryTrap);
    assert(definition.health==1800&&definition.reserveHealth==0&&definition.timeoutFrames==3300&&definition.spellCard==122&&definition.reactToBomb);
    // 图形夹具也在首符结束立即停止，避免把占位退场宣称为完整关底演出。
    const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
        .defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
        .phases={{{F::Behavior::CranberryTrap},{{{0,1,WHITE}}}}}};
    for(bool hz120:{true,false})for(auto id:{F::Behavior::CranberryTrap,F::Behavior::Laevatein,F::Behavior::FourOfAKind}) {
        auto selected=config;selected.phases[0].skills[0]=id;
        const int factor=hz120?2:1;F boss(hz120?behavior::UpdateRate::Hz120:behavior::UpdateRate::Hz60,false);boss.StartBoss(selected);
        auto bullets=std::make_unique<BulletSystem>();boss.SetPlayerBombActive(true);
        for(int tick=0;tick<120*factor;++tick) {boss.Update(1.0F/(60*factor),{},*bullets);assert(boss.CanBeTargeted());}
        boss.Update(1.0F/(60*factor),{},*bullets);assert(!boss.CanBeTargeted()&&boss.animationScript==165);
        const auto clock=boss.skillTimer;boss.SetPlayerBombActive(false);
        for(int tick=0;tick<60*factor-1;++tick) {boss.Update(1.0F/(60*factor),{},*bullets);assert(!boss.CanBeTargeted());}
        boss.Update(1.0F/(60*factor),{},*bullets);assert(boss.CanTakeDamage()&&boss.animationScript==160&&boss.skillTimer>clock);
        boss.StartBoss(selected);boss.Update(1.0F/(60*factor),{},*bullets);assert(boss.CanBeTargeted()&&boss.animationScript!=165);
    }
    for(bool spatial:{true,false}) {
        Vector3 expected{};std::vector<behavior::Spawn> baseline;float random=0;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;actor.Reset({0,0,5.71428585F},1800,7);actor.SetRank(32);F::Behaviors::ConfigurePractice(actor,spatial);
            actor.Start(F::Behaviors::CranberryTrap(actor));auto bullets=std::make_unique<BulletSystem>();std::vector<behavior::Spawn> children;
            for(int frame=0;frame<20*fps;++frame) {actor.Update(1.0F/fps,{2,3,-6},*bullets);children.insert(children.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());}
            assert(actor.phaseFrames==1200&&children.size()==20);const auto next=actor.Random(1);
            if(fps==120) {expected=actor.position;baseline=children;random=next;}
            else {Near(actor.position.x,expected.x);Near(actor.position.y,expected.y);Near(actor.position.z,expected.z);assert(next==random&&baseline.size()==children.size());}
            for(std::size_t n=0;n<children.size();++n)assert(children[n].sourceHealth==baseline[n].sourceHealth&&children[n].seed==baseline[n].seed);
        }
    }
}
// 独立指令侧持有源激光几何；生产侧只观察实际弹池，不用新配方构造期望。
void TestLaevateinSource() {
    constexpr float scale=20.0F/448,speedScale=scale*60;
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},1200,seed);actor.SetRank(rank);F::Behaviors::ConfigurePractice(actor,false);
        actor.Start(F::Behaviors::Laevatein(actor));
        stage_extra::reference::FlandreReference source(44,{192,96,0},seed,rank,1200);source.death=48;
        checkedSub=44;checkedRank=rank;int totalShots=0,longSwords=0;
        for(int frame=0;frame<3200;++frame) {
            checkedFrame=frame;source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            if(actor.animationScript!=source.animation||actor.soundEvents!=source.sounds)
                std::cerr<<"Laevatein state frame "<<frame<<" animation "<<actor.animationScript<<" vs "<<source.animation<<" sounds "<<actor.soundEvents.size()<<" vs "<<source.sounds.size()<<'\n';
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.dropEvents==source.drops);
            assert(actor.CanTakeDamage()==source.damageable&&actor.contactEnabled==source.contact&&actor.defeatActors==source.defeatActors);
            assert(source.card==123&&source.timeout==3300&&source.timeoutCallback==48&&source.nativeCallback==(frame>=120?10:-1));
            assert(actor.spawnEvents.empty());
            std::vector<const Bullet*> shots,beams;
            bullets->Pool().ForEachActive([&](const Bullet& b){(b.IsBeam()?beams:shots).push_back(&b);});
            if(shots.size()!=source.emissions.size())std::cerr<<"Laevatein shots frame "<<frame<<" got "<<shots.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(shots.size()==source.emissions.size());totalShots+=static_cast<int>(shots.size());
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&e.flags==532&&b.sampleMotionAtStart);
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
                Near(b.acceleration.x,std::cos(std::atan2(e.velocity.y,e.velocity.x))*e.acceleration*scale*3600,1e-4F);
                Near(b.acceleration.z,-std::sin(std::atan2(e.velocity.y,e.velocity.x))*e.acceleration*scale*3600,1e-4F);
                Near(b.accelerationSeconds,e.accelerationFrames/60,1e-5F);Near(b.spawnVisualSeconds,16.0F/60,1e-5F);
            }
            const auto newborn=std::count_if(beams.begin(),beams.end(),[](const Bullet* b){return b->age==0;});
            assert(newborn==std::count_if(source.beams.begin(),source.beams.end(),[](const auto& b){return b.active&&b.age==0;}));
            for(const auto& expected:source.beams)if(expected.active) {
                auto match=std::find_if(beams.begin(),beams.end(),[&](const Bullet* b) {
                    return std::abs(b->age-expected.age/60.0F)<1e-5F&&std::abs(b->radius-expected.width*scale/2)<1e-5F&&
                        std::abs(b->Direction().x-std::cos(expected.angle))<1e-5F&&std::abs(b->Direction().z+std::sin(expected.angle))<1e-5F;
                });
                if(match==beams.end())std::cerr<<"Laevatein beam frame "<<frame<<" age "<<expected.age<<" slot "<<expected.slot<<" angle "<<expected.angle<<'\n';
                assert(match!=beams.end());const auto* beam=*match;beams.erase(match);
                Near(beam->beamAnchor.x,(expected.anchor.x-192)*scale);Near(beam->beamAnchor.z,10-expected.anchor.y*scale);
                Near(beam->beamStartOffset,expected.start*scale);Near(beam->beamLength,(expected.end-expected.start)*scale);
                Near(beam->beamGrowSeconds,expected.grow/60.0F,1e-5F);Near(beam->beamFadeSeconds,expected.fade/60.0F,1e-5F);
                if(expected.age==0&&expected.slot==0)++longSwords;
                if(expected.forcedFade<0)Near(beam->maxAge,(expected.grow+expected.sustain+expected.fade)/60.0F,1e-5F);
                else Near(beam->maxAge,(expected.age-expected.forcedFade+expected.fade)/60.0F,1e-5F);
            }
            bullets->Pool().ForEachActive([](Bullet& b){if(!b.IsBeam())b.active=false;});bullets->Update(1.0F/60);
        }
        assert(totalShots==5240&&longSwords==18&&!bullets->Pool().DroppedSpawns());assert(actor.Random(1)==source.Random(1));
    }
}
void TestLaevateinClocksAndDisplayRates() {
    F inspector;
    for(unsigned shot=0;shot<4;++shot) {
        const auto skill=inspector.DescribeSkill(F::Behavior::Laevatein,static_cast<ShotType>(shot));
        assert(skill.health==1200&&skill.reserveHealth==0&&skill.timeoutFrames==3300&&skill.spellCard==123&&skill.reactToBomb);
        for(bool damage:{false,true}) {
            const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
                .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
                .phases={{{F::Behavior::Laevatein,F::Behavior::ThirdNonspell},{{{0,0.5F,PINK},{1,0.5F,WHITE}}}}}};
            F boss;boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false;
            for(int tick=0;tick<6700;++tick) {
                const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);bullets->Update(1.0F/120);
                if(damage&&tick>2400&&boss.CanTakeDamage())boss.health=0;
                if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.AdvanceSkill());advanced=true;break;}
            }
            assert(advanced&&boss.attack==2&&boss.health==8000&&!boss.CanTakeDamage());
            boss.Update(1.0F/120,{},*bullets);assert(!boss.CanTakeDamage()&&!bullets->Pool().ActiveCount());
        }
    }
    // 以下预期是独立展开的源调用时钟，不读取生产循环参数。完整周期为698帧，四段采样窗均从局部零时刻开始。
    for(bool hz120:{true,false}) {
        const int factor=hz120?2:1;behavior::Actor actor(hz120?behavior::UpdateRate::Hz120:behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},1200,7);actor.SetRank(32);F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::Laevatein(actor));
        auto bullets=std::make_unique<BulletSystem>();
        for(int tick=0;tick<factor*1600;++tick) {
            actor.Update(1.0F/(factor*60),{},*bullets);
            int actual=0;bullets->Pool().ForEachActive([&](const Bullet& b){if(!b.IsBeam())++actual;});
            int expected=0;
            if(tick>=120*factor&&tick%factor==0) {
                const int frame=(tick/factor-120)%698;
                if(frame<120&&frame%5==0)expected=17;
                if(frame>=300&&frame<364&&(frame-300)%5==0)expected=16;
                if(frame>=454&&frame<518&&(frame-454)%5==0)expected=16;
                if(frame>=518&&frame<638&&(frame-518)%5==0)expected=16;
            }
            assert(actual==expected&&actor.CanTakeDamage()==(tick>=120*factor));
            if(tick==88*factor-1) {Near(actor.position.x,0);Near(actor.position.z,6.42857143F);}
            if(tick==90*factor)assert(actor.animationScript==172);
            bullets->Pool().ForEachActive([](Bullet& b){if(!b.IsBeam())b.active=false;});bullets->Update(1.0F/(factor*60));
        }
        assert(actor.phaseFrames==1600&&!bullets->Pool().DroppedSpawns());
    }
    // 仅比较发射快照和引用修改；弹体积分由公共系统及真实Game验证，避免显示步长改变测试侧积分顺序。
    for(bool spatial:{true,false}) {
        Vector3 point{};std::vector<Bullet> expected;float random=0;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;actor.Reset({0,0,5.71428585F},1200,7);actor.SetRank(32);
            F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::Laevatein(actor));auto bullets=std::make_unique<BulletSystem>();
            for(int frame=0;frame<20*fps;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const float next=actor.Random(1);
            assert(actor.phaseFrames==1200&&!bullets->Pool().DroppedSpawns());
            if(fps==120) {point=actor.position;expected=actual;random=next;}
            else {
                Near(actor.position.x,point.x);Near(actor.position.y,point.y);Near(actor.position.z,point.z);assert(actual.size()==expected.size()&&next==random);
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& e=expected[n];assert(a.IsBeam()==e.IsBeam()&&a.nativeType==e.nativeType&&a.nativeColor==e.nativeColor);
                    Near(a.position.x,e.position.x);Near(a.position.y,e.position.y);Near(a.position.z,e.position.z);
                    Near(a.Velocity().x,e.Velocity().x,1e-4F);Near(a.Velocity().y,e.Velocity().y,1e-4F);Near(a.Velocity().z,e.Velocity().z,1e-4F);
                    Near(a.Direction().x,e.Direction().x,1e-5F);Near(a.Direction().y,e.Direction().y,1e-5F);Near(a.Direction().z,e.Direction().z,1e-5F);
                    Near(a.beamLength,e.beamLength);
                    // 普通敌弹无年龄上限，光束的有限截止值仍按原精度核对。
                    constexpr auto unlimited=std::numeric_limits<float>::infinity();
                    if(a.maxAge==unlimited||e.maxAge==unlimited)assert(a.maxAge==e.maxAge);
                    else Near(a.maxAge,e.maxAge,1e-5F);
                    Near(a.dangerEndSeconds,e.dangerEndSeconds,1e-5F);
                }
            }
        }
    }
}
// 差分侧按49→50/51及52…54执行，直接观察普通弹池和分身配置，不读取生产配方作为期望。
void TestFourOfAKindSource() {
    constexpr float scale=20.0F/448,speedScale=scale*60;
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},1900,seed);actor.visual=stage_assets::kBoss;actor.SetRank(rank);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::FourOfAKind(actor));
        stage_extra::reference::FlandreReference source(49,{192,96,0},seed,rank,1900);source.death=55;
        std::vector<behavior::Spawn> children;int effects=0;
        const auto check=[&](behavior::Actor& actual,stage_extra::reference::FlandreReference& expected,int sub,int frame) {
            checkedSub=sub;checkedFrame=frame;checkedRank=rank;
            bullets->Reset();const Vector3 player{210+80*std::sin(frame*0.013F),360+20*std::cos(frame*0.011F),0};
            expected.Tick(player);actual.Update(1.0F/60,{(player.x-192)*scale,0,10-player.y*scale},*bullets);
            Near(actual.position.x,(expected.position.x-192)*scale);Near(actual.position.z,10-expected.position.y*scale);
            if(actual.animationScript!=expected.animation||actual.soundEvents!=expected.sounds)
                std::cerr<<"Four of a Kind state "<<sub<<' '<<frame<<" animation "<<actual.animationScript<<" vs "<<expected.animation<<" sounds "<<actual.soundEvents.size()<<" vs "<<expected.sounds.size()<<'\n';
            assert(actual.animationScript==expected.animation&&actual.soundEvents==expected.sounds&&actual.dropEvents==expected.drops);
            assert(actual.damageable==expected.damageable&&actual.contactEnabled==expected.contact&&actual.interactable==expected.interactable);
            assert(actual.defeatActors==expected.defeatActors&&!actual.finished&&expected.active);
            std::vector<const Bullet*> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(&b);});
            if(shots.size()!=expected.emissions.size())std::cerr<<"Four of a Kind shots "<<sub<<' '<<frame<<" got "<<shots.size()<<" vs "<<expected.emissions.size()<<'\n';
            assert(shots.size()==expected.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=expected.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                assert(e.flags==(e.type==2?513:512));
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
            }
        };
        for(int frame=0;frame<3900;++frame) {
            check(actor,source,49,frame);koumakyou::stage_extra::reference::CompareEffects(actor,source);effects+=source.effectRequests;
            assert(source.effectRequests==((frame>=120&&frame<=213&&(frame-120)%3==0)?6:0));
            assert(source.card==124&&source.timeout==3900&&source.timeoutCallback==55&&source.health==1900);
            assert(source.nativeCallback==(frame>=120?10:-1));
            assert(actor.spawnEvents.size()==source.spawns.size()&&source.spawns.size()==(frame==216?3U:0U));
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];
                assert(e.sub==52+static_cast<int>(n)&&a.sourceHealth==e.health&&a.sourceScore==e.score&&a.sourceItem==e.item&&behavior::Valid(a));
                assert(a.visual==stage_assets::kBoss&&!a.retireOutside&&!a.damageable&&a.seed==seed+n+1);
                Near(a.position.x,(e.position.x-192)*scale);Near(a.position.z,10-e.position.y*scale);children.push_back(a);
            }
        }
        assert(children.size()==3&&effects==192&&actor.Random(1)==source.Random(1));
        for(int index=0;index<3;++index) {
            const auto& spawn=children[index];behavior::Actor clone(behavior::UpdateRate::Hz60);clone.Activate(spawn);clone.SetRank(rank);
            stage_extra::reference::FlandreReference child(52+index,{0,0,0},spawn.seed,rank,1200);
            int largeBursts=0;
            for(int frame=0;frame<3900;++frame) {
                check(clone,child,52+index,frame);Near(clone.hitRadius,child.radius*scale,1e-5F);
                assert(clone.health==1200&&clone.deathEffect==child.deathEffect&&child.deathEffect==2&&child.dropEffect==1);
                assert(clone.CanTakeDamage()==(frame>=60));
                if(!child.emissions.empty()&&child.emissions[0].type==6) {
                    ++largeBursts;assert(child.emissions.size()==(index==0?120U:index==1?80U:35U));
                }
            }
            assert(largeBursts==(index==0?6:index==1?8:7)&&clone.Random(1)==child.Random(1));
        }
    }
}
void TestFourOfAKindClocksAndDisplayRates() {
    F inspector;
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        const auto skill=inspector.DescribeSkill(F::Behavior::FourOfAKind,static_cast<ShotType>(shot));
        assert(skill.health==1900&&skill.reserveHealth==0&&skill.timeoutFrames==3900&&skill.spellCard==124&&skill.reactToBomb);
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::FourOfAKind,F::Behavior::FourthNonspell},{{{0,0.5F,PINK},{1,0.5F,WHITE}}}}}};
        F boss;boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false;int children=0;
        for(int tick=0;tick<7900;++tick) {
            bullets->Reset();const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            for(const auto& spawn:boss.SpawnEvents()) {++children;assert(tick==432&&spawn.visual==stage_assets::kBoss);}
            if(damage&&tick>2400&&boss.CanTakeDamage())boss.health=0;
            if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.AdvanceSkill());advanced=true;break;}
        }
        assert(children==3&&advanced&&boss.attack==2&&boss.health==8000&&!boss.CanTakeDamage());
        boss.Update(1.0F/120,{},*bullets);assert(boss.DefeatActorsRequested()&&!boss.CanTakeDamage());
    }
    for(bool hz120:{false,true}) {
        const int factor=hz120?2:1;behavior::Actor actor(hz120?behavior::UpdateRate::Hz120:behavior::UpdateRate::Hz60);
        actor.Reset({0,0,5.71428585F},1900,7);actor.visual=stage_assets::kBoss;
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::FourOfAKind(actor));auto bullets=std::make_unique<BulletSystem>();
        for(int tick=0;tick<=216*factor;++tick) {
            actor.Update(1.0F/(60*factor),{},*bullets);
            assert(actor.CanTakeDamage()==(tick>=120*factor));assert(actor.spawnEvents.size()==(tick==216*factor?3U:0U));
        }
    }
    for(bool spatial:{false,true}) {
        std::vector<behavior::Spawn> children;
        behavior::Actor parent;parent.Reset({0,0,5.71428585F},1900,7);parent.visual=stage_assets::kBoss;
        F::Behaviors::ConfigurePractice(parent,spatial);parent.Start(F::Behaviors::FourOfAKind(parent));auto bullets=std::make_unique<BulletSystem>();
        for(int tick=0;tick<434;++tick) {parent.Update(1.0F/120,{},*bullets);children.insert(children.end(),parent.spawnEvents.begin(),parent.spawnEvents.end());}
        assert(children.size()==3);
        for(int index=-1;index<3;++index) {
            std::vector<Bullet> expected;Vector3 position{};float random=0;
            for(int fps:{120,30,60,144,240}) {
                behavior::Actor actor;
                if(index<0) {actor.Reset({0,0,5.71428585F},1900,7);actor.visual=stage_assets::kBoss;F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::FourOfAKind(actor));}
                else actor.Activate(children[index]);
                actor.SetRank(32);bullets->Reset();
                for(int frame=0;frame<20*fps;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
                std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const auto next=actor.Random(1);
                assert(actor.phaseFrames==1200&&!bullets->Pool().DroppedSpawns());
                // 保留未积分的逐发快照，显示帧批处理也必须保持每颗弹的出生点、方向与随机流。
                if(fps==120) {position=actor.position;random=next;expected=actual;}
                else {
                    Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);assert(next==random&&actual.size()==expected.size());
                    for(std::size_t n=0;n<actual.size();++n) {
                        Near(actual[n].position.x,expected[n].position.x);Near(actual[n].position.y,expected[n].position.y);Near(actual[n].position.z,expected[n].position.z);
                        Near(actual[n].Velocity().x,expected[n].Velocity().x);Near(actual[n].Velocity().y,expected[n].Velocity().y);Near(actual[n].Velocity().z,expected[n].Velocity().z);
                        assert(actual[n].nativeType==expected[n].nativeType&&actual[n].nativeColor==expected[n].nativeColor);
                    }
                }
            }
        }
    }
}
void TestLaevateinMissingSwords() {
    constexpr float scale=20.0F/448;
    auto bullets=std::make_unique<BulletSystem>();
    // 自机弹占满项目池，确保准备清敌不会释放容量；参考侧独立注入创建器返回空，只验证缺引用的控制流。
    for(std::size_t n=0;n<BulletSystem::kCapacity;++n)assert(bullets->Spawn(BulletOwner::Player,{}, {},0.1F,WHITE,0));
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5.71428585F},1200,7);
    F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::Laevatein(actor));
    stage_extra::reference::FlandreReference source(44,{192,96,0},7,16,1200);source.createBeams=false;
    checkedSub=44;checkedRank=16;
    for(int frame=0;frame<140;++frame) {
        checkedFrame=frame;source.Tick({192,360,0});actor.Update(1.0F/60,{0,0,-6},*bullets);
        Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
        if(actor.animationScript!=source.animation||actor.soundEvents!=source.sounds)
            std::cerr<<"Laevatein missing swords frame "<<frame<<" animation "<<actor.animationScript<<" vs "<<source.animation<<" sounds "<<actor.soundEvents.size()<<" vs "<<source.sounds.size()<<'\n';
        assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&source.beams.empty());
    }
    assert(actor.position.x<0&&actor.phaseFrames==140&&bullets->Pool().DroppedSpawns()==3);
}
void TestRoleHandoff() {
    F inspector;
    for(unsigned shot=0;shot<4;++shot)for(const auto& spec:cases) {
        const auto p=inspector.DescribeSkill(spec.behavior,static_cast<ShotType>(shot));
        assert(p.health==spec.health-spec.reserve&&p.reserveHealth==spec.reserve&&p.timeoutFrames==1800&&p.entranceHealth==(spec.sub==38?15000:0)&&p.spellCard==0);
        for(auto d:{native_combat::Difficulty::Easy,native_combat::Difficulty::Hard,native_combat::Difficulty::Lunatic}) {
            bool rejected=false;try {inspector.DescribeSkill(spec.behavior,static_cast<ShotType>(shot),d);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
        }
    }
    for(bool damage:{true,false}) {
        // 只验证已实现段的独立交接，不把本夹具的占位退场当作源完整遭遇验收。
        bosses::StartConfig config{.position={-10,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .intro=F::Behavior::Entrance,.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::FirstNonspell,F::Behavior::SecondNonspell,F::Behavior::ThirdNonspell,F::Behavior::FourthNonspell,
                F::Behavior::FifthNonspell,F::Behavior::SixthNonspell,F::Behavior::SeventhNonspell,F::Behavior::EighthNonspell},
                {{{0,0.125F,WHITE},{1,0.125F,WHITE},{2,0.125F,WHITE},{3,0.125F,WHITE},{4,0.125F,WHITE},{5,0.125F,WHITE},{6,0.125F,WHITE},{7,0.125F,WHITE}}}}}};
        F boss;boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();
        for(int tick=0;tick<300;++tick)boss.Update(1.0F/120,{2,3,-6},*bullets);
        assert(boss.health==15000&&!boss.CanTakeDamage()&&boss.attack==0);
        // 三维入场把平面路程等长分配到高度，终点不要求仍落在源平面X；总路程不能多走或丢步。
        Near(std::hypot(boss.position.x+10,boss.position.y),10);Near(boss.position.z,5.71428585F);
        assert(boss.AuxiliaryAnimations()[0].visual==stage_assets::kOrb);
        boss.ResumeCombat();assert(boss.health==10000&&!boss.CanTakeDamage());
        for(int index=0;index<7;++index) {
            bool advanced=false;
            for(int tick=0;tick<3700;++tick) {
                bullets->Reset();const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
                if(damage&&tick>240&&boss.CanTakeDamage())boss.health=cases[index].reserve-1;
                if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.AdvanceSkill());advanced=true;break;}
            }
            assert(advanced&&boss.attack==index+2&&boss.health==cases[index+1].health&&!boss.CanTakeDamage());
            boss.Update(1.0F/120,{2,3,-6},*bullets);assert(!boss.CanTakeDamage());
        }
    }
}
// 完整配置逐次验证实际技能身份、剩余血条与分支；普通攻击先运行至开放受伤，准备使用真实回调完成。
void TestFullConfigurations() {
    const F::Behavior expected[]{F::Behavior::FirstNonspell,F::Behavior::CranberryTrap,F::Behavior::SecondNonspell,F::Behavior::Laevatein,F::Behavior::ThirdNonspell,F::Behavior::FourOfAKind,
        F::Behavior::FourthNonspell,F::Behavior::KagomeKagome,F::Behavior::FifthNonspell,F::Behavior::MazeOfLove,F::Behavior::SixthNonspell,F::Behavior::StarbowBreak,
        F::Behavior::SeventhNonspell,F::Behavior::Catadioptric,F::Behavior::EighthNonspell,F::Behavior::ClockOfThePast,F::Behavior::SurvivalPrelude,F::Behavior::AndThenWillThereBeNone,
        F::Behavior::FinalPrelude,F::Behavior::RipplesOf495Years};
    for(int shot=0;shot<4;++shot)for(int factor:{1,2})for(bool zero:{false,true})for(bool encore:{false,true}) {
        auto config=encore?stage_extra::FlandreEncoreConfig(static_cast<ShotType>(shot)):stage_extra::FlandreMainConfig(static_cast<ShotType>(shot));
        F boss(factor==1?behavior::UpdateRate::Hz60:behavior::UpdateRate::Hz120,false);auto bullets=std::make_unique<BulletSystem>();
        assert(config.phases.size()==(encore?3:10));
        if(!encore)for(int i=0;i<20;++i)assert(config.phases[i/2].skills[i%2].id==bosses::BehaviorId(expected[i]));
        boss.StartBoss(config);auto* identity=&boss;boss.Update(1.0F/(60*factor),{},*bullets);
        assert(!boss.CurrentSkill()&&!boss.CanTakeDamage());boss.ResumeCombat();int count=0;
        while(boss.CurrentSkill()) {
            assert(&boss==identity&&++count<=20);const int phase=boss.phase,skill=boss.skill,attack=boss.attack;
            const auto& slot=config.phases[phase-1].skills[skill-1];const auto* definition=boss.CurrentSkill();
            const bool preparation=slot.omitHealthBarSegment;const auto bar=boss.HealthBar();
            assert(bar.remainingBars==static_cast<int>(config.phases.size())-phase&&bar.count==(encore||phase>=9?1:2));
            if(preparation)assert(bar.segments[0].capacity==1&&bar.segments[0].remaining==1);
            if(preparation&&!zero) {
                int ticks=0;while(!boss.Update(1.0F/(60*factor),{},*bullets))assert(++ticks<220*factor);
                assert(ticks==(encore?210:90)*factor);assert(boss.AdvanceSkill()&&boss.attack==attack+1);continue;
            }
            // 零HP路线专门覆盖跳过预留符卡；常规路线以正阈值进入符卡，生存符受伤路径单独记录为失败。
            if(preparation)boss.Update(1.0F/(60*factor),{},*bullets);
            else for(int tick=0;tick<130*factor&&!boss.CanTakeDamage();++tick)boss.Update(1.0F/(60*factor),{},*bullets);
            const bool skipped=zero&&(preparation||(!encore&&skill==1));
            boss.health=skipped||definition->reserveHealth<=0?0:definition->reserveHealth-1;
            assert(boss.AdvanceSkill());
            if(skipped&&phase<static_cast<int>(config.phases.size()))assert(boss.phase==phase+1&&boss.skill==1);
            if(!boss.CurrentSkill())break;
        }
        assert(boss.active&&boss.Defeated()&&!boss.RetirementFromTimeout());
        int ticks=0;while(!boss.Update(1.0F/(60*factor),{},*bullets))assert(++ticks<65*factor);
        if(ticks!=60*factor)std::cerr<<"Full config exit shot="<<shot<<" factor="<<factor<<" zero="<<zero<<" encore="<<encore<<" ticks="<<ticks<<" timer="<<boss.skillTimer<<"\n";
        assert(ticks==60*factor&&!boss.AdvanceSkill()&&!boss.active);
        assert(count==(zero?(encore?3:10):(encore?5:20)));
    }
    for(auto difficulty:{native_combat::Difficulty::Easy,native_combat::Difficulty::Hard,native_combat::Difficulty::Lunatic}) {
        bool rejected=false;try {stage_extra::FlandreMainConfig(ShotType::HomingAmulet,difficulty);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
}

}
/// @brief 可独立运行的Extra芙兰朵露已接入段回归；不代表完整关卡/外部粒子验收。
void RunStageExtraFlandreTests() {
    TestFullConfigurations();TestSource();TestClocksAndDisplayRates();TestRoleHandoff();TestCranberrySource();TestCranberryEmitterDisplayRates();TestCranberryClocksAndBomb();TestLaevateinSource();TestLaevateinClocksAndDisplayRates();TestLaevateinMissingSwords();TestFourOfAKindSource();TestFourOfAKindClocksAndDisplayRates();
    std::cout<<"Validated Extra Flandre entrance, eight nonspells, Cranberry Trap, Laevatein and Four of a Kind source diffs, clocks and Bomb recovery\n";
}

/// @brief 独立核验主战/条件后继配置、血条、真实准备及阈值/归零分流；不替代正式Game流程。
void RunStageExtraConfigurationTests() {TestFullConfigurations();}

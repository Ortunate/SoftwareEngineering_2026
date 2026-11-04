// 本文件以97…100源指令独立验证额外入场、本体显现、11个演出对象及内容时钟，不把后继战斗视作已实现。
#include "stage_extra_flandre_reference.hpp"
#include "reference_external_effects.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>
namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;using Ref=stage_extra::reference::FlandreReference;
constexpr float scale=20.0F/448;
int frame=0;
void Near(float actual,float expected) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>5e-4F) {
        std::cerr<<"Extra encore entrance frame "<<frame<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
void ComparePosition(Vector3 actual,Vector3 expected) {
    const auto p=World(expected);Near(actual.x,p.x);Near(actual.y,p.y);Near(actual.z,p.z);
}
void TestSource() {
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset({-8.5714283F,0,10},15000,seed);actor.visual=stage_assets::kBoss;
        F::Behaviors::ConfigurePractice(actor,false);actor.SetRank(rank);actor.Start(F::Behaviors::EncoreEntrance(actor));
        Ref source(97,{0,0,0},seed,rank,15000);
        std::vector<behavior::Spawn> children;std::vector<Ref::Spawn> originals;
        for(frame=0;frame<2205;++frame) {
            source.Tick({192,360,0});actor.Update(1.0F/60,{0,0,-6},*bullets);
            ComparePosition(actor.position,source.position);
            assert(actor.damageable==source.damageable&&actor.contactEnabled==source.contact&&actor.visible==source.visible);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.health==source.health);
            assert(actor.visible==(frame>=100)&&!actor.finished&&actor.spawnEvents.size()==source.spawns.size());
            assert(actor.auxiliaryAnimations[0].script==source.auxiliaryAnimation&&source.auxiliaryAnimation==18);
            assert(!bullets->Pool().ActiveCount()&&source.emissions.empty()&&actor.dropEvents.empty()&&source.drops.empty());
            for(std::size_t n=0;n<source.spawns.size();++n) {
                const auto& a=actor.spawnEvents[n];const auto& e=source.spawns[n];
                assert(behavior::Valid(a)&&a.sourceHealth==e.health&&a.sourceScore==e.score&&a.sourceItem==e.item);
                assert(a.seed==seed+children.size()+1&&a.visual==stage_assets::kBoss&&a.visible==(e.sub!=100)&&a.damageable);
                assert(a.stageClearResponse==(e.sub==100?behavior::StageClearResponse::Remove:behavior::StageClearResponse::Defeat));
                ComparePosition(a.position,e.position);Near(a.hitRadius,6*scale);
                children.push_back(a);originals.push_back(e);
            }
        }
        assert(children.size()==11&&actor.Random(1)==source.Random(1));
        for(std::size_t index=0;index<children.size();++index) {
            const auto& spawn=children[index];const auto& raw=originals[index];
            behavior::Actor child(behavior::UpdateRate::Hz60);child.Activate(spawn);
            Ref original(raw.sub,raw.position,spawn.seed,rank,raw.health);int effects=0;
            for(frame=0;frame<=90;++frame) {
                original.Tick({192,360,0});child.Update(1.0F/60,{0,0,-6},*bullets);
                ComparePosition(child.position,original.position);CompareEffects(child,original);effects+=original.effectRequests;
                assert(child.animationScript==original.animation&&child.damageable==original.damageable&&child.contactEnabled==original.contact);
                assert(child.visible==original.visible&&child.interactable==original.interactable&&child.health==original.health);
                assert(child.finished==!original.active&&!bullets->Pool().ActiveCount());
                if(child.finished)break;
            }
            assert(frame==(raw.sub==100?80:90)&&effects==(raw.sub==100?240:0));
            assert(child.Random(1)==original.Random(1));
        }
    }
}
void TestHalfFramesAndDisplayRates() {
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        const int factor=rate==behavior::UpdateRate::Hz120?2:1;
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(rate);
        actor.Reset({},15000,7);actor.visual=stage_assets::kBoss;F::Behaviors::ConfigurePractice(actor,true);
        actor.Start(F::Behaviors::EncoreEntrance(actor));int births=0;behavior::Spawn hidden;
        for(int tick=0;tick<=152*factor;++tick) {
            actor.Update(1.0F/(60*factor),{},*bullets);births+=static_cast<int>(actor.spawnEvents.size());
            if(tick==0)hidden=actor.spawnEvents.front();
            assert(actor.visible==(tick>=100*factor));
            if(!actor.spawnEvents.empty())assert(tick==0||tick==30*factor||tick==38*factor||tick==46*factor||tick==54*factor||tick==62*factor);
        }
        assert(births==11&&!actor.CanTakeDamage()&&actor.visible);
        // 独立检查隐藏对象在半程只余1/16路程，避免生产与参考侧一起把58误写为三次缓出。
        behavior::Actor child(rate);child.Activate(hidden);
        for(int tick=0;tick<=60*factor;++tick) {
            child.Update(1.0F/(60*factor),{},*bullets);
            if(tick==30*factor)Near(child.position.z,4.82142857F);
            if(tick==60*factor)Near(child.position.z,5.71428585F);
        }
    }
    for(bool spatial:{false,true})for(int fps:{30,60,120,144,240}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({},15000,7);
        actor.visual=stage_assets::kBoss;F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::EncoreEntrance(actor));
        std::vector<behavior::Spawn> children;
        for(int tick=0;tick<40*fps;++tick) {
            actor.Update(1.0F/fps,{},*bullets);
            children.insert(children.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());
        }
        assert(children.size()==11&&actor.phaseFrames==2400&&actor.visible&&actor.animationScript==166&&!actor.CanTakeDamage());
        Near(actor.position.x,0);Near(actor.position.y,0);Near(actor.position.z,5.71428585F);
        Ref random(97,{},7,16,15000);assert(actor.Random(1)==random.Random(1));
    }
}
void TestPoolCleanupAndRoleContract() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor parent;
    parent.Reset({},15000,7);parent.visual=stage_assets::kBoss;F::Behaviors::ConfigurePractice(parent,false);
    parent.Start(F::Behaviors::EncoreEntrance(parent));std::vector<behavior::Spawn> children;
    for(int tick=0;tick<=62*2;++tick) {
        parent.Update(1.0F/120,{},*bullets);children.insert(children.end(),parent.spawnEvents.begin(),parent.spawnEvents.end());
    }
    behavior::ActorPool<11> pool;
    for(const auto& spawn:children)assert(pool.Spawn(spawn));
    assert(!pool.Spawn(children[0])&&pool.DroppedSpawns()==1);
    for(int tick=0;tick<=90*2;++tick)pool.ForEachActive([&](behavior::Actor& actor){actor.Update(1.0F/120,{},*bullets);});
    int alive=0;pool.ForEachActive([&](const behavior::Actor&){++alive;});assert(alive==0);
    auto* hidden=pool.Spawn(children[0]);assert(hidden&&!hidden->visible);
    hidden->Update(1.0F/120,{},*bullets);pool.Clear();assert(pool.DroppedSpawns()==0);
    auto* visible=pool.Spawn(children[1]);assert(visible&&visible->visible&&visible->damageable&&!visible->finished);
    // 入场是intro协议，不能通过DescribeSkill直接当符卡开放；这里只验证与已接入技能的通用放行能力。
    for(int shot=0;shot<4;++shot) {
        F boss;bool rejected=false;
        try {boss.DescribeSkill(F::Behavior::EncoreEntrance,static_cast<ShotType>(shot));}catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
        bosses::StartConfig config{.position={},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.intro=F::Behavior::EncoreEntrance,.defeated=F::Behavior::FinalExit,.retreat=F::Behavior::FinalExit,
            .phases={{{F::Behavior::FirstNonspell},{{{0,1,WHITE}}}}}};
        boss.StartBoss(config);int births=0;
        for(int tick=0;tick<2400;++tick) {
            assert(!boss.Update(1.0F/120,{},*bullets));births+=static_cast<int>(boss.SpawnEvents().size());
            assert(!boss.CurrentSkill()&&boss.attack==0&&!boss.CanTakeDamage());
        }
        assert(births==11&&boss.BodyVisible());boss.ResumeCombat();assert(boss.CurrentSkill()&&boss.attack==1&&!boss.CanTakeDamage());
    }
}
}
/// @brief 独立入口只验已实现入场；132…134战斗与正式Extra流程另行验收。
void RunStageExtraEncoreEntranceTests() {
    TestSource();TestHalfFramesAndDisplayRates();TestPoolCleanupAndRoleContract();
    std::cout<<"Validated Extra encore entrance, eleven source actors, clocks, display rates and lifecycle\n";
}

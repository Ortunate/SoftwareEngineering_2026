// 本文件对照Extra独立源程序验收已接入普通配方；不代表Extra整关或Boss已可玩。
#include "stage_extra_ordinary_reference.hpp"
#include "stages/stage_extra/behaviors.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace koumakyou;
struct Case {int sub;behavior::Pattern pattern;};
const std::array<Case,15> cases{{
    {0,stage_extra::kSingleCurve},{5,stage_extra::kLayeredFanTurn},{6,stage_extra::kTwinFanTurn},
    {7,stage_extra::kCrossingFanTurn},{8,stage_extra::kTightCurve},{9,stage_extra::kWideCurve},
    {1,stage_extra::kBlueSpiralRings},{2,stage_extra::kPairedBlueSpiralRings},
    {3,stage_extra::kPairedRedSpiralRings},{4,stage_extra::kExpandingCrystalRings},
    {10,stage_extra::kRedRingEmitter},{11,stage_extra::kRandomRedEmitter},{12,stage_extra::kBlueFanEmitter},
    {13,stage_extra::kBlueLayerEmitter},{14,stage_extra::kLayeredRedEmitter},
}};
int checkedSub=-1,checkedFrame=-1,checkedRank=-1;
void Near(float actual,float expected,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Extra sub "<<checkedSub<<" frame "<<checkedFrame<<" rank "<<checkedRank
                 <<" mismatch "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}

void TestOrdinarySource() {
    constexpr float scale=20.0F/448,speedScale=scale*60;
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32})for(bool mirror:{false,true})for(const auto& c:cases) {
        auto pattern=c.pattern;pattern.spatialShots=false;
        behavior::Spawn spawn{.position={0,0,10-32*scale},.pattern=pattern,.mirrored=mirror,.sourceHealth=100,
            .seed=seed,.visual=stage_assets::kFairy,.hitRadius=14*scale,.retireOutside=false};
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);stage_extra::ConfigureBehavior(actor,false);
        stage_extra::reference::OrdinaryReference source(c.sub,{192,32,0},seed,rank,mirror);
        // 最长扩张环串需覆盖538帧后的恢复；其余曲线/停留款沿用400帧，越界回收另由Spawn负责。
        for(int frame=0;frame<(c.sub>=1&&c.sub<=4 ? 600 : 400);++frame) {
            checkedSub=c.sub;checkedFrame=frame;checkedRank=rank;
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
            assert(actor.damageable==source.damageable);
            assert(actor.interactable==source.interactable);
            assert((actor.animationInterrupt ? actor.animationInterrupt->label : -1)==source.interrupt);
            const auto& auxiliary=actor.auxiliaryAnimations[0];
            assert(auxiliary.script==source.auxiliaryAnimation&&(auxiliary.interrupt?auxiliary.interrupt->label:-1)==source.auxiliaryInterrupt);
            assert(actor.finished==!source.active);Near(actor.hitRadius,source.radius*scale,1e-5F);
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            if(actual.size()!=source.emissions.size())std::cerr<<"Extra sub "<<c.sub<<" frame "<<frame<<" rank "<<rank<<" shots "<<actual.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                Near(b.Velocity().x,e.velocity.x*speedScale,1e-4F);Near(b.Velocity().y,0,1e-5F);Near(b.Velocity().z,-e.velocity.y*speedScale,1e-4F);
            }
        }
        assert(actor.Random(1)==source.Random(1));
        if(c.sub>=1&&c.sub<=4) {
            assert(actor.Defeat());source.Defeat();bullets->Reset();
            source.Tick({245,360,0});actor.Update(1.0F/60,{},*bullets);
            assert(actor.dropEvents==source.drops&&source.drops==std::vector<int>{8});
            assert(actor.finished&&!source.active);
        }
    }
    for(int sub:{-1,15,16,36,105}) {
        bool rejected=false;
        try {stage_extra::reference::OrdinaryReference invalid(sub,{},7,16,false);}
        catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    }
}

void TestDisplayRatesAndLifetime() {
    for(const auto& c:cases) {
        std::vector<Bullet> expected;Vector3 position{};float random=0;double phase=0;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;
            behavior::Spawn spawn{.position={0,0,8},.pattern=c.pattern,.sourceHealth=100,.seed=7,
                .visual=stage_assets::kFairy,.retireOutside=false};
            actor.Activate(spawn);actor.SetRank(32);stage_extra::ConfigureBehavior(actor,true);
            for(int frame=0;frame<fps*6;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
            const float next=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=next;phase=actor.phaseFrames;}
            else {
                assert(actual.size()==expected.size()&&next==random&&actor.phaseFrames==phase);
                Near(actor.position.x,position.x);Near(actor.position.z,position.z);
                for(std::size_t n=0;n<actual.size();++n) {
                    Near(actual[n].Velocity().x,expected[n].Velocity().x);Near(actual[n].Velocity().y,expected[n].Velocity().y);Near(actual[n].Velocity().z,expected[n].Velocity().z);
                }
            }
            assert(!bullets->Pool().DroppedSpawns());
        }
        // 独立检查寿命，避免子协程从零计时使寿命多出前置转弯或发射时间。
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;
        behavior::Spawn spawn{.pattern=c.pattern,.sourceHealth=100,.seed=7,.visual=stage_assets::kFairy,.retireOutside=false};actor.Activate(spawn);
        const int end=c.sub==5 ? 1000 : c.sub==10 ? 148 : c.sub==11 ? 144 : c.sub>=12 ? 180 : 10000;
        actor.Update((end-1)/60.0F,{},*bullets);assert(!actor.finished);
        actor.Update(1.0F/60,{},*bullets);assert(!actor.finished);
        actor.Update(1.0F/60,{},*bullets);assert(actor.finished);
    }
}

void TestEmitterDefeat() {
    for(const auto& c:cases)if(c.sub>=10)for(int defeatFrame:{1,40}) {
        auto pattern=c.pattern;pattern.spatialShots=false;
        behavior::Spawn spawn{.pattern=pattern,.sourceHealth=100,.seed=7,.visual=stage_assets::kFairy,.retireOutside=false};
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);stage_extra::ConfigureBehavior(actor,false);
        stage_extra::reference::OrdinaryReference source(c.sub,{192,224,0},7,16,false);
        auto bullets=std::make_unique<BulletSystem>();
        for(int n=0;n<defeatFrame;++n) {source.Tick({245,360,0});actor.Update(1.0F/60,{},*bullets);}
        assert(actor.Defeat());source.Defeat();
        for(int frame=0;frame<=30;++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{},*bullets);
            assert(actor.dropEvents==source.drops);
            if(frame==0)assert(source.drops==std::vector<int>{8});
            assert(actor.animationInterrupt&&actor.animationInterrupt->label==source.interrupt);
            assert(actor.auxiliaryAnimations[0].interrupt&&actor.auxiliaryAnimations[0].interrupt->label==source.auxiliaryInterrupt);
            assert(actor.finished==!source.active&&!actor.CanContactPlayer());
            assert(bullets->Pool().ActiveCount()==0);
        }
    }
}
}
/// @brief 验收Extra十五种普通配方的60 Hz源差分与默认120 Hz显示帧率/寿命；辅助槽图形与整关另行接入。
void RunStageExtraTests() {TestOrdinarySource();TestDisplayRatesAndLifetime();TestEmitterDefeat();}

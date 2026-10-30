// 本文件独立核对Extra全部普通出生、主战前异步等待及条件后继依赖；正式入口仍独立验收。
#include "stage_extra_ordinary_reference.hpp"
#include "stages/stage_extra/timeline.hpp"
#include "stages/stage_extra/boss_config.hpp"
#include "stages/stage_extra/behaviors.hpp"
#include "stages/stage_extra/ids.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

namespace {
using namespace koumakyou;
using namespace koumakyou::stage_extra;
void Near(float actual,float expected) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>5e-4F) {
        std::cerr<<"Extra timeline mismatch "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
/// @brief 原生数值目录是独立源出生夹具；只过滤实际普通入口，不复用生产表生成期望。
std::vector<const native_combat::HealthRecord*> OrdinaryBirths() {
    std::vector<const native_combat::HealthRecord*> result;
    for(const auto& record:native_combat::Stage(7)->health)
        if(record.scope==native_combat::HealthScope::TimelineSpawn&&record.targetSub<15)result.push_back(&record);
    assert(result.size()==338);return result;
}
void TestBirths() {
    const auto births=OrdinaryBirths();const auto events=ExtraMainTimeline();
    assert(events.size()==353&&events.data()==ExtraMainTimeline().data());
    constexpr std::array<behavior::Entry,15> entries{SingleCurve,BlueSpiralRings,PairedBlueSpiralRings,
        PairedRedSpiralRings,ExpandingCrystalRings,LayeredFanTurn,TwinFanTurn,CrossingFanTurn,TightCurve,WideCurve,
        RedRingEmitter,RandomRedEmitter,BlueFanEmitter,BlueLayerEmitter,LayeredRedEmitter};
    constexpr float scale=20.0F/448;
    std::set<unsigned> seeds;std::array<int,15> counts{};std::size_t index=0;int previousFrame=4643;
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto& event:events) {
        const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
        const auto& source=*births.at(index++);const auto& spawn=request->actor;const int sub=source.targetSub;
        assert(behavior::Valid(spawn)&&request->count==1&&request->interval==0);
        assert(spawn.pattern.routine==entries.at(sub)&&spawn.pattern.spatialShots);
        assert(spawn.sourceHealth==*source.value&&spawn.sourceScore==source.score&&spawn.sourceItem==source.item);
        assert(spawn.mirrored==(source.opcode==2)&&spawn.randomX==(source.opcode==4));
        assert(spawn.seed!=0&&seeds.insert(spawn.seed).second&&spawn.position.y==0);
        assert(spawn.visual==(sub==0||sub==8||sub==9?kFairy:sub>=5&&sub<=7?kDarkFairy:kOrb));
        assert(spawn.bulletBase==kNativeBulletBase&&spawn.laserBase==kNativeLaserBase&&spawn.defeatBase==kNativeDefeat);
        assert(spawn.retireOutside&&spawn.retirementHeights);
        if(source.frame<4640)assert(event.atFrame==source.frame&&!event.afterEndOf);
        else {
            assert(!event.atFrame);Near(event.delay,(source.frame-previousFrame)/60.0F);
            assert(event.afterEndOf==(source.frame==4763?std::optional<StageEventId>{1}:std::nullopt));
            previousFrame=source.frame;
        }
        Near(spawn.position.x,spawn.randomX?0:(source.position.x-192)*scale);
        Near(spawn.position.z,10-source.position.y*scale);++counts.at(sub);
        auto flat=spawn;flat.pattern.spatialShots=false;flat.retireOutside=false;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(flat);
        stage_extra::reference::OrdinaryReference reference(sub,source.position,spawn.seed,16,spawn.mirrored);
        if(spawn.randomX) {
            // 项目出生采样是独立的种子映射，不前移演员的攻击随机流；不宣称原生全局RNG等价。
            const float fraction=static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F;
            reference.position.x=fraction*384;
            Near(actor.position.x,(reference.position.x-192)*scale);
        }
        for(int frame=0;frame<160;++frame) {
            bullets->Reset();reference.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            Near(actor.position.x,(reference.position.x-192)*scale);Near(actor.position.z,10-reference.position.y*scale);
            assert(actor.animationScript==reference.animation&&actor.damageable==reference.damageable);
            assert(actor.interactable==reference.interactable&&actor.finished==!reference.active);
            assert(bullets->Pool().ActiveCount()==reference.emissions.size()&&!bullets->Pool().DroppedSpawns());
        }
        assert(actor.Random(1)==reference.Random(1));
    }
    assert(index==338&&(counts==std::array<int,15>{72,3,2,2,1,52,15,16,72,72,7,6,6,6,6}));
}
void TestWaits() {
    const auto births=OrdinaryBirths();
    for(int hz:{60,120})for(int midEnd:{5840,12000})for(int character:{0,1}) {
        StageDirector director(ExtraMainTimeline());std::size_t index=0;int mainStart=-1,mainBoss=-1,waiting=-1;
        std::vector<int> resumes;bool midDialogue=false;const int signal=character?2065:2066,end=character?11106:15923;
        // 源4643等待演员删除，随后时间轴前进到4763；等待对白结束不能取代完整退场。
        const int offset=midEnd-4643,expectedMain=8493+offset;
        for(int tick=1;tick<=(expectedMain+end+3)*(hz/60);++tick) {
            const double frame=static_cast<double>(tick)*60/hz;
            director.Update(1.0F/hz,[&](const StageEvent& event) {
                if(std::holds_alternative<SpawnActors>(event.action)) {
                    const auto& source=*births.at(index++);
                    assert(std::abs(frame-(source.frame+(source.frame>=4763?offset:0)))<1e-5);return true;
                }
                if(const auto* boss=std::get_if<StartBoss>(&event.action)) {
                    if(boss->battle==&kPatchouliMidboss)assert(frame==4640&&event.id==1);
                    else {assert(boss->battle==&kFlandreMain&&event.id==4);mainBoss=static_cast<int>(frame);}
                    return false;
                }
                if(const auto* dialogue=std::get_if<StartDialogue>(&event.action)) {
                    assert(!dialogue->afterBattle&&dialogue->clearPolicy==DialogueClearPolicy::PreserveWorld);
                    if(dialogue->scene==DialogueScene::Midboss) {assert(frame==4640);midDialogue=true;}
                    else {mainStart=static_cast<int>(frame);assert(mainStart==expectedMain);}
                    return false;
                }
                if(const auto* wait=std::get_if<WaitDialogueSignal>(&event.action)) {waiting=wait->signal;return false;}
                assert(std::holds_alternative<ResumeBoss>(event.action));resumes.push_back(static_cast<int>(frame));return true;
            });
            if(frame==5013) {assert(midDialogue&&director.EndEvent(2));}
            if(frame==midEnd) {assert(director.EndEvent(1)&&!director.EndEvent(1));}
            if(frame==expectedMain+signal) {assert(waiting==1&&director.EndEvent(6));}
            if(frame==expectedMain+end) {assert(waiting==0&&director.EndEvent(3)&&director.EndEvent(7));}
        }
        assert(index==338&&mainStart==expectedMain&&mainBoss==expectedMain+signal+1);
        assert((resumes==std::vector<int>{5014,expectedMain+end+1}));
        // 本轮只推进到主战结束前；后继仍必须等待完整退场，不能因对白已完成而提前启动。
        assert(director.EndEvent(4)&&!director.EndEvent(4));
    }
}
void TestPostfightDependencies() {
    const auto tail=ExtraMainTimeline().last(7);
    constexpr std::array<std::optional<StageEventId>,7> ids{8,9,10,11,12,std::nullopt,13};
    constexpr std::array<std::optional<StageEventId>,7> dependencies{4,8,std::nullopt,10,std::nullopt,12,11};
    constexpr std::array<int,7> frames{1,2,1,1,1,1,1};
    for(std::size_t i=0;i<tail.size();++i) {
        assert(tail[i].id==ids[i]&&tail[i].afterEndOf==dependencies[i]&&!tail[i].atFrame);
        Near(tail[i].delay,frames[i]/60.0F);
    }
    for(int i:{0,1,6}) {
        const auto& dialogue=std::get<StartDialogue>(tail[i].action);
        assert(dialogue.afterBattle==(i!=1)&&dialogue.scene==(i==0?DialogueScene::FinalBoss:DialogueScene::EncoreBoss));
        assert(dialogue.clearPolicy==DialogueClearPolicy::PreserveWorld);
    }
    assert(std::get<WaitDialogueSignal>(tail[2].action).signal==1);
    assert(std::get<StartBoss>(tail[3].action).battle==&kFlandreEncore);
    assert(std::get<WaitDialogueSignal>(tail[4].action).signal==0&&std::holds_alternative<ResumeBoss>(tail[5].action));
}
}
/// @brief 逐出生核对独立源单位轨迹及60/120 Hz的两角色信号、完整退场和等待后帧距。
void RunStageExtraTimelineTests() {TestBirths();TestWaits();TestPostfightDependencies();}

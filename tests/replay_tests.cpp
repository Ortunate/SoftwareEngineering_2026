// 本文件验证录像协议与关卡集成边界，防止未支持的关卡或损坏输入进入播放。
#include "replay/replay.hpp"
#include "app/spell_practice.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <limits>
#include <fstream>

int main() {
    using namespace koumakyou;
    replay::Data data;
    replay::BeginSession(data,{.seed=123,.phase=2});
    replay::BeginStage(data,{});
    data.segments[0].seed=123;
    data.frames.push_back({.dt=1.0F/120,.player={.fire=true,.aimAssistEnabled=false}});
    data.frames.push_back({.dt=1.0F/240,.player={.mouseDelta={1,-2},.focus=true,.aimAssistEnabled=true},.confirm=true});
    const replay::Result result{.score=4500,.lives=2,.power=8,.won=true};
    replay::FinishSession(data,result);
    std::string error;
    const auto directory=std::filesystem::temp_directory_path()/
        ("koumakyou_replay_merge_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(replay::CanPlay(data,error));
    auto unsupported=data;
    unsupported.segments[0].stage="unknown";
    assert(!replay::CanPlay(unsupported,error));
    unsupported=data;unsupported.segments[0].difficulty=replay::Difficulty::Extra;
    assert(!replay::CanPlay(unsupported,error));
    // 单关练习覆盖四难度与全部可玩关卡，Easy六面依目录拒绝。
    for(int difficulty=0;difficulty<4;++difficulty)for(int stage=1;stage<=6;++stage) {
        auto practice=data;practice.header.practice=true;practice.route="stage-practice";
        auto& segment=practice.segments.front();segment.stage="stage"+std::to_string(stage);
        segment.difficulty=static_cast<replay::Difficulty>(difficulty);segment.rank=difficulty==0?20:32;
        segment.power=stage==1?8:stage==2?64:128;
        assert(replay::CanPlay(practice,error)==!(stage==6 && difficulty==0));
        auto wrongPower=practice;wrongPower.segments[0].power=stage==1?64:8;
        assert(!replay::CanPlay(wrongPower,error));
    }
    // 每张符卡使用原典编号，不能以相同phase误认成另一关或另一难度的符卡。
    for(const auto& card:spell::Cards())for(unsigned shot=0;shot<4;++shot) {
        auto practice=data;practice.header.practice=practice.header.spell=true;
        practice.header.phase=card.phase;practice.header.shot=shot;practice.route="spell-practice";
        practice.character=shot<2?"reimu":"marisa";
        practice.shot=std::array<const char*,4>{"reimu-a","reimu-b","marisa-a","marisa-b"}[shot];
        auto& segment=practice.segments.front();segment.stage=card.stage==6?"extra":"stage"+std::to_string(card.stage+1);
        segment.difficulty=static_cast<replay::Difficulty>(card.difficulty);
        segment.rank=card.stage==6?18:card.difficulty==0?20:32;
        segment.lives=segment.bombs=0;segment.power=card.number<=2?32:card.number<=9?64:128;segment.entry="spell";segment.target="spell-"+std::to_string(card.number);
        assert(replay::CanPlay(practice,error));
        auto wrongPower=practice;wrongPower.segments[0].power=segment.power==128?64:128;
        assert(!replay::CanPlay(wrongPower,error));
        auto wrongStock=practice;wrongStock.segments[0].bombs=1;assert(!replay::CanPlay(wrongStock,error));
        wrongStock=practice;wrongStock.segments[0].lives=1;assert(!replay::CanPlay(wrongStock,error));
        replay::Entry entry;assert(replay::ResolveEntry(segment,entry));
        assert(entry.spellNumber==card.number && entry.stage==card.stage && entry.difficulty==card.difficulty);
        segment.target="spell-999";assert(!replay::CanPlay(practice,error));
    }
    auto extra=data;extra.route="extra";extra.segments[0].stage="extra";
    extra.segments[0].difficulty=replay::Difficulty::Extra;extra.segments[0].rank=18;
    assert(replay::CanPlay(extra,error));
    // 整轮主线按关卡连续拼接；Easy五关，其余六关。结果/入口库存为独立构造的固定基线。
    for(int difficulty=0;difficulty<4;++difficulty) {
        replay::Data campaign;replay::BeginSession(campaign,{.seed=123});campaign.route="campaign";
        const int stages=difficulty==0?5:6;
        for(int stage=1;stage<=stages;++stage) {
            replay::Segment segment;segment.stage="stage"+std::to_string(stage);segment.seed=123;
            segment.difficulty=static_cast<replay::Difficulty>(difficulty);segment.rank=difficulty==0?20:32;
            segment.score=(stage-1)*100;replay::BeginStage(campaign,segment);
            campaign.frames.push_back({.dt=1.0F/120});
            replay::EndStage(campaign,{.score=static_cast<unsigned>(stage*100),.lives=3,.power=8,.won=true});
        }
        replay::FinishSession(campaign,campaign.segments.back().result);
        assert(replay::CanPlay(campaign,error));
        std::filesystem::path saved;replay::Data restored;
        assert(replay::Save(directory,campaign,saved,error));
        assert(replay::Load(saved,restored,error) && replay::CanPlay(restored,error));
        assert(restored.segments.size()==static_cast<unsigned>(stages) && restored.result==campaign.result);
        assert(restored.segments.back().begin==static_cast<unsigned>(stages-1));
        auto invalidOrder=campaign;invalidOrder.segments[1].stage="stage4";assert(!replay::CanPlay(invalidOrder,error));
        auto invalidResources=campaign;invalidResources.segments[1].lives=4;assert(!replay::CanPlay(invalidResources,error));
        auto invalidSeed=campaign;invalidSeed.segments[1].seed=124;assert(!replay::CanPlay(invalidSeed,error));
        auto truncated=campaign;truncated.frames.pop_back();truncated.segments.pop_back();
        truncated.result=truncated.segments.back().result;assert(!replay::CanPlay(truncated,error));
    }
    auto invalid=data;invalid.header.shot=99;
    assert(!replay::CanPlay(invalid,error));
    invalid=data;invalid.frames[0].dt=std::numeric_limits<float>::quiet_NaN();
    assert(!replay::CanPlay(invalid,error));
    std::filesystem::path file;
    assert(replay::Save(directory,data,file,error));
    replay::Data loaded;
    assert(replay::Load(file,loaded,error)&&replay::CanPlay(loaded,error));
    assert(loaded.result==result&&loaded.frames.size()==2);
    assert(!loaded.frames[0].player.aimAssistEnabled&&loaded.frames[1].player.aimAssistEnabled);
    assert(loaded.frames[1].player.mouseDelta.y==-2&&loaded.frames[1].confirm);
    // 槽位发布必须先完整写入，未确认、无效槽或目标为目录都不能破坏既有录像。
    data.name="  Name ";assert(replay::SaveSlot(directory,data,1,false,file,error));
    assert(file.filename()=="th6_01.krep");replay::Summary summary;
    assert(replay::ReadSummary(file,summary,error) && summary.name==data.name && summary.recordedAt==data.recordedAt);
    assert(summary.result==data.result && summary.stage=="stage1" && summary.shot=="reimu-a");
    auto replacement=data;replacement.name="NEW";
    assert(!replay::SaveSlot(directory,replacement,1,false,file,error));
    assert(replay::Load(replay::SlotPath(directory,1),loaded,error) && loaded.name==data.name);
    assert(!replay::SaveSlot(directory,replacement,0,true,file,error));
    assert(!replay::SaveSlot(directory,replacement,61,true,file,error));
    assert(replay::SaveSlot(directory,replacement,1,true,file,error));
    assert(replay::Load(file,loaded,error) && loaded.name=="NEW");
    auto damaged=replacement;damaged.frames[0].dt=std::numeric_limits<float>::quiet_NaN();
    assert(!replay::SaveSlot(directory,damaged,1,true,file,error));
    assert(replay::Load(replay::SlotPath(directory,1),loaded,error) && loaded.name=="NEW");
    assert(std::filesystem::create_directory(replay::SlotPath(directory,60)));
    assert(!replay::SaveSlot(directory,replacement,60,true,file,error));
    assert(replay::Load(replay::SlotPath(directory,1),loaded,error) && loaded.name=="NEW");
    // 摘要读取只验证头及声明长度，真正播放前的完整Load仍拦截负载损坏。
    const auto corrupt=replay::SlotPath(directory,1);
    {std::fstream stream(corrupt,std::ios::binary|std::ios::in|std::ios::out);
        stream.seekg(-5,std::ios::end);char byte=0;stream.get(byte);stream.seekp(-5,std::ios::end);stream.put(byte^1);}
    assert(replay::ReadSummary(corrupt,summary,error));assert(!replay::Load(corrupt,loaded,error));
    {std::ofstream stream(directory/"bad.krep");stream<<"bad";}
    summary.name="keep";assert(!replay::ReadSummary(directory/"bad.krep",summary,error) && summary.name=="keep");
    std::filesystem::remove_all(directory);
}

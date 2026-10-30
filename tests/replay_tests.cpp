// 本文件验证录像协议与关卡集成边界，防止未支持的关卡或损坏输入进入播放。
#include "replay/replay.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <limits>

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
    assert(replay::CanPlay(data,error));
    auto unsupported=data;
    unsupported.segments[0].stage="stage2";
    assert(!replay::CanPlay(unsupported,error));
    unsupported=data;unsupported.segments[0].difficulty=replay::Difficulty::Hard;
    assert(!replay::CanPlay(unsupported,error));
    auto invalid=data;invalid.header.shot=99;
    assert(!replay::CanPlay(invalid,error));
    invalid=data;invalid.frames[0].dt=std::numeric_limits<float>::quiet_NaN();
    assert(!replay::CanPlay(invalid,error));
    const auto directory=std::filesystem::temp_directory_path()/
        ("koumakyou_replay_merge_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path file;
    assert(replay::Save(directory,data,file,error));
    replay::Data loaded;
    assert(replay::Load(file,loaded,error)&&replay::CanPlay(loaded,error));
    assert(loaded.result==result&&loaded.frames.size()==2);
    assert(!loaded.frames[0].player.aimAssistEnabled&&loaded.frames[1].player.aimAssistEnabled);
    assert(loaded.frames[1].player.mouseDelta.y==-2&&loaded.frames[1].confirm);
    std::filesystem::remove_all(directory);
}

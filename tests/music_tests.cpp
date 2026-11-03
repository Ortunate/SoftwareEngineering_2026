// 本文件在真实音频设备上静音验证全部Music Room曲目、按需切换与流释放，不创建图形窗口。
#include "raylib.h"
#include "resources/asset_manager.hpp"
#include "resources/music_catalog.hpp"
#include <set>

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace koumakyou {
/// @brief 音频测试只读试听流的实际状态，不向产品接口暴露可变句柄。
struct GameLifecycleProbe {
    static const Music& Stream(const AssetManager& assets) {return assets.roomMusic_;}
};
}
namespace {
/// @brief 验证生产试听入口确实解码并推进；测试独占管理器，主音量始终为零。
void CheckMusic(koumakyou::AssetManager& assets,std::size_t index) {
    using Probe=koumakyou::GameLifecycleProbe;
    if(!assets.PlayMusicRoomTrack(index) || assets.MusicRoomTrack()!=static_cast<int>(index))
        throw std::runtime_error("Music Room load failed: "+std::string(koumakyou::music::kTracks[index].path));
    for(int tick=0;tick<20;++tick) {assets.UpdateAudio();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    const auto& music=Probe::Stream(assets);const float before=GetMusicTimePlayed(music);
    if(!IsMusicStreamPlaying(music) || before<=0)throw std::runtime_error("Music Room stream did not advance");
    if(!assets.PlayMusicRoomTrack(index) || GetMusicTimePlayed(music)<before-0.02F)
        throw std::runtime_error("Music Room repeated selection restarted stream");
    assets.PauseMusic();if(IsMusicStreamPlaying(music))throw std::runtime_error("Music Room pause failed");
    assets.ResumeMusic();if(!IsMusicStreamPlaying(music))throw std::runtime_error("Music Room resume failed");
}
}

/// @brief 使用可执行文件旁的真实素材静音检查全部BGM；需可用音频设备，失败返回1。
int main() {
    SetTraceLogLevel(LOG_WARNING);
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        std::cerr << "Audio device initialization failed\n";
        return 1;
    }
    SetMasterVolume(0);
    int result = 0;
    try {
        const auto root = std::filesystem::path(GetApplicationDirectory()) / "assets";
        koumakyou::AssetManager assets(root);
        std::set<std::filesystem::path> listed,actual;
        for(std::size_t index=0;index<koumakyou::music::kTracks.size();++index) {
            const auto path=root/koumakyou::music::kTracks[index].path;
            if(!listed.insert(path).second)throw std::runtime_error("Duplicate Music Room track");
            CheckMusic(assets,index);
        }
        for(const auto& entry:std::filesystem::recursive_directory_iterator(root))
            if(entry.is_regular_file() && entry.path().extension()==".mp3")actual.insert(entry.path());
        if(listed!=actual)throw std::runtime_error("Music Room catalog does not cover every BGM asset");
        bool rejected=false;try{assets.PlayMusicRoomTrack(koumakyou::music::kTracks.size());}catch(const std::out_of_range&){rejected=true;}
        if(!rejected)throw std::runtime_error("Music Room accepted invalid track index");
        assets.SetMusicVolumePercent(35);assets.SetMusicGain(0.5F);
        assets.UnloadStage();
        if(assets.MusicRoomTrack()!=-1 || IsMusicValid(koumakyou::GameLifecycleProbe::Stream(assets)))
            throw std::runtime_error("Music Room stream survived unload");
        std::cout<<"All "<<listed.size()<<" Music Room tracks, MP3 streams and lifecycle checks passed (muted)\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    CloseAudioDevice();
    return result;
}

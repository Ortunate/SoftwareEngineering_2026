// 本文件在真实音频设备上静音验证中文 BGM 路径、MP3 解码与流式播放，不创建图形窗口。
#include "raylib.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
/// @brief 复用生产代码的窄字符文件链路；音乐须加载并实际推进，缺失或解码失败时抛异常。
/// path 指向真实 MP3；本函数独占流并在成功或失败时释放，主音量由入口保持为零。
void CheckMusic(const std::filesystem::path& path) {
    const std::string filename = path.string();
    if (!FileExists(filename.c_str())) throw std::runtime_error("BGM file not found: " + filename);
    Music music = LoadMusicStream(filename.c_str());
    if (!IsMusicValid(music)) throw std::runtime_error("BGM decode failed: " + filename);
    PlayMusicStream(music);
    // 给设备回调实际消费缓冲的机会，不能仅凭 PlayMusicStream 的调用就认定有输出。
    for (int tick = 0; tick < 20; ++tick) {
        UpdateMusicStream(music);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const bool playing = IsMusicStreamPlaying(music) && GetMusicTimePlayed(music) > 0;
    StopMusicStream(music);
    UnloadMusicStream(music);
    if (!playing) throw std::runtime_error("BGM stream did not advance: " + filename);
}
}

/// @brief 使用可执行文件旁的真实素材静音检查标题及一面 BGM；需可用音频设备，失败返回 1。
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
        constexpr std::array tracks{
            "common/比赤色更鲜红的梦.mp3",
            "stages/stage1/宛如鬼灯的鲜红之魂.mp3",
            "stages/stage1/妖魔夜行.mp3"};
        for (const char* track : tracks) CheckMusic(root / track);
        std::cout << "All 3 BGM paths, MP3 decoders and streams passed (muted)\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    CloseAudioDevice();
    return result;
}

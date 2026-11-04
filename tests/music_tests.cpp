// 本文件在真实音频设备上静音验证全部 BGM 的循环、定位、暂停、主线程停顿和资源释放。
#include "raylib.h"
#include "resources/asset_manager.hpp"
#include "resources/music_catalog.hpp"

#include <algorithm>
#include <chrono>
#include <vector>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace koumakyou {
/// @brief 音频测试只读播放器状态，不向产品接口暴露可变流句柄。
struct GameLifecycleProbe {
    static const audio::BgmPlayer& Stream(const AssetManager& assets) { return assets.roomMusic_; }
};
}
namespace {
/// @brief 真实等待音频设备消费 PCM；期间故意不调用 UpdateAudio，验证供样不依赖主线程。
void WaitAudio() { std::this_thread::sleep_for(std::chrono::milliseconds(150)); }

/// @brief 验证生产试听入口和原作循环；root 借用素材根目录，设备始终静音。
void CheckMusic(koumakyou::AssetManager& assets, std::size_t index, const std::filesystem::path& root,
                const std::filesystem::path& seamDirectory) {
    using Probe = koumakyou::GameLifecycleProbe;
    const auto path = root / koumakyou::music::kTracks[index].path;
    if (!assets.PlayMusicRoomTrack(index) || assets.MusicRoomTrack() != static_cast<int>(index))
        throw std::runtime_error("Music Room load failed: " + path.string());
    const auto& music = Probe::Stream(assets);
    WaitAudio();
    const double before = music.PositionSeconds();
    if (!music.Playing() || before <= 0 || music.PcmBytes() == 0)
        throw std::runtime_error("BGM callback did not advance without main-thread updates");
    if (!assets.PlayMusicRoomTrack(index) || music.PositionSeconds() < before)
        throw std::runtime_error("Music Room repeated selection restarted stream");
    assets.PauseMusic();
    const double paused = music.PositionSeconds();
    WaitAudio();
    if (music.Playing() || music.PositionSeconds() != paused)
        throw std::runtime_error("BGM advanced while paused");
    assets.ResumeMusic();
    if (!music.Playing()) throw std::runtime_error("BGM resume failed");

    auto loopPath = path; loopPath.replace_extension(".loop");
    std::ifstream input(loopPath);
    std::string magic; unsigned version = 0, rate = 0;
    std::uint64_t start = 0, end = 0;
    if (!(input >> magic >> version >> rate >> start >> end) || magic != "KOUMAKYOU_BGM_LOOP" ||
        version != 1 || rate != 48000 || start >= end || double(end) / rate > music.LengthSeconds())
        throw std::runtime_error("Invalid shipped BGM loop metadata");
    // 使用正式 FLAC 的真实 PCM 独立拼接期望，再逐样本比较生产循环游标的跨界输出。
    struct Decoded { Wave wave; ~Decoded() { if (wave.data) UnloadWave(wave); } } decoded{LoadWave(path.string().c_str())};
    const auto& wave = decoded.wave;
    constexpr std::uint64_t window = 48000;
    if (!wave.data || wave.sampleSize != 16 || wave.channels != 2 || end - start < window || end < window)
        throw std::runtime_error("Cannot inspect BGM PCM seam");
    const auto* pcm = static_cast<const std::int16_t*>(wave.data);
    std::vector<std::int16_t> seam(window * 2 * wave.channels);
    koumakyou::audio::LoopCursor cursor;
    if (!cursor.Configure(wave.frameCount, wave.channels, start, end))
        throw std::runtime_error("Cannot configure real PCM seam");
    cursor.Seek(end - window);
    if (!cursor.Render({pcm, std::size_t(wave.frameCount) * wave.channels}, seam) ||
        !std::equal(seam.begin(), seam.begin() + window * wave.channels, pcm + (end - window) * wave.channels) ||
        !std::equal(seam.begin() + window * wave.channels, seam.end(), pcm + start * wave.channels))
        throw std::runtime_error("Native BGM seam lost or duplicated PCM samples");
    if (!seamDirectory.empty()) {
        auto exported = wave;
        exported.frameCount = static_cast<unsigned>(window * 2); exported.data = seam.data();
        if (!ExportWave(exported, (seamDirectory / (std::to_string(index + 1) + ".wav")).string().c_str()))
            throw std::runtime_error("Cannot export native BGM seam");
    }
    const double loopStart = double(start) / rate, loopEnd = double(end) / rate;
    assets.SeekMusic(static_cast<float>(loopEnd - 0.04));
    WaitAudio();
    // 设备周期会提前索取少量样本；容差只用于真实设备计时，采样级接缝由无设备测试精确核对。
    if (std::abs(music.PositionSeconds() - (loopStart + 0.11)) > 0.12)
        throw std::runtime_error("BGM did not cross native loop boundary: " + path.string());
    assets.SeekMusic(static_cast<float>(loopEnd + 3 * (loopEnd - loopStart) + 0.025));
    WaitAudio();
    if (std::abs(music.PositionSeconds() - (loopStart + 0.175)) > 0.12)
        throw std::runtime_error("BGM replay seek used whole-file modulo");
    std::cout << path.filename().string() << ": loop " << loopStart << ".." << loopEnd
              << " s, PCM " << music.PcmBytes() << " bytes\n";
}
}

/// @brief 使用可执行文件旁真实素材验证全部 BGM；需音频设备，失败返回 1。
int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_WARNING);
    InitAudioDevice();
    if (!IsAudioDeviceReady()) { std::cerr << "Audio device initialization failed\n"; return 1; }
    SetMasterVolume(0);
    int result = 0;
    try {
        const auto root = std::filesystem::path(GetApplicationDirectory()) / "assets";
        const std::filesystem::path seamDirectory = argc > 1 ? argv[1] : "";
        if (!seamDirectory.empty()) std::filesystem::create_directories(seamDirectory);
        koumakyou::AssetManager assets(root);
        std::set<std::filesystem::path> listed, actual;
        for (std::size_t index = 0; index < koumakyou::music::kTracks.size(); ++index) {
            const auto path = root / koumakyou::music::kTracks[index].path;
            if (!listed.insert(path).second) throw std::runtime_error("Duplicate Music Room track");
            CheckMusic(assets, index, root, seamDirectory);
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            if (entry.is_regular_file() && entry.path().extension() == ".flac") actual.insert(entry.path());
        if (listed != actual) throw std::runtime_error("Music Room catalog does not cover every BGM asset");
        bool rejected = false;
        try { assets.PlayMusicRoomTrack(koumakyou::music::kTracks.size()); }
        catch (const std::out_of_range&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Music Room accepted invalid track index");
        assets.SetMusicVolumePercent(35); assets.SetMusicGain(0.5F);
        assets.UnloadStage();
        if (assets.MusicRoomTrack() != -1 || koumakyou::GameLifecycleProbe::Stream(assets).Valid())
            throw std::runtime_error("Music Room stream survived unload");
        // 超过回调槽容量的加载/销毁次数，防止卸载遗漏导致后续曲目无法播放。
        for (int attempt = 0; attempt < 12; ++attempt) {
            if (!assets.PlayMusicRoomTrack(0)) throw std::runtime_error("BGM callback slot leaked");
            assets.UnloadStage();
        }
        std::cout << "All " << listed.size() << " BGM native loops, replay seeks and lifecycle checks passed (muted)\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    CloseAudioDevice();
    return result;
}

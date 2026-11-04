// 本文件用手写 PCM 期望和 raylib 链接替身验证循环与生命周期；不初始化真实音频设备。
#include "audio/bgm_player.hpp"
#include "raylib.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

// 替身只实现本模块使用的 raylib 符号；真实解码器和设备仍须由另行的集成测试覆盖。
// 该目标直接链接本文件定义，静态 raylib 的 raudio 对象不会被拉入。
struct rAudioBuffer {
    AudioCallback callback = nullptr;
    unsigned channels = 1;
    bool playing = false;
    bool paused = false;
    float volume = 1;
};

namespace {
using koumakyou::audio::BgmPlayer;
using koumakyou::audio::LoopCursor;
std::mutex audioLock;
std::vector<rAudioBuffer*> streams;
bool deviceReady = false;
bool failDecode = false;
bool failConvert = false;
bool failStream = false;
bool invalidPartialStream = false;
unsigned decodedBits = 16;
int waves = 0;
int detached = 0;

/// @brief 模拟设备线程在 raylib 音频锁内请求 PCM；仅测试驱动分配输出，生产回调不分配。
std::vector<std::int16_t> Pump(rAudioBuffer* buffer, unsigned frames) {
    std::lock_guard guard(audioLock);
    std::vector<std::int16_t> output(static_cast<std::size_t>(frames)*buffer->channels, -999);
    if (buffer->playing && !buffer->paused && buffer->callback) buffer->callback(output.data(), frames);
    return output;
}

/// @brief 临时目录只包含旁路元数据；假解码器提供固定波形，析构清理所有测试文件。
struct Fixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path()/
        ("koumakyou-bgm-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path path = directory/"test.flac";
    Fixture() { std::filesystem::create_directory(directory); Metadata(); }
    ~Fixture() { std::error_code error; std::filesystem::remove_all(directory, error); }
    void Metadata(const std::string& text = "KOUMAKYOU_BGM_LOOP 1\n48000 2 5\n") {
        auto sidecar = path;
        sidecar.replace_extension(".loop");
        std::ofstream(sidecar) << text;
    }
};

/// @brief 期望直接手写，不调用归一化实现推导，验证前奏一次、尾部排除和多个缓冲的精确接缝。
void TestMono() {
    LoopCursor cursor;
    assert(cursor.Configure(7, 1, 2, 5));
    const std::array<std::int16_t, 7> pcm{10,20,30,40,50,60,70};
    std::array<std::int16_t, 4> first{};
    assert(cursor.Render(pcm, first));
    assert((first == std::array<std::int16_t,4>{10,20,30,40}));
    std::array<std::int16_t, 13> next{};
    assert(cursor.Render(pcm, next));
    assert((next == std::array<std::int16_t,13>{50,30,40,50,30,40,50,30,40,50,30,40,50}));
    assert(cursor.Position() == 2);
    std::array<std::int16_t, 3> boundary{};
    assert(cursor.Render(pcm, boundary));
    assert((boundary == std::array<std::int16_t,3>{30,40,50}));
    assert(cursor.Position() == 2);
    assert(cursor.Render(pcm, {}));
    assert(cursor.Position() == 2);
    cursor.Seek(0);
    std::array<std::int16_t, 20> large{};
    assert(cursor.Render(pcm, large));
    assert((large == std::array<std::int16_t,20>{10,20,30,40,50,30,40,50,30,40,50,30,40,50,30,40,50,30,40,50}));
}

/// @brief 左右声道必须作为完整采样帧回绕，负样本也必须逐位保留。
void TestStereoAndSeek() {
    LoopCursor cursor;
    const std::array<std::int16_t, 12> pcm{1,-1,2,-2,3,-3,4,-4,5,-5,6,-6};
    assert(cursor.Configure(6, 2, 1, 4));
    std::array<std::int16_t, 20> output{};
    assert(cursor.Render(pcm, output));
    assert((output == std::array<std::int16_t,20>{1,-1,2,-2,3,-3,4,-4,2,-2,3,-3,4,-4,2,-2,3,-3,4,-4}));
    const std::array<std::pair<std::uint64_t,std::uint64_t>,9> cases{{
        {0,0},{1,1},{2,2},{3,3},{4,1},{5,2},{7,1},{100,1},
        {std::numeric_limits<std::uint64_t>::max(),3}}};
    for (const auto& [requested, expected] : cases) {
        cursor.Seek(requested);
        assert(cursor.Position() == expected);
    }
    cursor.Seek(3);
    std::array<std::int16_t, 8> sought{};
    assert(cursor.Render(pcm, sought));
    assert((sought == std::array<std::int16_t,8>{4,-4,2,-2,3,-3,4,-4}));
    assert(cursor.Configure(1, 1, 0, 1));
    const std::array<std::int16_t,1> single{-32768};
    std::array<std::int16_t,5> repeated{};
    assert(cursor.Render(single, repeated));
    assert((repeated == std::array<std::int16_t,5>{-32768,-32768,-32768,-32768,-32768}));
}

void TestInvalidCursor() {
    LoopCursor cursor;
    std::array<std::int16_t,4> output{9,9,9,9};
    const auto unchanged = output;
    cursor.Seek(123);
    assert(!cursor.Render({}, output));
    assert(output == unchanged && cursor.Position() == 0);
    assert(!cursor.Configure(0,1,0,1));
    assert(!cursor.Configure(6,0,1,4));
    assert(!cursor.Configure(6,2,4,4));
    assert(!cursor.Configure(6,2,5,4));
    assert(!cursor.Configure(6,2,0,7));
    assert(!cursor.Configure(std::numeric_limits<std::uint64_t>::max(),2,0,1));
    assert(cursor.Configure(3,2,1,3));
    cursor.Seek(2);
    assert(!cursor.Configure(3,2,2,1));
    const std::array<std::int16_t,6> pcm{1,2,3,4,5,6};
    assert(!cursor.Render(std::span(pcm).first(4), output));
    assert(!cursor.Render(pcm, std::span(output).first(3)));
    assert(output == unchanged && cursor.Position() == 2);
}

/// @brief 验证无设备、元数据、解码、格式转换和临时流失败均不影响正在播放的旧实例。
void TestLoading(Fixture& fixture) {
    BgmPlayer player;
    assert(!player.Load(fixture.path));
    assert(!player.Valid() && !player.Playing());
    assert(player.PositionSeconds() == 0 && player.LengthSeconds() == 0 && player.PcmBytes() == 0);
    player.Play(); player.Stop(); player.Pause(); player.Resume(); player.Seek(1); player.SetVolume(1); player.Reset();
    deviceReady = true;
    decodedBits = 32;
    assert(player.Load(fixture.path));
    decodedBits = 16;
    assert(player.PcmBytes() == 12 && player.LengthSeconds() == 6.0/48000);
    player.Play();
    auto* original = streams.front();
    assert((Pump(original,4) == std::vector<std::int16_t>{10,20,30,40}));
    const auto position = player.PositionSeconds();
    const std::array<std::string,10> badMetadata{
        "", "OTHER 1\n48000 2 5\n", "KOUMAKYOU_BGM_LOOP 2\n48000 2 5\n",
        "KOUMAKYOU_BGM_LOOP 1\n44100 2 5\n", "KOUMAKYOU_BGM_LOOP 1\n48000 -1 5\n",
        "KOUMAKYOU_BGM_LOOP 1\n48000 5 5\n", "KOUMAKYOU_BGM_LOOP 1\n48000 6 5\n",
        "KOUMAKYOU_BGM_LOOP 1\n48000 2 7\n", "KOUMAKYOU_BGM_LOOP 1\n48000 2 5 extra\n",
        "KOUMAKYOU_BGM_LOOP 1\n48000 2 999999999999999999999999\n"};
    for (const auto& text : badMetadata) {
        fixture.Metadata(text);
        assert(!player.Load(fixture.path));
        assert(player.Playing() && player.PositionSeconds() == position);
        assert(streams.size() == 1 && waves == 1);
    }
    auto sidecar = fixture.path; sidecar.replace_extension(".loop");
    std::filesystem::remove(sidecar);
    assert(!player.Load(fixture.path));
    fixture.Metadata();
    failDecode = true; assert(!player.Load(fixture.path)); failDecode = false;
    decodedBits = 32; failConvert = true; assert(!player.Load(fixture.path));
    decodedBits = 16; failConvert = false;
    failStream = true; assert(!player.Load(fixture.path)); failStream = false;
    invalidPartialStream = true; assert(!player.Load(fixture.path)); invalidPartialStream = false;
    assert(player.Playing() && player.PositionSeconds() == position && waves == 1 && streams.size() == 1);
    assert(player.Load(fixture.path));
    assert(!player.Playing() && player.PositionSeconds() == 0 && streams.size() == 1 && waves == 1);
}

/// @brief 停顿后重复 Resume 不重启；主线程 seek 与回调并行时以序号隔离旧位置发布。
void TestControls(Fixture& fixture) {
    BgmPlayer player;
    assert(player.Load(fixture.path));
    auto* buffer = streams.front();
    player.Play();
    assert(player.PositionSeconds() == 0);
    Pump(buffer,4);
    assert(player.PositionSeconds() == 4.0/48000);
    player.Pause(); player.Pause();
    const auto paused = player.PositionSeconds();
    for (int i=0;i<8;++i) Pump(buffer,64);
    assert(!player.Playing() && player.PositionSeconds() == paused);
    player.Resume(); player.Resume();
    assert(player.Playing());
    assert((Pump(buffer,3) == std::vector<std::int16_t>{50,30,40}));
    player.Seek(8.5/48000);
    assert(player.PositionSeconds() == 2.0/48000);
    assert((Pump(buffer,3) == std::vector<std::int16_t>{30,40,50}));
    player.Seek(1.5/48000);
    assert(player.PositionSeconds() == 1.0/48000);
    player.Seek(-1); player.Seek(std::numeric_limits<double>::infinity());
    player.Seek(std::numeric_limits<double>::quiet_NaN()); player.Seek(std::numeric_limits<double>::max());
    assert(player.PositionSeconds() == 1.0/48000);
    player.SetVolume(-2); assert(buffer->volume == 0);
    player.SetVolume(2); assert(buffer->volume == 1);
    player.SetVolume(0.25F); player.SetVolume(std::numeric_limits<float>::quiet_NaN());
    assert(buffer->volume == 0.25F);
    player.Play(); assert(player.PositionSeconds() == 0);
    assert((Pump(buffer,3) == std::vector<std::int16_t>{10,20,30}));
    player.Stop(); assert(player.PositionSeconds() == 0 && !player.Playing());
    player.Resume(); player.Pause(); player.Resume();
    assert(!player.Playing() && player.PositionSeconds() == 0);
    player.Play();
    std::atomic<bool> running{true};
    std::thread worker([&] { while (running.load()) Pump(buffer,7); });
    for (int i=0;i<10000;++i) {
        player.Seek((i%5+0.25)/48000);
        const auto position = player.PositionSeconds();
        assert(position >= 0 && position < 5.0/48000);
    }
    player.Pause();
    const auto frozen = player.PositionSeconds();
    for (int i=0;i<1000;++i) assert(player.PositionSeconds() == frozen);
    running.store(false); worker.join();
    player.Reset(); player.Reset();
}

/// @brief 8 个 trampoline 独立寻址；第 9 个失败，移动保持回调状态地址，回收后可再次加载。
void TestSlotsAndMoves(Fixture& fixture) {
    static_assert(!std::is_copy_constructible_v<BgmPlayer> && !std::is_copy_assignable_v<BgmPlayer>);
    static_assert(std::is_nothrow_move_constructible_v<BgmPlayer> && std::is_nothrow_move_assignable_v<BgmPlayer>);
    std::array<BgmPlayer,8> players;
    std::array<rAudioBuffer*,8> buffers{};
    for (std::size_t i=0;i<players.size();++i) {
        assert(players[i].Load(fixture.path));
        buffers[i] = streams.back();
        players[i].Play();
        players[i].Seek((i%5+0.25)/48000);
    }
    BgmPlayer ninth;
    assert(!ninth.Load(fixture.path));
    assert(!players[0].Load(fixture.path) && players[0].Playing());
    const std::array<std::int16_t,8> expected{10,20,30,40,50,10,20,30};
    for (std::size_t i=0;i<players.size();++i) assert(Pump(buffers[i],1).front() == expected[i]);
    BgmPlayer moved(std::move(players[0]));
    assert(!players[0].Valid() && moved.Valid());
    assert(Pump(buffers[0],1).front() == 20);
    players[1] = std::move(moved);
    assert(!moved.Valid() && players[1].Playing());
    assert(Pump(buffers[0],1).front() == 30);
    assert(ninth.Load(fixture.path));
    players[2].Reset();
    assert(players[3].Load(fixture.path));
    assert(!players[3].Playing() && players[3].PositionSeconds() == 0);
}
} // namespace

// 下列替身复现本模块依赖的锁同步契约；故障开关只由测试主线程设置。
bool IsAudioDeviceReady() { return deviceReady; }
Wave LoadWave(const char*) {
    if (failDecode) return {};
    auto* pcm = std::malloc(6*sizeof(std::int16_t));
    if (!pcm) throw std::bad_alloc();
    const std::array<std::int16_t,6> samples{10,20,30,40,50,60};
    std::memcpy(pcm,samples.data(),sizeof(samples));
    ++waves;
    return Wave{6,48000,decodedBits,1,pcm};
}
void UnloadWave(Wave wave) { std::free(wave.data); --waves; }
void WaveFormat(Wave* wave, int, int bits, int) { if (!failConvert) wave->sampleSize = bits; }
AudioStream LoadAudioStream(unsigned rate, unsigned bits, unsigned channels) {
    if (failStream) return {};
    auto* buffer = new rAudioBuffer;
    buffer->channels = channels;
    streams.push_back(buffer);
    return AudioStream{buffer,nullptr,invalidPartialStream ? 0U : rate,bits,channels};
}
bool IsAudioStreamValid(AudioStream stream) { return stream.buffer && stream.sampleRate != 0; }
void SetAudioStreamCallback(AudioStream stream, AudioCallback callback) {
    std::lock_guard guard(audioLock);
    if (!callback) ++detached;
    stream.buffer->callback = callback;
}
void UnloadAudioStream(AudioStream stream) {
    std::lock_guard guard(audioLock);
    assert(stream.buffer->callback == nullptr && !stream.buffer->playing);
    streams.erase(std::find(streams.begin(),streams.end(),stream.buffer));
    delete stream.buffer;
}
void PlayAudioStream(AudioStream stream) {
    std::lock_guard guard(audioLock);
    stream.buffer->playing = true; stream.buffer->paused = false;
}
void StopAudioStream(AudioStream stream) {
    std::lock_guard guard(audioLock);
    stream.buffer->playing = false; stream.buffer->paused = false;
}
void PauseAudioStream(AudioStream stream) {
    std::lock_guard guard(audioLock); stream.buffer->paused = true;
}
void ResumeAudioStream(AudioStream stream) {
    std::lock_guard guard(audioLock); stream.buffer->paused = false;
}
bool IsAudioStreamPlaying(AudioStream stream) {
    std::lock_guard guard(audioLock); return stream.buffer->playing && !stream.buffer->paused;
}
void SetAudioStreamVolume(AudioStream stream, float volume) {
    std::lock_guard guard(audioLock); stream.buffer->volume = volume;
}

/// @brief 独立测试入口；失败触发断言，成功返回 0，全程不访问物理设备。
int main() {
    TestMono(); TestStereoAndSeek(); TestInvalidCursor();
    Fixture fixture;
    TestLoading(fixture);
    assert(streams.empty() && waves == 0);
    TestControls(fixture);
    TestSlotsAndMoves(fixture);
    assert(streams.empty() && waves == 0 && detached > 0);
    std::cout << "BGM cursor, playback controls, atomic seek and eight-slot lifecycle passed (no device)\n";
}

// 本文件实现 PCM 预解码、无缝循环拷贝及固定回调槽的安全资源生命周期。
#include "audio/bgm_player.hpp"
#include "raylib.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

namespace koumakyou::audio {

bool LoopCursor::Configure(std::uint64_t frameCount, unsigned channels,
                           std::uint64_t loopStart, std::uint64_t loopEnd) noexcept {
    if (channels == 0 || loopStart >= loopEnd || loopEnd > frameCount ||
        frameCount > std::numeric_limits<std::size_t>::max()/sizeof(std::int16_t)/channels) return false;
    frameCount_ = frameCount;
    channels_ = channels;
    loopStart_ = loopStart;
    loopEnd_ = loopEnd;
    position_ = 0;
    return true;
}

void LoopCursor::Seek(std::uint64_t frame) noexcept {
    if (channels_ == 0) return;
    position_ = frame < loopEnd_ ? frame : loopStart_ + (frame-loopStart_)%(loopEnd_-loopStart_);
}

bool LoopCursor::Render(std::span<const std::int16_t> pcm, std::span<std::int16_t> output) noexcept {
    if (channels_ == 0 || pcm.size() != frameCount_*channels_ || output.size()%channels_ != 0) return false;
    std::size_t written = 0;
    while (written < output.size()) {
        const auto frames = std::min<std::uint64_t>(loopEnd_-position_, (output.size()-written)/channels_);
        const auto samples = static_cast<std::size_t>(frames)*channels_;
        std::memcpy(output.data()+written, pcm.data()+static_cast<std::size_t>(position_)*channels_,
                    samples*sizeof(std::int16_t));
        written += samples;
        position_ += frames;
        // 必须在当前缓冲内回绕，不能等待下次回调，否则会夹入尾部或静音。
        if (position_ == loopEnd_) position_ = loopStart_;
    }
    return true;
}

/// @brief 地址在整个注册期保持不变；cursor 仅由回调使用，其他字段初始化后只读。
/// 槽只在主线程分配；先写槽再安装回调，卸回调同步完成后才清槽，借 raylib 锁建立可见性。
struct BgmPlayer::State {
    static constexpr std::size_t kSlots = 8;
    static constexpr std::uint64_t kFrameMask = 0xffffffffULL;
    static inline std::array<State*, kSlots> slots{};
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "BGM callback requires lock-free frame atomics");

    Wave wave{};
    AudioStream stream{};
    LoopCursor cursor;
    std::uint64_t loopStart = 0;
    std::uint64_t loopEnd = 0;
    std::size_t slot = kSlots;
    // Wave::frameCount 为 32bit；高 32bit 是主线程请求序号，低 32bit 是源帧。
    // position 携带对应序号，查询可识别尚未发布的 seek，避免 exchange 清 pending 后的旧值窗口。
    std::atomic<std::uint64_t> pending{0};
    std::atomic<std::uint64_t> position{0};
    std::uint32_t requestSerial = 0; // 仅主线程访问；相邻请求序号不同，允许自然回绕。
    std::uint64_t applied = 0; // 仅回调访问；主线程不能修改 cursor 或此字段。
    bool started = false;

    ~State() {
        if (stream.buffer) {
            StopAudioStream(stream);
            // nullptr 的安装由 raylib 音频锁同步正在执行的回调；在此之前不得销毁 PCM 或释放槽。
            SetAudioStreamCallback(stream, nullptr);
            UnloadAudioStream(stream);
        }
        if (slot != kSlots) slots[slot] = nullptr;
        if (wave.data) UnloadWave(wave);
    }

    /// @brief raylib 传入的 frames 是源 PCM 帧数；不分配、不读盘、不调用 raylib 或自有锁。
    template<std::size_t Index>
    static void Callback(void* output, unsigned frames) noexcept {
        auto& state = *slots[Index];
        const auto command = state.pending.load(std::memory_order_acquire);
        if (command != state.applied) {
            state.cursor.Seek(command & kFrameMask);
            state.applied = command;
        }
        state.cursor.Render(
            {static_cast<const std::int16_t*>(state.wave.data),
             static_cast<std::size_t>(state.wave.frameCount)*state.wave.channels},
            {static_cast<std::int16_t*>(output), static_cast<std::size_t>(frames)*state.wave.channels});
        state.position.store((command & ~kFrameMask) | state.cursor.Position(), std::memory_order_release);
    }

    static constexpr std::array<AudioCallback, kSlots> callbacks{
        Callback<0>, Callback<1>, Callback<2>, Callback<3>,
        Callback<4>, Callback<5>, Callback<6>, Callback<7>};

    /// @brief 主线程发布目标；不修改回调独占的 cursor。连续请求按最后一次为准。
    void Request(std::uint64_t frame) noexcept {
        ++requestSerial;
        pending.store((static_cast<std::uint64_t>(requestSerial) << 32) | frame, std::memory_order_release);
    }
};

BgmPlayer::BgmPlayer() noexcept = default;
BgmPlayer::~BgmPlayer() { Reset(); }
BgmPlayer::BgmPlayer(BgmPlayer&&) noexcept = default;
BgmPlayer& BgmPlayer::operator=(BgmPlayer&& other) noexcept {
    if (this != &other) { Reset(); state_ = std::move(other.state_); }
    return *this;
}

bool BgmPlayer::Load(const std::filesystem::path& path) {
    if (!IsAudioDeviceReady()) return false;
    try {
        const auto freeSlot = std::find(State::slots.begin(), State::slots.end(), nullptr);
        if (freeSlot == State::slots.end()) return false;
        auto next = std::make_unique<State>();
        next->wave = LoadWave(path.string().c_str());
        auto& wave = next->wave;
        if (!wave.data || wave.frameCount == 0 || wave.sampleRate == 0 ||
            wave.sampleRate > static_cast<unsigned>(std::numeric_limits<int>::max()) ||
            (wave.channels != 1 && wave.channels != 2) ||
            (wave.sampleSize != 8 && wave.sampleSize != 16 && wave.sampleSize != 32)) return false;

        auto metadata = path;
        metadata.replace_extension(".loop");
        std::ifstream input(metadata);
        std::string magic, extra;
        // 使用有符号数先拒绝负数，避免流提取到无符号数时把 -1 当作极大正数。
        std::int64_t version = 0, rate = 0, start = 0, end = 0;
        if (!(input >> magic >> version >> rate >> start >> end) ||
            magic != "KOUMAKYOU_BGM_LOOP" || version != 1 || rate != wave.sampleRate ||
            start < 0 || end <= start || static_cast<std::uint64_t>(end) > wave.frameCount ||
            (input >> extra) || !input.eof()) return false;
        const auto originalFrames = wave.frameCount;
        const auto originalRate = wave.sampleRate;
        const auto originalChannels = wave.channels;
        if (wave.sampleSize != 16) WaveFormat(&wave, static_cast<int>(wave.sampleRate), 16,
                                              static_cast<int>(wave.channels));
        if (!wave.data || wave.sampleSize != 16 || wave.frameCount != originalFrames ||
            wave.sampleRate != originalRate || wave.channels != originalChannels ||
            !next->cursor.Configure(wave.frameCount, wave.channels, start, end)) return false;
        next->loopStart = static_cast<std::uint64_t>(start);
        next->loopEnd = static_cast<std::uint64_t>(end);

        // 临时 State 从建流前就持有全部资源；任何失败都先卸临时流，不触及旧播放器。
        next->stream = LoadAudioStream(wave.sampleRate, 16, wave.channels);
        if (!IsAudioStreamValid(next->stream)) return false;
        next->slot = static_cast<std::size_t>(freeSlot-State::slots.begin());
        State::slots[next->slot] = next.get();
        SetAudioStreamCallback(next->stream, State::callbacks[next->slot]);
        Reset();
        state_ = std::move(next);
        return true;
    } catch (...) {
        // 文件系统、路径转换和分配失败遵循与解码失败相同的事务语义。
        return false;
    }
}

void BgmPlayer::Reset() { state_.reset(); }
bool BgmPlayer::Valid() const { return state_ != nullptr; }
void BgmPlayer::Play() {
    if (!state_) return;
    StopAudioStream(state_->stream);
    state_->Request(0);
    state_->started = true;
    PlayAudioStream(state_->stream);
}
void BgmPlayer::Stop() {
    if (!state_) return;
    StopAudioStream(state_->stream);
    state_->started = false;
    state_->Request(0);
}
void BgmPlayer::Pause() { if (state_) PauseAudioStream(state_->stream); }
void BgmPlayer::Resume() { if (state_ && state_->started) ResumeAudioStream(state_->stream); }
void BgmPlayer::SetVolume(float volume) {
    if (state_ && std::isfinite(volume)) SetAudioStreamVolume(state_->stream, std::clamp(volume, 0.0F, 1.0F));
}
void BgmPlayer::Seek(double seconds) {
    if (!state_ || !std::isfinite(seconds) || seconds < 0) return;
    const long double frame = std::floor(static_cast<long double>(seconds)*state_->wave.sampleRate);
    // 用 2^64 而不是转换 UINT64_MAX，避免 long double 与 double 同精度的平台边界上溢。
    if (!(frame < std::ldexp(1.0L, 64))) return;
    auto target = static_cast<std::uint64_t>(frame);
    if (target >= state_->loopEnd)
        target = state_->loopStart+(target-state_->loopStart)%(state_->loopEnd-state_->loopStart);
    state_->Request(target);
}
bool BgmPlayer::Playing() const { return state_ && IsAudioStreamPlaying(state_->stream); }
double BgmPlayer::PositionSeconds() const {
    if (!state_) return 0;
    const auto pending = state_->pending.load(std::memory_order_acquire);
    const auto published = state_->position.load(std::memory_order_acquire);
    const auto frame = ((pending >> 32) == (published >> 32) ? published : pending) & State::kFrameMask;
    return static_cast<double>(frame)/state_->wave.sampleRate;
}
double BgmPlayer::LengthSeconds() const {
    return state_ ? static_cast<double>(state_->wave.frameCount)/state_->wave.sampleRate : 0;
}
std::size_t BgmPlayer::PcmBytes() const {
    return state_ ? static_cast<std::size_t>(state_->wave.frameCount)*state_->wave.channels*sizeof(std::int16_t) : 0;
}

} // namespace koumakyou::audio

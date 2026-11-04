// 本文件声明独立 PCM 循环游标与拥有预解码音频和回调流的 BGM 播放器。
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace koumakyou::audio {

/// @brief 无设备、无分配的交错 16bit PCM 游标；所有位置均为整组声道的采样帧。
/// 循环区间为 [loopStart, loopEnd)，首次从 0 开始，loopEnd 后的尾部不参与播放。
/// 本类不做线程同步，生产环境由音频回调独占。
class LoopCursor {
public:
    /// @brief 要求 frameCount>0、channels>0、loopStart<loopEnd<=frameCount，且 PCM 字节数可表示。
    /// 配置成功复位至 0；失败返回 false 并保留原配置。
    bool Configure(std::uint64_t frameCount, unsigned channels,
                   std::uint64_t loopStart, std::uint64_t loopEnd) noexcept;
    /// @brief frame<loopEnd 时保留原位置，否则映射为 loopStart+(frame-loopStart)%(loopEnd-loopStart)。
    /// 尚未配置时不操作。
    void Seek(std::uint64_t frame) noexcept;
    /// @brief pcm 必须恰为完整源 PCM，output 长度必须为声道数的整数倍且不得与 pcm 重叠。
    /// 同一次调用可跨任意多个循环边界；不分配、不读盘。无效参数返回 false 且不修改输出或游标。
    bool Render(std::span<const std::int16_t> pcm, std::span<std::int16_t> output) noexcept;
    /// @brief 返回下一次 Render 将读取的源采样帧；循环边界已归一化。
    std::uint64_t Position() const noexcept { return position_; }

private:
    std::uint64_t frameCount_ = 0;
    std::uint64_t loopStart_ = 0;
    std::uint64_t loopEnd_ = 0;
    std::uint64_t position_ = 0;
    unsigned channels_ = 0;
};

/// @brief 主线程 API；音频设备须由调用方初始化，并在所有播放器 Reset/析构后才关闭。
/// 独占 LoadWave 预解码的 16bit PCM 和 AudioStream，最多同时持有 8 个有效实例。
/// 移动不改变回调状态地址；不可复制。回调只做 PCM 拷贝与无锁原子交接。
class BgmPlayer {
public:
    BgmPlayer() noexcept;
    ~BgmPlayer();
    BgmPlayer(BgmPlayer&&) noexcept;
    BgmPlayer& operator=(BgmPlayer&&) noexcept;
    BgmPlayer(const BgmPlayer&) = delete;
    BgmPlayer& operator=(const BgmPlayer&) = delete;

    /// @brief 预解码文件，要求同名 .loop 文本为 KOUMAKYOU_BGM_LOOP 1\nrate start end\n。
    /// rate 必须匹配源采样率，start/end 以 PCM 帧计且 0<=start<end<=总帧数。
    /// 成功停掉并替换旧流，新流停在 0；无设备、槽满、解码/元数据/建流失败返回 false，保留旧播放器。
    bool Load(const std::filesystem::path& path);
    /// @brief 停流并在 raylib 音频锁下同步卸除回调后释放 PCM/槽；可重复调用。
    void Reset();
    bool Valid() const;
    /// @brief 每次均从 0 重新播放；Play/Stop 返回后位置立即归零。
    void Play();
    void Stop();
    /// @brief 重复 Pause/Resume 不改变位置；停止的流不会被 Resume 启动。
    void Pause();
    void Resume();
    /// @brief 音量钳制到 [0,1]；非有限值忽略，空播放器不操作。
    void SetVolume(float volume);
    /// @brief 秒数向下取整到 PCM 帧，按 LoopCursor::Seek 归一化；下一次回调开始应用。
    /// 非有限值、负数或换算后超出 uint64_t 范围的请求忽略；查询立即可见最新待应用位置。
    void Seek(double seconds);
    /// @brief 仅实际播放时为 true，暂停/停止/空播放器均为 false。
    bool Playing() const;
    /// @brief 返回待 seek 或回调已渲染的下一源位置，单位秒；不等同于设备扬声器的精确播放位置。
    double PositionSeconds() const;
    /// @brief 完整解码源长度（含未播放尾部），单位秒；空播放器返回 0。
    double LengthSeconds() const;
    /// @brief 仅返回自有预解码 PCM 字节数，不含 raylib 流缓冲；空播放器返回 0。
    std::size_t PcmBytes() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace koumakyou::audio

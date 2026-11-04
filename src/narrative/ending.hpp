// 本文件定义新典六条结局的选择规则、不可变演出内容与独立的 60 Hz 播放状态。
#pragma once

#include "resources/visual_assets.hpp"
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <optional>

namespace koumakyou::ending {
/// @brief 稳定结局编号；前四项分别为灵/梦/魔/恋好结局，后两项为灵梦/魔理沙坏结局。
enum class Id : std::uint8_t { ReimuA, ReimuB, MarisaA, MarisaB, ReimuBad, MarisaBad, Count };
/// @brief 原作选择输入；shot 为四御札 0～3，difficulty 为 Easy～Extra 的 0～4，计数非负。
/// completedMainline 仅在本局依次完成该难度终点（Easy五面，其余六面）时为 true；练习、预览、调试靶场必须传 false。
struct Request {
    int shot=0,difficulty=1,continues=0,misses=0;
    bool challenge=false,completedMainline=false;
};
/// @brief 未完整通关或输入无效返回空；新典挑战模式 Miss >5 与续关/Easy 均进入角色坏结局。
[[nodiscard]] std::optional<Id> Select(const Request& request);
/// @brief 返回中文标识或源脚本文件名；无效编号抛 invalid_argument，不拥有返回字符串。
[[nodiscard]] std::string_view Title(Id id);
[[nodiscard]] std::string_view SourceScript(Id id);

/// @brief 有序内容操作；Wait/TextDelay/Fade 的参数为 60 Hz 内容帧，MusicFade 的参数为秒。
/// Background 的 argument 为此结局素材清单内的外观 ID，Color 为原生 BGR，文本借用静态 UTF-8 内容。
enum class Op { Background, TextDelay, Color, Text, Wait, WaitClear, FadeIn, FadeOut, MusicFade, Finish };
struct Event { Op op; int argument=0,extra=0; std::string_view text; };
/// @brief 仅借用静态演出表与素材清单；清单只包含该分支用到的背景和结局音乐，不包含制作人员素材。
[[nodiscard]] std::span<const Event> Events(Id id);
[[nodiscard]] const StageAssetManifest& Assets(Id id);

/// @brief 当前可见文本块；保留官方换行及各说话人的原生颜色，借用静态结局表。
struct Line { std::string_view text; Color color; };
/// @brief confirm 为单次边沿，held 为确认键持续状态；fastForward 只在已看过本结局时生效。
struct Input { bool confirm=false,held=false,fastForward=false; };
/// @brief 独占演出游标、文字与等待状态，不持有 GPU/音频资源；暂停时不调用 Update。
class Player {
public:
    /// @brief 重置并执行首批同步指令；seenBefore 控制 Ctrl 快进及直接跳过，不在这里写入存档。
    void Start(Id id,bool seenBefore);
    /// @brief seconds 为非负有限秒数；固定按 60 Hz 补算，不受显示帧率影响，确认边沿最多消费一次。
    void Update(double seconds,Input input={});
    /// @brief 仅已看过的结局可整体跳过；成功时进入完成态，调用方仍须完成后续成绩保存流程。
    bool Skip();
    [[nodiscard]] bool Finished() const { return finished_; }
    [[nodiscard]] bool SeenBefore() const { return seenBefore_; }
    [[nodiscard]] Id Variant() const { return id_; }
    [[nodiscard]] std::span<const Line> Lines() const { return lines_; }
    [[nodiscard]] VisualId Background() const { return background_; }
    [[nodiscard]] float WhiteOpacity() const;
    [[nodiscard]] float MusicOpacity() const;
    [[nodiscard]] std::size_t EventIndex() const { return cursor_; }
private:
    void Process();
    void Tick();
    Id id_=Id::ReimuA;
    std::span<const Event> events_;
    std::size_t cursor_=0;
    std::vector<Line> lines_;
    VisualId background_=kInvalidVisualId;
    Color color_{100,100,100,255};
    int longDelay_=70,shortDelay_=12,wait_=0,minimumWait_=0;
    int fadeFrames_=0,fadeAge_=0,musicFadeFrames_=0,musicFadeAge_=0;
    double remainingFrames_=0;
    bool fadeOut_=false,clearAfterWait_=false,seenBefore_=false,finished_=true;
    bool pendingConfirm_=false,confirmHeld_=false,fastForward_=false;
};
} // namespace koumakyou::ending

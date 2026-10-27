// 本文件实现新典结局条件和确定性的内容帧播放，不依赖关卡、窗口输入或可执行文件。
#include "narrative/ending.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace koumakyou::ending {
namespace {
#include "narrative/ending_content.inc"

constexpr VisualDefinition Background(int id,std::string_view path) {
    return {.id=static_cast<VisualId>(id),.path=path};
}
constexpr std::array kReimuAImages{
    Background(0,"endings/native/end00.dds"),Background(1,"endings/native/end01.dds"),
    Background(2,"endings/native/end02.dds"),Background(3,"endings/native/end03.dds")};
constexpr std::array kReimuBImages{
    Background(0,"endings/native/end00.dds"),Background(4,"endings/native/end04.dds"),
    Background(5,"endings/native/end05.dds"),Background(6,"endings/native/end06.dds")};
constexpr std::array kMarisaAImages{
    Background(7,"endings/native/end07.dds"),Background(8,"endings/native/end08.dds"),
    Background(9,"endings/native/end09.dds"),Background(10,"endings/native/end10.dds")};
constexpr std::array kMarisaBImages{
    Background(7,"endings/native/end07.dds"),Background(11,"endings/native/end11.dds"),
    Background(12,"endings/native/end12.dds"),Background(13,"endings/native/end13.dds")};
constexpr std::array kBadImages{Background(14,"endings/native/end00b.dds")};
const std::array<StageAssetManifest,6> kAssets{{
    {kReimuAImages,"endings/ending.mp3",{},{}},
    {kReimuBImages,"endings/ending.mp3",{},{}},
    {kMarisaAImages,"endings/ending.mp3",{},{}},
    {kMarisaBImages,"endings/ending.mp3",{},{}},
    {kBadImages,"endings/ending.mp3",{},{}},
    {kBadImages,"endings/ending.mp3",{},{}}
}};
constexpr std::array<std::string_view,6> kTitles{
    "灵梦 · 灵之御札结局","灵梦 · 梦之御札结局","魔理沙 · 魔之御札结局","魔理沙 · 恋之御札结局",
    "灵梦 · Bad Ending","魔理沙 · Bad Ending"};
constexpr std::array<std::string_view,6> kScripts{"end00.end","end01.end","end10.end","end11.end","end00b.end","end10b.end"};
std::size_t Index(Id id) {
    const auto index=static_cast<std::size_t>(id);
    if(index>=kAssets.size())throw std::invalid_argument("Invalid ending id");
    return index;
}
} // namespace

std::optional<Id> Select(const Request& r) {
    if(!r.completedMainline || r.shot<0 || r.shot>=4 || r.difficulty<0 || r.difficulty>=4 || r.continues<0 || r.misses<0)
        return std::nullopt;
    if(r.difficulty==0 || r.continues>0 || (r.challenge && r.misses>5))
        return r.shot<2 ? Id::ReimuBad : Id::MarisaBad;
    return static_cast<Id>(r.shot);
}
std::string_view Title(Id id) { return kTitles[Index(id)]; }
std::string_view SourceScript(Id id) { return kScripts[Index(id)]; }
const StageAssetManifest& Assets(Id id) { return kAssets[Index(id)]; }
std::span<const Event> Events(Id id) {
    switch(id) {
        case Id::ReimuA:return kEnd00;
        case Id::ReimuB:return kEnd01;
        case Id::MarisaA:return kEnd10;
        case Id::MarisaB:return kEnd11;
        case Id::ReimuBad:return kEnd00b;
        case Id::MarisaBad:return kEnd10b;
        default:throw std::invalid_argument("Invalid ending id");
    }
}
void Player::Start(Id id,bool seenBefore) {
    *this=Player{};id_=id;events_=Events(id);seenBefore_=seenBefore;finished_=false;Process();
}
void Player::Process() {
    while(!finished_ && wait_==0 && cursor_<events_.size()) {
        const auto& e=events_[cursor_++];
        switch(e.op) {
            case Op::Background:background_=static_cast<VisualId>(e.argument);break;
            case Op::TextDelay:longDelay_=e.argument;shortDelay_=e.extra;break;
            case Op::Color:color_={static_cast<unsigned char>(e.argument&255),static_cast<unsigned char>((e.argument>>8)&255),
                                  static_cast<unsigned char>((e.argument>>16)&255),255};break;
            case Op::Text:
                lines_.push_back({e.text,color_});wait_=minimumWait_=confirmHeld_?shortDelay_:longDelay_;break;
            case Op::Wait:case Op::WaitClear:
                wait_=e.argument;minimumWait_=e.extra;clearAfterWait_=e.op==Op::WaitClear;break;
            case Op::FadeIn:case Op::FadeOut:
                fadeFrames_=e.argument;fadeAge_=0;fadeOut_=e.op==Op::FadeOut;break;
            case Op::MusicFade:musicFadeFrames_=e.argument*60;musicFadeAge_=0;break;
            case Op::Finish:finished_=true;break;
        }
    }
}
void Player::Tick() {
    if(fadeAge_<fadeFrames_)++fadeAge_;
    if(musicFadeAge_<musicFadeFrames_)++musicFadeAge_;
    if(wait_>0) {
        --wait_;if(minimumWait_>0)--minimumWait_;
        if(minimumWait_==0 && (pendingConfirm_ || (seenBefore_ && fastForward_)))wait_=0;
        if(wait_==0 && clearAfterWait_) {lines_.clear();clearAfterWait_=false;}
    }
    pendingConfirm_=false;
    Process();
}
void Player::Update(double seconds,Input input) {
    if(!std::isfinite(seconds) || seconds<0)throw std::invalid_argument("Invalid ending delta time");
    if(finished_)return;
    pendingConfirm_=pendingConfirm_||input.confirm;confirmHeld_=input.held;fastForward_=input.fastForward;
    remainingFrames_+=seconds*60;
    while(!finished_ && remainingFrames_+1e-9>=1) {remainingFrames_-=1;Tick();}
}
bool Player::Skip() {if(!seenBefore_ || finished_)return false;finished_=true;return true;}
float Player::WhiteOpacity() const {
    if(fadeFrames_<=0)return fadeOut_?1.0F:0.0F;
    const float progress=std::clamp(static_cast<float>(fadeAge_)/fadeFrames_,0.0F,1.0F);
    return fadeOut_?progress:1-progress;
}
float Player::MusicOpacity() const {
    return musicFadeFrames_>0 ? std::clamp(1-static_cast<float>(musicFadeAge_)/musicFadeFrames_,0.0F,1.0F) : 1.0F;
}
} // namespace koumakyou::ending

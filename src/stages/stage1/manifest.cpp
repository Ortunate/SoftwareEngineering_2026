// 本文件负责第一面专用贴图、音乐和音效的素材清单。

#include "stages/stage1/data.hpp"
#include "stages/stage1/ids.hpp"

#include <array>

namespace koumakyou::stage1 {
namespace {
// 对话截取全身图上部；横向收拢灵梦全身图的裙摆留白，不改变公告板的原纹理或判定锚点。
constexpr Rectangle kReimuFullDialogueCrop{0.20F, 0, 0.80F, 0.60F};
constexpr Rectangle kRumiaDialogueCrop{0, 0, 1, 0.60F};
constexpr std::array<VisualDefinition, 21> kVisuals{{
    {kBlueBullet, "stages/stage1/bullet_pearl_runtime.png", VisualMode::Bullet, 1.0F,
     {0.5F, 0.5F}, 2.6F, true},
    {kRedBullet, "stages/stage1/bullet_red_runtime.png", VisualMode::Bullet, 1.0F,
     {0.5F, 0.5F}, 2.6F, true},
    {kGreenSphere, {}, VisualMode::Sphere},
    {kFairy, "stages/stage1/fairy_modern.png", VisualMode::Character, 1.8F,
     {0.51F, 0.40F}},
    {kDarkFairy, "stages/stage1/dark_fairy_modern.png", VisualMode::Character, 1.8F,
     {0.50F, 0.45F}},
    {kOrb, "stages/stage1/bullet_pearl_runtime.png", VisualMode::Character, 1.488F},
    {kRumia, "stages/stage1/portraits/rumia/face03a_0.png", VisualMode::Character, 3.2F,
     {0.70F, 0.36F}, 2.6F, false, kRumiaDialogueCrop},
    {kReimuPortrait, "stages/stage1/portraits/reimu/face00a_0.png", VisualMode::Character},
    {kPlayerGoldCard, "stages/stage1/spell_card_gold.png", VisualMode::Bullet,
     1.0F, {0.5F, 0.5F}, 6.0F},
    {kPlayerCyanCard, "stages/stage1/spell_card_cyan.png", VisualMode::Bullet,
     1.0F, {0.5F, 0.5F}, 6.0F},
    {kPowerPoint, "stages/stage1/power_point.png", VisualMode::Character,
     0.75F},
    {kScorePoint, "stages/stage1/score_point.png", VisualMode::Character,
     0.75F},
    {kStarPoint, "stages/stage1/star_point.png", VisualMode::Character,
     0.75F},
    {kReimuPortraits[1], "stages/stage1/portraits/reimu/face00a_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kReimuFullDialogueCrop},
    {kReimuPortraits[2], "stages/stage1/portraits/reimu/face00b_0.png", VisualMode::Character},
    {kReimuPortraits[3], "stages/stage1/portraits/reimu/face00b_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kReimuFullDialogueCrop},
    {kReimuPortraits[4], "stages/stage1/portraits/reimu/face00c_0.png", VisualMode::Character},
    {kReimuPortraits[5], "stages/stage1/portraits/reimu/face00c_1.png", VisualMode::Character},
    {kRumiaPortraits[1], "stages/stage1/portraits/rumia/face03a_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kRumiaDialogueCrop},
    {kRumiaPortraits[2], "stages/stage1/portraits/rumia/face03b_0.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kRumiaDialogueCrop},
    {kRumiaPortraits[3], "stages/stage1/portraits/rumia/face03b_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kRumiaDialogueCrop},
}};
constexpr std::array<SoundDefinition, 0> kSounds{};
const StageAssetManifest kAssets{
    kVisuals, "stages/stage1/宛如鬼灯的鲜红之魂.mp3",
    "stages/stage1/妖魔夜行.mp3", kSounds};
}  // namespace

const StageAssetManifest& Stage1Manifest() { return kAssets; }
}  // namespace koumakyou::stage1

// 本文件负责第一面专用贴图、音乐和音效的素材清单。

#include "stages/stage1/data.hpp"
#include "stages/stage1/ids.hpp"

#include <array>

namespace koumakyou::stage1 {
namespace {
constexpr std::array<VisualDefinition, 13> kVisuals{{
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
    {kRumia, "stages/stage1/rumia_cutout.png", VisualMode::Character, 3.2F,
     {0.50F, 0.36F}},
    {kReimuPortrait, "stages/stage1/reimu_cutout.png", VisualMode::Character},
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
}};
constexpr std::array<SoundDefinition, 0> kSounds{};
const StageAssetManifest kAssets{
    kVisuals, "stages/stage1/宛如鬼灯的鲜红之魂.mp3",
    "stages/stage1/妖魔夜行.mp3", kSounds};
}  // namespace

const StageAssetManifest& Stage1Manifest() { return kAssets; }
}  // namespace koumakyou::stage1

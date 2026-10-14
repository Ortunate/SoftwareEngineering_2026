// 本文件负责第一面专用贴图、音乐和音效的素材清单。

#include "stages/stage1/data.hpp"
#include "stages/stage1/ids.hpp"

#include <array>
#include <vector>

namespace koumakyou::stage1 {
namespace {
// 对话截取全身图上部；横向收拢灵梦全身图的裙摆留白，不改变公告板的原纹理或判定锚点。
constexpr Rectangle kReimuFullDialogueCrop{0.20F, 0, 0.80F, 0.60F};
constexpr Rectangle kRumiaDialogueCrop{0, 0, 1, 0.60F};
const std::vector<VisualDefinition> kVisuals=[] {
    std::vector<VisualDefinition> visuals{
    {.id=kBlueBullet, .mode=VisualMode::Bullet, .bulletDiameterScale=4.0F, .animation=battle::EnemyBullet(3,6)},
    {.id=kRedBullet, .mode=VisualMode::Bullet, .bulletDiameterScale=4.0F, .animation=battle::EnemyBullet(3,2)},
    {.id=kGreenSphere, .mode=VisualMode::Bullet, .bulletDiameterScale=4.0F, .animation=battle::EnemyBullet(3,10)},
    {.id=kFairy, .worldHeight=1.8F, .animation={battle::Atlas::Stage1Enemies,0}, .defeatVisual=kNativeDefeat},
    {.id=kDarkFairy, .worldHeight=1.8F, .animation={battle::Atlas::Stage1Enemies,3}, .defeatVisual=kNativeDefeat},
    {.id=kOrb, .worldHeight=1.488F, .animation=battle::EnemyBullet(6,3), .defeatVisual=kNativeDefeat},
    {.id=kRumia, .worldHeight=3.2F, .pivot={0.5F,0.5F}, .animation={battle::Atlas::Rumia,128}, .defeatVisual=kNativeDefeat},
    {kReimuPortrait, "stages/stage1/portraits/reimu/face00a_0.png", VisualMode::Character},
    {.id=kPlayerGoldCard, .mode=VisualMode::Bullet, .bulletDiameterScale=6.0F, .animation={battle::Atlas::Reimu,64}},
    {.id=kPlayerCyanCard, .mode=VisualMode::Bullet, .bulletDiameterScale=6.0F, .animation={battle::Atlas::Reimu,65}},
    {.id=kPowerPoint, .worldHeight=0.75F, .animation=battle::Item(0)},
    {.id=kScorePoint, .worldHeight=0.75F, .animation=battle::Item(1)},
    {.id=kStarPoint, .worldHeight=0.75F, .animation=battle::Item(6)},
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
    {kRumiaPortraits[0], "stages/stage1/portraits/rumia/face03a_0.png", VisualMode::Character,
     1.0F, {0.5F,0.5F}, 2.6F, false, kRumiaDialogueCrop},
    };
    for(int type=0;type<10;++type)for(int color=0;color<16;++color)
        visuals.push_back({.id=NativeBulletVisual(type,color), .mode=VisualMode::Bullet,
                           .bulletDiameterScale=4.0F, .animation=battle::EnemyBullet(type,color)});
    visuals.push_back({.id=kLargePowerPoint, .worldHeight=0.95F, .animation=battle::Item(2)});
    for(int color=0;color<16;++color)
        visuals.push_back({.id=static_cast<VisualId>(kNativeLaserBase+color), .mode=VisualMode::Bullet,
                           .animation={battle::Atlas::BeamBullets,0,color}});
    for(int effect=0;effect<3;++effect)
        visuals.push_back({.id=static_cast<VisualId>(kNativeDefeat+effect), .worldHeight=1.2F,
                           .animation={battle::Atlas::BulletEffects,3+effect}});
    return visuals;
}();
constexpr std::array<SoundDefinition, 0> kSounds{};
const StageAssetManifest kAssets{
    kVisuals, "stages/stage1/宛如鬼灯的鲜红之魂.mp3",
    "stages/stage1/妖魔夜行.mp3", kSounds};
}  // namespace

const StageAssetManifest& Stage1Manifest() { return kAssets; }
}  // namespace koumakyou::stage1

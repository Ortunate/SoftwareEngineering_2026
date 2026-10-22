// 本文件负责第一面专用贴图、音乐和音效的素材清单。

#include "stages/stage1/data.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/actors/player.hpp"

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
    visuals.push_back({.id=kPlayerOptionLeft, .worldHeight=Player::kOptionWorldHeight, .animation={battle::Atlas::Reimu,128}});
    visuals.push_back({.id=kPlayerOptionRight, .worldHeight=Player::kOptionWorldHeight, .animation={battle::Atlas::Reimu,129}});
    // 针裁图高 104 逻辑像素，沿用太极圈 36 像素的世界比例；判定半径由 Player 固定为 0.09。
    visuals.push_back({.id=kPlayerNeedle, .mode=VisualMode::Bullet,
                       .bulletDiameterScale=(104.0F/36)*Player::kOptionWorldHeight/0.09F,
                       .animation={battle::Atlas::Reimu,66}});
    visuals.push_back({.id=kMarisaPrimary, .mode=VisualMode::Bullet, .bulletDiameterScale=8.0F,
                       .animation={battle::Atlas::Marisa,64}});
    for(int level=0;level<4;++level)
        visuals.push_back({.id=static_cast<VisualId>(kMarisaMissile+level), .mode=VisualMode::Bullet,
                           .bulletDiameterScale=8.0F, .animation={battle::Atlas::Marisa,65+level}});
    for(int level=0;level<3;++level)
        visuals.push_back({.id=static_cast<VisualId>(kMarisaLaser+level), .mode=VisualMode::Bullet,
                           .animation={battle::Atlas::Marisa,69+level}});
    for(int level=0;level<4;++level)
        visuals.push_back({.id=static_cast<VisualId>(kMarisaExplosion+level), .mode=VisualMode::Bullet,
                           .bulletDiameterScale=2.0F, .animation={battle::Atlas::Marisa,97+level}});
    visuals.push_back({.id=kMarisaOptionLeft, .worldHeight=Player::kOptionWorldHeight, .animation={battle::Atlas::Marisa,128}});
    visuals.push_back({.id=kMarisaOptionRight, .worldHeight=Player::kOptionWorldHeight, .animation={battle::Atlas::Marisa,129}});
    constexpr std::array<std::string_view,6> marisaFaces{
        "common/portraits/marisa/face01a_0.png","common/portraits/marisa/face01a_1.png",
        "common/portraits/marisa/face01b_0.png","common/portraits/marisa/face01b_1.png",
        "common/portraits/marisa/face01c_0.png","common/portraits/marisa/face01c_1.png"};
    for(int expression=0;expression<6;++expression)
        visuals.push_back({.id=kMarisaPortraits[expression], .path=marisaFaces[expression], .mode=VisualMode::Character,
                           .dialogueCrop=(expression%2==1 ? Rectangle{0.20F,0,0.80F,0.60F} : Rectangle{0,0,1,1})});
    return visuals;
}();
constexpr std::array<SoundDefinition, 0> kSounds{};
const StageAssetManifest kAssets{
    kVisuals, "stages/stage1/宛如鬼灯的鲜红之魂.mp3",
    "stages/stage1/妖魔夜行.mp3", kSounds};
}  // namespace

const StageAssetManifest& Stage1Manifest() { return kAssets; }
}  // namespace koumakyou::stage1

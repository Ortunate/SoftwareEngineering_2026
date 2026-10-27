// 本文件配置公共玩家、弹幕、道具与自机立绘；演员和 Boss 素材由各关补齐。
#include "stages/common/assets.hpp"
#include "stages/common/ids.hpp"
#include "gameplay/effects/player_bomb.hpp"
#include "gameplay/actors/player.hpp"
#include <array>
namespace koumakyou::stage_assets {
std::vector<VisualDefinition> CommonBattleVisuals() {
    constexpr Rectangle kReimuFullDialogueCrop{0.20F,0,0.80F,0.60F};
    std::vector<VisualDefinition> visuals{
    {.id=kBlueBullet, .mode=VisualMode::Bullet, .animation=battle::EnemyBullet(3,6)},
    {.id=kRedBullet, .mode=VisualMode::Bullet, .animation=battle::EnemyBullet(3,2)},
    {.id=kGreenSphere, .mode=VisualMode::Bullet, .animation=battle::EnemyBullet(3,10)},
    {kReimuPortrait, "stages/stage1/portraits/reimu/face00a_0.png", VisualMode::Character},
    {.id=kPlayerGoldCard, .mode=VisualMode::Bullet, .animation={battle::Atlas::Reimu,64}},
    {.id=kPlayerCyanCard, .mode=VisualMode::Bullet, .animation={battle::Atlas::Reimu,65}},
    {.id=kPowerPoint, .animation=battle::Item(0)},
    {.id=kScorePoint, .animation=battle::Item(1)},
    {.id=kStarPoint, .animation=battle::Item(6)},
    {kReimuPortraits[1], "stages/stage1/portraits/reimu/face00a_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kReimuFullDialogueCrop},
    {kReimuPortraits[2], "stages/stage1/portraits/reimu/face00b_0.png", VisualMode::Character},
    {kReimuPortraits[3], "stages/stage1/portraits/reimu/face00b_1.png", VisualMode::Character,
     1.0F, {0.5F, 0.5F}, 2.6F, false, kReimuFullDialogueCrop},
    {kReimuPortraits[4], "stages/stage1/portraits/reimu/face00c_0.png", VisualMode::Character},
    {kReimuPortraits[5], "stages/stage1/portraits/reimu/face00c_1.png", VisualMode::Character},
    };
    for(int type=0;type<10;++type)for(int color=0;color<16;++color)
        visuals.push_back({.id=NativeBulletVisual(type,color), .mode=VisualMode::Bullet,
                           .animation=battle::EnemyBullet(type,color)});
    visuals.push_back({.id=kLargePowerPoint, .animation=battle::Item(2)});
    visuals.push_back({.id=kBombPoint,.animation=battle::Item(3)});
    visuals.push_back({.id=kFullPowerPoint,.animation=battle::Item(4)});
    visuals.push_back({.id=kLifePoint,.animation=battle::Item(5)});
    for(int color=0;color<16;++color)
        visuals.push_back({.id=static_cast<VisualId>(kNativeLaserBase+color), .mode=VisualMode::Bullet,
                           .animation={battle::Atlas::BeamBullets,0,color}});
    for(int effect=0;effect<3;++effect)
        visuals.push_back({.id=static_cast<VisualId>(kNativeDefeat+effect), .animation={battle::Atlas::BulletEffects,3+effect}});
    visuals.push_back({.id=kPlayerOptionLeft, .animation={battle::Atlas::Reimu,128}});
    visuals.push_back({.id=kPlayerOptionRight, .animation={battle::Atlas::Reimu,129}});
    // 原生针的显示尺寸为 14×46 逻辑像素；裁图 32×104 只用于 UV，判定另读火力表。
    visuals.push_back({.id=kPlayerNeedle, .mode=VisualMode::Bullet,
                       .animation={battle::Atlas::Reimu,66}});
    visuals.push_back({.id=kMarisaPrimary, .mode=VisualMode::Bullet,
                       .animation={battle::Atlas::Marisa,64}});
    for(int level=0;level<4;++level)
        visuals.push_back({.id=static_cast<VisualId>(kMarisaMissile+level), .mode=VisualMode::Bullet,
                           .animation={battle::Atlas::Marisa,65+level}});
    for(int level=0;level<3;++level)
        visuals.push_back({.id=static_cast<VisualId>(kMarisaLaser+level), .mode=VisualMode::Bullet,
                           .animation={battle::Atlas::Marisa,69+level}});
    for(int level=0;level<4;++level)
        visuals.push_back({.id=static_cast<VisualId>(kMarisaExplosion+level), .mode=VisualMode::Bullet,
                           .animation={battle::Atlas::Marisa,97+level}});
    visuals.push_back({.id=kMarisaOptionLeft, .animation={battle::Atlas::Marisa,128}});
    visuals.push_back({.id=kMarisaOptionRight, .animation={battle::Atlas::Marisa,129}});
    constexpr std::array<std::string_view,6> marisaFaces{
        "common/portraits/marisa/face01a_0.png","common/portraits/marisa/face01a_1.png",
        "common/portraits/marisa/face01b_0.png","common/portraits/marisa/face01b_1.png",
        "common/portraits/marisa/face01c_0.png","common/portraits/marisa/face01c_1.png"};
    for(int expression=0;expression<6;++expression)
        visuals.push_back({.id=kMarisaPortraits[expression], .path=marisaFaces[expression], .mode=VisualMode::Character,
                           // 魔理沙六张重绘均为全身图，同一上部取景避免切换到偶数差分时人物突然缩小。
                           .dialogueCrop={0.20F,0,0.80F,0.60F}});
    for(const auto shot:{ShotType::HomingAmulet,ShotType::Needle,ShotType::MagicMissile,ShotType::IllusionLaser}) {
        const auto& bomb=BombDefinition(shot);
        const VisualId first=shot==ShotType::HomingAmulet ? kReimuBombSeal : shot==ShotType::Needle ? kReimuBombBarrier :
                             shot==ShotType::MagicMissile ? kMarisaBombStars : kMarisaBombSpark;
        for(int layer=0;layer<bomb.layers;++layer)
            visuals.push_back({.id=static_cast<VisualId>(first+layer),.animation={bomb.atlas,bomb.firstScript+layer}});
    }
    return visuals;
}
}

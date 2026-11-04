// 本文件将公共玩家素材槽接入四御札的弹、Bomb、副机与角色立绘接口。
#include "stages/common/content.hpp"
namespace koumakyou::stage_assets {
std::array<VisualId,4> NativeBattleContent::PlayerBombVisuals(ShotType shot) const {
    const VisualId first=shot==ShotType::HomingAmulet ? kReimuBombSeal : shot==ShotType::Needle ? kReimuBombBarrier :
                         shot==ShotType::MagicMissile ? kMarisaBombStars : kMarisaBombSpark;
    return {first,static_cast<VisualId>(first+1),static_cast<VisualId>(first+2),
            shot==ShotType::MagicMissile ? kInvalidVisualId : static_cast<VisualId>(first+3)};
}

PlayerShotVisuals NativeBattleContent::PlayerVisuals(ShotType shot) const {
    auto result=StageContent::PlayerVisuals(shot);
    if(shot!=ShotType::MagicMissile && shot!=ShotType::IllusionLaser)return result;
    result.primary=kMarisaPrimary;
    for(int index=0;index<4;++index) {
        result.secondary[index]=static_cast<VisualId>(shot==ShotType::MagicMissile ? kMarisaMissile+index : kMarisaLaser+std::min(index,2));
        result.impact[index]=static_cast<VisualId>(kMarisaExplosion+index);
    }
    return result;
}

VisualId NativeBattleContent::HeroPortraitVisual(int expression, ShotType shot) const {
    if(shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser)
        return expression>=0 && expression<static_cast<int>(kMarisaPortraits.size()) ? kMarisaPortraits[expression] : kMarisaPortraits.front();
    return expression >= 0 && expression < static_cast<int>(kReimuPortraits.size())
        ? kReimuPortraits[expression] : kReimuPortraits.front();
}
}

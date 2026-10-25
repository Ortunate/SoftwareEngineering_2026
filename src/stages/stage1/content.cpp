// 本文件实现第一面的素材、时间轴、对话和 Boss 工厂，普通演员由时间轴直接配置。

#include "stages/stage1/content.hpp"
#include "stages/stage1/data.hpp"

#include "stages/stage1/boss.hpp"


namespace koumakyou::stage1 {
std::array<VisualId,4> Stage1Content::PlayerBombVisuals(ShotType shot) const {
    const VisualId first=shot==ShotType::HomingAmulet ? kReimuBombSeal : shot==ShotType::Needle ? kReimuBombBarrier :
                         shot==ShotType::MagicMissile ? kMarisaBombStars : kMarisaBombSpark;
    return {first,static_cast<VisualId>(first+1),static_cast<VisualId>(first+2),
            shot==ShotType::MagicMissile ? kInvalidVisualId : static_cast<VisualId>(first+3)};
}

const StageAssetManifest& Stage1Content::Assets() const { return Stage1Manifest(); }
std::span<const StageEvent> Stage1Content::Events() const { return Stage1Timeline(); }
std::span<const DialogueLine> Stage1Content::Dialogue(bool afterBattle, ShotType shot) const {
    return Stage1Dialogue(afterBattle,shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser);
}
std::span<const DialogueCue> Stage1Content::DialogueCues(bool afterBattle, ShotType shot) const {
    return Stage1DialogueCues(afterBattle,shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser);
}

PlayerShotVisuals Stage1Content::PlayerVisuals(ShotType shot) const {
    auto result=StageContent::PlayerVisuals(shot);
    if(shot!=ShotType::MagicMissile && shot!=ShotType::IllusionLaser)return result;
    result.primary=kMarisaPrimary;
    for(int index=0;index<4;++index) {
        result.secondary[index]=static_cast<VisualId>(shot==ShotType::MagicMissile ? kMarisaMissile+index : kMarisaLaser+std::min(index,2));
        result.impact[index]=static_cast<VisualId>(kMarisaExplosion+index);
    }
    return result;
}

VisualId Stage1Content::HeroPortraitVisual(int expression, ShotType shot) const {
    if(shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser)
        return expression>=0 && expression<static_cast<int>(kMarisaPortraits.size()) ? kMarisaPortraits[expression] : kMarisaPortraits.front();
    return expression >= 0 && expression < static_cast<int>(kReimuPortraits.size())
        ? kReimuPortraits[expression] : kReimuPortraits.front();
}

VisualId Stage1Content::BossPortraitVisual(int expression) const {
    return expression >= 0 && expression < static_cast<int>(kRumiaPortraits.size())
        ? kRumiaPortraits[expression] : kRumiaPortraits.front();
}

std::unique_ptr<Boss> Stage1Content::CreateBoss(bool final) const {
    auto boss = std::make_unique<RumiaBoss>();
    if (final) boss->StartFinalBoss();
    else boss->StartMidboss();
    return boss;
}

}  // namespace koumakyou::stage1

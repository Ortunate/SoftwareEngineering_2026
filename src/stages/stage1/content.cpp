// 本文件实现第一面的素材、时间轴、对话和 Boss 工厂，普通演员由时间轴直接配置。

#include "stages/stage1/content.hpp"
#include "stages/stage1/data.hpp"

#include "stages/stage1/boss_config.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/scenery.hpp"


namespace koumakyou::stage1 {

behavior::Spawn Stage1Content::ConfigureSpawn(const behavior::Spawn& spawn) const {
    return stage1::ConfigureSpawn(spawn,Difficulty());
}


const StageAssetManifest& Stage1Content::Assets() const { return Stage1Manifest(); }
void Stage1Content::DrawScenery(float seconds,const AssetManager&) const { stage1::DrawScenery(seconds); }
std::span<const StageEvent> Stage1Content::Events() const { return Stage1Timeline(); }
std::span<const DialogueLine> Stage1Content::Dialogue(bool afterBattle, ShotType shot) const {
    return Stage1Dialogue(afterBattle,shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser);
}
std::span<const DialogueCue> Stage1Content::DialogueCues(bool afterBattle, ShotType shot) const {
    return Stage1DialogueCues(afterBattle,shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser);
}





VisualId Stage1Content::BossPortraitVisual(int expression) const {
    return expression >= 0 && expression < static_cast<int>(kRumiaPortraits.size())
        ? kRumiaPortraits[expression] : kRumiaPortraits.front();
}

const bosses::Battle& Stage1Content::PracticeBoss() const {return kRumiaFinal;}
const bosses::Battle& Stage1Content::PracticeBoss(int spellCard) const {
    const int difficulty=native_combat::DifficultyIndex(Difficulty());
    if(difficulty>=2&&spellCard==difficulty-1)return kRumiaMidboss;
    if((difficulty>=1&&spellCard==difficulty+2)||spellCard==difficulty+6)return kRumiaFinal;
    throw std::invalid_argument("Stage 1 spell does not match difficulty");
}

}  // namespace koumakyou::stage1

// 本文件将二面静态内容接入 StageContent；Boss 工厂只选择遭遇，不按原始子程序编号分派行为。
#include "stages/stage2/content.hpp"
#include "stages/stage2/data.hpp"
#include "stages/stage2/boss_config.hpp"
#include "stages/stage2/scenery.hpp"
namespace koumakyou::stage2 {
const StageAssetManifest& Stage2Content::Assets() const { return Stage2Manifest(); }
void Stage2Content::DrawScenery(float seconds,const AssetManager&) const { stage2::DrawScenery(seconds); }
std::span<const StageEvent> Stage2Content::Events() const { return Stage2Timeline(); }
const bosses::Battle& Stage2Content::PracticeBoss() const {return kCirnoFinal;}
const bosses::Battle& Stage2Content::PracticeBoss(int spellCard) const {
    const int difficulty=native_combat::DifficultyIndex(Difficulty());
    if(spellCard==10+difficulty||spellCard==14+difficulty||
       (difficulty>0&&spellCard==17+difficulty))return kCirnoFinal;
    throw std::invalid_argument("Stage 2 spell does not match difficulty");
}
std::span<const DialogueLine> Stage2Content::Dialogue(bool afterBattle,ShotType shot) const {
    return Stage2Dialogue(afterBattle,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
std::span<const DialogueCue> Stage2Content::DialogueCues(bool afterBattle,ShotType shot) const {
    return Stage2DialogueCues(afterBattle,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
VisualId Stage2Content::BossPortraitVisual(int expression) const {
    return expression>=0&&expression<static_cast<int>(kCirnoPortraits.size()) ? kCirnoPortraits[expression] : kCirnoPortraits.front();
}
}

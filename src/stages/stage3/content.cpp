// 本文件将三面独立时间轴、红美铃工厂、分遭遇对话及场景接入内容接口。
#include "stages/stage3/content.hpp"
#include "stages/stage3/data.hpp"
#include "stages/stage3/boss_config.hpp"
#include "stages/stage3/scenery.hpp"
namespace koumakyou::stage3 {
const StageAssetManifest& Stage3Content::Assets() const {return Stage3Manifest();}
void Stage3Content::DrawScenery(float seconds,const AssetManager&) const {stage3::DrawScenery(seconds);}
std::span<const StageEvent> Stage3Content::Events() const {return Stage3Timeline();}
const bosses::Battle& Stage3Content::PracticeBoss() const {return kMeilingFinal;}
const bosses::Battle& Stage3Content::PracticeBoss(int spellCard) const {
    const int difficulty=native_combat::DifficultyIndex(Difficulty());
    if(spellCard==21+difficulty)return kMeilingMidboss;
    if(spellCard==25+difficulty||spellCard==31+difficulty||
       (difficulty>=2&&spellCard==27+difficulty)||
       (difficulty>=1&&spellCard==34+difficulty))return kMeilingFinal;
    throw std::invalid_argument("Stage 3 spell does not match difficulty");
}
std::span<const DialogueLine> Stage3Content::Dialogue(bool after,ShotType shot) const {return SceneDialogue(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueCue> Stage3Content::DialogueCues(bool after,ShotType shot) const {return SceneDialogueCues(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueLine> Stage3Content::SceneDialogue(DialogueScene scene,bool after,ShotType shot) const {
    return Stage3Dialogue(scene,after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
std::span<const DialogueCue> Stage3Content::SceneDialogueCues(DialogueScene scene,bool after,ShotType shot) const {
    return Stage3DialogueCues(scene,after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
VisualId Stage3Content::BossPortraitVisual(int expression) const {
    return kMeilingPortraits[expression>=0&&expression<4 ? expression : 0];
}
}

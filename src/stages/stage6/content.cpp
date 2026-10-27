// 本文件将六面独立时间轴、蕾米莉亚·斯卡蕾特工厂、分遭遇对话及场景接入内容接口。
#include "stages/stage6/content.hpp"
#include "stages/stage6/data.hpp"
#include "stages/stage6/boss.hpp"
#include "stages/stage6/scenery.hpp"
namespace koumakyou::stage6 {
const StageAssetManifest& Stage6Content::Assets() const {return Stage6Manifest();}
void Stage6Content::DrawScenery(float seconds) const {stage6::DrawScenery(seconds);}
std::span<const StageEvent> Stage6Content::Events() const {return Stage6Timeline();}
std::unique_ptr<Boss> Stage6Content::CreateBoss(bool final) const {return std::make_unique<Stage6Boss>(final);}
std::span<const DialogueLine> Stage6Content::Dialogue(bool after,ShotType shot) const {return SceneDialogue(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueCue> Stage6Content::DialogueCues(bool after,ShotType shot) const {return SceneDialogueCues(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueLine> Stage6Content::SceneDialogue(DialogueScene scene,bool after,ShotType shot) const {
    return Stage6Dialogue(scene,after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
std::span<const DialogueCue> Stage6Content::SceneDialogueCues(DialogueScene scene,bool after,ShotType shot) const {
    return Stage6DialogueCues(scene,after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
VisualId Stage6Content::SceneBossPortraitVisual(DialogueScene scene,int expression) const {
    return scene==DialogueScene::Midboss ? kSakuyaPortraits[expression==1 ? 1 : 0] : BossPortraitVisual(expression);
}
VisualId Stage6Content::BossPortraitVisual(int expression) const {
    return kRemiliaPortraits[expression>=0&&expression<4 ? expression : 0];
}
}

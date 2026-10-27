// 本文件将五面独立时间轴、十六夜咲夜工厂、分遭遇对话及场景接入内容接口。
#include "stages/stage5/content.hpp"
#include "stages/stage5/data.hpp"
#include "stages/stage5/boss.hpp"
#include "stages/stage5/scenery.hpp"
namespace koumakyou::stage5 {
const StageAssetManifest& Stage5Content::Assets() const {return Stage5Manifest();}
void Stage5Content::DrawScenery(float seconds) const {stage5::DrawScenery(seconds);}
std::span<const StageEvent> Stage5Content::Events() const {return Stage5Timeline();}
std::unique_ptr<Boss> Stage5Content::CreateBoss(bool final) const {return std::make_unique<Stage5Boss>(final);}
std::span<const DialogueLine> Stage5Content::Dialogue(bool after,ShotType shot) const {return SceneDialogue(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueCue> Stage5Content::DialogueCues(bool after,ShotType shot) const {return SceneDialogueCues(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueLine> Stage5Content::SceneDialogue(DialogueScene scene,bool after,ShotType shot) const {
    return Stage5Dialogue(scene,after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
std::span<const DialogueCue> Stage5Content::SceneDialogueCues(DialogueScene scene,bool after,ShotType shot) const {
    return Stage5DialogueCues(scene,after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
VisualId Stage5Content::BossPortraitVisual(int expression) const {
    return kSakuyaPortraits[expression>=0&&expression<4 ? expression : 0];
}
}

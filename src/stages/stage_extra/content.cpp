// 本文件实现Extra对白分派、单符入口与源取得计分；条件判断只读本局快照，不借用存档或修改游戏状态。
#include "stages/stage_extra/content.hpp"
#include "stages/stage_extra/manifest.hpp"
#include "stages/stage_extra/timeline.hpp"
#include "stages/stage_extra/boss_config.hpp"
#include "stages/stage_extra/ids.hpp"
#include "stages/stage_extra/scenery.hpp"
#include "resources/asset_manager.hpp"
namespace koumakyou::stage_extra {
namespace {
const DialogueScript& Script(DialogueScene scene,bool after,ShotType shot) {
    switch(scene) {
        case DialogueScene::FinalBoss:return ExtraDialogue(after?DialogueSection::MainAfter:DialogueSection::MainBefore,shot);
        case DialogueScene::Midboss:
            if(after)throw std::invalid_argument("Extra has no midboss postfight dialogue");
            return ExtraDialogue(DialogueSection::Midboss,shot);
        case DialogueScene::EncoreBoss:return ExtraDialogue(after?DialogueSection::EncoreAfter:DialogueSection::EncoreBefore,shot);
    }
    throw std::invalid_argument("Invalid Extra dialogue scene");
}
}
const StageAssetManifest& StageExtraContent::Assets() const {return StageExtraManifest();}
void StageExtraContent::DrawScenery(float seconds,const AssetManager& assets) const {
    stage_extra::DrawScenery(seconds,assets.GetVisual(kBackground).texture);
}
std::span<const StageEvent> StageExtraContent::Events() const {return ExtraMainTimeline();}
const bosses::Battle& StageExtraContent::PracticeBoss() const {throw std::invalid_argument("Extra practice entry is not registered");}
const bosses::Battle& StageExtraContent::PracticeBoss(int spellCard) const {return ExtraSpellBattle(spellCard);}
std::span<const DialogueLine> StageExtraContent::Dialogue(bool after,ShotType shot) const {return SceneDialogue(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueCue> StageExtraContent::DialogueCues(bool after,ShotType shot) const {return SceneDialogueCues(DialogueScene::FinalBoss,after,shot);}
std::span<const DialogueLine> StageExtraContent::SceneDialogue(DialogueScene scene,bool after,ShotType shot) const {return Script(scene,after,shot).lines;}
std::span<const DialogueCue> StageExtraContent::SceneDialogueCues(DialogueScene scene,bool after,ShotType shot) const {return Script(scene,after,shot).cues;}
float StageExtraContent::DialogueMinimumDuration(DialogueScene scene,bool after,ShotType shot) const {return Script(scene,after,shot).endFrame/60.0F;}
int StageExtraContent::CaptureProgress(int previous,int card) const {
    if(previous<0)throw std::invalid_argument("Negative Extra capture progress");
    if(card<122||card>134)return previous;
    return card==131?10:previous+1;
}
DialogueFlow StageExtraContent::ResolveDialogueFlow(DialogueScene scene,bool after,ShotType shot,const StagePerformance& performance) const {
    const auto& script=Script(scene,after,shot);
    if(scene==DialogueScene::FinalBoss&&after&&performance.captures>=6&&(!performance.challenge||performance.netMisses<=2))
        return {.skip=true};
    return {.finishStage=after,.awardFrame=script.clearFrame,.endFrame=script.endFrame};
}
VisualId StageExtraContent::BossPortraitVisual(int expression) const {return SceneBossPortraitVisual(DialogueScene::FinalBoss,expression);}
VisualId StageExtraContent::SceneBossPortraitVisual(DialogueScene scene,int expression) const {
    const auto& ids=scene==DialogueScene::Midboss?kPatchouliPortraits:kFlandrePortraits;
    return ids[expression>=0&&expression<4?expression:0];
}
VisualId StageExtraContent::SpellPortraitVisual(int card) const {return card>=119&&card<=121?kPatchouliPortraits[0]:kFlandrePortraits[2];}
}

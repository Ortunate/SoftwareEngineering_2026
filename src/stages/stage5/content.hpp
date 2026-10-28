// 本文件声明五面 Normal 静态内容和分遭遇对白，公共自机/道具素材由共享基类提供。
#pragma once
#include "stages/common/content.hpp"
#include "stages/stage5/ids.hpp"
namespace koumakyou::stage5 {
/// @brief 五面内容不持有活动演员；事件表、台词和清单为静态存储，可安全借用至卸载。
class Stage5Content final : public stage_assets::NativeBattleContent {
public:
    const native_combat::StageData* CombatData() const override {return native_combat::Stage(5);}
    const StageAssetManifest& Assets() const override;
    void DrawScenery(float seconds) const override;
    std::span<const StageEvent> Events() const override;
    const bosses::Battle& PracticeBoss() const override;
    std::span<const DialogueLine> Dialogue(bool afterBattle=false,ShotType shot=ShotType::HomingAmulet) const override;
    std::span<const DialogueCue> DialogueCues(bool afterBattle,ShotType shot) const override;
    std::span<const DialogueLine> SceneDialogue(DialogueScene scene,bool afterBattle,ShotType shot) const override;
    std::span<const DialogueCue> SceneDialogueCues(DialogueScene scene,bool afterBattle,ShotType shot) const override;
    float DialogueLineDuration() const override {return 5;}
    VisualId BossPortraitVisual(int expression=0) const override;
    VisualId SpellPortraitVisual() const override {return BossPortraitVisual(1);}
    std::array<std::string_view,2> BossIntroduction() const override {return {"红魔馆的女仆","十六夜咲夜"};}
    const char* StartTitle() const override {return "Stage 5  Scarlet Moon  Normal";}
    StageDisplayInfo DisplayInfo() const override {return {"STAGE 05","潇洒从者寄身红月","Scarlet Moon"};}
    const char* ClearTitle() const override {return "STAGE 5 CLEAR";}
};
}

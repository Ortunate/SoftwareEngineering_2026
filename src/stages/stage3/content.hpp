// 本文件声明三面 Normal 静态内容和分遭遇对白，公共自机/道具素材由共享基类提供。
#pragma once
#include "stages/common/content.hpp"
#include "stages/stage3/ids.hpp"
namespace koumakyou::stage3 {
/// @brief 三面内容不持有活动演员；事件表、台词和清单为静态存储，可安全借用至卸载。
class Stage3Content final : public stage_assets::NativeBattleContent {
public:
    const native_combat::StageData* CombatData() const override {return native_combat::Stage(3);}
    const StageAssetManifest& Assets() const override;
    void DrawScenery(float seconds) const override;
    std::span<const StageEvent> Events() const override;
    std::unique_ptr<Boss> CreateBoss(bool final) const override;
    std::span<const DialogueLine> Dialogue(bool afterBattle=false,ShotType shot=ShotType::HomingAmulet) const override;
    std::span<const DialogueCue> DialogueCues(bool afterBattle,ShotType shot) const override;
    std::span<const DialogueLine> SceneDialogue(DialogueScene scene,bool afterBattle,ShotType shot) const override;
    std::span<const DialogueCue> SceneDialogueCues(DialogueScene scene,bool afterBattle,ShotType shot) const override;
    float DialogueLineDuration() const override {return 5;}
    VisualId BossPortraitVisual(int expression=0) const override;
    VisualId SpellPortraitVisual() const override {return BossPortraitVisual(1);}
    std::array<std::string_view,2> BossIntroduction() const override {return {"华人小姑娘","红美铃"};}
    const char* StartTitle() const override {return "Stage 3  Scarlet Land  Normal";}
    StageDisplayInfo DisplayInfo() const override {return {"STAGE 03","红色境域","Scarlet Land"};}
    const char* ClearTitle() const override {return "STAGE 3 CLEAR";}
    const char* BossStartMessage(bool) const override {return "Hong Meiling";}
    const char* MidbossEndMessage() const override {return "Meiling retreated";}
};
}

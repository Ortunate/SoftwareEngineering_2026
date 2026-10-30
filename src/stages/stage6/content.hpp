// 本文件声明六面N/H/L内容和分遭遇对白，公共自机/道具素材由共享基类提供；Easy无六面可玩入口。
#pragma once
#include "stages/common/content.hpp"
#include "stages/stage6/ids.hpp"
namespace koumakyou::stage6 {
/// @brief 六面内容不持有活动演员；事件表、台词和清单为静态存储，可安全借用至卸载。
class Stage6Content final : public stage_assets::NativeBattleContent {
public:
    using NativeBattleContent::NativeBattleContent;
    const native_combat::StageData* CombatData() const override {return native_combat::Stage(6);}
    const StageAssetManifest& Assets() const override;
    void DrawScenery(float seconds,const AssetManager& assets) const override;
    std::span<const StageEvent> Events() const override;
    const bosses::Battle& PracticeBoss() const override;
    /// @brief 单符卡练习按本内容难度解析101…118，错配编号抛invalid_argument。
    const bosses::Battle& PracticeBoss(int spellCard) const override;
    std::span<const DialogueLine> Dialogue(bool afterBattle=false,ShotType shot=ShotType::HomingAmulet) const override;
    std::span<const DialogueCue> DialogueCues(bool afterBattle,ShotType shot) const override;
    std::span<const DialogueLine> SceneDialogue(DialogueScene scene,bool afterBattle,ShotType shot) const override;
    std::span<const DialogueCue> SceneDialogueCues(DialogueScene scene,bool afterBattle,ShotType shot) const override;
    float DialogueMinimumDuration(DialogueScene scene,bool after,ShotType) const override {return scene==DialogueScene::FinalBoss&&after ? 1 : 0;}
    VisualId SceneBossPortraitVisual(DialogueScene scene,int expression=0) const override;
    float DialogueLineDuration() const override {return 5;}
    VisualId BossPortraitVisual(int expression=0) const override;
    VisualId SpellPortraitVisual() const override {return BossPortraitVisual(2);}
    VisualId SpellPortraitVisual(int spellCard) const override {return spellCard>=101&&spellCard<=103?kSakuyaPortraits[1]:SpellPortraitVisual();}
    std::array<std::string_view,2> BossIntroduction() const override {return {"永远鲜红的幼月","蕾米莉亚·斯卡蕾特"};}
    const char* StartTitle() const override {return "Stage 6  Bloody Scarlet";}
    StageDisplayInfo DisplayInfo() const override {return {"STAGE 06","至福乐土降下血雨","Bloody Scarlet"};}
    const char* ClearTitle() const override {return "STAGE 6 CLEAR";}
};
}

// 本文件组合Extra独立关卡的素材、显式时间轴、命名对白和追加战取得门槛。
#pragma once
#include "stages/common/content.hpp"
#include "stages/stage_extra/dialogue.hpp"
namespace koumakyou::stage_extra {
/// @brief 单套Extra内容，内部Normal上下文不代表主线Normal关卡；所有借用表由模块静态持有。
/// 本类不持有活动演员、取得计数或本局模式，Game逐事件提供这些状态的快照。
class StageExtraContent final : public stage_assets::NativeBattleContent {
public:
    const native_combat::StageData* CombatData() const override {return native_combat::Stage(7);}
    const StageAssetManifest& Assets() const override;
    void DrawScenery(float seconds,const AssetManager& assets) const override;
    std::span<const StageEvent> Events() const override;
    const bosses::Battle& PracticeBoss() const override;
    /// @brief 借用119…131的静态单符配方；其余编号拒绝，整场练习仍不开放。
    const bosses::Battle& PracticeBoss(int spellCard) const override;
    std::span<const DialogueLine> Dialogue(bool after=false,ShotType shot=ShotType::HomingAmulet) const override;
    std::span<const DialogueCue> DialogueCues(bool after,ShotType shot) const override;
    std::span<const DialogueLine> SceneDialogue(DialogueScene scene,bool after,ShotType shot) const override;
    std::span<const DialogueCue> SceneDialogueCues(DialogueScene scene,bool after,ShotType shot) const override;
    float DialogueMinimumDuration(DialogueScene scene,bool after,ShotType shot) const override;
    /// @brief 只统计全局122…134；131成功直接置10，119…121道中符卡不计入追加战门槛。
    int CaptureProgress(int previous,int card) const override;
    /// @brief 主战战后取得>=6且Standard或Challenge净Miss<=2时短路对白，顺序进入追加战。
    /// 未达门槛与追加战战后各按真实MSG9/11边界加分和结束，禁止以确认键跳过保护等待。
    DialogueFlow ResolveDialogueFlow(DialogueScene scene,bool after,ShotType shot,const StagePerformance& performance) const override;
    float DialogueLineDuration() const override {return 5;}
    VisualId BossPortraitVisual(int expression=0) const override;
    VisualId SceneBossPortraitVisual(DialogueScene scene,int expression=0) const override;
    VisualId SpellPortraitVisual(int card) const override;
    std::array<std::string_view,2> BossIntroduction() const override {return kFlandreIntroduction;}
    const char* StartTitle() const override {return "Extra  东方红魔狂";}
    StageDisplayInfo DisplayInfo() const override {return {"EXTRA STAGE","东方红魔狂","Sister of Scarlet"};}
    const char* ClearTitle() const override {return "EXTRA STAGE CLEAR";}
};
}

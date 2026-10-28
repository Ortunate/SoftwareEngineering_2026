// 本文件声明二面独立的时间轴、遭遇、角色对话与素材入口，公共玩家素材接口来自共享基类。
#pragma once
#include "stages/common/content.hpp"
#include "stages/stage2/ids.hpp"
namespace koumakyou::stage2 {
/// @brief 二面 Normal 内容；不持有运行中演员，返回的静态表覆盖调度器及加载期。
class Stage2Content final : public stage_assets::NativeBattleContent {
public:
    const native_combat::StageData* CombatData() const override { return native_combat::Stage(2); }
    const StageAssetManifest& Assets() const override;
    void DrawScenery(float seconds) const override;
    std::span<const StageEvent> Events() const override;
    const bosses::Battle& PracticeBoss() const override;
    std::span<const DialogueLine> Dialogue(bool afterBattle=false,ShotType shot=ShotType::HomingAmulet) const override;
    std::span<const DialogueCue> DialogueCues(bool afterBattle,ShotType shot) const override;
    float DialogueLineDuration() const override { return 5; }
    VisualId BossPortraitVisual(int expression=0) const override;
    VisualId SpellPortraitVisual() const override { return BossPortraitVisual(1); }
    std::array<std::string_view,2> BossIntroduction() const override { return {"湖上的冰精","琪露诺"}; }
    const char* StartTitle() const override { return "Stage 2  Water Magus  Normal"; }
    StageDisplayInfo DisplayInfo() const override { return {"STAGE 02","湖面的魔精","Water Magus"}; }
    const char* ClearTitle() const override { return "STAGE 2 CLEAR"; }
};
}

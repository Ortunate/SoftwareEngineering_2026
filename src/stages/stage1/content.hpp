// 本文件声明第一面事件、资源、对话及 Boss 工厂，普通演员直接通过行为组件生成。

#pragma once

#include "stages/common/content.hpp"
#include "stages/stage1/ids.hpp"

namespace koumakyou::stage1 {

/// @brief 第一面完整内容定义；新关卡实现 StageContent 即可接入总控。
class Stage1Content final : public stage_assets::NativeBattleContent {
public:
    [[nodiscard]] const native_combat::StageData* CombatData() const override { return native_combat::Stage(1); }
    [[nodiscard]] const StageAssetManifest& Assets() const override;
    void DrawScenery(float seconds) const override;
    [[nodiscard]] std::span<const StageEvent> Events() const override;
    const bosses::Battle& PracticeBoss() const override;
    [[nodiscard]] std::span<const DialogueLine> Dialogue(bool afterBattle = false, ShotType shot=ShotType::HomingAmulet) const override;
    [[nodiscard]] std::span<const DialogueCue> DialogueCues(bool afterBattle, ShotType shot) const override;
    [[nodiscard]] float DialogueLineDuration() const override { return 5.0F; }
    [[nodiscard]] VisualId BossPortraitVisual(int expression = 0) const override;
    /// @brief 第一面符卡演出使用张口笑的露米娅全身差分，表情编号为 2。
    [[nodiscard]] VisualId SpellPortraitVisual() const override { return BossPortraitVisual(2); }
    [[nodiscard]] std::array<std::string_view,2> BossIntroduction() const override { return {"宵暗的妖怪","露米娅"}; }
    [[nodiscard]] const char* StartTitle() const override {
        return "Stage 1  Mystic Flier  Normal";
    }
    [[nodiscard]] StageDisplayInfo DisplayInfo() const override {
        return {"STAGE 01", "梦幻夜行绘卷", "Mystic Flier"};
    }
    [[nodiscard]] const char* ClearTitle() const override { return "STAGE 1 CLEAR"; }
};

}  // namespace koumakyou::stage1

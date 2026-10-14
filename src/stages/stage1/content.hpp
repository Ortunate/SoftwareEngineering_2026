// 本文件负责声明第一面关卡内容和敌人、Boss 工厂。

#pragma once

#include "stage/content.hpp"
#include "stages/stage1/ids.hpp"

namespace koumakyou::stage1 {

/// @brief 第一面完整内容定义；新关卡实现 StageContent 即可接入总控。
class Stage1Content final : public StageContent {
public:
    [[nodiscard]] const StageAssetManifest& Assets() const override;
    [[nodiscard]] std::span<const StageEvent> Events() const override;
    [[nodiscard]] std::unique_ptr<Enemy> CreateEnemy(const EnemySpec& spec) const override;
    [[nodiscard]] std::unique_ptr<Boss> CreateBoss(bool final) const override;
    [[nodiscard]] std::span<const DialogueLine> Dialogue(bool afterBattle = false) const override;
    [[nodiscard]] float DialogueLineDuration() const override { return 5.0F; }
    [[nodiscard]] VisualId PlayerBulletVisual() const override { return kPlayerGoldCard; }
    [[nodiscard]] VisualId PlayerHomingVisual() const override { return kPlayerCyanCard; }
    [[nodiscard]] VisualId PowerPickupVisual() const override { return kPowerPoint; }
    [[nodiscard]] VisualId ScorePickupVisual() const override { return kScorePoint; }
    [[nodiscard]] VisualId StarPickupVisual() const override { return kStarPoint; }
    [[nodiscard]] VisualId HeroPortraitVisual(int expression = 0) const override;
    [[nodiscard]] VisualId BossPortraitVisual(int expression = 0) const override;
    /// @brief 第一面符卡演出使用张口笑的露米娅全身差分，表情编号为 2。
    [[nodiscard]] VisualId SpellPortraitVisual() const override { return BossPortraitVisual(2); }
    [[nodiscard]] const char* StartTitle() const override {
        return "Stage 1  Mystic Flier  Normal";
    }
    [[nodiscard]] const char* ClearTitle() const override { return "STAGE 1 CLEAR"; }
    [[nodiscard]] const char* BossStartMessage(bool final) const override {
        return final ? "Rumia  Stage 1 Boss" : "Rumia  Midboss";
    }
    [[nodiscard]] const char* MidbossEndMessage() const override {
        return "Rumia retreated";
    }
};

}  // namespace koumakyou::stage1

// 本文件负责声明第一面关卡内容和敌人、Boss 工厂。

#pragma once

#include "stage/stage_content.hpp"
#include "stages/stage1/stage1_ids.hpp"

namespace redmist::stage1 {

/// @brief 第一面完整内容定义；新关卡实现 StageContent 即可接入总控。
class Stage1Content final : public StageContent {
public:
    [[nodiscard]] const StageAssetManifest& Assets() const override;
    [[nodiscard]] std::span<const StageEvent> Events() const override;
    [[nodiscard]] std::unique_ptr<Enemy> CreateEnemy(
        std::uint16_t archetype, const EnemySpawn& spawn) const override;
    [[nodiscard]] std::unique_ptr<Boss> CreateBoss(bool final) const override;
    bool HandleEvent(const StageEvent& event, StageEventSink& sink) const override;
    [[nodiscard]] std::span<const DialogueLine> Dialogue() const override;
    [[nodiscard]] float DialogueLineDuration() const override { return 2.5F; }
    [[nodiscard]] VisualId PlayerBulletVisual() const override { return kBlueBullet; }
    [[nodiscard]] VisualId HeroPortraitVisual() const override { return kReimuPortrait; }
    [[nodiscard]] VisualId BossPortraitVisual() const override { return kRumia; }
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

}  // namespace redmist::stage1

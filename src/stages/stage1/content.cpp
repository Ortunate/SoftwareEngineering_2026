// 本文件负责第一面的素材、时间轴、对话与带参敌人、Boss 工厂。

#include "stages/stage1/content.hpp"
#include "stages/stage1/data.hpp"

#include "stages/stage1/boss.hpp"
#include "stages/stage1/enemies.hpp"

#include <type_traits>

namespace koumakyou::stage1 {

const StageAssetManifest& Stage1Content::Assets() const { return Stage1Manifest(); }
std::span<const StageEvent> Stage1Content::Events() const { return Stage1Timeline(); }
std::span<const DialogueLine> Stage1Content::Dialogue(bool afterBattle, ShotType shot) const {
    return Stage1Dialogue(afterBattle,shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser);
}

PlayerShotVisuals Stage1Content::PlayerVisuals(ShotType shot) const {
    auto result=StageContent::PlayerVisuals(shot);
    if(shot!=ShotType::MagicMissile && shot!=ShotType::IllusionLaser)return result;
    result.primary=kMarisaPrimary;
    for(int index=0;index<4;++index) {
        result.secondary[index]=static_cast<VisualId>(shot==ShotType::MagicMissile ? kMarisaMissile+index : kMarisaLaser+std::min(index,2));
        result.impact[index]=static_cast<VisualId>(kMarisaExplosion+index);
    }
    return result;
}

VisualId Stage1Content::HeroPortraitVisual(int expression, ShotType shot) const {
    if(shot==ShotType::MagicMissile || shot==ShotType::IllusionLaser)
        return expression>=0 && expression<static_cast<int>(kMarisaPortraits.size()) ? kMarisaPortraits[expression] : kMarisaPortraits.front();
    return expression >= 0 && expression < static_cast<int>(kReimuPortraits.size())
        ? kReimuPortraits[expression] : kReimuPortraits.front();
}

VisualId Stage1Content::BossPortraitVisual(int expression) const {
    return expression >= 0 && expression < static_cast<int>(kRumiaPortraits.size())
        ? kRumiaPortraits[expression] : kRumiaPortraits.front();
}

std::unique_ptr<Enemy> Stage1Content::CreateEnemy(const EnemySpec& spec) const {
    if (!IsValidEnemySpec(spec)) return nullptr;
    return std::visit([](const auto& params) -> std::unique_ptr<Enemy> {
        using Params = std::decay_t<decltype(params)>;
        if constexpr (std::is_same_v<Params, ReferenceEnemyParams>) {
            return std::make_unique<ReferenceEnemy>(params);
        } else if constexpr (std::is_same_v<Params, FairyParams>) {
            return std::make_unique<FairyEnemy>(params);
        } else if constexpr (std::is_same_v<Params, DarkFairyParams>) {
            return std::make_unique<DarkFairyEnemy>(params);
        } else if constexpr (std::is_same_v<Params, OrbParams>) {
            return std::make_unique<OrbEnemy>(params);
        } else {
            // EnemySpec 可包含其他关卡类型；当前工厂只接收本关卡支持的配置。
            return nullptr;
        }
    }, spec);
}

std::unique_ptr<Boss> Stage1Content::CreateBoss(bool final) const {
    auto boss = std::make_unique<RumiaBoss>();
    if (final) boss->StartFinalBoss();
    else boss->StartMidboss();
    return boss;
}

}  // namespace koumakyou::stage1

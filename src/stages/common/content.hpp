// 本文件声明使用公共原生战斗素材槽的关卡基类；具体关卡仍独立提供时间轴、Boss、对话和清单。
#pragma once
#include "stage/content.hpp"
#include "stages/common/ids.hpp"
namespace koumakyou::stage_assets {
/// @brief 复用玩家/道具/自机立绘接口，不持有资源；Assets 须包含 CommonBattleVisuals 的全部公共槽。
class NativeBattleContent : public StageContent {
public:
    std::array<VisualId,4> PlayerBombVisuals(ShotType shot) const override;
    PlayerShotVisuals PlayerVisuals(ShotType shot) const override;
    VisualId HeroPortraitVisual(int expression=0,ShotType shot=ShotType::HomingAmulet) const override;
    VisualId PlayerBulletVisual() const override { return kPlayerGoldCard; }
    VisualId PlayerHomingVisual() const override { return kPlayerCyanCard; }
    VisualId PlayerNeedleVisual() const override { return kPlayerNeedle; }
    VisualId PlayerOptionVisual(int side,ShotType shot=ShotType::HomingAmulet) const override {
        if(side<0||side>1)return kInvalidVisualId;
        const bool marisa=shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser;
        return side==0 ? (marisa?kMarisaOptionLeft:kPlayerOptionLeft) : (marisa?kMarisaOptionRight:kPlayerOptionRight);
    }
    VisualId PowerPickupVisual() const override { return kPowerPoint; }
    VisualId LargePowerPickupVisual() const override { return kLargePowerPoint; }
    VisualId ScorePickupVisual() const override { return kScorePoint; }
    VisualId StarPickupVisual() const override { return kStarPoint; }
    VisualId ItemVisual(int item) const override {
        constexpr std::array<VisualId,7> items{kPowerPoint,kScorePoint,kLargePowerPoint,kBombPoint,kFullPowerPoint,kLifePoint,kStarPoint};
        return item>=0&&item<static_cast<int>(items.size()) ? items[item] : kInvalidVisualId;
    }
};
}

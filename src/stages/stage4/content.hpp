// 本文件声明四面 Normal 静态内容；活动对象由游戏拥有，资源和对话表借用程序静态存储。
#pragma once
#include "stages/common/content.hpp"
#include "stages/stage4/ids.hpp"
namespace koumakyou::stage4 {
/// @brief 图书馆关卡的素材、时间轴、Boss 工厂与两角色对白，四机体战斗分支由 Boss 接收机体后选择。
class Stage4Content final : public stage_assets::NativeBattleContent {
public:
    const native_combat::StageData* CombatData() const override {return native_combat::Stage(4);}
    const StageAssetManifest& Assets() const override;
    void DrawScenery(float seconds) const override;
    std::span<const StageEvent> Events() const override;
    std::unique_ptr<Boss> CreateBoss(bool final) const override;
    std::span<const DialogueLine> Dialogue(bool afterBattle=false,ShotType shot=ShotType::HomingAmulet) const override;
    std::span<const DialogueCue> DialogueCues(bool afterBattle,ShotType shot) const override;
    float DialogueLineDuration() const override {return 5;}
    VisualId BossPortraitVisual(int expression=0) const override;
    VisualId SpellPortraitVisual() const override {return BossPortraitVisual(1);}
    std::array<std::string_view,2> BossIntroduction() const override {return {"知识与避世的少女","帕秋莉·诺蕾姬"};}
    const char* StartTitle() const override {return "Stage 4  Unmoving Great Library  Normal";}
    StageDisplayInfo DisplayInfo() const override {return {"STAGE 04","不动的大图书馆","Unmoving Great Library"};}
    const char* ClearTitle() const override {return "STAGE 4 CLEAR";}
    const char* BossStartMessage(bool final) const override {return final ? "Patchouli Knowledge" : "Koakuma";}
    const char* MidbossEndMessage() const override {return "Koakuma retreated";}
};
}

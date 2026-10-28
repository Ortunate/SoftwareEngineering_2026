// 本文件将四面显式时间轴、分机体 Boss、台词与图书馆场景接入共享关卡接口。
#include "stages/stage4/content.hpp"
#include "stages/stage4/data.hpp"
#include "stages/stage4/boss_config.hpp"
#include "stages/stage4/scenery.hpp"
namespace koumakyou::stage4 {
const StageAssetManifest& Stage4Content::Assets() const {return Stage4Manifest();}
void Stage4Content::DrawScenery(float seconds) const {stage4::DrawScenery(seconds);}
std::span<const StageEvent> Stage4Content::Events() const {return Stage4Timeline();}
const bosses::Battle& Stage4Content::PracticeBoss() const {return kPatchouliFinal;}
std::span<const DialogueLine> Stage4Content::Dialogue(bool after,ShotType shot) const {
    return Stage4Dialogue(after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
std::span<const DialogueCue> Stage4Content::DialogueCues(bool after,ShotType shot) const {
    return Stage4DialogueCues(after,shot==ShotType::MagicMissile||shot==ShotType::IllusionLaser);
}
VisualId Stage4Content::BossPortraitVisual(int expression) const {return kPatchouliPortraits[expression>=0&&expression<4 ? expression : 0];}
}

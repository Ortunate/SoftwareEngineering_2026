// 本文件负责注册并构造可供游戏总控选择的关卡内容。

#include "stage/catalog.hpp"

#include "stages/stage1/content.hpp"
#include "stages/stage2/content.hpp"
#include "stages/stage3/content.hpp"
#include "stages/stage4/content.hpp"
#include "stages/stage5/content.hpp"
#include "stages/stage6/content.hpp"
#include "stages/stage_extra/content.hpp"

#include <stdexcept>

namespace koumakyou {

bool SupportsStage(StageId id,native_combat::Difficulty difficulty,StageEntry entry) {
    if(StageNumber(id)==0||native_combat::DifficultyIndex(difficulty)<0)return false;
    // Extra只有一套脚本上下文，与主线Normal成绩和连续闯关完全分离。
    if(id==StageId::Extra || entry==StageEntry::Extra)
        return id==StageId::Extra && (entry==StageEntry::Extra||entry==StageEntry::SpellPractice) &&
            difficulty==native_combat::Difficulty::Normal;
    switch(entry) {
        case StageEntry::Campaign:case StageEntry::Practice:case StageEntry::SpellPractice:
            // Easy本篇在五面结束；六面仅N/H/L拥有经过独立源对照的可玩遭遇。
            return id!=StageId::Stage6||difficulty!=native_combat::Difficulty::Easy;
        case StageEntry::Extra:return false;
    }
    return false;
}
bool SupportsDifficulty(native_combat::Difficulty difficulty,StageEntry entry) {
    if(native_combat::DifficultyIndex(difficulty)<0)return false;
    if(entry==StageEntry::Extra)return SupportsStage(StageId::Extra,difficulty,entry);
    if(entry==StageEntry::Campaign) {
        for(const auto stage:kPracticeStages) {
            if(!stage || !SupportsStage(*stage,difficulty,entry))return false;
            if(CampaignEndsAfter(*stage,difficulty))return true;
        }
        return false;
    }
    if(entry==StageEntry::Practice || entry==StageEntry::SpellPractice)
        for(const auto stage:kPracticeStages)if(stage && SupportsStage(*stage,difficulty,entry))return true;
    return false;
}
std::unique_ptr<StageContent> CreateStageContent(StageId id,native_combat::Difficulty difficulty,StageEntry entry) {
    if(!SupportsStage(id,difficulty,entry))throw std::invalid_argument("Unsupported stage entry or difficulty");
    switch (id) {
        case StageId::Stage1: return std::make_unique<stage1::Stage1Content>(difficulty);
        case StageId::Stage2: return std::make_unique<stage2::Stage2Content>(difficulty);
        case StageId::Stage3: return std::make_unique<stage3::Stage3Content>(difficulty);
        case StageId::Stage4: return std::make_unique<stage4::Stage4Content>(difficulty);
        case StageId::Stage5: return std::make_unique<stage5::Stage5Content>(difficulty);
        case StageId::Stage6: return std::make_unique<stage6::Stage6Content>(difficulty);
        case StageId::Extra: return std::make_unique<stage_extra::StageExtraContent>();
    }
    throw std::invalid_argument("Unknown StageId");
}

}  // namespace koumakyou

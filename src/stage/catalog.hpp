// 本文件负责声明关卡编号和关卡内容工厂接口。

#pragma once

#include "stage/content.hpp"

#include <cstdint>
#include <array>
#include <optional>
#include <memory>

namespace koumakyou {

/// @brief 可选择的关卡内容编号；注册新关卡只修改目录模块。
enum class StageId : std::uint8_t { Stage1, Stage2, Stage3, Stage4, Stage5, Stage6, Extra };

/// @brief 关卡练习列表按原典 1～6 面排列；空槽仅展示，输入不得确认或传给内容工厂。
inline constexpr std::array<std::optional<StageId>,6> kPracticeStages{StageId::Stage1,StageId::Stage2,
    StageId::Stage3,StageId::Stage4,StageId::Stage5,StageId::Stage6};
/// @brief 符卡目录使用0起算关卡槽0…6，最后一槽为Extra；非法槽返回空，不增加整关练习或成绩槽。
[[nodiscard]] inline std::optional<StageId> SpellPracticeStage(int index) {
    if(index==6)return StageId::Extra;
    return index>=0&&index<static_cast<int>(kPracticeStages.size())?kPracticeStages[index]:std::nullopt;
}
/// @brief 映射源关卡号1～7，Extra为7；整关成绩表仅接受1～6，未登记编号返回0。
[[nodiscard]] inline int StageNumber(StageId id) {
    if(id==StageId::Extra)return 7;
    for(std::size_t index=0;index<kPracticeStages.size();++index)
        if(kPracticeStages[index]==id)return static_cast<int>(index)+1;
    return 0;
}
/// @brief 连续闯关的下一张已开放内容；末关或未登记编号返回空，练习/预览是否衔接由 Game 决定。
[[nodiscard]] inline std::optional<StageId> NextStage(StageId id) {
    const int number=StageNumber(id);
    return number>0&&number<static_cast<int>(kPracticeStages.size()) ? kPracticeStages[number] : std::nullopt;
}

/// @brief 入口类型独立于关卡编号；未完成验证的难度和入口不开放。
enum class StageEntry { Campaign, Practice, SpellPractice, Extra };
/// @brief 查询可用的关卡/脚本难度/入口组合，无副作用；非法枚举返回false，Extra接受独立入口/符卡练习和Normal上下文。
[[nodiscard]] bool SupportsStage(StageId id,native_combat::Difficulty difficulty,StageEntry entry);
/// @brief 主线结算后的终止条件；Easy 在第五关结束，其余主线难度在第六关结束。
/// 非法关卡或难度抛 invalid_argument；练习入口应由调用方单独处理。
[[nodiscard]] inline bool CampaignEndsAfter(StageId id,native_combat::Difficulty difficulty) {
    const int stage=StageNumber(id);
    if(stage<1 || stage>6 || native_combat::DifficultyIndex(difficulty)<0)throw std::invalid_argument("Invalid campaign identity");
    return stage>=6 || (stage==5 && difficulty==native_combat::Difficulty::Easy);
}
/// @brief 本篇要求到难度终点每关均已支持，练习要求至少一关支持；Extra仅接受独立Normal上下文。
[[nodiscard]] bool SupportsDifficulty(native_combat::Difficulty difficulty,StageEntry entry);
/// @brief 构造指定关卡的内容对象；不支持的难度或入口抛 invalid_argument。
/// @return 调用方独占的关卡内容。
/// @throws std::invalid_argument 未注册的关卡编号。
[[nodiscard]] std::unique_ptr<StageContent> CreateStageContent(StageId id,native_combat::Difficulty difficulty=native_combat::Difficulty::Normal,
    StageEntry entry=StageEntry::Campaign);

}  // namespace koumakyou

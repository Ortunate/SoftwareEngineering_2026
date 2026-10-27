// 本文件负责声明关卡编号和关卡内容工厂接口。

#pragma once

#include "stage/content.hpp"

#include <cstdint>
#include <array>
#include <optional>
#include <memory>

namespace koumakyou {

/// @brief 可选择的关卡内容编号；注册新关卡只修改目录模块。
enum class StageId : std::uint8_t { Stage1, Stage2, Stage3, Stage4, Stage5, Stage6 };

/// @brief 关卡练习列表按原典 1～6 面排列；空槽仅展示，输入不得确认或传给内容工厂。
inline constexpr std::array<std::optional<StageId>,6> kPracticeStages{StageId::Stage1,StageId::Stage2,
    StageId::Stage3,StageId::Stage4,StageId::Stage5,StageId::Stage6};
/// @brief 将内容编号映射到存档中的 1～6 关卡号；未登记编号返回 0，不能写入成绩槽。
[[nodiscard]] inline int StageNumber(StageId id) {
    for(std::size_t index=0;index<kPracticeStages.size();++index)
        if(kPracticeStages[index]==id)return static_cast<int>(index)+1;
    return 0;
}
/// @brief 连续闯关的下一张已开放内容；末关或未登记编号返回空，练习/预览是否衔接由 Game 决定。
[[nodiscard]] inline std::optional<StageId> NextStage(StageId id) {
    const int number=StageNumber(id);
    return number>0&&number<static_cast<int>(kPracticeStages.size()) ? kPracticeStages[number] : std::nullopt;
}

/// @brief 构造指定关卡的内容对象。
/// @return 调用方独占的关卡内容。
/// @throws std::invalid_argument 未注册的关卡编号。
[[nodiscard]] std::unique_ptr<StageContent> CreateStageContent(StageId id);

}  // namespace koumakyou

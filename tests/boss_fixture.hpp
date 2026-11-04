// 本文件为测试按事件顺序选择明确的战斗定义；生产入口直接使用命名配置，不推断道中/关底角色。
#pragma once
#include "stage/content.hpp"
namespace koumakyou::testing {
/// @brief 返回本事件表第 index 场战斗（0 起算）；没有对应事件则抛 out_of_range，返回引用借用静态表。
inline const bosses::Battle& BattleAt(const StageContent& content,std::size_t index) {
    for(const auto& event:content.Events())if(const auto* boss=std::get_if<StartBoss>(&event.action)) {
        if(index--==0)return *boss->battle;
    }
    throw std::out_of_range("Missing test battle");
}
}

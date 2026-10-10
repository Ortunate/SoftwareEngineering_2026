// 本文件负责汇总关卡支持的带参敌人类型，并声明配置校验接口。

#pragma once

#include "stages/stage1/enemy_params.hpp"

#include <variant>

namespace koumakyou {

/// @brief 按值保存一种小怪的完整出生配置；重复出怪复用配置而创建独立实例。
/// 新增关卡敌人时须在此登记参数类型，并扩展对应关卡的工厂。
using EnemySpec = std::variant<stage1::FairyParams, stage1::DarkFairyParams,
                               stage1::OrbParams, stage1::ReferenceEnemyParams>;

/// @brief 校验出生坐标、对应敌人的移动枚举或退场等待时间是否有效。
/// @return 配置可用于创建敌人时返回 true；不创建对象或修改配置。
[[nodiscard]] bool IsValidEnemySpec(const EnemySpec& spec);

}  // namespace koumakyou

// 本文件负责检查带参敌人配置的坐标、移动枚举与退场等待时间约束。

#include "stage/enemy_spec.hpp"

#include <cmath>
#include <type_traits>

namespace redmist {

bool IsValidEnemySpec(const EnemySpec& spec) {
    return std::visit([](const auto& params) {
        using Params = std::decay_t<decltype(params)>;
        if (!std::isfinite(params.position.x) ||
            !std::isfinite(params.position.y) ||
            !std::isfinite(params.position.z)) return false;
        // 枚举按连续编号定义；转换为无符号值也会拒绝负数伪造的枚举。
        if constexpr (std::is_same_v<Params, stage1::OrbParams>) {
            return static_cast<unsigned>(params.movement) <=
                   static_cast<unsigned>(Params::Movement::CrossArena);
        } else if constexpr (std::is_same_v<Params, stage1::FairyParams>) {
            return static_cast<unsigned>(params.movement) <=
                   static_cast<unsigned>(Params::Movement::CrossFromRight);
        } else if constexpr (std::is_same_v<Params, stage1::DarkFairyParams>) {
            return std::isfinite(params.retreatDelay) && params.retreatDelay >= 0.0F;
        } else {
            static_assert(std::is_same_v<Params, void>, "New EnemySpec requires validation");
        }
    }, spec);
}

}  // namespace redmist

// 本文件负责声明按外观资源绘制战斗对象的渲染接口。

#pragma once

#include "gameplay/actors/boss.hpp"
#include "gameplay/bullets/system.hpp"
#include "gameplay/actors/enemy.hpp"
#include "resources/loaded_assets.hpp"

#include "raylib.h"

#include <span>
#include <vector>

namespace redmist {

/// @brief 绘制角色贴图、弹幕本体和光晕。
class SpriteRenderer {
public:
    /// @brief 绘制一个战斗帧的空间精灵并恢复深度状态。
    /// @param camera 当前 3D 相机。
    /// @param bullets 全部活动弹幕。
    /// @param enemies 当前可见敌人指针序列。
    /// @param boss 当前 Boss 状态。
    /// @param visuals 当前 Stage 的稠密外观资源表。
    /// @param shaders 不持有的弹幕与角色着色器集合。
    void Draw(const Camera3D& camera, const BulletSystem& bullets,
              std::span<const Enemy*> enemies, const Boss* boss,
              std::span<const VisualResource> visuals,
              const SpriteShaders& shaders) const;
#ifdef RED_MIST_DEBUG_FEATURES
    /// @brief 在 Debug 构建中叠加判定球与发弹点。
    /// @param bullets 全部活动弹幕。
    /// @param enemies 当前可见敌人指针序列。
    /// @param boss 当前 Boss 状态。
    /// @param playerPosition 计算发弹点所需的玩家位置。
    void DrawDebugGeometry(const BulletSystem& bullets,
                           std::span<const Enemy*> enemies,
                           const Boss* boss, Vector3 playerPosition) const;
#endif
private:
    /// @brief 按外观分组，避免每种弹幕都重新扫描固定容量弹池。
    mutable std::vector<std::vector<const Bullet*>> bulletGroups_;
};

}  // namespace redmist

#pragma once

#include "game/boss.hpp"
#include "game/bullet_system.hpp"
#include "game/enemy.hpp"

#include "raylib.h"

#include <filesystem>
#include <span>

namespace redmist {

/// @brief Game 拥有的贴图集合；此结构仅向渲染器传递非持有句柄。
struct SpriteAssets {
    Texture2D blueBullet{};
    Texture2D redBullet{};
    Texture2D fairy{};
    Texture2D elite{};
    Texture2D boss{};
};

/// @brief 绘制角色贴图、弹幕本体、光晕和可选的判定调试层。
class SpriteRenderer {
public:
    /// @brief 创建尚未加载着色器的渲染器。
    SpriteRenderer() = default;
    /// @brief 禁止复制着色器资源的所有权。
    SpriteRenderer(const SpriteRenderer&) = delete;
    /// @brief 禁止复制赋值以避免重复释放着色器。
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;
    /// @brief 释放由此渲染器加载的着色器。
    ~SpriteRenderer();

    /// @brief 加载弹幕及角色贴图使用的透明处理着色器。
    /// @param assetsPath 资源目录，内部读取 shaders 子目录。
    void Load(const std::filesystem::path& assetsPath);
    /// @brief 绘制一个战斗帧的空间精灵并恢复深度状态。
    /// @param camera 当前 3D 相机。
    /// @param bullets 全部活动弹幕。
    /// @param enemies 当前可见敌人指针序列。
    /// @param boss 当前 Boss 状态。
    /// @param playerPosition 调试发弹点计算所用的玩家位置。
    /// @param assets 不持有的贴图资源集合。
    /// @param debugHitboxes 是否叠加判定球和发弹点。
    void Draw(const Camera3D& camera, const BulletSystem& bullets,
              std::span<const Enemy*> enemies, const RumiaBoss& boss,
              Vector3 playerPosition, const SpriteAssets& assets,
              bool debugHitboxes) const;

private:
    Shader bulletCoreShader_{};
    Shader cutoutShader_{};
};

}  // namespace redmist

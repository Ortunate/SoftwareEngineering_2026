// 本文件负责定义加载后的 GPU 外观和绘制器借用的着色器资源。

#pragma once

#include "resources/visual_assets.hpp"

namespace redmist {
/// @brief 加载后的外观，纹理所有权仅属于 AssetManager。
struct VisualResource {
    Texture2D texture{};
    VisualMode mode = VisualMode::Character;
    float worldHeight = 1.0F;
    Vector2 pivot{0.5F, 0.5F};
    float bulletDiameterScale = 2.6F;
    bool glow = false;
};

/// @brief 由素材管理器持有、绘制器仅借用的透明处理着色器。
struct SpriteShaders {
    Shader bulletCore{};
    Shader cutout{};
};

}  // namespace redmist

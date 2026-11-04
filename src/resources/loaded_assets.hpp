// 本文件负责定义加载后的 GPU 外观和绘制器借用的着色器资源。

#pragma once

#include "resources/visual_assets.hpp"
#include <memory>

namespace koumakyou {
/// @brief 加载后的外观，纹理所有权仅属于 AssetManager；尺寸与锚点沿用 VisualDefinition 的单位。
struct VisualResource {
    Texture2D texture{};
    VisualMode mode = VisualMode::Character;
    float worldHeight = 1.0F;
    Vector2 pivot{0.5F, 0.5F};
    float bulletDiameterScale = 2.6F;
    bool glow = false;
    Rectangle dialogueCrop{0, 0, 1, 1}; // 清单取景内 Alpha >16 的可见边界（归一化），仅影响对话，完整纹理保留。
    battle::Selection animation{};
    VisualId defeatVisual = kInvalidVisualId; // 当前关卡的击破动画编号。
    std::shared_ptr<const anm::Archive> archive; // 借用 GPU 纹理，共享只读 CPU 指令，不拥有图集纹理。
    float worldUnitsPerPixel = 0; // 原生战斗使用统一世界/逻辑像素比例；零保留手工/旧外观的指定高度。
};

/// @brief 按需加载的战斗图集；GPU 纹理由 AssetManager 独占，所有引用在 UnloadStage 后失效。
struct BattleAtlasResource {
    Texture2D texture{};
    std::shared_ptr<const anm::Archive> archive;
};

/// @brief 由素材管理器持有、绘制器仅借用的透明处理着色器。
struct SpriteShaders {
    Shader bulletCore{};
    Shader cutout{};
};

}  // namespace koumakyou

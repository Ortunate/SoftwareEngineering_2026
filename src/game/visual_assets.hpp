#pragma once

#include "raylib.h"

#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace redmist {

/// @brief 仅在当前 Stage 的素材清单内有效的稠密外观编号。
using VisualId = std::uint16_t;
constexpr VisualId kInvalidVisualId = std::numeric_limits<VisualId>::max();

/// @brief 角色公告板、弹幕公告板或三维球体的通用绘制方式。
enum class VisualMode : std::uint8_t { Character, Bullet, Sphere };

/// @brief Stage 提供的外观定义；路径相对 assets 根目录。
struct VisualDefinition {
    VisualId id = kInvalidVisualId;
    std::string_view path;
    VisualMode mode = VisualMode::Character;
    float worldHeight = 1.0F;
    Vector2 pivot{0.5F, 0.5F};
    float bulletDiameterScale = 2.6F;
    bool glow = false;
};

/// @brief 加载后的外观，纹理所有权仅属于 AssetManager。
struct VisualResource {
    Texture2D texture{};
    VisualMode mode = VisualMode::Character;
    float worldHeight = 1.0F;
    Vector2 pivot{0.5F, 0.5F};
    float bulletDiameterScale = 2.6F;
    bool glow = false;
};

/// @brief 声音清单项，路径相对 assets 根目录。
struct SoundDefinition {
    std::string_view name;
    std::string_view path;
};

/// @brief 一个 Stage 的图片、音乐、音效与着色器清单。
struct StageAssetManifest {
    std::span<const VisualDefinition> visuals;
    std::string_view stageBgm;
    std::string_view bossBgm;
    std::span<const SoundDefinition> sounds;
};

}  // namespace redmist

// 本文件负责定义关卡外观 ID、绘制参数和音频素材清单。

#pragma once

#include "raylib.h"

#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace koumakyou {

/// @brief 仅在当前 Stage 的素材清单内有效的稠密外观编号。
using VisualId = std::uint16_t;
/// @brief 不指向任何外观资源的哨兵值，不得用作清单项 ID。
constexpr VisualId kInvalidVisualId = std::numeric_limits<VisualId>::max();

/// @brief 角色公告板、弹幕公告板或三维球体的通用绘制方式。
enum class VisualMode : std::uint8_t { Character, Bullet, Sphere };

/// @brief Stage 提供的外观定义；路径相对 assets 根目录，字符串由 Stage 保持有效。
/// worldHeight 为世界单位，pivot 是贴图中判定中心的归一化坐标。
/// bulletDiameterScale 将弹幕判定半径换算为公告板直径的倍数。
struct VisualDefinition {
    VisualId id = kInvalidVisualId;
    std::string_view path;
    VisualMode mode = VisualMode::Character;
    float worldHeight = 1.0F;
    Vector2 pivot{0.5F, 0.5F};
    float bulletDiameterScale = 2.6F;
    bool glow = false;
    /// @brief 对话专用的归一化裁剪矩形，限定在 [0,1]；世界公告板和符卡仍显示完整图。
    /// 全身差分裁到上半身，使脸部尺寸接近半身差分，底部藏在台词面板之后。
    Rectangle dialogueCrop{0, 0, 1, 1};
};

/// @brief 声音清单项，路径相对 assets 根目录；name 是播放时使用的查找键。
struct SoundDefinition {
    std::string_view name;
    std::string_view path;
};

/// @brief 一个 Stage 的图片、音乐与音效清单；所有 span 与字符串在 LoadStage 调用期间须有效。
struct StageAssetManifest {
    std::span<const VisualDefinition> visuals;
    std::string_view stageBgm;
    std::string_view bossBgm;
    std::span<const SoundDefinition> sounds;
};

}  // namespace koumakyou

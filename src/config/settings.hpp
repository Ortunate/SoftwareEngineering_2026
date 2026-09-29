// 本文件负责声明可持久化的游戏设置及其读写接口。

#pragma once

#include "config/key_bindings.hpp"

#include <array>
#include <filesystem>

namespace redmist {

/// @brief 运行目录中的本地配置文件名。
inline constexpr char kSettingsFilename[] = "red_mist_settings.cfg";
/// @brief 可选帧率上限；0 表示不限制。
inline constexpr std::array<int, 6> kSupportedFpsLimits{
    30, 60, 120, 144, 240, 0,
};

/// @brief 需要持久化的战斗键位和 FPS 上限。
struct GameSettings {
    KeyBindings bindings{};
    int fpsLimit = 60;
};

/// @brief 读取本地配置；缺失或无效的配置会使用默认值。
/// @param path 配置文件路径。
/// @return 可用于本局游戏的设置。
[[nodiscard]] GameSettings LoadOrCreateSettings(
    const std::filesystem::path& path);
/// @brief 将 FPS 上限和战斗键位保存为文本配置。
/// @param path 配置文件路径。
/// @param settings 待保存的设置。
/// @return 成功写入返回 true，否则返回 false。
[[nodiscard]] bool SaveSettingsFile(const std::filesystem::path& path,
                                    const GameSettings& settings);

}  // namespace redmist

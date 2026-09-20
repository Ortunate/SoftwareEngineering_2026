#pragma once

#include "game/game_types.hpp"

#include <array>
#include <filesystem>

namespace redmist {

inline constexpr char kSettingsFilename[] = "red_mist_settings.cfg";
inline constexpr std::array<int, 6> kSupportedFpsLimits{
    30, 60, 120, 144, 240, 0,
};

struct GameSettings {
    KeyBindings bindings{};
    int fpsLimit = 60;
};

[[nodiscard]] GameSettings LoadOrCreateSettings(
    const std::filesystem::path& path);
[[nodiscard]] bool SaveSettingsFile(const std::filesystem::path& path,
                                    const GameSettings& settings);

}  // namespace redmist

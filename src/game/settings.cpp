#include "game/settings.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace redmist {
namespace {

/// @brief 将配置文件中的字段名映射到 KeyBindings 成员。
struct BindingField {
    std::string_view name;
    int KeyBindings::*member;
};

constexpr std::array<BindingField, 10> kBindingFields{{
    {"forward", &KeyBindings::forward},
    {"backward", &KeyBindings::backward},
    {"move_left", &KeyBindings::moveLeft},
    {"move_right", &KeyBindings::moveRight},
    {"ascend", &KeyBindings::ascend},
    {"descend", &KeyBindings::descend},
    {"focus", &KeyBindings::focus},
    {"fire", &KeyBindings::fire},
    {"bomb", &KeyBindings::bomb},
    {"pause", &KeyBindings::pause},
}};

/// @brief 去掉配置行首尾的空白字符。
/// @param text 原始文本视图。
/// @return 指向去空白后内容的视图。
std::string_view Trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// @brief 严格解析十进制整数，不接受剩余非数字字符。
/// @param text 待解析文本。
/// @return 成功时返回整数，失败时返回空值。
std::optional<int> ParseInteger(std::string_view text) {
    text = Trim(text);
    int value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// @brief 检查 FPS 限制是否是界面提供的档位。
/// @param value 候选帧率上限。
/// @return 属于支持列表时返回 true。
bool IsSupportedFpsLimit(int value) {
    return std::find(kSupportedFpsLimits.begin(), kSupportedFpsLimits.end(),
                     value) != kSupportedFpsLimits.end();
}

/// @brief 检查全部战斗按键有效且无重复。
/// @param bindings 待检查的键位映射。
/// @return 每个动作都有唯一有效键码时返回 true。
bool HasUniqueBindings(const KeyBindings& bindings) {
    for (std::size_t left = 0; left < kBindingFields.size(); ++left) {
        const int leftKey = bindings.*(kBindingFields[left].member);
        if (leftKey <= KEY_NULL) return false;
        for (std::size_t right = left + 1; right < kBindingFields.size(); ++right) {
            if (leftKey == bindings.*(kBindingFields[right].member)) return false;
        }
    }
    return true;
}

}  // namespace

GameSettings LoadOrCreateSettings(const std::filesystem::path& path) {
    GameSettings settings{};
    std::error_code fileError;
    if (!std::filesystem::exists(path, fileError) || fileError) {
        (void)SaveSettingsFile(path, settings);
        return settings;
    }

    std::ifstream input(path);
    if (!input) return settings;

    bool shouldRewrite = false;
    std::string line;
    while (std::getline(input, line)) {
        const std::string_view trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string_view::npos) {
            shouldRewrite = true;
            continue;
        }

        const std::string_view name = Trim(trimmed.substr(0, separator));
        const std::optional<int> value = ParseInteger(trimmed.substr(separator + 1));
        if (!value) {
            shouldRewrite = true;
            continue;
        }

        if (name == "version") {
            shouldRewrite = shouldRewrite || *value != 1;
        } else if (name == "fps_limit") {
            if (IsSupportedFpsLimit(*value)) {
                settings.fpsLimit = *value;
            } else {
                shouldRewrite = true;
            }
        } else {
            const auto field = std::find_if(
                kBindingFields.begin(), kBindingFields.end(),
                [name](const BindingField& candidate) {
                    return candidate.name == name;
                });
            if (field != kBindingFields.end() && *value > KEY_NULL) {
                settings.bindings.*(field->member) = *value;
            } else if (field != kBindingFields.end()) {
                shouldRewrite = true;
            }
        }
    }

    if (!HasUniqueBindings(settings.bindings)) {
        settings.bindings = KeyBindings{};
        shouldRewrite = true;
    }
    if (shouldRewrite) (void)SaveSettingsFile(path, settings);
    return settings;
}

bool SaveSettingsFile(const std::filesystem::path& path,
                      const GameSettings& settings) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) return false;

    output << "# Red Mist 3D local settings\n";
    output << "version=1\n";
    output << "fps_limit=" << settings.fpsLimit << '\n';
    for (const BindingField& field : kBindingFields) {
        output << field.name << '=' << settings.bindings.*(field.member) << '\n';
    }
    return output.good();
}

}  // namespace redmist

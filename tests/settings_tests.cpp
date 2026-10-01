// 本文件验证设置持久化、版本迁移、键鼠编码与冲突交换，不需要打开窗口。
#include "config/settings.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

/// @brief 验证默认值、持久化边界、旧配置和键鼠冲突；失败时断言终止。
void RunSettingsTests() {
    using namespace redmist;
    const auto path = std::filesystem::temp_directory_path() /
        ("red_mist_settings_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".cfg");
    const auto defaults = LoadOrCreateSettings(path);
    assert(std::filesystem::exists(path));
    assert(defaults.fpsLimit == 60 && defaults.musicVolume == 50);
    assert(defaults.mouseSensitivity == 100);
    assert(defaults.bindings.focus == MouseBinding(MOUSE_BUTTON_SIDE));
    assert(defaults.bindings.descend == KEY_LEFT_SHIFT);
    assert(defaults.bindings.fireAlt == MouseBinding(MOUSE_BUTTON_LEFT));
    // 往返覆盖静音/最大音量及灵敏度两端，避免边界在重启后回退。
    for (const int volume : {0, 35, 100}) {
        for (const int sensitivity : {10, 170, 300}) {
            auto settings = defaults;
            settings.fpsLimit = 144;
            settings.musicVolume = volume;
            settings.mouseSensitivity = sensitivity;
            settings.bindings.fire = KEY_F1;
            settings.bindings.pause = MouseBinding(MOUSE_BUTTON_EXTRA);
            assert(SaveSettingsFile(path, settings));
            const auto loaded = LoadOrCreateSettings(path);
            assert(loaded.fpsLimit == 144 && loaded.musicVolume == volume);
            assert(loaded.mouseSensitivity == sensitivity);
            assert(loaded.bindings.fire == KEY_F1);
            assert(loaded.bindings.pause == MouseBinding(MOUSE_BUTTON_EXTRA));
        }
    }
    auto bindings = defaults.bindings;
    assert(Rebind(bindings, 6, MouseBinding(MOUSE_BUTTON_LEFT)));
    assert(bindings.focus == MouseBinding(MOUSE_BUTTON_LEFT));
    assert(bindings.fireAlt == MouseBinding(MOUSE_BUTTON_SIDE));
    assert(Rebind(bindings, 7, KEY_W));
    assert(bindings.fire == KEY_W && bindings.forward == KEY_Z);
    assert(!Rebind(bindings, -1, KEY_A));
    assert(!Rebind(bindings, 12, KEY_A));
    for (int invalid : std::array<int, 10>{KEY_NULL, KEY_ESCAPE, KEY_F3, KEY_F4, KEY_F5, KEY_F6, -1, 999, 1007, 99999})
        assert(!Rebind(bindings, 0, invalid));
    for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_BACK; ++button)
        assert(IsValidBinding(MouseBinding(button)));
    // 旧默认 C 升级为 Mouse 4；旧自定义 Focus 和 FPS 保留。
    for (int oldFocus : {KEY_C, KEY_V}) {
        { std::ofstream out(path); out << "version=1\nfps_limit=240\nfocus=" << oldFocus << '\n'; }
        const auto migrated = LoadOrCreateSettings(path);
        assert(migrated.fpsLimit == 240);
        assert(migrated.bindings.focus == (oldFocus == KEY_C ? MouseBinding(MOUSE_BUTTON_SIDE) : KEY_V));
        assert(migrated.musicVolume == 50 && migrated.mouseSensitivity == 100);
        assert(LoadOrCreateSettings(path).bindings.focus == migrated.bindings.focus);
    }
    { std::ofstream out(path); out << "version=2\nfocus=67\n"; }
    assert(LoadOrCreateSettings(path).bindings.focus == KEY_C);
    { std::ofstream out(path); out << "version=2\nmusic_volume=101\nmouse_sensitivity=0\nfire=1007\nfps_limit=13\n"; }
    const auto invalid = LoadOrCreateSettings(path);
    assert(invalid.musicVolume == 50 && invalid.mouseSensitivity == 100 && invalid.fpsLimit == 60);
    assert(invalid.bindings.fire == KEY_Z);
    { std::ofstream out(path); out << "version=2\nmusic_volume=-1\nmouse_sensitivity=301\n"; }
    assert(LoadOrCreateSettings(path).mouseSensitivity == 100);
    { std::ofstream out(path); out << "version=2\nmusic_volume=nan\nmouse_sensitivity=100junk\nforward=1000\n"; }
    const auto repaired = LoadOrCreateSettings(path);
    assert(repaired.musicVolume == 50 && repaired.mouseSensitivity == 100);
    assert(repaired.bindings.forward == KEY_W); // 与 FIRE ALT 重复，回退键位组。
    std::filesystem::remove(path);
}

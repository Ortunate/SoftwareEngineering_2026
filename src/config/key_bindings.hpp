// 本文件负责定义键盘/鼠标统一绑定编码、动作映射与冲突交换规则。
#pragma once
#include "raylib.h"
#include <array>
#include <cstddef>

namespace koumakyou {
/// @brief 鼠标编码使用独立区间，避免与 raylib 键盘键码混淆；配置文件沿用整数。
inline constexpr int kMouseBindingBase = 1000;
/// @brief 将 raylib 鼠标按钮编号（0～6）转换成持久化绑定编码。
constexpr int MouseBinding(int button) { return kMouseBindingBase + button; }
/// @brief 是否为支持的鼠标按钮编码；滚轮滚动不是按钮，中键按下可以绑定。
constexpr bool IsMouseBinding(int code) {
    return code >= MouseBinding(MOUSE_BUTTON_LEFT) && code <= MouseBinding(MOUSE_BUTTON_BACK);
}
/// @brief 是否为可绑定键；Esc 保留作返回/取消，F3～F6 保留作调试快捷键。
constexpr bool IsValidBinding(int code) {
    if (IsMouseBinding(code)) return true;
    if (code == KEY_ESCAPE || (code >= KEY_F3 && code <= KEY_F6)) return false;
    return code == KEY_SPACE || code == KEY_APOSTROPHE || code == KEY_COMMA ||
        code == KEY_MINUS || code == KEY_PERIOD || code == KEY_SLASH ||
        (code >= KEY_ZERO && code <= KEY_NINE) || code == KEY_SEMICOLON ||
        code == KEY_EQUAL || (code >= KEY_A && code <= KEY_Z) ||
        (code >= KEY_LEFT_BRACKET && code <= KEY_RIGHT_BRACKET) || code == KEY_GRAVE ||
        (code >= KEY_ENTER && code <= KEY_END) ||
        (code >= KEY_CAPS_LOCK && code <= KEY_PAUSE) ||
        (code >= KEY_F1 && code <= KEY_F12) ||
        (code >= KEY_KP_0 && code <= KEY_KP_EQUAL) ||
        (code >= KEY_LEFT_SHIFT && code <= KEY_KB_MENU);
}
/// @brief 查询统一绑定的持续按住状态；必须在 raylib 初始化后调用，无效编码返回 false。
inline bool BindingDown(int code) {
    return IsMouseBinding(code) ? IsMouseButtonDown(code - kMouseBindingBase)
                               : (IsValidBinding(code) && IsKeyDown(code));
}
/// @brief 查询统一绑定本帧首次按下状态；必须在 raylib 初始化后调用。
inline bool BindingPressed(int code) {
    return IsMouseBinding(code) ? IsMouseButtonPressed(code - kMouseBindingBase)
                               : (IsValidBinding(code) && IsKeyPressed(code));
}
/// @brief 可持久化的战斗键位；射击/Bomb 各有两个可编辑入口，均参与冲突交换。
struct KeyBindings {
    int forward = KEY_W;
    int backward = KEY_S;
    int moveLeft = KEY_A;
    int moveRight = KEY_D;
    int ascend = KEY_SPACE;
    int descend = KEY_LEFT_SHIFT;
    int focus = MouseBinding(MOUSE_BUTTON_SIDE);
    int fire = KEY_Z;
    int bomb = KEY_X;
    int pause = KEY_P;
    int fireAlt = MouseBinding(MOUSE_BUTTON_LEFT);
    int bombAlt = MouseBinding(MOUSE_BUTTON_RIGHT);
};
/// @brief 设置列表/配置文件共用的成员顺序；前十项与旧版一致。
inline constexpr std::array<int KeyBindings::*, 12> kBindingMembers{
    &KeyBindings::forward, &KeyBindings::backward, &KeyBindings::moveLeft,
    &KeyBindings::moveRight, &KeyBindings::ascend, &KeyBindings::descend,
    &KeyBindings::focus, &KeyBindings::fire, &KeyBindings::bomb, &KeyBindings::pause,
    &KeyBindings::fireAlt, &KeyBindings::bombAlt,
};
/// @brief 为动作设置合法键码；冲突时交换原键，不触发保存；无效索引/键返回 false。
inline bool Rebind(KeyBindings& bindings, int index, int code) {
    if (index < 0 || index >= static_cast<int>(kBindingMembers.size()) || !IsValidBinding(code)) return false;
    int& target = bindings.*kBindingMembers[index];
    const int previous = target;
    for (int other = 0; other < static_cast<int>(kBindingMembers.size()); ++other) {
        if (other != index && bindings.*kBindingMembers[other] == code) {
            bindings.*kBindingMembers[other] = previous;
            break;
        }
    }
    target = code;
    return true;
}
} // namespace koumakyou

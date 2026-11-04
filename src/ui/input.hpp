// 本文件统一界面的单帧键鼠操作；战斗移动和射击仍使用独立的可配置绑定。
#pragma once

#include "raylib.h"

namespace koumakyou::ui {
/// @brief 当前帧的菜单输入快照；导航值为 -1/0/1，鼠标位置为窗口逻辑像素。
/// 确认和返回只响应首次按下；调用方应先处理返回并结束本帧，避免输入穿透到新页面。
struct MenuInput {
    int vertical = 0;
    int horizontal = 0;
    bool confirm = false;
    bool back = false;
    bool click = false;
    bool pointerActive = false;
    Vector2 mouse{};

    /// @brief 读取 raylib 本帧状态；Enter 保留为确认别名，静止鼠标不覆盖键盘选择。
    [[nodiscard]] static MenuInput Read() {
        MenuInput input;
        input.vertical = (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) -
                         (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W));
        input.horizontal = (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) -
                           (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A));
        input.confirm = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER);
        input.back = IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        input.click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const auto delta = GetMouseDelta();
        input.pointerActive = input.click || delta.x != 0 || delta.y != 0;
        input.mouse = GetMousePosition();
        return input;
    }
    /// @brief 一维菜单使用两组方向键；同时跨轴输入时优先纵向，每帧至多移动一项。
    [[nodiscard]] int Direction() const { return vertical != 0 ? vertical : horizontal; }
    /// @brief 判断本帧左键是否点击指定屏幕矩形；不把空白区域当作当前项的确认。
    [[nodiscard]] bool Clicked(Rectangle bounds) const { return click && CheckCollisionPointRec(mouse, bounds); }
};
} // namespace koumakyou::ui

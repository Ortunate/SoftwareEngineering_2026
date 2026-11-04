// 本文件负责在隐藏窗口中验证输入法屏蔽、重复应用、窗口重建及原生按键透传。

#include "platform/window_input.hpp"
#include "raylib.h"

#include <iostream>
#include <stdexcept>

/// @brief 平台测试桥接：检查本窗口确实没有文字合成上下文，不修改系统输入源。
extern "C" bool KoumakyouCheckNativeTextInputDisabled(void* nativeWindow);
/// @brief 在目标原生视图或消息过程发送 W 按下/松开，不注入系统或其他应用。
extern "C" void KoumakyouSendNativeKeyEvent(void* nativeWindow, bool down);

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void RunWindowTest() {
    void* window = GetWindowHandle();
    Check(koumakyou::DisableWindowInputMethods(window), "disable input methods failed");
    Check(koumakyou::DisableWindowInputMethods(window), "reapplying policy failed");
    Check(KoumakyouCheckNativeTextInputDisabled(window), "text input context still active");
    while (GetCharPressed() != 0) {}
    KoumakyouSendNativeKeyEvent(window, true);
    Check(IsKeyDown(KEY_W), "native key press no longer reaches raylib");
#if defined(__APPLE__)
    Check(GetCharPressed() == 'w', "raw ASCII name input did not reach raylib");
#else
    Check(GetCharPressed() == 0, "unexpected character without a character message");
#endif
    KoumakyouSendNativeKeyEvent(window, false);
    Check(IsKeyUp(KEY_W), "native key release no longer reaches raylib");
    SetWindowSize(520, 360);
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
    Check(KoumakyouCheckNativeTextInputDisabled(GetWindowHandle()), "resize re-enabled input methods");
}
}

/// @brief 仅 macOS/Windows 构建，需要图形会话；不弹出可见窗口或初始化音频。
int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_RESIZABLE);
    try {
        Check(!koumakyou::DisableWindowInputMethods(nullptr), "null window unexpectedly accepted");
        // 重建窗口复用平台策略；不能依赖前一个窗口对象或临时注册数据。
        for (int i = 0; i < 2; ++i) {
            InitWindow(400, 300, "Window input tests");
            SetTargetFPS(0);
            SetExitKey(KEY_NULL);
            RunWindowTest();
            CloseWindow();
        }
    } catch (const std::exception& error) {
        if (IsWindowReady()) CloseWindow();
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Native window input tests passed\n";
    return 0;
}

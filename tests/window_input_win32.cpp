// 本文件负责用 Win32 窗口消息验证 IMM 屏蔽和 GLFW 按键透传，不注入全局键盘事件。

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>

/// @brief 无关联 IMM 上下文即窗口已禁用合成；借用的非空上下文须成对释放。
extern "C" bool KoumakyouCheckNativeTextInputDisabled(void* nativeWindow) {
    const HWND window = static_cast<HWND>(nativeWindow);
    const HIMC context = ImmGetContext(window);
    if (!context) return true;
    ImmReleaseContext(window, context);
    return false;
}

/// @brief 向本窗口同步发送 W 按下/松开；模拟消息的扫描码来自当前 Windows 键盘映射。
extern "C" void KoumakyouSendNativeKeyEvent(void* nativeWindow, bool down) {
    const HWND window = static_cast<HWND>(nativeWindow);
    const UINT scan = MapVirtualKeyW('W', MAPVK_VK_TO_VSC);
    const LPARAM flags = (static_cast<LPARAM>(scan) << 16) | 1 |
        (down ? 0 : static_cast<LPARAM>(0xC0000000ULL));
    SendMessageW(window, down ? WM_KEYDOWN : WM_KEYUP, 'W', flags);
}

// 本文件负责分派平台输入法屏蔽；Windows 解除游戏窗口的 IMM 上下文关联。

#include "platform/window_input.hpp"

#if defined(_WIN32)
// 此文件不引入 raylib.h，避免 Windows 的 DrawText、CloseWindow 等 API 同名冲突。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>
#elif defined(__APPLE__)
/// @brief Objective-C 桥接入口，借用 NSWindow*，不转移窗口所有权。
extern "C" bool RedMistDisableCocoaInputMethods(void* nativeWindow);
#endif

namespace redmist {

bool DisableWindowInputMethods(void* nativeWindow) {
    if (!nativeWindow) return false;
#if defined(_WIN32)
    const HWND window = static_cast<HWND>(nativeWindow);
    if (!IsWindow(window) || GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId())
        return false;
    // flags=0 处理指定窗口本身；IACE_CHILDREN 只作用于子窗口，不能替代这次解除关联。
    // 不释放原上下文：它由系统持有；本游戏没有文字输入框，也无需重建上下文。
    return ImmAssociateContextEx(window, nullptr, 0) != FALSE;
#elif defined(__APPLE__)
    return RedMistDisableCocoaInputMethods(nativeWindow);
#else
    return false;
#endif
}

}  // namespace redmist

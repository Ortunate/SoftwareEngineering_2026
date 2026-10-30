// 本文件负责声明游戏窗口的输入法屏蔽策略，保留物理键盘和鼠标事件。

#pragma once

namespace koumakyou {
/// @brief 临时启用命名页文字合成；不支持的平台返回 false，调用方须保留屏幕字符选择。
[[nodiscard]] bool EnableWindowTextInput(void* nativeWindow);

/// @brief 禁用指定游戏窗口的文字合成，避免中文等输入法的候选框干扰操作。
/// @param nativeWindow raylib GetWindowHandle 返回的 HWND（Windows）或 NSWindow*（macOS）。
/// 必须在 InitWindow 之后、首帧处理输入之前，在创建窗口的主线程调用；窗口销毁后不能调用。
/// 不改变系统输入源或其他应用；本窗口不再用于文字录入，可重复调用。
/// @return macOS/Windows 成功返回 true；空句柄、原生设置失败或不支持的平台返回 false。
[[nodiscard]] bool DisableWindowInputMethods(void* nativeWindow);

}  // namespace koumakyou

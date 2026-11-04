// 本文件负责用 macOS 原生事件验证游戏视图的文字输入屏蔽与 GLFW 按键回调。

#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdbool.h>

/// @brief 验证只有当前视图派生类禁用合成，原 GLFW 类的方法未被全局替换。
bool KoumakyouCheckNativeTextInputDisabled(void* nativeWindow) {
    NSWindow* window = (NSWindow*)nativeWindow;
    NSView* view = [window contentView];
    Class applied = object_getClass(view);
    Class original = class_getSuperclass(applied);
    if ([view inputContext] != nil) return false;
    if (class_getMethodImplementation(applied, @selector(interpretKeyEvents:)) ==
        class_getMethodImplementation(original, @selector(interpretKeyEvents:))) return false;
    // 更换第一响应者及恢复不会改变全局焦点，只检验本窗口策略仍保持。
    [window makeFirstResponder:nil];
    [window makeFirstResponder:view];
    return [view inputContext] == nil;
}

/// @brief 经本窗口的 sendEvent: 分派给第一响应者；keyCode=13 是 macOS 的物理 W 键。
void KoumakyouSendNativeKeyEvent(void* nativeWindow, bool down) {
    NSWindow* window = (NSWindow*)nativeWindow;
    NSEvent* event = [NSEvent keyEventWithType:down ? NSEventTypeKeyDown : NSEventTypeKeyUp
        location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber]
        context:nil characters:@"w" charactersIgnoringModifiers:@"w" isARepeat:NO keyCode:13];
    [window sendEvent:event];
}

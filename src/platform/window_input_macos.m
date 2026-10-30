// 本文件禁用 macOS 游戏视图的输入法合成，保留 GLFW 物理按键，并为成绩命名转交可打印 ASCII。

#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdbool.h>

/// @brief 游戏视图不提供文字输入上下文；不切换或修改系统输入源。
static id NoInputContext(id self, SEL selector) {
    (void)self;
    (void)selector;
    return nil;
}

/// @brief GLFW keyDown: 已先报告物理按键；不启动 IME，只把可打印 ASCII 交回原 insertText: 回调。
/// 这样 WASD 仍不会弹出候选窗，成绩页也能通过 GetCharPressed 读取字母、数字和标点。
static void ForwardAsciiText(id self, SEL selector, NSArray* events) {
    (void)selector;
    for (NSEvent* event in events) {
        if ([event modifierFlags] & (NSEventModifierFlagControl | NSEventModifierFlagCommand)) continue;
        NSString* characters = [event charactersIgnoringModifiers];
        NSMutableString* ascii = [NSMutableString string];
        for (NSUInteger i=0;i<[characters length];++i) {
            const unichar character=[characters characterAtIndex:i];
            if (character>=32 && character<=126) [ascii appendFormat:@"%C",character];
        }
        if ([ascii length]) [(id<NSTextInputClient>)self insertText:ascii replacementRange:NSMakeRange(NSNotFound,0)];
    }
}

/// @brief 仅借用本窗口，主线程重复调用安全；成功后策略持续到视图销毁。
/// 注册的无 ivar 子类由 Objective-C runtime 持有，不能在视图仍存活时释放。
bool KoumakyouDisableCocoaInputMethods(void* nativeWindow) {
    if (!nativeWindow || ![NSThread isMainThread]) return false;
    NSWindow* window = (NSWindow*)nativeWindow;
    NSView* view = [window contentView];
    if (!view) return false;
    Class original = object_getClass(view);
    if (class_getMethodImplementation(original, @selector(inputContext)) == (IMP)NoInputContext)
        return true;

    // 不修改 GLFWContentView 的全局方法；只给当前实例换上无 ivar 的派生类。
    // 必须继承原 keyDown:/keyUp:/flagsChanged:，以保留 GLFW 到 raylib 的输入回调。
    NSString* name = [@"KoumakyouRawKeyView_" stringByAppendingString:NSStringFromClass(original)];
    Class rawKeyClass = NSClassFromString(name);
    if (!rawKeyClass) {
        rawKeyClass = objc_allocateClassPair(original, [name UTF8String], 0);
        if (!rawKeyClass) return false;
        const Method inputContext = class_getInstanceMethod(original, @selector(inputContext));
        const Method interpretation = class_getInstanceMethod(original, @selector(interpretKeyEvents:));
        if (!inputContext || !interpretation ||
            !class_addMethod(rawKeyClass, @selector(inputContext), (IMP)NoInputContext,
                             method_getTypeEncoding(inputContext)) ||
            !class_addMethod(rawKeyClass, @selector(interpretKeyEvents:), (IMP)ForwardAsciiText,
                             method_getTypeEncoding(interpretation))) {
            objc_disposeClassPair(rawKeyClass);
            return false;
        }
        objc_registerClassPair(rawKeyClass);
    }
    if (class_getSuperclass(rawKeyClass) != original) return false;

    [[view inputContext] discardMarkedText];
    if ([view respondsToSelector:@selector(unmarkText)]) [(id<NSTextInputClient>)view unmarkText];
    // 通过系统的响应者切换退掉已激活的旧上下文，不能直接调用 inputContext.deactivate。
    // 只调整本窗口的第一响应者，不激活窗口或抢夺其他应用焦点。
    const BOOL wasFirstResponder = [window firstResponder] == view;
    if (wasFirstResponder && ![window makeFirstResponder:nil]) return false;
    object_setClass(view, rawKeyClass);
    if (wasFirstResponder && ![window makeFirstResponder:view]) return false;
    return [view inputContext] == nil;
}

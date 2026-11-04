// 本文件为 macOS 直接运行及应用包设置 Dock 图标，不修改其他应用或磁盘上的图标。
#import <AppKit/AppKit.h>
#include <stdbool.h>

/// @brief 主线程读取借用的 UTF-8 路径；NSApp 保留图像，失败时保留默认图标并返回 false。
bool KoumakyouSetCocoaApplicationIcon(const char* path) {
    if (!path || !NSApp || ![NSThread isMainThread]) return false;
    @autoreleasepool {
        NSString* filename = [NSString stringWithUTF8String:path];
        if (!filename) return false;
        NSImage* icon = [[NSImage alloc] initWithContentsOfFile:filename];
        if (!icon || ![icon isValid]) {
            [icon release];
            return false;
        }
        [NSApp setApplicationIconImage:icon];
        [icon release];
        return true;
    }
}

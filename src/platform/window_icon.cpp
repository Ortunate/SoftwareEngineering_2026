// 本文件从运行时素材设置应用图标，macOS 使用原生桥接以支持直接运行的可执行文件。
#include "platform/window_icon.hpp"

#include "raylib.h"

#include <filesystem>

#ifdef __APPLE__
// UTF-8 路径仅在调用期间借用；图像加载与 AppKit 所有权由桥接函数管理。
extern "C" bool KoumakyouSetCocoaApplicationIcon(const char* path);
#endif

namespace koumakyou {

bool SetApplicationIcon() {
    const auto path = std::filesystem::path(GetApplicationDirectory()) / "assets/common/app_icon.png";
#ifdef __APPLE__
    // GLFW 的 macOS 窗口图标接口不生效，直接启动也须显式设置应用的 Dock 图标。
    return KoumakyouSetCocoaApplicationIcon(path.string().c_str());
#else
    Image icon = LoadImage(path.string().c_str());
    if (!IsImageValid(icon)) return false;
    ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    SetWindowIcon(icon);
    UnloadImage(icon);
    return true;
#endif
}

}  // namespace koumakyou

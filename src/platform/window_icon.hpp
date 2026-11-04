// 本文件声明应用图标设置接口，统一原生 Dock 与 raylib 窗口图标入口。
#pragma once

namespace koumakyou {

/// @brief 从可执行文件旁的 assets/common/app_icon.png 设置当前应用图标。
/// 必须在 InitWindow 后的主线程调用；图像在调用内释放，平台持有自己的副本。
/// @return 素材无效或原生设置失败时返回 false，调用方可继续使用默认图标。
[[nodiscard]] bool SetApplicationIcon();

}  // namespace koumakyou

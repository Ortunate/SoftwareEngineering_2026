// 本文件声明原始战斗 DDS 的内存解码入口，不生成或改写图片文件。
#pragma once
#include "raylib.h"
#include <filesystem>
namespace koumakyou {
/// @brief 解码 BGRA/RGBA32、BC1/BC2/BC3/BC7 的首层图像，含 DX10 头及非 4 倍数边长。
/// 返回 RGBA8 Image，调用方须 UnloadImage；文件缺失、尺寸非法或格式不支持时抛 runtime_error。
[[nodiscard]] Image LoadOriginalDds(const std::filesystem::path& path);
}

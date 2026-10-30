// 本文件声明六面102/118的独立源请求快照，保留像素、源轴和整数ARGB，不依赖生产效果配方。
#pragma once
#include "raylib.h"
#include <cstdint>
namespace koumakyou::stage6::reference {
/// @brief 一次源指令效果：position/radius为像素，axis为无量纲源XYZ；type为效果表编号，count为正数。
struct ExternalEffect {int type,count;std::uint32_t color;Vector3 position;Vector3 axis{};float radius=0;};
}

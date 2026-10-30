// 本文件声明Extra正式素材清单；只有资源绑定，不注册尚未完成的可玩关卡。
#pragma once
#include "resources/visual_assets.hpp"
namespace koumakyou::stage_extra {
/// @brief 返回静态清单，包含独立本体/立绘、原始背景及三首音乐；借用存储覆盖异步加载和关卡期。
/// 帕秋莉与芙兰朵露不可复用同一VisualId；背景仅提供纹理，世界几何与STD参考另行适配。
const StageAssetManifest& StageExtraManifest();
}

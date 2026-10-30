// 本文件提供接收任意 raylib Model 的公用收缩显现效果；不加载替代模型。
#pragma once
#include "effects_asset.hpp"
#include <filesystem>
namespace koumakyou::projectiles {
// 在有效 OpenGL 上下文内构造/析构；只持有公用 shader，不持有调用方模型。
class SpawnRenderer {
public:
 explicit SpawnRenderer(const std::filesystem::path& root);
 ~SpawnRenderer();
 SpawnRenderer(const SpawnRenderer&)=delete;
 SpawnRenderer& operator=(const SpawnRenderer&)=delete;
 /// @brief 在 BeginMode3D 中绘制借用的 model：由大缩小，透明度由高到低，逐渐显现。
 /// 保留模型 transform、纹理和材质色；不修改 Model、材质、网格或碰撞。调用方负责模型生命周期。
 /// 到期后保持最终状态继续绘制；调用方也可切回正常弹体绘制，不应同时绘制两份。
 EffectSample Draw(const Model& model,const SpawnParams& params);
private:
 Shader shader_{};
};
}

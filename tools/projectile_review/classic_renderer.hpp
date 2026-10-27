// 本文件提供可独立接入 raylib 的低面数弹体实例渲染与颜色光效接口。
#pragma once
#include "raylib.h"
#include "collision.hpp"
#include <span>
#include <memory>
#include <filesystem>
#include <cstddef>
namespace review {
/// @brief 颜色采用 sRGB 8 位；强度范围 [0,2]，宽度以标准化模型长度计 [0,0.025]。
/// 光效不参与碰撞。白亮面由 glTF COLOR_0.r 遮罩确定，不依赖相机方向。
struct ProjectileAppearance {
    Color bodyColor{244,38,65,255},highlightColor{255,250,246,255},glowColor{255,35,55,255};
    float glowStrength=.55F,glowWidth=.015F;
};
/// @brief 位置为世界单位，rotation 为单位四元数，scale 为正的统一缩放。
struct ProjectileInstance {
    Vector3 position{};
    Quaternion rotation{0,0,0,1};
    float scale=1;
    ProjectileAppearance appearance{};
};
/// @brief 原色号 0..15 的参数预设；超出范围抛异常，不加载图片或复制网格。
ProjectileAppearance ClassicPalette(int color);
struct BatchStats { std::size_t submitted=0,visible=0; int drawCalls=0,triangles=0; };
/// @brief 构造/析构必须在同一有效 OpenGL 3.3 上下文内；不可复制。
/// 网格与实例缓冲只在构造时分配；Prepare/Draw 不进行逐帧堆或 GPU 对象分配。
class ClassicRenderer {
public:
    ClassicRenderer(const std::filesystem::path& asset,const std::filesystem::path& shaders,std::size_t capacity);
    ~ClassicRenderer();
    ClassicRenderer(const ClassicRenderer&)=delete;
    ClassicRenderer& operator=(const ClassicRenderer&)=delete;
    /// @brief 打包并上传实例；超容量/非法参数抛异常。剔除只影响显示，cameraAspect 为渲染区宽高比。
    void Prepare(std::span<const ProjectileInstance> instances,const Camera3D& camera,float cameraAspect,bool cull=true);
    /// @brief 绘制本体和可选光效。应在 BeginMode3D 内调用；恢复 GL 深度/混合/剔除/绑定状态。
    void Draw(bool glow=true,bool clay=false);
    const BatchStats& Stats() const;
    std::size_t BufferBytes() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
/// @brief 实体球接触：包围球初筛后在本地 BVH 查询；不读取颜色/光效。scratch 由调用线程复用。
/// narrow 可累计实际进入 BVH 的次数；仅支持正统一缩放和单位四元数。
bool TouchesInstance(const CollisionAsset& asset,CollisionScratch& scratch,const ProjectileInstance& instance,
                     Vector3 point,float radius,std::size_t* narrow=nullptr);
} // namespace review

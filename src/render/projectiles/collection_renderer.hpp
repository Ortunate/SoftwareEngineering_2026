// 本文件声明十类普通弹体的共享资源实例渲染；独立于游戏场景与对象池。
#pragma once
#include "classic_renderer.hpp"
#include <array>
namespace koumakyou::projectiles {
/// @brief 固定 nativeType 0..9 的资源约定；名称指向项目内资产目录，颜色数量不是网格数量。
struct ProjectileKind {const char* directory;const char* title;int colors,hardLimit;};
const std::array<ProjectileKind,10>& ProjectileKinds();
/// @brief 输入色号须为 0..15，依本类有效色数取模；颜色和光效参数可由调用方覆盖。
ProjectileAppearance AppearanceFor(int type,int color);
/// @brief 统一缩放为正、四元数为单位；shellOpacity [0,1]，phase 为有限弧度。
struct CollectionInstance {int type=0;ProjectileInstance transform{};float shellOpacity=.65F,phase=0;};
/// @brief 构造和销毁均须处于有效 OpenGL 3.3 上下文。总容量共享，不能每种各用 capacity 发。
/// 不复制网格/纹理；实例 GPU/CPU 缓冲只在构造时分配。Prepare/Draw 不创建逐弹资源。
class CollectionRenderer {
public:
    CollectionRenderer(const std::filesystem::path& root,std::size_t capacity);
    ~CollectionRenderer();
    CollectionRenderer(const CollectionRenderer&)=delete;
    CollectionRenderer& operator=(const CollectionRenderer&)=delete;
    /// @brief Prepare 做保守视锥剔除和分组上传；不修改调用方物理状态，aspect 为渲染区宽高比。
    void Prepare(std::span<const CollectionInstance>,const Camera3D&,float aspect,bool cull=true);
    /// @brief 在 BeginMode3D 内绘制；秒数有限非负。mode 0成品/1灰模/2仅实体/3全部装饰/4线框/5仅外壳/6仅尖丝或外焰。
    /// firstPass/lastPass 为主体 0、透明装饰 1、外辉光 2；可分别接入统一战斗绘制队列。
    /// 球壳与尖丝加色，泡内球乘法衰减背景；透明层关闭深度写入，保留深度测试。
    void Draw(float seconds,bool glow=true,int mode=0,int firstPass=0,int lastPass=2);
    const BatchStats& Stats()const;
    const CollisionAsset& Collision(int type)const;
    int Triangles(int type)const;
    std::size_t BufferBytes()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
} // namespace koumakyou::projectiles

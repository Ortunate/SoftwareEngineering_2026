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
/// 不复制网格/纹理；实例 GPU/CPU 存储在构造时分配，Upload 由驱动同步在途访问，不进行逐小批存储重命名。
class CollectionRenderer {
public:
    CollectionRenderer(const std::filesystem::path& root,std::size_t capacity);
    ~CollectionRenderer();
    CollectionRenderer(const CollectionRenderer&)=delete;
    CollectionRenderer& operator=(const CollectionRenderer&)=delete;
    /// @brief Prepare 做保守视锥剔除和分组上传；不修改调用方物理状态，aspect 为渲染区宽高比。
    void Prepare(std::span<const CollectionInstance>,const Camera3D&,float aspect,bool cull=true);
    /// @brief 开始透明阶段的帧内批次收集；相机与 aspect 共用于所有批次，旧句柄立即失效。
    void BeginFrame(const Camera3D&,float aspect);
    /// @brief 校验、保守剔除并追加一个独立批次，返回仅本帧有效的句柄；累计实例最多 2*capacity（半透明主体和装饰各一份）。
    std::size_t Append(std::span<const CollectionInstance>,bool cull=true);
    /// @brief 收集完成后统一上传；Upload 后才能 Select/Draw，禁止与未提交的 rlgl 几何混用。
    void Upload();
    /// @brief 选择本帧已上传的批次；仅改变本渲染器的 CPU 状态，不修改 GL 状态。
    void Select(std::size_t batch);
    /// @brief 在 BeginMode3D 内绘制；秒数有限非负。mode 0成品/1灰模/2仅实体/3全部装饰/4线框/5仅外壳/6仅尖丝或外焰。
    /// firstPass/lastPass 为主体 0、透明装饰 1、外辉光 2；可分别接入统一战斗绘制队列。
    /// 球壳与尖丝加色，泡内球乘法衰减背景；透明层关闭深度写入，保留深度测试。
    void Draw(float seconds,bool glow=true,int mode=0,int firstPass=0,int lastPass=2);
    /// @brief submitted/visible 来自最近 Prepare/Select；绘制计数由最近 Draw 重置，不累计整帧。
    const BatchStats& Stats()const;
    const CollisionAsset& Collision(int type)const;
    int Triangles(int type)const;
    std::size_t BufferBytes()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
} // namespace koumakyou::projectiles

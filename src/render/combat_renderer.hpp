// 本文件协调正式模型与旧公告板队列；只借用本帧战斗状态和关卡共享资源。
#pragma once
#include "render/projectiles/projectile_frame_builder.hpp"
#include "render/sprite_renderer.hpp"
#include <array>
#include <vector>
namespace koumakyou {
class CombatRenderer {
public:
 CombatRenderer();
 /// @brief 预分配实例交给统一透明队列按真实视深排序；不持有跨帧弹池指针，年龄只读取不推进。
 /// playerPosition 是实际自机世界坐标，仅用于近镜头敌弹淡化；回放观察相机不得代替它。
 void Prepare(const ProjectileAssets*,const BulletSystem&,std::span<const VisualResource>,const Camera3D&,Vector3 playerPosition,float aspect,float simulationSeconds=0);
 bool Available()const{return assets_!=nullptr;}
 bool Handles(const Bullet&,const VisualResource&)const;
 void DrawOpaque();
 int TransparentGroups()const{return static_cast<int>(transparent_.size());}
 float GroupDepth(int group)const{return depths_[group];}
 /// @brief 本帧实例主体是否半透明；此类实例必须独立 PrepareGroup，保留远到近的 Alpha 顺序。
 bool GroupTranslucent(std::size_t group)const{return transparent_.at(group).transform.appearance.bodyColor.a<255;}
 void DrawGroup(int group);
 /// @brief 合并统一队列内连续的模型命令；不能跨越演员/道具的透明绘制命令。
 void DrawGroups(std::span<const std::size_t> groups);
 /// @brief Prepare 后开始透明阶段收集，清除上次的批次句柄；仅借用本帧素材。
 void BeginGroups();
 /// @brief 素材须 Available；groups 使用本帧 TransparentGroups 的索引，返回本阶段句柄。
 /// 半透明主体须单独传入；多实例只用于不透明主体的装饰合批，避免按弹种重排 Alpha 主体。
 /// 调用序列为 BeginGroups → PrepareGroup（可多次）→ UploadGroups → DrawPreparedGroup（按原透明顺序）。
 std::size_t PrepareGroup(std::span<const std::size_t> groups);
 void UploadGroups();
 void DrawPreparedGroup(std::size_t batch);
 bool DrawBullet(const Bullet&,const VisualResource&);
 /// @brief 按显示光束归一化区间 [from,to] 绘制透明切片，0<=from<to<=1；不改变判定及生命周期。
 /// 魔理沙显示跨度由 SampleMarisaBeamGeometry 决定，排序端须共用其起点与长度。
 /// 魔理沙两层模型临时启用 GL_CLIP_DISTANCE0/1，并在返回前关闭；调用方不得借用这两个裁剪槽。
 bool DrawBeam(const Bullet&,const VisualResource&,float from,float to);
 /// @brief 已绑定原始 Item(0…6) 时绘制共享薄板；未知外观返回 false。
 bool DrawItem(const PickupSprite&,const VisualResource&);
 bool DrawOption(const PickupSprite&,const VisualResource&);
 bool DrawBomb(const BombVolumeSprite&,const VisualResource&,float from,float to);
private:
 const ProjectileAssets* assets_=nullptr;Camera3D camera_{};float aspect_=1,time_=0;
 std::vector<projectiles::CollectionInstance> opaque_;
 std::vector<projectiles::CollectionInstance> transparent_,grouped_,spawning_;
 std::vector<float> depths_;
 struct PreparedGroup {std::size_t body,decoration;};
 std::vector<PreparedGroup> preparedGroups_;
 std::vector<projectiles::LaserInstance> lasers_;
 // 按 std::less 排序的本帧 Bullet 借用地址 → 激光输入索引；每次 Prepare 清空，不改变弹池顺序。
 std::vector<std::pair<const Bullet*,std::size_t>> laserIndices_;
};
}

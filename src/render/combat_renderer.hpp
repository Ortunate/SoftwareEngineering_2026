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
 void Prepare(const ProjectileAssets*,const BulletSystem&,std::span<const VisualResource>,const Camera3D&,float aspect,float simulationSeconds=0);
 bool Available()const{return assets_!=nullptr;}
 bool Handles(const Bullet&,const VisualResource&)const;
 void DrawOpaque();
 int TransparentGroups()const{return static_cast<int>(transparent_.size());}
 float GroupDepth(int group)const{return depths_[group];}
 void DrawGroup(int group);
 /// @brief 合并统一队列内连续的模型命令；不能跨越演员/道具的透明绘制命令。
 void DrawGroups(std::span<const std::size_t> groups);
 /// @brief Prepare 后开始透明阶段收集，清除上次的批次句柄；仅借用本帧素材。
 void BeginGroups();
 /// @brief 素材须 Available；groups 使用本帧 TransparentGroups 的索引，返回本阶段句柄。
 /// 调用序列为 BeginGroups → PrepareGroup（可多次）→ UploadGroups → DrawPreparedGroup（按原透明顺序）。
 std::size_t PrepareGroup(std::span<const std::size_t> groups);
 void UploadGroups();
 void DrawPreparedGroup(std::size_t batch);
 bool DrawBullet(const Bullet&,const VisualResource&);
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

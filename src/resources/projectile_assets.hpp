// 本文件持有战斗模型、分层光效和 CPU 碰撞的共享资源；所有 GPU 操作在图形线程执行。
#pragma once
#include "resources/projectile_catalog.hpp"
#include "render/projectiles/collection_renderer.hpp"
#include "render/projectiles/laser_renderer.hpp"
#include <array>
namespace koumakyou {
struct ProjectileModelResource {
 std::array<Model,3> layers{};
 std::unique_ptr<projectiles::CollisionAsset> collision;
 float referenceRadius=.5F,forwardExtent=1;
};
// 与关卡同生命周期；模型实例只借用资源，禁止自行 UnloadModel/UnloadShader。
class ProjectileAssets {
public:
 explicit ProjectileAssets(const std::filesystem::path&root,bool immediate=true);
 /// @brief 图形线程每次推进一款模型（或共享 shader/批次），完成后返回 true；失败须销毁重建。
 bool AdvanceLoad();
 bool Ready()const{return !failed_&&loadStep_==ProjectileModels.size()+2;}
 float LoadProgress()const{return float(loadStep_)/float(ProjectileModels.size()+2); }
 ~ProjectileAssets();
 ProjectileAssets(const ProjectileAssets&)=delete;
 ProjectileAssets& operator=(const ProjectileAssets&)=delete;
 /// @brief 借用资源直到关卡卸载；未知名称抛 invalid_argument。
 const ProjectileModelResource& Get(std::string_view name)const;
 const ProjectileModelResource& Get(ProjectileAssetId id)const;
 projectiles::CollectionRenderer& Collection()const{return *collection_;}
 projectiles::LaserRenderer& Lasers()const{return *lasers_;}
 Texture2D OptionHalo()const{return optionHalo_;}
 Shader Marisa()const{return marisa_;} Shader Bomb()const{return bomb_;} Shader Needle()const{return needle_;}
private:
 void Release();
 void LoadModelResource(std::size_t index);
 std::filesystem::path root_;
 std::size_t loadStep_=0;bool failed_=false;
 std::array<ProjectileModelResource,ProjectileModels.size()> resources_{};
 std::unique_ptr<projectiles::CollectionRenderer> collection_;
 std::unique_ptr<projectiles::LaserRenderer> lasers_;
 Texture2D optionHalo_{};
 Shader marisa_{},bomb_{},needle_{};
};
}

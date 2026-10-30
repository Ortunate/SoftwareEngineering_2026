// 本文件加载一次共享模型和着色器，显式释放 glTF 引用的独立纹理，避免关卡重试泄漏。
#include "projectile_assets.hpp"
#include "resources/projectile_paths.hpp"
#include <unordered_set>
#include <cmath>
#include "rlgl.h"
#include "external/cgltf.h"
namespace koumakyou {
namespace {
Shader Load(const std::filesystem::path&dir,const char*name){auto s=LoadShader((dir/(std::string(name)+".vs")).string().c_str(),(dir/(std::string(name)+".fs")).string().c_str());if(!IsShaderValid(s)||s.id==rlGetShaderIdDefault())throw std::runtime_error("Projectile shader load failed");return s;}
// raylib 可能在模型解析失败时返回占位立方体；先验证原文件，禁止把损坏素材当作成功加载。
void ValidateModel(const std::filesystem::path&file){
 cgltf_options options{};cgltf_data* raw=nullptr;
 if(cgltf_parse_file(&options,file.string().c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("Invalid glTF: "+file.string());
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 if(cgltf_load_buffers(&options,data.get(),file.string().c_str())!=cgltf_result_success||cgltf_validate(data.get())!=cgltf_result_success)throw std::runtime_error("Invalid glTF buffers: "+file.string());
 for(size_t i=0;i<data->images_count;i++)if(data->images[i].uri&&std::string_view(data->images[i].uri).find("data:")!=0){
  auto path=file.parent_path()/data->images[i].uri;if(!std::filesystem::exists(path))throw std::runtime_error("Missing glTF texture: "+file.string());
  Image image=LoadImage(path.string().c_str());bool valid=IsImageValid(image);UnloadImage(image);if(!valid)throw std::runtime_error("Invalid glTF texture: "+path.string());
 }
}
void ValidateGeometry(const Model&model){
 for(int i=0;i<model.meshCount;i++){
  const auto&m=model.meshes[i];if(m.vertexCount<=0||m.triangleCount<=0||!m.vertices||!m.normals)throw std::runtime_error("Incomplete projectile geometry");
  for(int j=0;j<m.vertexCount*3;j++)if(!std::isfinite(m.vertices[j])||!std::isfinite(m.normals[j]))throw std::runtime_error("Nonfinite projectile geometry");
  if(m.indices)for(int j=0;j<m.triangleCount*3;j++)if(m.indices[j]>=m.vertexCount)throw std::runtime_error("Projectile index out of bounds");
 }
}
}
ProjectileAssets::ProjectileAssets(const std::filesystem::path&root,bool immediate):root_(root){
 if(immediate){try{while(!AdvanceLoad()){} }catch(...){Release();throw;}}
}
void ProjectileAssets::LoadModelResource(std::size_t i){
 auto base=projectiles::ProjectileAssetDirectory(root_);

  auto name=std::string(ProjectileModels[i].name);auto dir=base/name;auto& r=resources_[i];
  const std::string files[]{name+".gltf",std::filesystem::exists(dir/"glow.gltf")?"glow.gltf":"shell.gltf","effect.gltf"};
  for(int layer=0;layer<3;layer++)if(std::filesystem::exists(dir/files[layer])){ValidateModel(dir/files[layer]);r.layers[layer]=LoadModel((dir/files[layer]).string().c_str());if(!r.layers[layer].meshCount)throw std::runtime_error("Projectile model missing: "+name);ValidateGeometry(r.layers[layer]);}
  if(!r.layers[0].meshCount)throw std::runtime_error("Projectile body missing: "+name);
  auto box=GetModelBoundingBox(r.layers[0]);auto axis=ProjectileModels[i].direction.forward;r.forwardExtent=std::abs(axis.x)*(box.max.x-box.min.x)+std::abs(axis.y)*(box.max.y-box.min.y)+std::abs(axis.z)*(box.max.z-box.min.z);if(r.forwardExtent<=0)throw std::runtime_error("Empty projectile extent");
  if(std::filesystem::exists(dir/"collision.bin")){r.collision=std::make_unique<projectiles::CollisionAsset>(dir/"collision.bin");r.referenceRadius=r.collision->BoundingRadius();}
}
bool ProjectileAssets::AdvanceLoad(){
 if(failed_)throw std::logic_error("Failed projectile load must be restarted");
 if(loadStep_==LoadSteps)return true;
 try{
  if(loadStep_==0){auto shaders=projectiles::ProjectileShaderDirectory(root_);marisa_=Load(shaders,"marisa");bomb_=Load(shaders,"bombs");needle_=Load(shaders,"reimu_cone");
   // 副机外辉光复用一张柔边纹理，不加载原精灵，也不为左右实例重复创建纹理。
   Image halo=GenImageColor(256,256,BLANK);
   for(int y=0;y<256;y++)for(int x=0;x<256;x++){float dx=(x-127.5F)/127.5F,dy=(y-127.5F)/127.5F;float a=std::pow(std::max(0.F,1-dx*dx-dy*dy),3);ImageDrawPixel(&halo,x,y,{255,255,255,static_cast<unsigned char>(a*110)});}
   optionHalo_=LoadTextureFromImage(halo);UnloadImage(halo);if(!IsTextureValid(optionHalo_))throw std::runtime_error("Option halo texture failed");SetTextureFilter(optionHalo_,TEXTURE_FILTER_BILINEAR);
  }
  else if(loadStep_<=ProjectileModels.size())LoadModelResource(loadStep_-1);
  else if(loadStep_<=ProjectileModels.size()+7){
   constexpr const char*names[]={"power_small","point","power_large","bomb","full_power","life","star"};
   const auto index=loadStep_-ProjectileModels.size()-1;
   auto base=projectiles::ProjectileAssetDirectory(root_).parent_path()/"items";
   auto file=base/names[index]/(std::string(names[index])+".gltf");
   ValidateModel(file);items_[index]=LoadModel(file.string().c_str());ValidateGeometry(items_[index]);
  }
  else{collection_=std::make_unique<projectiles::CollectionRenderer>(root_,8192);lasers_=std::make_unique<projectiles::LaserRenderer>(root_,8192);}
  ++loadStep_;return loadStep_==LoadSteps;
 }catch(...){failed_=true;Release();throw;}
}
ProjectileAssets::~ProjectileAssets(){Release();}
void ProjectileAssets::Release(){
 collection_.reset();lasers_.reset();if(optionHalo_.id)UnloadTexture(optionHalo_);optionHalo_={};std::unordered_set<unsigned int> textures;
 for(auto&r:resources_)for(auto&m:r.layers)if(m.meshCount){for(int i=0;i<m.materialCount;i++)for(int j=0;j<12;j++){auto t=m.materials[i].maps[j].texture;if(t.id>1&&textures.insert(t.id).second)UnloadTexture(t);}UnloadModel(m);m={};}
 for(auto&m:items_)if(m.meshCount){for(int i=0;i<m.materialCount;i++)for(int j=0;j<12;j++){auto t=m.materials[i].maps[j].texture;if(t.id>1&&textures.insert(t.id).second)UnloadTexture(t);}UnloadModel(m);m={};}
 if(marisa_.id)UnloadShader(marisa_);if(bomb_.id)UnloadShader(bomb_);if(needle_.id)UnloadShader(needle_);marisa_={};bomb_={};needle_={};for(auto&r:resources_)r.collision.reset();
}
const Model& ProjectileAssets::Item(int type)const{if(!Ready())throw std::logic_error("Items are still loading");if(type<0||type>6)throw std::invalid_argument("Invalid Item type");return items_[type];}
const ProjectileModelResource& ProjectileAssets::Get(std::string_view name)const{return Get(DescribeProjectileModel(name).id);}
const ProjectileModelResource& ProjectileAssets::Get(ProjectileAssetId id)const{if(!Ready())throw std::logic_error("Projectile assets are still loading");auto index=static_cast<std::size_t>(id);if(index>=resources_.size())throw std::invalid_argument("Unknown projectile resource ID");return resources_[index];}
}

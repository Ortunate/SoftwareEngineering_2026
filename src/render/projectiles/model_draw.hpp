// 本文件提供共享模型分层绘制辅助函数，不改变调用方材质及纹理所有权。
#pragma once
#include "core/projectile_pose.hpp"
#include "resources/projectile_assets.hpp"
#include <array>
#include <algorithm>
namespace koumakyou::projectile_detail {
/// @brief 设置已加载模型 shader 的 uniform；位置由素材所有者缓存，缺失名称沿用 raylib 的 -1 语义。
inline void Real(const ProjectileAssets* assets,Shader s,const char*n,float v){SetShaderValue(s,assets->UniformLocation(s,n),&v,SHADER_UNIFORM_FLOAT);}
inline void Int(const ProjectileAssets* assets,Shader s,const char*n,int v){SetShaderValue(s,assets->UniformLocation(s,n),&v,SHADER_UNIFORM_INT);}
inline void Vec(const ProjectileAssets* assets,Shader s,const char*n,Vector3 v){SetShaderValue(s,assets->UniformLocation(s,n),&v,SHADER_UNIFORM_VEC3);}
inline void DrawLayer(const Model&m,Shader shader,const ProjectilePose&p,Color tint=WHITE){
 if(!m.meshCount)return;Matrix transform=MatrixMultiply(MatrixMultiply(MatrixMultiply(m.transform,MatrixScale(p.scale,p.scale,p.scale)),QuaternionToMatrix(p.rotation)),MatrixTranslate(p.position.x,p.position.y,p.position.z));
 for(int i=0;i<m.meshCount;i++){Material material=m.materials[m.meshMaterial[i]];if(shader.id)material.shader=shader;std::array<MaterialMap,12> maps;std::copy(material.maps,material.maps+12,maps.begin());material.maps=maps.data();auto c=maps[MATERIAL_MAP_DIFFUSE].color;maps[MATERIAL_MAP_DIFFUSE].color={static_cast<unsigned char>(c.r*tint.r/255),static_cast<unsigned char>(c.g*tint.g/255),static_cast<unsigned char>(c.b*tint.b/255),static_cast<unsigned char>(c.a*tint.a/255)};DrawMesh(m.meshes[i],material,transform);}
}
}

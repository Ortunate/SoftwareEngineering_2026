// 本文件为显示网格生成保留硬边的平滑法线；不改变顶点位置、索引、材质角色或碰撞资源。
#pragma once
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <tuple>
#include <vector>
namespace koumakyou::projectiles {
/// @brief mesh 须有有限的 positions/normals 及合法三角索引，返回与 vertexCount 等长的新法线。
/// 同位置、同 COLOR_0.g 显示角色的顶点按三角形角度加权；超过 60° 的折角保持硬边。
/// 仅在资源加载时调用；返回值独立拥有存储，既不修改原网格，也不访问 GPU。
inline std::vector<Vector3> SmoothProjectileNormals(const Mesh& mesh){
 struct Contribution {Vector3 normal;float angle;};
 std::map<std::tuple<float,float,float,int>,std::size_t> lookup;
 std::vector<std::vector<Contribution>> groups;std::vector<std::size_t> group(mesh.vertexCount);
 const auto position=[&](int i){return Vector3{mesh.vertices[3*i],mesh.vertices[3*i+1],mesh.vertices[3*i+2]};};
 for(int i=0;i<mesh.vertexCount;++i){
  const auto p=position(i);const int role=mesh.colors?mesh.colors[4*i+1]:0;
  const auto [entry,added]=lookup.emplace(std::tuple{p.x,p.y,p.z,role},groups.size());
  if(added)groups.emplace_back();group[i]=entry->second;
 }
 for(int t=0;t<mesh.triangleCount;++t){
  std::array<int,3> id{};std::array<Vector3,3> p{};
  for(int j=0;j<3;++j){id[j]=mesh.indices?mesh.indices[3*t+j]:3*t+j;p[j]=position(id[j]);}
  const auto cross=Vector3CrossProduct(Vector3Subtract(p[1],p[0]),Vector3Subtract(p[2],p[0]));
  if(Vector3LengthSqr(cross)<1e-16F)continue;
  const auto normal=Vector3Normalize(cross);
  for(int j=0;j<3;++j){
   const auto a=Vector3Normalize(Vector3Subtract(p[(j+1)%3],p[j])),b=Vector3Normalize(Vector3Subtract(p[(j+2)%3],p[j]));
   const float angle=std::acos(std::clamp(Vector3DotProduct(a,b),-1.F,1.F));
   groups[group[id[j]]].push_back({normal,angle});
  }
 }
 std::vector<Vector3> result(mesh.vertexCount);
 for(int i=0;i<mesh.vertexCount;++i){
  const Vector3 original=Vector3Normalize({mesh.normals[3*i],mesh.normals[3*i+1],mesh.normals[3*i+2]});Vector3 sum{};
  // 以原法线约束可平滑的相邻面，避免跨过刀背/刃口或薄片正背面混合，导致高光翻转。
  for(const auto& face:groups[group[i]])if(Vector3DotProduct(original,face.normal)>=.5F-.00001F)
   sum=Vector3Add(sum,Vector3Scale(face.normal,face.angle));
  result[i]=Vector3LengthSqr(sum)>1e-16F?Vector3Normalize(sum):original;
 }
 return result;
}
}

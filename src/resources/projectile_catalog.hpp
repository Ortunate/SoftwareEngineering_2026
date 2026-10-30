// 本文件登记所有已建模资产的本地方向；不加载 GPU 资源或依赖游戏状态。
#pragma once
#include "raylib.h"
#include <array>
#include <string_view>
#include <stdexcept>
#include <cstdint>
namespace koumakyou {
// 轴均为导出后 glTF 本地单位向量；surfaceFront 是图案面，不等于运动正向。
struct ModelDirection {Vector3 forward{0,1,0},up{0,0,1},surfaceFront{0,0,1};};
// 公共模型编号独立于 Stage::VisualId；新增资产只能追加编号，不能重排已有值。
enum class ProjectileAssetId : std::uint8_t {
 BombSeal=0,
 BombSpark=1,
 BombStar=2,
 BubbleClassic=3,
 FireballClassic=4,
 KnifeClassic=5,
 KunaiClassic=6,
 LargeOrbClassic=7,
 LaserDefault=8,
 LaserRound=9,
 MarisaExplosion=10,
 MarisaLaser=11,
 MarisaMissile=12,
 MarisaOption=13,
 MarisaPrimary=14,
 NeedleClassic=15,
 OrbClassic=16,
 PelletClassic=17,
 ReimuCyan=18,
 ReimuGold=19,
 ReimuNeedle=20,
 ReimuOption=21,
 RiceClassic=22,
 RingOrbClassic=23,
 Invalid=255
};
struct ProjectileModelDefinition {ProjectileAssetId id;std::string_view name;ModelDirection direction;};
inline constexpr std::array<ProjectileModelDefinition,24> ProjectileModels{{
 {ProjectileAssetId::BombSeal,"bomb_seal",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::BombSpark,"bomb_spark",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::BombStar,"bomb_star",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::BubbleClassic,"bubble_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::FireballClassic,"fireball_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::KnifeClassic,"knife_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::KunaiClassic,"kunai_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::LargeOrbClassic,"large_orb_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::LaserDefault,"laser_default",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::LaserRound,"laser_round",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::MarisaExplosion,"marisa_explosion",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::MarisaLaser,"marisa_laser",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::MarisaMissile,"marisa_missile",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::MarisaOption,"marisa_option",{{0,0,1},{0,1,0},{0,0,1}}},
 {ProjectileAssetId::MarisaPrimary,"marisa_primary",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::NeedleClassic,"needle_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::OrbClassic,"orb_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::PelletClassic,"pellet_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::ReimuCyan,"reimu_cyan",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::ReimuGold,"reimu_gold",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::ReimuNeedle,"reimu_needle",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::ReimuOption,"reimu_option",{{0,0,1},{0,1,0},{0,0,1}}},
 {ProjectileAssetId::RiceClassic,"rice_classic",{{0,1,0},{0,0,1},{0,0,1}}},
 {ProjectileAssetId::RingOrbClassic,"ring_orb_classic",{{0,1,0},{0,0,1},{0,0,1}}},
}};
static_assert([]{for(std::size_t i=0;i<ProjectileModels.size();i++)if(static_cast<std::size_t>(ProjectileModels[i].id)!=i)return false;return true;}());
/// @brief 稳定模型 ID 查询；不属于目录的编号抛 invalid_argument。
inline const ProjectileModelDefinition& DescribeProjectileModel(ProjectileAssetId id){auto index=static_cast<std::size_t>(id);if(index>=ProjectileModels.size())throw std::invalid_argument("Unknown projectile model ID");return ProjectileModels[index];}
/// @brief 返回静态模型方向描述；未知资产名抛 invalid_argument。
inline const ProjectileModelDefinition& DescribeProjectileModel(std::string_view name){for(const auto& d:ProjectileModels)if(d.name==name)return d;throw std::invalid_argument("Unknown projectile model");}
}

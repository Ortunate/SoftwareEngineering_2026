// 本文件枚举对照查看器的弹体与原作动画绑定；只读目录不参与生产关卡编排。
#pragma once
#include "resources/projectile_catalog.hpp"
#include "gameplay/effects/player_bomb.hpp"
#include <array>
namespace koumakyou::comparison {
/// @brief 查看器的渲染路径分类，编号不属于生产弹种或关卡素材空间。
enum class Kind {Enemy,Laser,Player,Bomb,Option};
/// @brief variants 为有效颜色/档位/素材层数；index 为敌弹种、光束套组或 Bomb 的 ShotType。
/// model 为正式模型编号；封魔阵使用生产平面几何，明确标为 Invalid，不能伪造独立模型。
struct Entry {const char* key;const char* title;Kind kind;ProjectileAssetId model;int index,variants; battle::Atlas atlas=battle::Atlas::None;int script=0;};
inline constexpr std::array<Entry,25> kEntries{{
 {"pellet","粒弹",Kind::Enemy,ProjectileAssetId::PelletClassic,0,16},
 {"ring","环形小玉",Kind::Enemy,ProjectileAssetId::RingOrbClassic,1,16},
 {"rice","米弹",Kind::Enemy,ProjectileAssetId::RiceClassic,2,16},
 {"orb","实心小玉",Kind::Enemy,ProjectileAssetId::OrbClassic,3,16},
 {"kunai","苦无弹",Kind::Enemy,ProjectileAssetId::KunaiClassic,4,16},
 {"needle","针形弹",Kind::Enemy,ProjectileAssetId::NeedleClassic,5,16},
 {"large-orb","大玉",Kind::Enemy,ProjectileAssetId::LargeOrbClassic,6,8},
 {"fireball","火球",Kind::Enemy,ProjectileAssetId::FireballClassic,7,1},
 {"knife","刀弹",Kind::Enemy,ProjectileAssetId::KnifeClassic,8,8},
 {"bubble","大泡",Kind::Enemy,ProjectileAssetId::BubbleClassic,9,5},
 {"laser-default","敌激光·默认",Kind::Laser,ProjectileAssetId::LaserDefault,0,16},
 {"laser-round","敌激光·圆润",Kind::Laser,ProjectileAssetId::LaserRound,1,16},
 {"reimu-gold","灵梦·直射符札",Kind::Player,ProjectileAssetId::ReimuGold,0,1,battle::Atlas::Reimu,64},
 {"reimu-cyan","灵梦·追踪符札",Kind::Player,ProjectileAssetId::ReimuCyan,0,1,battle::Atlas::Reimu,65},
 {"reimu-needle","灵梦·侧针",Kind::Player,ProjectileAssetId::ReimuNeedle,0,1,battle::Atlas::Reimu,66},
 {"marisa-primary","魔理沙·主弹",Kind::Player,ProjectileAssetId::MarisaPrimary,0,1,battle::Atlas::Marisa,64},
 {"marisa-missile","魔理沙·导弹",Kind::Player,ProjectileAssetId::MarisaMissile,0,4,battle::Atlas::Marisa,65},
 {"marisa-laser","魔理沙·激光",Kind::Player,ProjectileAssetId::MarisaLaser,0,3,battle::Atlas::Marisa,69},
 {"marisa-explosion","魔理沙·爆破",Kind::Player,ProjectileAssetId::MarisaExplosion,0,4,battle::Atlas::Marisa,97},
 {"bomb-seal","Bomb·梦想封印",Kind::Bomb,ProjectileAssetId::BombSeal,0,4},
 {"bomb-barrier","Bomb·封魔阵",Kind::Bomb,ProjectileAssetId::Invalid,1,4},
 {"bomb-stars","Bomb·星群单体",Kind::Bomb,ProjectileAssetId::BombStar,2,3},
 {"bomb-spark","Bomb·Master Spark",Kind::Bomb,ProjectileAssetId::BombSpark,3,4},
 {"reimu-option","附加·灵梦副机",Kind::Option,ProjectileAssetId::ReimuOption,0,2,battle::Atlas::Reimu,128},
 {"marisa-option","附加·魔理沙副机",Kind::Option,ProjectileAssetId::MarisaOption,1,2,battle::Atlas::Marisa,128}
}};
/// @brief 返回本类有效变体的原作绑定；variant 为零起算，越界抛 out_of_range。
inline battle::Selection Reference(const Entry& e,int variant){
 if(variant<0||variant>=e.variants)throw std::out_of_range("Invalid comparison variant");
 if(e.kind==Kind::Enemy)return battle::EnemyBullet(e.index,variant);
 if(e.kind==Kind::Laser)return {battle::Atlas::BeamBullets,e.index,variant,e.index*16+variant};
 if(e.kind==Kind::Bomb){const auto& d=BombDefinition(static_cast<ShotType>(e.index));return {d.atlas,d.firstScript+variant,0,e.index==0?131:e.index==1?132:e.index==2?18:19};}
 return {e.atlas,e.script+variant};
}
/// @brief 演出帧采用 60 Hz；爆破在 20 帧内结束，其余素材保留足够长的审阅循环。
inline int FrameCount(const Entry& e){
 if(e.kind==Kind::Bomb)return BombDefinition(static_cast<ShotType>(e.index)).durationFrames;
 return e.model==ProjectileAssetId::MarisaExplosion?20:e.model==ProjectileAssetId::MarisaLaser?150:120;
}
/// @brief 返回 60 Hz 默认审阅帧，避开素材淡入的空白阶段；爆破取 6 帧、Bomb 取 60 帧。
inline int InitialFrame(const Entry& e){return e.model==ProjectileAssetId::MarisaExplosion?6:e.kind==Kind::Bomb?60:30;}
}

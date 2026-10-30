// 本文件实现准星射线汇聚及最多六度的目标球边缘辅助。
#include "gameplay/actors/aim.hpp"
#include "app/math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace koumakyou {
namespace {
constexpr float kDefaultAimDepth=20;
bool Valid(const PlayerAimTarget& t) {
    return t.radius>0 && std::isfinite(t.radius) && std::isfinite(t.position.x) && std::isfinite(t.position.y) && std::isfinite(t.position.z);
}
/// @brief 求最近准星命中球体的深度；forward 须为单位向量，返回正世界单位，未命中时使用默认深度。
float AimDepth(Vector3 position, Vector3 forward, std::span<const PlayerAimTarget> targets) {
    float nearestEntry = std::numeric_limits<float>::max();
    float depth = kDefaultAimDepth;
    for (const auto& target : targets) {
        if (!Valid(target)) continue;
        const auto relative = game_math::Subtract(target.position, position);
        const float along = relative.x * forward.x + relative.y * forward.y + relative.z * forward.z;
        const auto perpendicular = game_math::Subtract(relative, game_math::Scale(forward, along));
        const float halfChordSquared = target.radius * target.radius -
            (perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y + perpendicular.z * perpendicular.z);
        if (along <= 0.0F || halfChordSquared < 0.0F) continue;
        const float entry = std::max(0.0F, along - std::sqrt(halfChordSquared));
        if (entry >= nearestEntry) continue;
        nearestEntry = entry;
        // 选择首先遮住射线的判定体，但瞄向射线上穿过其内部的点，避免从偏下的模块擦过球面。
        // 不能直接瞄准球心，否则准星边缘的目标会获得额外吸附；极近距离保留正深度防止零向量。
        depth = std::max(0.05F, along);
    }
    return depth;
}

}
PlayerAimResult ResolvePlayerAim(Vector3 position,Vector3 forward,std::span<const PlayerAimTarget> targets,bool enabled) {
    const float depth=AimDepth(position,forward,targets);
    PlayerAimResult result{game_math::Add(position,game_math::Scale(forward,depth)),forward,depth,false};
    // 直接命中优先，即使目标当前免伤，也保持旧射线汇聚规则。
    for(const auto& t:targets) if(Valid(t)) {
        const auto v=game_math::Subtract(t.position,position);
        const float along=v.x*forward.x+v.y*forward.y+v.z*forward.z;
        if(along>0 && game_math::DistanceSquared(t.position,game_math::Add(position,game_math::Scale(forward,along)))<=t.radius*t.radius) return result;
    }
    if(!enabled)return result;
    constexpr float limit=6*DEG2RAD;
    float best=std::numeric_limits<float>::max(),nearest=best;
    for(const auto& t:targets) {
        if(!Valid(t) || !t.eligible)continue;
        const auto v=game_math::Subtract(t.position,position);
        const float distance=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
        const float along=v.x*forward.x+v.y*forward.y+v.z*forward.z;
        if(!std::isfinite(distance) || distance<=t.radius || along<=0)continue;
        const float angle=std::acos(std::clamp(along/distance,-1.0F,1.0F));
        const float edge=std::max(0.0F,angle-std::asin(std::clamp(t.radius/distance,0.0F,1.0F)));
        if(edge>limit || edge>best || (edge==best && distance>=nearest))continue;
        best=edge;nearest=distance;
        const float turn=std::min(angle,edge+std::min(0.25F*DEG2RAD,limit-edge));
        const auto tangent=game_math::Normalize(game_math::Subtract(v,game_math::Scale(forward,along)));
        result.forward=game_math::Add(game_math::Scale(forward,std::cos(turn)),game_math::Scale(tangent,std::sin(turn)));
        // 使用修正射线上最接近球心的内部点，避免贴近时副机越过目标。
        const float projection=v.x*result.forward.x+v.y*result.forward.y+v.z*result.forward.z;
        result.point=game_math::Add(position,game_math::Scale(result.forward,projection));
        result.depth=std::max(0.05F,projection*std::cos(turn));result.assisted=true;
        result.targetPosition=t.position;result.targetRadius=t.radius;
    }
    return result;
}
}

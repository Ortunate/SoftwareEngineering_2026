// 本文件集中管理新典 Bomb 的帧伤害、符卡保护及尚未替换的普通弹 Boss 倍率。
#include "gameplay/combat/damage_rules.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace koumakyou {
DamageResult ResolveDamage(const DamageContext& context,const DamageRuleProvider* provider) {
    const auto valid=[](float value){return std::isfinite(value) && value>=0;};
    if(!valid(context.baseDamage) || !valid(context.contactSeconds))
        throw std::invalid_argument("Invalid damage context");
    if(!context.damageable)return {0,DamageCoverage::BaseOnly};
    if(provider)if(auto result=provider->Resolve(context)) {
        if(!valid(result->amount))throw std::invalid_argument("Invalid damage rule result");
        return *result;
    }
    // 新典 RVA 0x393dd 先截到 70，0x3943b 起区分该符卡是否使用 Bomb，再按整数 /3。
    // 调用方先合并本帧命中通道；若逐纹理层调用，这个上限和恋札的单通道约束都会失效。
    if(context.source==DamageSource::Bomb) {
        float damage=std::min(70.0F,std::floor(context.baseDamage));
        if(context.spell)damage=!context.bombUsedDuringSpell ? 0 : damage>0 ? std::max(1.0F,std::floor(damage/3)) : 0;
        return {damage,DamageCoverage::NativeRule};
    }
    float base=context.baseDamage;
    if(context.bombActive && base>0) {
        const float frames=context.contactSeconds>0 ? context.contactSeconds*60 : 1;
        // 原通道伤害为整数；先消除 HP/秒和秒数换算误差，避免 144 FPS 把应为 16 的值向下取成 15。
        base=std::max(1.0F,std::floor(base/frames/3+0.00001F))*frames;
    }
    if(context.target==DamageTarget::Boss)
        return {base*0.8F/(context.spell ? 7.0F : 1.0F),DamageCoverage::Fallback};
    return {base,DamageCoverage::BaseOnly};
}
} // namespace koumakyou

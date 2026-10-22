// 本文件集中管理尚未逆向的伤害规则回退，避免临时倍率混入原始数据或散落在碰撞逻辑中。
#include "gameplay/combat/damage_rules.hpp"
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
    // 旧项目 HP 曾除以 25；临时 Bomb 22/35 恢复到相同的原始 HP 量纲，不宣称是新典 Bomb。
    if(context.source==DamageSource::Bomb)
        return {context.target==DamageTarget::Enemy ? 550.0F : 875.0F/(context.spell ? 3.0F : 1.0F),DamageCoverage::Fallback};
    if(context.target==DamageTarget::Boss)
        return {context.baseDamage*0.8F/(context.spell ? 7.0F : 1.0F),DamageCoverage::Fallback};
    return {context.baseDamage,DamageCoverage::BaseOnly};
}
} // namespace koumakyou

// 本文件定义原始伤害的结算边界，供尚未逆向的 Bomb、减伤与同帧上限逐步替换。
#pragma once
#include "gameplay/data/native_combat.hpp"
#include <optional>

namespace koumakyou {
enum class DamageSource { Projectile, Laser, Explosion, Bomb };
enum class DamageTarget { Enemy, Boss };
/// @brief BaseOnly 只应用已导入的基准伤害，未验证引擎最终结算；Fallback 为项目临时规则。
/// NativeRule 仅供已经核对执行代码的规则提供方返回，不能由静态火力表推断。
enum class DamageCoverage { BaseOnly, Fallback, NativeRule };
/// @brief 一次接触的借用快照；baseDamage 已累计接触秒数，单位为原始 HP，Bomb 时为 0。
/// targetIdentity 不转移所有权，只可作当帧目标键，不可解引用或跨帧缓存（对象池会复用地址）；sourceRva=0 表示非原表生成。
/// stageSeconds 使用关卡游戏时钟；contactSeconds 为本次尚未结算的持续接触秒数，普通弹为 0。
struct DamageContext {
    DamageSource source;
    DamageTarget target;
    ShotType shot;
    float baseDamage;
    bool damageable, spell;
    const void* targetIdentity;
    std::uint32_t sourceRva;
    double stageSeconds;
    float contactSeconds;
};
struct DamageResult { float amount; DamageCoverage coverage; };
/// @brief 关卡持有的可选规则提供方，可维护同帧上限等状态；换局须由关卡重置状态。
/// 未实现的情况返回 nullopt 交给显式回退；结果须为非负有限 HP。生命周期由 StageContent 管理。
class DamageRuleProvider {
public:
    virtual ~DamageRuleProvider() = default;
    [[nodiscard]] virtual std::optional<DamageResult> Resolve(const DamageContext&) const = 0;
};
/// @brief 保护状态优先返回 0，其余先调用提供方；非法输入/提供方结果抛 invalid_argument。
/// 缺省小怪使用原始基准；Boss 沿用临时倍率，Bomb 沿用换算至原始 HP 的项目值。
/// 未导出的同帧上限、Bomb 阶段等不得被此回退标记为 NativeRule。
[[nodiscard]] DamageResult ResolveDamage(const DamageContext& context, const DamageRuleProvider* provider=nullptr);
} // namespace koumakyou

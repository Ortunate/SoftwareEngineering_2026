// 本文件定义原始伤害结算边界，集中处理已核对的 Bomb 上限/符卡减伤及其余项目回退。
#pragma once
#include "gameplay/data/native_combat.hpp"
#include <optional>

namespace koumakyou {
enum class DamageSource { Projectile, Laser, Explosion, Bomb };
enum class DamageTarget { Enemy, Boss };
/// @brief BaseOnly 只应用已导入的基准伤害，未验证引擎最终结算；Fallback 为项目临时规则。
/// NativeRule 仅供已经核对执行代码的规则提供方返回，不能由静态火力表推断。
enum class DamageCoverage { BaseOnly, Fallback, NativeRule };
/// @brief 一次接触的借用快照；baseDamage 为原始 HP，Bomb 必须传入同一逻辑帧所有命中通道的合计。
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
    bool bombActive=false; // 普通弹在 Bomb 期间按每次原始接触伤害 /3 取整，最低 1；持续弹按接触帧数换算。
    bool bombUsedDuringSpell=true; // 新符卡开始前已释放的 Bomb 不得伤害该符卡，与捕获记录使用同一计数基准。
};
struct DamageResult { float amount; DamageCoverage coverage; };
/// @brief 关卡持有的可选 HP 规则提供方，不负责空间接触；无敌目标或 Bomb 零接触不会调用。
/// 可维护同帧上限等状态；换局须由关卡重置状态。
/// 未实现的情况返回 nullopt 交给显式回退；结果须为非负有限 HP。生命周期由 StageContent 管理。
class DamageRuleProvider {
public:
    virtual ~DamageRuleProvider() = default;
    [[nodiscard]] virtual std::optional<DamageResult> Resolve(const DamageContext&) const = 0;
};
/// @brief 保护状态及 Bomb 零接触优先返回 0，其余先调用提供方；非法输入/提供方结果抛 invalid_argument。
/// Bomb 按新典每逻辑帧 70 上限及符卡 /3 整数减伤结算；不再有固定全场伤害。
/// 其余 Boss 普通弹暂沿用现有倍率；NativeRule 不代表三维几何来自二维原版。
[[nodiscard]] DamageResult ResolveDamage(const DamageContext& context, const DamageRuleProvider* provider=nullptr);
} // namespace koumakyou

// 本文件定义角色行为标识、阶段技能组合及血条外观配置；不保存技能血量或攻击实现。
#pragma once
#include "gameplay/actors/boss.hpp"
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>
namespace koumakyou::bosses {
class ConfiguredBoss;
/// @brief 每种角色枚举独占作用域，防止不同 Boss 的同序号行为被混用。
template<class Enum> inline constexpr char kBehaviorScope=0;
/// @brief 角色内部 Behavior 枚举的类型安全引用；空值只用于未配置的入场，不能作为技能。
struct BehaviorId {
    const void* scope=nullptr;
    int value=-1;
    constexpr BehaviorId()=default;
    template<class Enum> requires std::is_enum_v<Enum>
    constexpr BehaviorId(Enum id):scope(&kBehaviorScope<Enum>),value(static_cast<int>(id)) {}
    template<class Enum> bool Is() const {return scope==&kBehaviorScope<Enum>;}
    explicit operator bool() const {return scope!=nullptr;}
    bool operator==(const BehaviorId&) const=default;
};
/// @brief 角色专属行为入口；闭包按值拥有参数，只借用正在执行的演员与 Boss。
using Attack=std::function<behavior::Routine(behavior::Actor&,ConfiguredBoss&)>;
/// @brief 将角色内部的一参数静态协程绑定到公共执行签名，调用时借用当前 Actor。
inline Attack Run(behavior::Entry entry) {
    return [entry](behavior::Actor& actor,ConfiguredBoss&) {return entry(actor);};
}
/// @brief 技能的只读定义，由角色解析，启动配置只能引用它，不能覆盖血量。
/// health 为该技能实际可消耗 HP；reserveHealth 是原生命寄存器的保留偏移，不属于后继技能状态。
/// Actor 和 Boss::health 为与原行为差分兼容仍使用 health+reserveHealth，展示按剩余值减去偏移计算。
/// 所有时间为 60 Hz 内容帧；spellName 借用角色静态文本，行为闭包按值保存。
struct SkillDefinition {
    Attack behavior;
    float health=1,reserveHealth=0,entranceHealth=0;
    int timeoutFrames=-1,timeoutArmFrame=0;
    double initialFrame=0;
    bool thresholdDefeatsSpell=false;
    const char* spellName=nullptr;
    int spellCard=0,spellRevealFrame=0;
    bool spellNeedsTimer=false,reactToBomb=false;
    float hitRadius=0; // 0 继承入场或上一技能半径，否则使用世界单位。
    float NativeHealth() const {return health+reserveHealth;}
};
/// @brief 技能完成后的控制流；Jump 的 phase/skill 均为配置内 0 起算索引，其他类型不使用索引。
struct Transition {
    enum class Kind {Next,Exit,Jump};
    Kind kind=Kind::Next;
    std::size_t phase=0,skill=0;
    static Transition Exit() {return {Kind::Exit};}
    static Transition Jump(std::size_t phase,std::size_t skill) {return {Kind::Jump,phase,skill};}
};
/// @brief 阶段内一次技能使用；同一技能可重复引用，每次使用独立交接。默认顺序执行，末技能转下一阶段。
struct SkillSlot {
    BehaviorId id;
    Transition onDamage{},onTimeout{};
    SkillSlot()=default;
    SkillSlot(BehaviorId id):id(id) {}
    template<class Enum> requires std::is_enum_v<Enum>
    SkillSlot(Enum id):id(id) {}
};
/// @brief 血条一段的显式外观；skillIndex 引用本阶段技能（0 起算），ratio 为整条固定宽度比例。
/// 段按屏幕从左到右排列，与技能执行顺序独立；同阶段每个技能必须恰好对应一段，比例之和为 1。
struct HealthBarSegmentConfig {
    std::size_t skillIndex=0;
    float ratio=1;
    Color color=WHITE;
};
/// @brief 阶段血条的外观，段数随技能数量变化，不限制为三段；启动时复制并校验。
struct HealthBarConfig {
    std::vector<HealthBarSegmentConfig> segments;
    Color background{64,35,50,160}; // 空段底色也属于配置，Boss 画笔不按技能类型推导颜色。
};
/// @brief 一阶段可以包含任意多个非符/符卡技能；技能只引用角色定义，外观完全由 config 明确指定。
struct Phase {
    std::vector<SkillSlot> skills;
    HealthBarConfig healthBar;
};
/// @brief StartBoss 复制的完整战斗配置；phases.size() 为阶段数，每阶段 skills.size() 为技能数。
/// intro 非空时等待通知，resumeSteps 每项消耗一次通知，下一次通知才开战。素材借用当前关卡。
struct StartConfig {
    Vector3 position{};
    float hitRadius=native_sizes::kBoss48Radius;
    std::uint32_t seed=1;
    VisualId visual=kInvalidVisualId,defeatBase=kInvalidVisualId;
    void (*configure)(behavior::Actor&,bool)=nullptr;
    ShotType shot=ShotType::HomingAmulet;
    BehaviorId intro{},defeated{},retreat{};
    std::vector<BehaviorId> resumeSteps;
    std::vector<Phase> phases;
    bool initialDamageable=false,initialContact=true;
    bool musicOnEntrance=false,controlsWorldTime=false;
    int defeatScore=0,retreatScore=0;
};
/// @brief 事件表借用的静态战斗工厂；解析机体配置后返回已启动角色，调用方取得唯一所有权。
struct Battle {std::unique_ptr<Boss> (*create)(ShotType)=nullptr;};
template<class Character,StartConfig (*Build)(ShotType)>
std::unique_ptr<Boss> Create(ShotType shot) {
    auto boss=std::make_unique<Character>();boss->StartBoss(Build(shot));return boss;
}
/// @brief 完成哨兵仅唤醒公共交接；行为不选择下一个技能，Actor::finished 表示整场脚本已退场。
behavior::Routine SkillComplete(behavior::Actor& actor);
} // namespace koumakyou::bosses

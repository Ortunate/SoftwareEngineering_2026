// 本文件定义 DaiyouseiBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/daiyousei.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void DaiyouseiBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(2,0,frame,sub));}
}
Attack DaiyouseiBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Midboss:return Run(Behaviors::Midboss);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::Timeout:return Run(Behaviors::Timeout);
        default:throw std::invalid_argument("Unknown Daiyousei behavior");
    }
}
SkillDefinition DaiyouseiBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition DaiyouseiBoss::ResolveSkillForDifficulty(BehaviorId id,[[maybe_unused]] ShotType shot,native_combat::Difficulty difficulty) const {
    if(native_combat::DifficultyIndex(difficulty)<0)throw std::invalid_argument("Invalid difficulty");
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    if(selected==Behavior::Midboss)return CombatSkill(ResolveBehavior(id),Spawn(2498,20),native_combat::RequireLiteral(native_combat::Setting(2,20,115,0,difficulty)));
    throw std::invalid_argument("Daiyousei behavior is not a combat skill");
}
}

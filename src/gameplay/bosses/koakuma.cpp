// 本文件定义 KoakumaBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/koakuma.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void KoakumaBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
Attack KoakumaBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Encounter:return Run(Behaviors::Encounter);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::Timeout:return Run(Behaviors::Timeout);
        default:throw std::invalid_argument("Unknown Koakuma behavior");
    }
}
SkillDefinition KoakumaBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition KoakumaBoss::ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const {
    if(native_combat::DifficultyIndex(difficulty)<0||static_cast<unsigned>(shot)>3)
        throw std::invalid_argument("Invalid Koakuma difficulty or shot");
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    if(selected==Behavior::Encounter)return CombatSkill(ResolveBehavior(id),
        native_combat::RequireLiteral(native_combat::Spawn(4,0,4058,21)),
        native_combat::RequireLiteral(native_combat::Setting(4,21,115,0,difficulty)));
    throw std::invalid_argument("Koakuma behavior is not a combat skill");
}
}

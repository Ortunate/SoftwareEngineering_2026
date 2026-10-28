// 本文件定义 KoakumaBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/koakuma.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void KoakumaBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
float S(int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(4,sub,opcode));}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(4,0,frame,sub));}
}
Attack KoakumaBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Encounter:return Run(Behaviors::Encounter);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::Timeout:return Run(Behaviors::Timeout);
        default:throw std::invalid_argument("Unknown Koakuma behavior");
    }
}
SkillDefinition KoakumaBoss::ResolveSkill(BehaviorId id,[[maybe_unused]] ShotType shot) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    if(selected==Behavior::Encounter)return CombatSkill(ResolveBehavior(id),Spawn(4058,21),S(21,115));
    throw std::invalid_argument("Koakuma behavior is not a combat skill");
}
}

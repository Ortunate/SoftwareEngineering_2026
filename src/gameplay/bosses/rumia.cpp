// 本文件定义 RumiaBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/rumia.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void RumiaBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
float S(int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(1,sub,opcode));}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(1,0,frame,sub));}
}
Attack RumiaBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Midboss:return Run(Behaviors::Midboss);
        case Behavior::MidbossDefeated:return Run(Behaviors::MidbossDefeated);
        case Behavior::MidbossTimeout:return Run(Behaviors::MidbossTimeout);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::SecondNonspell:return Run(Behaviors::SecondNonspell);
        case Behavior::NightBird:return Run(Behaviors::NightBird);
        case Behavior::Demarcation:return Run(Behaviors::Demarcation);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::End:return Run(Behaviors::End);
        default:throw std::invalid_argument("Unknown Rumia behavior");
    }
}
SkillDefinition RumiaBoss::ResolveSkill(BehaviorId id,[[maybe_unused]] ShotType shot) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    switch(selected) {
        case Behavior::Midboss:return CombatSkill(ResolveBehavior(id),Spawn(1882,9),S(9,115));
        case Behavior::FirstNonspell: {auto p=CombatSkill(ResolveBehavior(id),S(14,111),S(14,115),S(14,113));p.initialFrame=S(14,112);return p;}
        case Behavior::NightBird:return SpellSkill(CombatSkill(ResolveBehavior(id),S(14,113),S(27,115)),"夜符「Night Bird」",3);
        case Behavior::SecondNonspell: {auto p=CombatSkill(ResolveBehavior(id),S(19,111),S(19,115),S(19,113));p.initialFrame=S(19,112);return p;}
        case Behavior::Demarcation:return SpellSkill(CombatSkill(ResolveBehavior(id),S(19,113),S(30,115)),"暗符「Demarcation」",7);
        default:break;
    }
    throw std::invalid_argument("Rumia behavior is not a combat skill");
}
}

// 本文件定义 RumiaBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/rumia.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void RumiaBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(1,0,frame,sub));}
}
Attack RumiaBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Midboss:return Run(Behaviors::Midboss);
        case Behavior::MoonlightRay:return Run(Behaviors::MoonlightRay);
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
SkillDefinition RumiaBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition RumiaBoss::ResolveSkillForDifficulty(BehaviorId id,[[maybe_unused]] ShotType shot,native_combat::Difficulty difficulty) const {
    const int index=native_combat::DifficultyIndex(difficulty);
    if(index<0)throw std::invalid_argument("Invalid difficulty");
    const auto setting=[difficulty](int sub,int opcode) {
        return native_combat::RequireLiteral(native_combat::Setting(1,sub,opcode,0,difficulty));
    };
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    switch(selected) {
        case Behavior::Midboss:return CombatSkill(ResolveBehavior(id),Spawn(1882,9),setting(9,115),index>=2?setting(9,113):0);
        case Behavior::MoonlightRay:
            if(index<2)throw std::invalid_argument("Moonlight Ray is unavailable on this difficulty");
            return SpellSkill(CombatSkill(ResolveBehavior(id),setting(9,113),setting(11,115)),"月符「Moonlight Ray」",index-1);
        case Behavior::FirstNonspell: {auto p=CombatSkill(ResolveBehavior(id),setting(14,111),setting(14,115),index==0?0:setting(14,113));p.initialFrame=setting(14,112);return p;}
        case Behavior::NightBird:
            if(index==0)throw std::invalid_argument("Night Bird is unavailable on Easy");
            return SpellSkill(CombatSkill(ResolveBehavior(id),setting(14,113),setting(27,115)),"夜符「Night Bird」",index+2);
        case Behavior::SecondNonspell: {auto p=CombatSkill(ResolveBehavior(id),setting(19,111),setting(19,115),setting(19,113));p.initialFrame=setting(19,112);return p;}
        case Behavior::Demarcation:return SpellSkill(CombatSkill(ResolveBehavior(id),setting(19,113),setting(30,115)),"暗符「Demarcation」",index+6);
        default:break;
    }
    throw std::invalid_argument("Rumia behavior is not a combat skill");
}
}

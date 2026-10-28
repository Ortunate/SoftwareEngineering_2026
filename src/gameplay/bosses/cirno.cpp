// 本文件定义 CirnoBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/cirno.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void CirnoBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
float S(int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(2,sub,opcode));}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(2,0,frame,sub));}
}
Attack CirnoBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::IcicleFall:return Run(Behaviors::IcicleFall);
        case Behavior::SecondNonspell:return Run(Behaviors::SecondNonspell);
        case Behavior::PerfectFreeze:return Run(Behaviors::PerfectFreeze);
        case Behavior::DiamondBlizzard:return Run(Behaviors::DiamondBlizzard);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::End:return Run(Behaviors::End);
        default:throw std::invalid_argument("Unknown Cirno behavior");
    }
}
SkillDefinition CirnoBoss::ResolveSkill(BehaviorId id,[[maybe_unused]] ShotType shot) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    switch(selected) {
        case Behavior::FirstNonspell:return CombatSkill(ResolveBehavior(id),S(22,111),S(22,115),S(22,113));
        case Behavior::IcicleFall:return SpellSkill(CombatSkill(ResolveBehavior(id),S(22,113),S(31,115)),"冰符「Icicle Fall」",11);
        case Behavior::SecondNonspell:return CombatSkill(ResolveBehavior(id),S(25,111),S(25,115),S(25,113));
        case Behavior::PerfectFreeze: {auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(25,113),S(37,115),S(36,113)),"冻符「Perfect Freeze」",15);p.thresholdDefeatsSpell=true;return p;}
        case Behavior::DiamondBlizzard: {auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(36,113),S(40,115)),"雪符「Diamond Blizzard」",18);p.timeoutArmFrame=60;return p;}
        default:break;
    }
    throw std::invalid_argument("Cirno behavior is not a combat skill");
}
}

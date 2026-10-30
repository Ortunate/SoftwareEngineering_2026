// 本文件定义 CirnoBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/cirno.hpp"
#include "gameplay/bosses/skill_definition.hpp"
#include <array>
namespace koumakyou::bosses {
void CirnoBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
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
SkillDefinition CirnoBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition CirnoBoss::ResolveSkillForDifficulty(BehaviorId id,[[maybe_unused]] ShotType shot,native_combat::Difficulty difficulty) const {
    const int index=native_combat::DifficultyIndex(difficulty);
    if(index<0)throw std::invalid_argument("Invalid difficulty");
    const auto setting=[&](int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(2,sub,opcode,0,difficulty));};
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    switch(selected) {
        case Behavior::FirstNonspell:return CombatSkill(ResolveBehavior(id),setting(22,111),setting(22,115),setting(22,113));
        case Behavior::IcicleFall:return SpellSkill(CombatSkill(ResolveBehavior(id),setting(22,113),setting(index<2 ? 31 : 34,115)),index<2 ? "冰符「Icicle Fall」" : "雹符「Hailstorm」",std::array{10,11,12,13}.at(index));
        case Behavior::SecondNonspell:return CombatSkill(ResolveBehavior(id),setting(25,111),setting(25,115),setting(25,113));
        case Behavior::PerfectFreeze: {auto p=SpellSkill(CombatSkill(ResolveBehavior(id),setting(25,113),setting(37,115),index==0 ? 0 : setting(36,113)),"冻符「Perfect Freeze」",std::array{14,15,16,17}.at(index));p.thresholdDefeatsSpell=true;return p;}
        case Behavior::DiamondBlizzard: {if(index==0)throw std::invalid_argument("Diamond Blizzard is unavailable on Easy");auto p=SpellSkill(CombatSkill(ResolveBehavior(id),setting(36,113),setting(40,115)),"雪符「Diamond Blizzard」",std::array{0,18,19,20}.at(index));p.timeoutArmFrame=60;return p;}
        default:break;
    }
    throw std::invalid_argument("Cirno behavior is not a combat skill");
}
}

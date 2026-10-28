// 本文件定义 SakuyaBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/sakuya.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void SakuyaBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
float S(int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(5,sub,opcode));}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(5,0,frame,sub));}
float I(int stage,int sub,int index,int opcode) {
    const auto* r=native_combat::Instruction(stage,sub,index);
    if(!r||r->opcode!=opcode)throw std::logic_error("Invalid character skill provenance");
    return native_combat::RequireLiteral(r);
}
float Setting(int stage,int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(stage,sub,opcode));}
float SpawnAt(int stage,int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(stage,0,frame,sub));}
}
Attack SakuyaBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::MisdirectionNonspell:return Run(Behaviors::MisdirectionNonspell);
        case Behavior::Misdirection:return Run(Behaviors::Misdirection);
        case Behavior::MisdirectionRetreat:return Run(Behaviors::MisdirectionRetreat);
        case Behavior::MisdirectionDefeated:return Run(Behaviors::MisdirectionDefeated);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::ClockCorpse:return Run(Behaviors::ClockCorpse);
        case Behavior::SecondNonspell:return Run(Behaviors::SecondNonspell);
        case Behavior::LunaClock:return Run(Behaviors::LunaClock);
        case Behavior::ThirdNonspell:return Run(Behaviors::ThirdNonspell);
        case Behavior::Marionette:return Run(Behaviors::Marionette);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::SkillComplete:return Run(Behaviors::SkillComplete);
        case Behavior::EternalMeekEntrance:return Run(Behaviors::EternalMeekEntrance);
        case Behavior::EternalMeekNonspell:return Run(Behaviors::EternalMeekNonspell);
        case Behavior::EternalMeek:return Run(Behaviors::EternalMeek);
        case Behavior::EternalMeekRetreat:return Run(Behaviors::EternalMeekRetreat);
        case Behavior::EternalMeekDefeated:return Run(Behaviors::EternalMeekDefeated);
        default:throw std::invalid_argument("Unknown Sakuya behavior");
    }
}
SkillDefinition SakuyaBoss::ResolveSkill(BehaviorId id,[[maybe_unused]] ShotType shot) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Sakuya shot");
    switch(selected) {
        case Behavior::MisdirectionNonspell: {auto p=CombatSkill(ResolveBehavior(id),Spawn(3272,12),S(13,115),S(13,113));p.hitRadius=native_sizes::kBoss40Radius;return p;}
        case Behavior::Misdirection: {auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(13,113),S(19,115)),"奇术「Misdirection」",86);p.hitRadius=native_sizes::kBoss40Radius;return p;}
        case Behavior::FirstNonspell: {auto p=CombatSkill(ResolveBehavior(id),S(24,111),S(24,115),S(24,113));p.entranceHealth=1;return p;}
        case Behavior::ClockCorpse:return SpellSkill(CombatSkill(ResolveBehavior(id),S(24,113),S(47,115)),"幻在「Clock Corpse」",90);
        case Behavior::SecondNonspell:return CombatSkill(ResolveBehavior(id),S(36,111),S(36,115),S(36,113));
        case Behavior::LunaClock:return SpellSkill(CombatSkill(ResolveBehavior(id),S(36,113),S(53,115)),"幻象「Luna Clock」",94);
        case Behavior::ThirdNonspell:return CombatSkill(ResolveBehavior(id),S(43,111),S(43,115),S(43,113));
        case Behavior::Marionette:return SpellSkill(CombatSkill(ResolveBehavior(id),S(43,113),S(61,115)),"女仆秘技「提线玩偶」",98);
        case Behavior::EternalMeekNonspell:case Behavior::EternalMeek: {
            constexpr std::array<int,4> thresholds{19,24,29,34};const float hp=I(6,9,thresholds[index],113);
            const bool spell=selected==Behavior::EternalMeek;
            auto p=CombatSkill(ResolveBehavior(id),spell ? hp : SpawnAt(6,2493,8),Setting(6,spell ? 15 : 9,115),spell ? 0 : hp);
            p.hitRadius=native_sizes::kBoss40Radius;return spell ? SpellSkill(std::move(p),"奇术「Eternal Meek」",101) : p;
        }
        default:break;
    }
    throw std::invalid_argument("Sakuya behavior is not a combat skill");
}
}

// 本文件定义 MeilingBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/meiling.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void MeilingBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
float S(int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(3,sub,opcode));}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(3,0,frame,sub));}
}
Attack MeilingBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Midboss:return Run(Behaviors::Midboss);
        case Behavior::GorgeousSweetFlower:return Run(Behaviors::GorgeousSweetFlower);
        case Behavior::MidbossDefeated:return Run(Behaviors::MidbossDefeated);
        case Behavior::MidbossTimeout:return Run(Behaviors::MidbossTimeout);
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::RainbowWindChime:return Run(Behaviors::RainbowWindChime);
        case Behavior::SummoningNonspell:return Run(Behaviors::SummoningNonspell);
        case Behavior::RainNonspell:return Run(Behaviors::RainNonspell);
        case Behavior::ColorfulRain:return Run(Behaviors::ColorfulRain);
        case Behavior::DazzlingColorTyphoon:return Run(Behaviors::DazzlingColorTyphoon);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::End:return Run(Behaviors::End);
        default:throw std::invalid_argument("Unknown Meiling behavior");
    }
}
SkillDefinition MeilingBoss::ResolveSkill(BehaviorId id,[[maybe_unused]] ShotType shot) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    switch(selected) {
        case Behavior::Midboss:return CombatSkill(ResolveBehavior(id),Spawn(3474,9),S(9,115),S(9,113));
        case Behavior::GorgeousSweetFlower:return SpellSkill(CombatSkill(ResolveBehavior(id),S(9,113),S(14,115)),"华符「芳华绚烂」",22);
        case Behavior::FirstNonspell: {auto p=CombatSkill(ResolveBehavior(id),S(22,111),S(22,115),S(22,113));p.entranceHealth=1;return p;}
        case Behavior::RainbowWindChime:return SpellSkill(CombatSkill(ResolveBehavior(id),S(22,113),S(34,115)),"虹符「彩虹的风铃」",26);
        case Behavior::SummoningNonspell:return CombatSkill(ResolveBehavior(id),S(24,111),S(24,115));
        case Behavior::RainNonspell:return CombatSkill(ResolveBehavior(id),S(30,111),S(30,115),S(30,113));
        case Behavior::ColorfulRain: {auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(30,113),S(40,115),S(39,113)),"彩符「彩雨」",32);p.thresholdDefeatsSpell=true;return p;}
        case Behavior::DazzlingColorTyphoon:return SpellSkill(CombatSkill(ResolveBehavior(id),S(39,113),S(45,115)),"彩符「极彩台风」",35);
        default:break;
    }
    throw std::invalid_argument("Meiling behavior is not a combat skill");
}
}

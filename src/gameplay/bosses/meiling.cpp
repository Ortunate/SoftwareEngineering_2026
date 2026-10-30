// 本文件定义 MeilingBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/meiling.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void MeilingBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(3,0,frame,sub));}
}
Attack MeilingBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Midboss:return Run(Behaviors::Midboss);
        case Behavior::GorgeousSweetFlower:return Run(Behaviors::GorgeousSweetFlower);
        case Behavior::Selaginella:return Run(Behaviors::Selaginella);
        case Behavior::MidbossDefeated:return Run(Behaviors::MidbossDefeated);
        case Behavior::MidbossTimeout:return Run(Behaviors::MidbossTimeout);
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::RainbowWindChime:return Run(Behaviors::RainbowWindChime);
        case Behavior::SummoningNonspell:return Run(Behaviors::SummoningNonspell);
        case Behavior::FlowerDream:return Run(Behaviors::FlowerDream);
        case Behavior::RainNonspell:return Run(Behaviors::RainNonspell);
        case Behavior::ColorfulRain:return Run(Behaviors::ColorfulRain);
        case Behavior::ColorLightDance:return Run(Behaviors::ColorLightDance);
        case Behavior::DazzlingColorTyphoon:return Run(Behaviors::DazzlingColorTyphoon);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::End:return Run(Behaviors::End);
        default:throw std::invalid_argument("Unknown Meiling behavior");
    }
}
SkillDefinition MeilingBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition MeilingBoss::ResolveSkillForDifficulty(BehaviorId id,[[maybe_unused]] ShotType shot,native_combat::Difficulty difficulty) const {
    if(native_combat::DifficultyIndex(difficulty)<0)
        throw std::invalid_argument("Meiling difficulty not yet implemented");
    const bool easy=difficulty==native_combat::Difficulty::Easy;
    const auto S=[difficulty](int sub,int opcode) {
        return native_combat::RequireLiteral(native_combat::Setting(3,sub,opcode,0,difficulty));
    };
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    const bool hard=native_combat::DifficultyIndex(difficulty)>=2;
    const int index=native_combat::DifficultyIndex(difficulty);
    switch(selected) {
        case Behavior::Midboss:return CombatSkill(ResolveBehavior(id),Spawn(3474,9),S(9,115),S(9,113));
        case Behavior::GorgeousSweetFlower:
            if(hard)throw std::invalid_argument("Gorgeous Sweet Flower requires Easy or Normal");
            return SpellSkill(CombatSkill(ResolveBehavior(id),S(9,113),S(14,115)),"华符「芳华绚烂」",easy?21:22);
        case Behavior::Selaginella:
            if(!hard)throw std::invalid_argument("Selaginella requires Hard or Lunatic");
            return SpellSkill(CombatSkill(ResolveBehavior(id),S(9,113),S(17,115)),"华符「Selaginella 9」",difficulty==native_combat::Difficulty::Hard?23:24);
        case Behavior::FirstNonspell: {auto p=CombatSkill(ResolveBehavior(id),S(22,111),S(22,115),S(22,113));p.entranceHealth=1;return p;}
        case Behavior::RainbowWindChime:return SpellSkill(CombatSkill(ResolveBehavior(id),S(22,113),S(34,115)),"虹符「彩虹的风铃」",25+index);
        case Behavior::SummoningNonspell:return CombatSkill(ResolveBehavior(id),S(24,111),S(24,115),hard?S(24,113):0);
        case Behavior::FlowerDream: {
            if(!hard)throw std::invalid_argument("Flower Dream requires Hard or Lunatic");
            const auto timeout=native_combat::RequireLiteral(native_combat::Setting(3,37,115,1,difficulty));
            auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(24,113),timeout),"幻符「华想梦葛」",27+index,1);
            p.timeoutArmFrame=1;return p;
        }
        case Behavior::RainNonspell:return CombatSkill(ResolveBehavior(id),S(30,111),S(30,115),S(30,113));
        case Behavior::ColorfulRain: {if(hard)throw std::invalid_argument("Colorful Rain requires Easy or Normal");auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(30,113),S(40,115),easy?0:S(39,113)),"彩符「彩雨」",easy?31:32);p.thresholdDefeatsSpell=!easy;return p;}
        case Behavior::ColorLightDance: {
            if(!hard)throw std::invalid_argument("Color Light Dance requires Hard or Lunatic");
            auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(30,113),S(43,115),S(42,113)),"彩符「彩光乱舞」",31+index);
            p.thresholdDefeatsSpell=true;return p;
        }
        case Behavior::DazzlingColorTyphoon:
            if(easy)throw std::invalid_argument("Dazzling Color Typhoon is unavailable on Easy");
            return SpellSkill(CombatSkill(ResolveBehavior(id),S(hard?42:39,113),S(45,115)),"彩符「极彩台风」",34+index);
        default:break;
    }
    throw std::invalid_argument("Meiling behavior is not a combat skill");
}
}

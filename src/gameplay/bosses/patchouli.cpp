// 本文件定义 PatchouliBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/patchouli.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void PatchouliBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
float S(int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(4,sub,opcode));}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(4,0,frame,sub));}
float I(int stage,int sub,int index,int opcode) {
    const auto* r=native_combat::Instruction(stage,sub,index);
    if(!r||r->opcode!=opcode)throw std::logic_error("Invalid character skill provenance");
    return native_combat::RequireLiteral(r);
}
}
Attack PatchouliBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::SecondNonspell:return Run(Behaviors::SecondNonspell);
        case Behavior::AgniShine:return Run(Behaviors::AgniShine);
        case Behavior::AgniShineAdvanced:return Run(Behaviors::AgniShineAdvanced);
        case Behavior::PrincessUndine:return Run(Behaviors::PrincessUndine);
        case Behavior::SylphyHorn:return Run(Behaviors::SylphyHorn);
        case Behavior::SylphyHornAdvanced:return Run(Behaviors::SylphyHornAdvanced);
        case Behavior::LazyTrilithon:return Run(Behaviors::LazyTrilithon);
        case Behavior::LazyTrilithonAdvanced:return Run(Behaviors::LazyTrilithonAdvanced);
        case Behavior::MetalFatigue:return Run(Behaviors::MetalFatigue);
        case Behavior::LavaCromlech:return Run(Behaviors::LavaCromlech);
        case Behavior::ForestBlaze:return Run(Behaviors::ForestBlaze);
        case Behavior::WaterElf:return Run(Behaviors::WaterElf);
        case Behavior::MercuryPoison:return Run(Behaviors::MercuryPoison);
        case Behavior::EmeraldMegalith:return Run(Behaviors::EmeraldMegalith);
        case Behavior::CombinationEntrance:return Run(Behaviors::CombinationEntrance);
        case Behavior::SkillComplete:return Run(Behaviors::SkillComplete);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        default:throw std::invalid_argument("Unknown Patchouli behavior");
    }
}
SkillDefinition PatchouliBoss::ResolveSkill(BehaviorId id,[[maybe_unused]] ShotType shot) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Patchouli shot");
    constexpr std::array<int,4> firstHp{21,28,35,40},secondHp{49,56,63,68},firstTime{19,26,33,38},secondTime{47,54,61,66};
    if(selected==Behavior::FirstNonspell||selected==Behavior::SecondNonspell) {
        const bool first=selected==Behavior::FirstNonspell;const int sub=first ? 27 : 37;
        auto p=CombatSkill(ResolveBehavior(id),S(sub,111),I(4,sub,(first ? firstTime : secondTime)[index],115),I(4,sub,(first ? firstHp : secondHp)[index],113));
        p.entranceHealth=first ? 1 : 0;return p;
    }
    struct Spell {Behavior id;behavior::Entry entry;int preparation,card;const char* name;bool combination=false,last=false;};
    static const std::array<Spell,15> spells{{
        {Behavior::AgniShine,Behaviors::AgniShine,43,39,"火符「Agni Shine」"},
        {Behavior::AgniShineAdvanced,Behaviors::AgniShineAdvanced,46,40,"火符「Agni Shine上级」"},
        {Behavior::PrincessUndine,Behaviors::PrincessUndine,52,46,"水符「Princess Undine」"},
        {Behavior::SylphyHorn,Behaviors::SylphyHorn,58,50,"木符「Sylphy Horn」"},
        {Behavior::SylphyHornAdvanced,Behaviors::SylphyHornAdvanced,61,51,"木符「Sylphy Horn上级」"},
        {Behavior::LazyTrilithon,Behaviors::LazyTrilithon,67,57,"土符「Lazy Trilithon」"},
        {Behavior::LazyTrilithonAdvanced,Behaviors::LazyTrilithonAdvanced,70,58,"土符「Lazy Trilithon上级」"},
        {Behavior::MetalFatigue,Behaviors::MetalFatigue,76,63,"金符「Metal Fatigue」"},
        {Behavior::LavaCromlech,Behaviors::LavaCromlech,82,67,"火&土符「Lava Cromlech」",true},
        {Behavior::ForestBlaze,Behaviors::ForestBlaze,85,71,"木&火符「Forest Blaze」",true},
        {Behavior::WaterElf,Behaviors::WaterElf,88,75,"水&木符「Water Elf」",true},
        {Behavior::MercuryPoison,Behaviors::MercuryPoison,91,78,"金&水符「Mercury Poison」",true,true},
        {Behavior::EmeraldMegalith,Behaviors::EmeraldMegalith,94,82,"土&金符「Emerald Megalith」",true},
        {Behavior::WaterElfFinal,Behaviors::WaterElf,88,75,"水&木符「Water Elf」",true,true},
        {Behavior::EmeraldMegalithFinal,Behaviors::EmeraldMegalith,94,82,"土&金符「Emerald Megalith」",true,true},
    }};
    for(const auto& s:spells)if(s.id==selected) {
        float hp=0,reserve=0;const int duration=S(s.preparation,115);
        if(s.combination) {hp=s.last ? S(39,113) : S(39,111);reserve=s.last ? 0 : S(39,113);}
        if(!s.combination) {
            int sub=27,record=0;
            switch(selected) {
                case Behavior::AgniShine:record=21;break;
                case Behavior::PrincessUndine:record=28;break;
                case Behavior::SylphyHorn:record=35;break;
                case Behavior::LazyTrilithon:record=40;break;
                case Behavior::LazyTrilithonAdvanced:sub=37;record=49;break;
                case Behavior::SylphyHornAdvanced:sub=37;record=56;break;
                case Behavior::AgniShineAdvanced:sub=37;record=63;break;
                case Behavior::MetalFatigue:sub=37;record=68;break;
                default:throw std::logic_error("Missing Patchouli skill health");
            }
            hp=I(4,sub,record,113);
        }
        Attack behavior=Run(s.entry);
        if(s.combination)behavior=[entry=s.entry,duration](behavior::Actor& a,ConfiguredBoss&) {return Behaviors::Combination(a,entry,duration);};
        auto p=SpellSkill(CombatSkill(std::move(behavior),hp,duration+(s.combination ? 120 : 0),reserve),s.name,s.card,s.combination ? 120 : 0);
        p.thresholdDefeatsSpell=s.combination&&!s.last;return p;
    }
    throw std::invalid_argument("Patchouli behavior is not a combat skill");
}
}

// 本文件定义 PatchouliBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/patchouli.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void PatchouliBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
/// @brief index 只定位经过核对的共享原始指令；同时检查难度掩码，不能将 Normal 独占记录借给其他难度。
float I(int stage,int sub,int index,int opcode,native_combat::Difficulty difficulty=native_combat::Difficulty::Normal) {
    const auto* r=native_combat::Instruction(stage,sub,index);
    if(!r||r->opcode!=opcode||!(r->rankMask&static_cast<unsigned>(difficulty)))throw std::logic_error("Invalid character skill provenance");
    return native_combat::RequireLiteral(r);
}
}
Attack PatchouliBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::SilentSelene:return Run(Behaviors::SilentSelene);
        case Behavior::RoyalFlare:return Run(Behaviors::RoyalFlare);
        case Behavior::ExtraRetreat:return Run(Behaviors::ExtraRetreat);
        case Behavior::PhilosophersStone:return Run(Behaviors::PhilosophersStone);
        case Behavior::ExtraEntrance:return Run(Behaviors::ExtraEntrance);
        case Behavior::ExtraDefeated:return Run(Behaviors::ExtraDefeated);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::SecondNonspell:return Run(Behaviors::SecondNonspell);
        case Behavior::AgniShine:return Run(Behaviors::AgniShine);
        case Behavior::AgniShineAdvanced:return Run(Behaviors::AgniShineAdvanced);
        case Behavior::AgniRadiance:return Run(Behaviors::AgniRadiance);
        case Behavior::BuryInLake:return Run(Behaviors::BuryInLake);
        case Behavior::PrincessUndine:return Run(Behaviors::PrincessUndine);
        case Behavior::SylphyHorn:return Run(Behaviors::SylphyHorn);
        case Behavior::SylphyHornAdvanced:return Run(Behaviors::SylphyHornAdvanced);
        case Behavior::GreenStorm:return Run(Behaviors::GreenStorm);
        case Behavior::LazyTrilithon:return Run(Behaviors::LazyTrilithon);
        case Behavior::LazyTrilithonAdvanced:return Run(Behaviors::LazyTrilithonAdvanced);
        case Behavior::TrilithonShake:return Run(Behaviors::TrilithonShake);
        case Behavior::SilverDragon:return Run(Behaviors::SilverDragon);
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
SkillDefinition PatchouliBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition PatchouliBoss::ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const {
    const int d=native_combat::DifficultyIndex(difficulty);
    if(d<0)throw std::invalid_argument("Invalid Patchouli difficulty");
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Patchouli shot");
    if(selected==Behavior::SilentSelene||selected==Behavior::RoyalFlare||selected==Behavior::PhilosophersStone) {
        if(difficulty!=native_combat::Difficulty::Normal)
            throw std::invalid_argument("Extra Patchouli uses a single canonical difficulty context");
        const int phase=selected==Behavior::SilentSelene?0:selected==Behavior::RoyalFlare?1:2;
        constexpr std::array<const char*,3> names{"月符「Silent Selene」","日符「Royal Flare」","火水木金土符「贤者之石」"};
        auto skill=SpellSkill(CombatSkill(ResolveBehavior(id),
            native_combat::RequireLiteral(native_combat::Setting(7,17+phase,111,0,difficulty)),
            native_combat::RequireLiteral(native_combat::Setting(7,23+phase*3,115,0,difficulty))),names[phase],119+phase);
        if(phase==0)skill.entranceHealth=native_combat::RequireLiteral(native_combat::Spawn(7,0,4640,16));
        return skill;
    }
    if(selected==Behavior::FirstNonspell||selected==Behavior::SecondNonspell) {
        const bool first=selected==Behavior::FirstNonspell;const int sub=first ? 27 : 37;
        // 这些分支指令均为 0xff00 共享记录，按源难度/机体条件选择，独立执行器另验实际走到的设置。
        constexpr std::array<int,4> normalFirst{21,28,35,40},hardFirst{48,55,62,67};
        constexpr std::array<int,4> easySecond{22,29,36,41},normalSecond{49,56,63,68},hardSecond{76,83,90,95};
        const int threshold=first ? (d==0 ? -1 : (d==1?normalFirst:hardFirst)[index]) :
            (d==0?easySecond:d==1?normalSecond:hardSecond)[index];
        const int timeout=threshold<0?13:threshold-2;
        auto p=CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(4,sub,111,0,difficulty)),
            I(4,sub,timeout,115,difficulty),threshold<0?0:I(4,sub,threshold,113,difficulty));
        p.entranceHealth=first ? 1 : 0;return p;
    }
    if(selected==Behavior::AgniShine||selected==Behavior::AgniShineAdvanced||selected==Behavior::AgniRadiance) {
        int preparation=0,healthSub=0,healthIndex=0,card=0;const char* name=nullptr;
        if(selected==Behavior::AgniShine) {
            if(d>=2)throw std::invalid_argument("Agni Shine requires Easy or Normal");
            preparation=43;healthSub=d==0?37:27;healthIndex=d==0?22:21;card=38+d;name="火符「Agni Shine」";
        } else if(selected==Behavior::AgniShineAdvanced) {
            if(d==0)throw std::invalid_argument("Advanced Agni Shine is unavailable on Easy");
            preparation=46;healthSub=d==1?37:27;healthIndex=d==1?63:48;card=39+d;name="火符「Agni Shine上级」";
        } else {
            if(d<2)throw std::invalid_argument("Agni Radiance requires Hard or Lunatic");
            preparation=49;healthSub=37;healthIndex=90;card=41+d;name="火符「Agni Radiance」";
        }
        return SpellSkill(CombatSkill(ResolveBehavior(id),I(4,healthSub,healthIndex,113,difficulty),
            native_combat::RequireLiteral(native_combat::Setting(4,preparation,115,0,difficulty))),name,card);
    }
    if(selected==Behavior::SylphyHorn||selected==Behavior::SylphyHornAdvanced||selected==Behavior::GreenStorm) {
        int preparation=0,healthSub=0,healthIndex=0,card=0;const char* name=nullptr;
        if(selected==Behavior::SylphyHorn) {
            if(d>=2)throw std::invalid_argument("Sylphy Horn requires Easy or Normal");
            preparation=58;healthSub=d==0?37:27;healthIndex=d==0?36:35;card=49+d;name="木符「Sylphy Horn」";
        } else if(selected==Behavior::SylphyHornAdvanced) {
            if(d==0)throw std::invalid_argument("Advanced Sylphy Horn is unavailable on Easy");
            preparation=61;healthSub=d==1?37:27;healthIndex=d==1?56:62;card=50+d;name="木符「Sylphy Horn上级」";
        } else {
            if(d<2)throw std::invalid_argument("Green Storm requires Hard or Lunatic");
            preparation=64;healthSub=37;healthIndex=83;card=52+d;name="木符「Green Storm」";
        }
        return SpellSkill(CombatSkill(ResolveBehavior(id),I(4,healthSub,healthIndex,113,difficulty),
            native_combat::RequireLiteral(native_combat::Setting(4,preparation,115,0,difficulty))),name,card);
    }
    if(selected==Behavior::LazyTrilithon||selected==Behavior::LazyTrilithonAdvanced||selected==Behavior::TrilithonShake) {
        int preparation=0,healthSub=0,healthIndex=0,card=0;const char* name=nullptr;
        if(selected==Behavior::LazyTrilithon) {
            if(d>=2)throw std::invalid_argument("Lazy Trilithon requires Easy or Normal");
            preparation=67;healthSub=d==0?37:27;healthIndex=d==0?41:40;card=56+d;name="土符「Lazy Trilithon」";
        } else if(selected==Behavior::LazyTrilithonAdvanced) {
            if(d==0)throw std::invalid_argument("Advanced Lazy Trilithon is unavailable on Easy");
            preparation=70;healthSub=d==1?37:27;healthIndex=d==1?49:67;card=57+d;name="土符「Lazy Trilithon上级」";
        } else {
            if(d<2)throw std::invalid_argument("Trilithon Shake requires Hard or Lunatic");
            preparation=73;healthSub=37;healthIndex=76;card=59+d;name="土符「Trilithon Shake」";
        }
        return SpellSkill(CombatSkill(ResolveBehavior(id),I(4,healthSub,healthIndex,113,difficulty),
            native_combat::RequireLiteral(native_combat::Setting(4,preparation,115,0,difficulty))),name,card);
    }
    if(selected==Behavior::MetalFatigue||selected==Behavior::SilverDragon) {
        const bool silver=selected==Behavior::SilverDragon;
        if(silver?d<2:d!=1)throw std::invalid_argument("Metal spell difficulty is unavailable");
        const int preparation=silver?79:76,healthIndex=silver?95:68,card=silver?62+d:63;
        return SpellSkill(CombatSkill(ResolveBehavior(id),I(4,37,healthIndex,113,difficulty),
            native_combat::RequireLiteral(native_combat::Setting(4,preparation,115,0,difficulty))),
            silver?"金符「Silver Dragon」":"金符「Metal Fatigue」",card);
    }
    if(selected==Behavior::PrincessUndine||selected==Behavior::BuryInLake) {
        const bool lake=selected==Behavior::BuryInLake;
        if(lake?d<2:d>=2)throw std::invalid_argument("Water spell difficulty is unavailable");
        const int preparation=lake?55:52,healthSub=d==0?37:27,healthIndex=d==0?29:d==1?28:55;
        return SpellSkill(CombatSkill(ResolveBehavior(id),I(4,healthSub,healthIndex,113,difficulty),
            native_combat::RequireLiteral(native_combat::Setting(4,preparation,115,0,difficulty))),
            lake?"水符「Bury in Lake」":"水符「Princess Undine」",45+d);
    }
    enum class Position {First,Middle,Last};
    struct Combination {Behavior id;behavior::Entry entry;int preparation,cardBase;const char* name;Position position;};
    static const std::array<Combination,12> spells{{
        {Behavior::LavaCromlech,Behaviors::LavaCromlech,82,66,"火&土符「Lava Cromlech」",Position::First},
        {Behavior::ForestBlaze,Behaviors::ForestBlaze,85,70,"木&火符「Forest Blaze」",Position::First},
        {Behavior::WaterElf,Behaviors::WaterElf,88,74,"水&木符「Water Elf」",Position::First},
        {Behavior::MercuryPoison,Behaviors::MercuryPoison,91,77,"金&水符「Mercury Poison」",Position::Last},
        {Behavior::EmeraldMegalith,Behaviors::EmeraldMegalith,94,81,"土&金符「Emerald Megalith」",Position::First},
        {Behavior::WaterElfFinal,Behaviors::WaterElf,88,74,"水&木符「Water Elf」",Position::Last},
        {Behavior::EmeraldMegalithFinal,Behaviors::EmeraldMegalith,94,81,"土&金符「Emerald Megalith」",Position::Last},
        {Behavior::LavaCromlechFinal,Behaviors::LavaCromlech,82,66,"火&土符「Lava Cromlech」",Position::Last},
        {Behavior::ForestBlazeFinal,Behaviors::ForestBlaze,85,70,"木&火符「Forest Blaze」",Position::Last},
        {Behavior::MercuryPoisonMiddle,Behaviors::MercuryPoison,91,77,"金&水符「Mercury Poison」",Position::Middle},
        {Behavior::EmeraldMegalithMiddle,Behaviors::EmeraldMegalith,94,81,"土&金符「Emerald Megalith」",Position::Middle},
        {Behavior::WaterElfMiddle,Behaviors::WaterElf,88,74,"水&木符「Water Elf」",Position::Middle},
    }};
    for(const auto& s:spells)if(s.id==selected) {
        if((s.preparation==91&&d==0)||(s.position==Position::Middle&&d<2)||(s.position==Position::Last&&d==0))
            throw std::invalid_argument("Patchouli combination position is unavailable on this difficulty");
        // 首段由 Sub39 设置总生命；后继沿用上一段阈值，不能用同一保留偏移覆盖高难三段。
        const float total=native_combat::RequireLiteral(native_combat::Setting(4,39,111,0,difficulty));
        const float firstReserve=d==0?0:native_combat::RequireLiteral(native_combat::Setting(4,39,113,0,difficulty));
        const float middleReserve=d>=2?native_combat::RequireLiteral(native_combat::Setting(4,40,113,0,difficulty)):0;
        const float hp=s.position==Position::First?total:s.position==Position::Middle?firstReserve:d==1?firstReserve:middleReserve;
        const float reserve=s.position==Position::First?firstReserve:s.position==Position::Middle?middleReserve:0;
        const int duration=native_combat::RequireLiteral(native_combat::Setting(4,s.preparation,115,0,difficulty));
        Attack behavior=[entry=s.entry,duration,flash=s.position!=Position::First](behavior::Actor& a,ConfiguredBoss&) {
            return Behaviors::Combination(a,entry,duration,flash);
        };
        auto p=SpellSkill(CombatSkill(std::move(behavior),hp,duration+120,reserve),s.name,s.cardBase+d,120);
        p.thresholdDefeatsSpell=reserve>0;return p;
    }
    throw std::invalid_argument("Patchouli behavior is not a combat skill");
}
}

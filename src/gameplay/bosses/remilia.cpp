// 本文件维护蕾米莉亚的跨阶段充能记忆及配置启用的防 Bomb 化蝠反应。
#include "gameplay/bosses/remilia.hpp"
#include <algorithm>
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
// 派生状态的生命周期短于基类成员，先销毁可能借用 memory_ 的协程。
RemiliaBoss::~RemiliaBoss() {actor_.Reset({},1,1);}
void RemiliaBoss::StartBoss(const StartConfig& config) {
    ConfiguredBoss::StartBoss(config);memory_={};bombActive_=batTransformed_=false;batRecoveryFrames_=0;
}
void RemiliaBoss::AfterUpdate(float seconds) {
    const auto* p=CurrentSkill();if(!p||!p->reactToBomb||health<=0)return;
    if(bombActive_) {
        batRecoveryFrames_=60;
        if(!batTransformed_||actor_.poses[3]>=0) {batTransformed_=true;actor_.SetPoses({-1,-1,-1,-1,-1});actor_.Animate(165);}
        actor_.interactable=false;
    } else if(batTransformed_) {
        batRecoveryFrames_=std::max(0.0,batRecoveryFrames_-seconds*60);
        if(batRecoveryFrames_<=1e-5) {
            batTransformed_=false;actor_.interactable=true;actor_.SetPoses({160,163,164,161,162});actor_.Animate(160);
        }
    }
}
namespace {
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(6,0,frame,sub));}
}
Attack RemiliaBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::Prelude:return Run(Behaviors::Prelude);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::ScarletNetherworld:return Run(Behaviors::ScarletNetherworld);
        case Behavior::ThousandNeedles:return Run(Behaviors::ThousandNeedles);
        case Behavior::ThirdNonspell:return Run(Behaviors::ThirdNonspell);
        case Behavior::VladTepesCurse:return Run(Behaviors::VladTepesCurse);
        case Behavior::VampireIllusion:return Run(Behaviors::VampireIllusion);
        case Behavior::BatNonspell:return Run(Behaviors::BatNonspell);
        case Behavior::ScarletShoot:return Run(Behaviors::ScarletShoot);
        case Behavior::ScarletMeister:return Run(Behaviors::ScarletMeister);
        case Behavior::RedMagic:return Run(Behaviors::RedMagic);
        case Behavior::ScarletGensokyo:return Run(Behaviors::ScarletGensokyo);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::SkillComplete:return Run(Behaviors::SkillComplete);
        case Behavior::StarOfVanities:return [](behavior::Actor& actor,ConfiguredBoss& boss) {
            return Behaviors::StarOfVanities(actor,dynamic_cast<RemiliaBoss&>(boss).PhaseState());
        };
        case Behavior::YoungDemonLord:return [](behavior::Actor& actor,ConfiguredBoss& boss) {
            return Behaviors::YoungDemonLord(actor,dynamic_cast<RemiliaBoss&>(boss).PhaseState());
        };
        case Behavior::SecondNonspell:return [](behavior::Actor& actor,ConfiguredBoss& boss) {
            return Behaviors::SecondNonspell(actor,dynamic_cast<RemiliaBoss&>(boss).PhaseState());
        };
        default:throw std::invalid_argument("Unknown Remilia behavior");
    }
}
SkillDefinition RemiliaBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition RemiliaBoss::ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    const int d=native_combat::DifficultyIndex(difficulty);
    if(d<1||static_cast<unsigned>(shot)>3)throw std::invalid_argument("Invalid Remilia combat identity");
    if(selected==Behavior::YoungDemonLord) {
        if(d<2)throw std::invalid_argument("Young Demon Lord spell absent at selected difficulty");
        return SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(6,19,113,0,difficulty)),
            native_combat::RequireLiteral(native_combat::Setting(6,36,115,0,difficulty))),"神罚「幼小的恶魔领主」",103+d);
    }
    if(selected==Behavior::ThousandNeedles) {
        if(d<2)throw std::invalid_argument("Thousand Needles spell absent at selected difficulty");
        return SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(6,21,113,0,difficulty)),
            native_combat::RequireLiteral(native_combat::Setting(6,42,115,0,difficulty))),"狱符「千针山」",106+d);
    }
    if(selected==Behavior::ScarletMeister) {
        if(d<2)throw std::invalid_argument("Scarlet Meister spell absent at selected difficulty");
        return SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(6,28,113,0,difficulty)),
            native_combat::RequireLiteral(native_combat::Setting(6,56,115,0,difficulty))),"红符「Scarlet Meister」",112+d);
    }
    if(selected==Behavior::VampireIllusion) {
        if(d<2)throw std::invalid_argument("Vampire Illusion spell absent at selected difficulty");
        return SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(6,25,113,0,difficulty)),
            native_combat::RequireLiteral(native_combat::Setting(6,48,115,0,difficulty))),"神术「吸血鬼幻想」",109+d);
    }
    if(selected==Behavior::ScarletGensokyo) {
        if(d<2)throw std::invalid_argument("Scarlet Gensokyo spell absent at selected difficulty");
        auto p=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(6,64,111,0,difficulty)),
            native_combat::RequireLiteral(native_combat::Setting(6,65,115,0,difficulty))),"「红色幻想乡」",115+d);
        p.timeoutArmFrame=60;p.spellNeedsTimer=true;p.reactToBomb=true;return p;
    }
    if(d!=1&&selected!=Behavior::FirstNonspell&&selected!=Behavior::SecondNonspell&&selected!=Behavior::ThirdNonspell&&selected!=Behavior::BatNonspell)
        throw std::invalid_argument("Remilia spell absent at selected difficulty");
    const auto S=[&](int sub,int opcode){return native_combat::RequireLiteral(native_combat::Setting(6,sub,opcode,0,difficulty));};
    switch(selected) {
        case Behavior::FirstNonspell: {auto p=CombatSkill(ResolveBehavior(id),S(19,111),S(19,115),S(19,113));p.entranceHealth=Spawn(3098,17);return p;}
        case Behavior::StarOfVanities:return SpellSkill(CombatSkill(ResolveBehavior(id),S(19,113),S(33,115)),"天罚「Star of Vanities」",104);
        case Behavior::SecondNonspell:return CombatSkill(ResolveBehavior(id),S(21,111),S(21,115),S(21,113));
        case Behavior::ScarletNetherworld:return SpellSkill(CombatSkill(ResolveBehavior(id),S(21,113),S(39,115)),"冥符「红色冥界」",107);
        case Behavior::ThirdNonspell:return CombatSkill(ResolveBehavior(id),S(25,111),S(25,115),S(25,113));
        case Behavior::VladTepesCurse:return SpellSkill(CombatSkill(ResolveBehavior(id),S(25,113),S(45,115)),"咒诅「弗拉德·采佩什的诅咒」",110);
        case Behavior::BatNonspell:return CombatSkill(ResolveBehavior(id),S(28,111),S(28,115),S(28,113));
        case Behavior::ScarletShoot:return SpellSkill(CombatSkill(ResolveBehavior(id),S(28,113),S(53,115)),"红符「Scarlet Shoot」",113);
        case Behavior::RedMagic: {auto p=SpellSkill(CombatSkill(ResolveBehavior(id),S(61,111),S(62,115)),"「Red Magic」",116);p.timeoutArmFrame=60;p.spellNeedsTimer=true;p.reactToBomb=true;return p;}
        default:break;
    }
    throw std::invalid_argument("Remilia behavior is not a combat skill");
}
}

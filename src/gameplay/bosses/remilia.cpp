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
float S(int sub,int opcode) {return native_combat::RequireLiteral(native_combat::Setting(6,sub,opcode));}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(6,0,frame,sub));}
}
Attack RemiliaBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::Prelude:return Run(Behaviors::Prelude);
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::ScarletNetherworld:return Run(Behaviors::ScarletNetherworld);
        case Behavior::ThirdNonspell:return Run(Behaviors::ThirdNonspell);
        case Behavior::VladTepesCurse:return Run(Behaviors::VladTepesCurse);
        case Behavior::BatNonspell:return Run(Behaviors::BatNonspell);
        case Behavior::ScarletShoot:return Run(Behaviors::ScarletShoot);
        case Behavior::RedMagic:return Run(Behaviors::RedMagic);
        case Behavior::Defeated:return Run(Behaviors::Defeated);
        case Behavior::SkillComplete:return Run(Behaviors::SkillComplete);
        case Behavior::StarOfVanities:return [](behavior::Actor& actor,ConfiguredBoss& boss) {
            return Behaviors::StarOfVanities(actor,dynamic_cast<RemiliaBoss&>(boss).PhaseState());
        };
        case Behavior::SecondNonspell:return [](behavior::Actor& actor,ConfiguredBoss& boss) {
            return Behaviors::SecondNonspell(actor,dynamic_cast<RemiliaBoss&>(boss).PhaseState());
        };
        default:throw std::invalid_argument("Unknown Remilia behavior");
    }
}
SkillDefinition RemiliaBoss::ResolveSkill(BehaviorId id,[[maybe_unused]] ShotType shot) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
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

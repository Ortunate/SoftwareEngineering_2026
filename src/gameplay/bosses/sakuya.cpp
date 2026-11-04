// 本文件定义 SakuyaBoss 的技能血量、时限、符卡元数据和统一启动入口。
#include "gameplay/bosses/sakuya.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void SakuyaBoss::StartBoss(const StartConfig& config) {ConfiguredBoss::StartBoss(config);}
namespace {
float S(int sub,int opcode,native_combat::Difficulty difficulty=native_combat::Difficulty::Normal) {
    return native_combat::RequireLiteral(native_combat::Setting(5,sub,opcode,0,difficulty));
}
[[maybe_unused]] float Spawn(int frame,int sub) {return native_combat::RequireLiteral(native_combat::Spawn(5,0,frame,sub));}
float I(int stage,int sub,int index,int opcode) {
    const auto* r=native_combat::Instruction(stage,sub,index);
    if(!r||r->opcode!=opcode)throw std::logic_error("Invalid character skill provenance");
    return native_combat::RequireLiteral(r);
}
float Setting(int stage,int sub,int opcode,native_combat::Difficulty difficulty) {return native_combat::RequireLiteral(native_combat::Setting(stage,sub,opcode,0,difficulty));}
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
        case Behavior::JackTheLudoBile:return Run(Behaviors::JackTheLudoBile);
        case Behavior::SecondNonspell:return Run(Behaviors::SecondNonspell);
        case Behavior::LunaClock:return Run(Behaviors::LunaClock);
        case Behavior::TheWorld:return Run(Behaviors::TheWorld);
        case Behavior::ThirdNonspell:return Run(Behaviors::ThirdNonspell);
        case Behavior::Marionette:return Run(Behaviors::Marionette);
        case Behavior::KillingDoll:return Run(Behaviors::KillingDoll);
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
SkillDefinition SakuyaBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition SakuyaBoss::ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const {
    const int d=native_combat::DifficultyIndex(difficulty);if(d<0)throw std::invalid_argument("Invalid Sakuya difficulty");
    if(!id.Is<Behavior>())throw std::invalid_argument("Skill belongs to another Boss");
    const auto selected=static_cast<Behavior>(id.value);
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Sakuya shot");
    if(selected==Behavior::MisdirectionNonspell||selected==Behavior::Misdirection) {
        const bool spell=selected==Behavior::Misdirection;const float reserve=S(13,113,difficulty);
        // 四难度共用时间轴出生生命和710阈值；攻击时限仍从各难度准备表读取。
        auto p=CombatSkill(ResolveBehavior(id),spell?reserve:Spawn(3272,12),S(spell?19:13,115,difficulty),spell?0:reserve);
        p.hitRadius=native_sizes::kBoss40Radius;
        return spell?SpellSkill(std::move(p),d>=2?"奇术「幻惑Misdirection」":"奇术「Misdirection」",85+d):p;
    }
    if(selected==Behavior::FirstNonspell||selected==Behavior::SecondNonspell||selected==Behavior::ThirdNonspell) {
        const int sub=selected==Behavior::FirstNonspell?24:selected==Behavior::SecondNonspell?36:43;
        auto p=CombatSkill(ResolveBehavior(id),S(sub,111,difficulty),S(sub,115,difficulty),S(sub,113,difficulty));
        if(selected==Behavior::FirstNonspell)p.entranceHealth=1;
        return p;
    }
    if(selected==Behavior::ClockCorpse||selected==Behavior::JackTheLudoBile||selected==Behavior::LunaClock||
       selected==Behavior::TheWorld||selected==Behavior::Marionette||selected==Behavior::KillingDoll) {
        const bool high=selected==Behavior::JackTheLudoBile||selected==Behavior::TheWorld||selected==Behavior::KillingDoll;
        if(high!=(d>=2))throw std::invalid_argument("Sakuya spell absent at selected difficulty");
        const int group=selected==Behavior::ClockCorpse||selected==Behavior::JackTheLudoBile?0:
            selected==Behavior::LunaClock||selected==Behavior::TheWorld?1:2;
        const int nonspell=group==0?24:group==1?36:43,prepare=group==0?(high?50:47):group==1?(high?56:53):61;
        const char* name=group==0?(high?"幻幽「Jack the Ludo Bile」":"幻在「Clock Corpse」"):
            group==1?(high?"幻世「The World」":"幻象「Luna Clock」"):(high?"女仆秘技「杀人玩偶」":"女仆秘技「提线玩偶」");
        return SpellSkill(CombatSkill(ResolveBehavior(id),S(nonspell,113,difficulty),S(prepare,115,difficulty)),name,89+group*4+d);
    }
    switch(selected) {
        case Behavior::EternalMeekNonspell:case Behavior::EternalMeek: {
            constexpr std::array<int,4> thresholds{19,24,29,34};const float hp=I(6,9,thresholds[index],113);
            const bool spell=selected==Behavior::EternalMeek;
            if(spell&&d==0)throw std::invalid_argument("Eternal Meek spell absent at Easy difficulty");
            auto p=CombatSkill(ResolveBehavior(id),spell ? hp : SpawnAt(6,2493,8),Setting(6,spell ? 15 : 9,115,difficulty),spell ? 0 : hp);
            p.hitRadius=native_sizes::kBoss40Radius;return spell ? SpellSkill(std::move(p),"奇术「Eternal Meek」",100+d) : p;
        }
        default:break;
    }
    throw std::invalid_argument("Sakuya behavior is not a combat skill");
}
}

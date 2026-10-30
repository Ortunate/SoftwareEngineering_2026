// 本文件绑定芙兰朵露技能数值与入场生命，并维护原生10的防Bomb恢复状态；不定义关卡组合。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/bosses/skill_definition.hpp"
namespace koumakyou::bosses {
void FlandreBoss::StartBoss(const StartConfig& config) {
    ConfiguredBoss::StartBoss(config);bombActive_=batTransformed_=false;batRecoveryFrames_=0;
}
void FlandreBoss::AfterUpdate(float seconds) {
    const auto* skill=CurrentSkill();
    // 原生回调的安装点由角色技能定义提供；133在准备前安装17，其余已接入符在120帧准备后安装。
    if(!skill||!skill->reactToBomb||actor_.phaseFrames<=skill->bombReactionFrame||health<=0)return;
    if(bombActive_) {
        batRecoveryFrames_=60;
        if(!batTransformed_||actor_.poses[3]>=0) {batTransformed_=true;actor_.SetPoses({-1,-1,-1,-1,-1});actor_.Animate(165);}
        actor_.interactable=false;
    } else if(batRecoveryFrames_>0) {
        batRecoveryFrames_=std::max(0.0,batRecoveryFrames_-seconds*60);
        if(batRecoveryFrames_<=1e-5) {
            batTransformed_=false;actor_.interactable=true;
            const auto poses=skill->bombRecoveryPoses.value_or(std::array{160,163,164,161,162});
            actor_.SetPoses(poses);actor_.Animate(poses[0]);
        }
    }
}

Attack FlandreBoss::ResolveBehavior(BehaviorId id) const {
    if(!id.Is<Behavior>())throw std::invalid_argument("Behavior belongs to another Boss");
    switch(static_cast<Behavior>(id.value)) {
        case Behavior::CranberryTrap:return Run(Behaviors::CranberryTrap);
        case Behavior::Laevatein:return Run(Behaviors::Laevatein);
        case Behavior::FourOfAKind:return Run(Behaviors::FourOfAKind);
        case Behavior::KagomeKagome:return Run(Behaviors::KagomeKagome);
        case Behavior::MazeOfLove:return Run(Behaviors::MazeOfLove);
        case Behavior::StarbowBreak:return Run(Behaviors::StarbowBreak);
        case Behavior::Catadioptric:return Run(Behaviors::Catadioptric);
        case Behavior::ClockOfThePast:return Run(Behaviors::ClockOfThePast);
        case Behavior::SurvivalPrelude:return Run(Behaviors::SurvivalPrelude);
        case Behavior::AndThenWillThereBeNone:return Run(Behaviors::AndThenWillThereBeNone);
        case Behavior::FinalPrelude:return Run(Behaviors::FinalPrelude);
        case Behavior::RipplesOf495Years:return Run(Behaviors::RipplesOf495Years);
        case Behavior::FinalExit:return [](behavior::Actor& actor,ConfiguredBoss& boss) {return Behaviors::FinalExit(actor,!boss.RetirementFromTimeout());};
        case Behavior::Entrance:return Run(Behaviors::Entrance);
        case Behavior::EncoreEntrance:return Run(Behaviors::EncoreEntrance);
        case Behavior::FragileWing:return Run(Behaviors::FragileWing);
        case Behavior::CreepingBloodstainPrelude:return Run(Behaviors::CreepingBloodstainPrelude);
        case Behavior::CreepingBloodstain:return Run(Behaviors::CreepingBloodstain);
        case Behavior::DanmakuHeartPrelude:return Run(Behaviors::DanmakuHeartPrelude);
        case Behavior::DanmakuHeart:return Run(Behaviors::DanmakuHeart);
        case Behavior::EncoreFinalExit:return [](behavior::Actor& actor,ConfiguredBoss& boss) {return Behaviors::EncoreFinalExit(actor,!boss.RetirementFromTimeout());};
        case Behavior::FirstNonspell:return Run(Behaviors::FirstNonspell);
        case Behavior::SecondNonspell:return Run(Behaviors::SecondNonspell);
        case Behavior::ThirdNonspell:return Run(Behaviors::ThirdNonspell);
        case Behavior::FourthNonspell:return Run(Behaviors::FourthNonspell);
        case Behavior::FifthNonspell:return Run(Behaviors::FifthNonspell);
        case Behavior::SixthNonspell:return Run(Behaviors::SixthNonspell);
        case Behavior::SeventhNonspell:return Run(Behaviors::SeventhNonspell);
        case Behavior::EighthNonspell:return Run(Behaviors::EighthNonspell);
        default:throw std::invalid_argument("Unknown Flandre behavior");
    }
}
SkillDefinition FlandreBoss::ResolveSkill(BehaviorId id,ShotType shot) const {
    return ResolveSkillForDifficulty(id,shot,native_combat::Difficulty::Normal);
}
SkillDefinition FlandreBoss::ResolveSkillForDifficulty(BehaviorId id,ShotType shot,native_combat::Difficulty difficulty) const {
    if(difficulty!=native_combat::Difficulty::Normal||static_cast<unsigned>(shot)>3||!id.Is<Behavior>())
        throw std::invalid_argument("Invalid Extra Flandre combat identity");
    const auto selected=static_cast<Behavior>(id.value);
    if(selected==Behavior::CranberryTrap) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,38,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,40,115))),"禁忌「Cranberry Trap」",122);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::Laevatein) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,43,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,45,115))),"禁忌「Laevatein」",123);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::FourOfAKind) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,48,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,50,115))),"禁忌「Four of a Kind」",124);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::KagomeKagome) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,55,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,57,115))),"禁忌「笼中鸟」",125);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::MazeOfLove) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,61,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,63,115))),"禁忌「恋之迷宫」",126);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::StarbowBreak) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,65,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,67,115))),"禁弹「Starbow Break」",127);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::Catadioptric) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,71,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,73,115))),"禁弹「Catadioptric」",128);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::ClockOfThePast) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,76,113)),
            native_combat::RequireLiteral(native_combat::Setting(7,78,115))),"禁弹「铭刻往昔之钟」",129);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::AndThenWillThereBeNone) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,81,111)),
            native_combat::RequireLiteral(native_combat::Setting(7,83,115))),"秘弹「无人生还？」",130);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;result.survival=true;return result;
    }
    if(selected==Behavior::RipplesOf495Years) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,91,111)),
            native_combat::RequireLiteral(native_combat::Setting(7,93,115))),"QED「495年的波纹」",131);
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::FragileWing) {
        auto result=SpellSkill(CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,101,111)),
            native_combat::RequireLiteral(native_combat::Setting(7,101,115))),"咒弹「Fragile Wing」",132);
        result.entranceHealth=native_combat::RequireLiteral(native_combat::Spawn(7,0,8500,97));
        result.hitRadius=1.25F;result.reactToBomb=true;result.bombReactionFrame=120;return result;
    }
    if(selected==Behavior::CreepingBloodstain||selected==Behavior::CreepingBloodstainPrelude) {
        auto result=CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,106,111)),
            native_combat::RequireLiteral(native_combat::Setting(7,106,115)));
        // 106的113(-1)表示禁用阈值，不是负生命预留；107以133重置阶段时钟后沿用同一生命与时限。
        if(selected==Behavior::CreepingBloodstain) {
            result=SpellSkill(std::move(result),"咒弹「伏行之血痕」",133);result.reactToBomb=true;result.bombReactionFrame=0;
        }
        result.hitRadius=1.25F;return result;
    }
    if(selected==Behavior::DanmakuHeart||selected==Behavior::DanmakuHeartPrelude) {
        auto result=CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,112,111)),
            native_combat::RequireLiteral(native_combat::Setting(7,112,115)));
        if(selected==Behavior::DanmakuHeart) {
            result=SpellSkill(std::move(result),"「弹幕的心脏」",134);result.reactToBomb=true;result.bombReactionFrame=120;
            result.bombRecoveryPoses=std::array{176,166,166,161,162};
        }
        result.hitRadius=1.25F;return result;
    }
    // 原编号只用于数值目录溯源；实际控制流由命名行为和关卡阶段配置决定。
    int sub=0;
    switch(selected) {
        case Behavior::FirstNonspell:sub=38;break;
        case Behavior::SecondNonspell:sub=43;break;
        case Behavior::ThirdNonspell:sub=48;break;
        case Behavior::FourthNonspell:sub=55;break;
        case Behavior::FifthNonspell:sub=61;break;
        case Behavior::SixthNonspell:sub=65;break;
        case Behavior::SeventhNonspell:sub=71;break;
        case Behavior::EighthNonspell:sub=76;break;
        case Behavior::SurvivalPrelude:sub=81;break;
        case Behavior::FinalPrelude:sub=91;break;
        default:throw std::invalid_argument("Flandre behavior is not an implemented skill");
    }
    const float reserve=selected==Behavior::FinalPrelude?0:native_combat::RequireLiteral(native_combat::Setting(7,sub,113));
    auto result=CombatSkill(ResolveBehavior(id),native_combat::RequireLiteral(native_combat::Setting(7,sub,111)),
        native_combat::RequireLiteral(native_combat::Setting(7,sub,115)),reserve);
    result.hitRadius=1.25F;
    if(selected==Behavior::FirstNonspell)result.entranceHealth=native_combat::RequireLiteral(native_combat::Spawn(7,0,8494,37));
    return result;
}
}

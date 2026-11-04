// 本文件定义Extra道中、主战、条件后继及单符练习的组合、血条、入退场与奖励；技能生命由角色解析。
#include "stages/stage_extra/boss_config.hpp"
#include "gameplay/bosses/patchouli.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "stages/stage_extra/ids.hpp"
namespace koumakyou::stage_extra {
bosses::StartConfig PatchouliMidbossConfig(ShotType shot,native_combat::Difficulty difficulty) {
    if(static_cast<unsigned>(shot)>3||difficulty!=native_combat::Difficulty::Normal)
        throw std::invalid_argument("Extra Patchouli has a single canonical context");
    using P=bosses::PatchouliBoss;
    bosses::StartConfig config{.position={-10,0,4.64285707F},.hitRadius=1.25F,.seed=164640,
        .visual=kPatchouli,.defeatBase=stage_assets::kNativeDefeat,.configure=P::Behaviors::ConfigureExtraPractice,
        .shot=shot,.difficulty=difficulty,.intro=P::Behavior::ExtraEntrance,
        .defeated=P::Behavior::ExtraDefeated,.retreat=P::Behavior::ExtraDefeated,
        .phases={
            {{P::Behavior::SilentSelene},{{{0,1,{239,138,152,255}}}}},
            {{P::Behavior::RoyalFlare},{{{0,1,{239,138,152,255}}}}},
            {{P::Behavior::PhilosophersStone},{{{0,1,{239,138,152,255}}}}},
        },.initialDamageable=false,.initialContact=false,.defeatScore=200000,.retreatScore=200000};
    return config;
}
namespace {
using namespace bosses;
using F=FlandreBoss::Behavior;
constexpr Color kPink{239,138,152,255};
StartConfig FlandreBase(ShotType shot,native_combat::Difficulty difficulty) {
    if(static_cast<unsigned>(shot)>3||difficulty!=native_combat::Difficulty::Normal)
        throw std::invalid_argument("Extra Flandre has a single canonical context");
    return {.position={-10,0,5.71428585F},.hitRadius=1.25F,.seed=8494,
        .visual=kFlandre,.defeatBase=stage_assets::kNativeDefeat,.configure=FlandreBoss::Behaviors::ConfigurePractice,
        .shot=shot,.difficulty=difficulty,.intro=F::Entrance,.defeated=F::FinalExit,.retreat=F::FinalExit,
        .initialDamageable=false,.initialContact=false,.defeatScore=200000,.retreatScore=200000};
}
// 准备仍是一项真实技能使用，只显式省略HUD段；不能给零有效HP或重复生命的包装分配占位血条。
SkillSlot Preparation(F id) {SkillSlot slot{id};slot.omitHealthBarSegment=true;return slot;}
}
bosses::StartConfig FlandreMainConfig(ShotType shot,native_combat::Difficulty difficulty) {
    using namespace bosses;auto c=FlandreBase(shot,difficulty);
    c.phases={
        {{F::FirstNonspell,F::CranberryTrap},{{{1,.18F,kPink},{0,.82F,WHITE}}}},
        {{F::SecondNonspell,F::Laevatein},{{{1,.15F,kPink},{0,.85F,WHITE}}}},
        {{F::ThirdNonspell,F::FourOfAKind},{{{1,.2375F,kPink},{0,.7625F,WHITE}}}},
        {{F::FourthNonspell,F::KagomeKagome},{{{1,.3F,kPink},{0,.7F,WHITE}}}},
        {{F::FifthNonspell,F::MazeOfLove},{{{1,.14F,kPink},{0,.86F,WHITE}}}},
        {{F::SixthNonspell,F::StarbowBreak},{{{1,.22F,kPink},{0,.78F,WHITE}}}},
        {{F::SeventhNonspell,F::Catadioptric},{{{1,.152F,kPink},{0,.848F,WHITE}}}},
        {{F::EighthNonspell,F::ClockOfThePast},{{{1,.18F,kPink},{0,.82F,WHITE}}}},
        {{Preparation(F::SurvivalPrelude),F::AndThenWillThereBeNone},{{{1,1,kPink}}}},
        {{Preparation(F::FinalPrelude),F::RipplesOf495Years},{{{1,1,kPink}}}},
    };
    // 38…81的108直接指向下一包装；114/116则进入本阶段符卡，不能把归零和阈值混成同一路。
    c.phases[0].skills[0].onDefeat=Transition::Jump(1,0);
    c.phases[1].skills[0].onDefeat=Transition::Jump(2,0);
    c.phases[2].skills[0].onDefeat=Transition::Jump(3,0);
    c.phases[3].skills[0].onDefeat=Transition::Jump(4,0);
    c.phases[4].skills[0].onDefeat=Transition::Jump(5,0);
    c.phases[5].skills[0].onDefeat=Transition::Jump(6,0);
    c.phases[6].skills[0].onDefeat=Transition::Jump(7,0);
    c.phases[7].skills[0].onDefeat=Transition::Jump(8,0);
    c.phases[8].skills[0].onDefeat=Transition::Jump(9,0);
    c.phases[9].skills[0].onDefeat=Transition::Exit();return c;
}
bosses::StartConfig FlandreEncoreConfig(ShotType shot,native_combat::Difficulty difficulty) {
    auto c=FlandreBase(shot,difficulty);c.seed=8500;c.intro=F::EncoreEntrance;c.defeated=c.retreat=F::EncoreFinalExit;
    c.phases={
        {{F::FragileWing},{{{0,1,kPink}}}},
        {{Preparation(F::CreepingBloodstainPrelude),F::CreepingBloodstain},{{{1,1,kPink}}}},
        {{Preparation(F::DanmakuHeartPrelude),F::DanmakuHeart},{{{1,1,kPink}}}},
    };
    c.phases[1].skills[0].onDefeat=bosses::Transition::Jump(2,0);
    c.phases[2].skills[0].onDefeat=bosses::Transition::Exit();return c;
}
namespace {
std::unique_ptr<Boss> CreateMidboss(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<bosses::PatchouliBoss>();boss->StartBoss(PatchouliMidbossConfig(shot,difficulty));return boss;
}
}
const bosses::Battle kPatchouliMidboss{CreateMidboss};
namespace {
std::unique_ptr<Boss> CreateMain(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<bosses::FlandreBoss>();boss->StartBoss(FlandreMainConfig(shot,difficulty));return boss;
}
std::unique_ptr<Boss> CreateEncore(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<bosses::FlandreBoss>();boss->StartBoss(FlandreEncoreConfig(shot,difficulty));return boss;
}
// 隔离练习跳过整场入场与对白，但保留角色技能自身准备；本体初始位置使用世界坐标。
template<PatchouliBoss::Behavior Skill>
std::unique_ptr<Boss> CreatePatchouliSpell(ShotType shot,native_combat::Difficulty difficulty) {
    auto c=PatchouliMidbossConfig(shot,difficulty);c.position={0,0,6.42857143F};c.intro={};c.initialContact=true;
    c.phases={{{Skill},{{{0,1,kPink}}}}};
    auto boss=std::make_unique<PatchouliBoss>();boss->StartBoss(c);return boss;
}
void ConfigureFlandreSpell(behavior::Actor& actor,bool spatial) {
    FlandreBoss::Behaviors::ConfigurePractice(actor,spatial);
    // 整场由Entrance创建光圈，单符入口也必须绑定ExtraEnemies，不能从Flandre本体图集借用脚本18。
    actor.AnimateAuxiliary(0,18,stage_assets::kOrb);
}
template<F Skill>
std::unique_ptr<Boss> CreateFlandreSpell(ShotType shot,native_combat::Difficulty difficulty) {
    auto c=FlandreBase(shot,difficulty);c.position={0,0,5.71428585F};c.intro={};c.initialContact=true;
    c.configure=ConfigureFlandreSpell;c.phases={{{Skill},{{{0,1,kPink}}}}};
    auto boss=std::make_unique<FlandreBoss>();boss->StartBoss(c);return boss;
}
template<PatchouliBoss::Behavior Skill> const Battle kPatchouliSpell{CreatePatchouliSpell<Skill>};
template<F Skill> const Battle kFlandreSpell{CreateFlandreSpell<Skill>};
}
const bosses::Battle kFlandreMain{CreateMain},kFlandreEncore{CreateEncore};
const bosses::Battle& ExtraSpellBattle(int spellCard,native_combat::Difficulty difficulty) {
    if(difficulty!=native_combat::Difficulty::Normal)throw std::invalid_argument("Extra spell requires canonical Normal context");
    using P=PatchouliBoss::Behavior;
    switch(spellCard) {
        case 119:return kPatchouliSpell<P::SilentSelene>;
        case 120:return kPatchouliSpell<P::RoyalFlare>;
        case 121:return kPatchouliSpell<P::PhilosophersStone>;
        case 122:return kFlandreSpell<F::CranberryTrap>;
        case 123:return kFlandreSpell<F::Laevatein>;
        case 124:return kFlandreSpell<F::FourOfAKind>;
        case 125:return kFlandreSpell<F::KagomeKagome>;
        case 126:return kFlandreSpell<F::MazeOfLove>;
        case 127:return kFlandreSpell<F::StarbowBreak>;
        case 128:return kFlandreSpell<F::Catadioptric>;
        case 129:return kFlandreSpell<F::ClockOfThePast>;
        case 130:return kFlandreSpell<F::AndThenWillThereBeNone>;
        case 131:return kFlandreSpell<F::RipplesOf495Years>;
    }
    throw std::invalid_argument("Extra spell is outside the practice catalog");
}
}

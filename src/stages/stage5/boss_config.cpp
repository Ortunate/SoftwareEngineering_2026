// 本文件定义第 5 面阶段的技能组合、血条比例/颜色/排列与入退场参数；技能血量由角色内部定义。
#include "stages/stage5/boss_config.hpp"
#include "stages/stage5/behaviors.hpp"
#include "stages/stage5/ids.hpp"
#include "gameplay/bosses/sakuya.hpp"
namespace koumakyou::stage5 {
namespace {
using namespace bosses;
constexpr Color kSpellPink{239,138,152,255};
StartConfig Base(Vector3 position,std::uint32_t seed,VisualId visual,float radius,ShotType shot) {
    StartConfig c;c.position=position;c.seed=seed;c.visual=visual;c.hitRadius=radius;c.shot=shot;
    c.defeatBase=kNativeDefeat;c.configure=ConfigureBehavior;return c;
}
} // namespace
using namespace bosses;
StartConfig SakuyaMidbossConfig(ShotType shot) {
    return SakuyaMidbossConfig(shot,native_combat::Difficulty::Normal);
}
StartConfig SakuyaMidbossConfig(ShotType shot,native_combat::Difficulty difficulty) {
    if(native_combat::DifficultyIndex(difficulty)<0)throw std::invalid_argument("Invalid Sakuya midboss difficulty");
    using S=SakuyaBoss::Behavior;
    auto c=Base({-10,0,4.28571415F},3272,kSakuya,native_sizes::kBoss56Radius,shot);c.intro=S::Entrance;c.controlsWorldTime=true;c.difficulty=difficulty;
    c.phases={{{S::MisdirectionNonspell,S::Misdirection},{{{1,71.0F/1500,kSpellPink},{0,1429.0F/1500,WHITE}}}}};
    c.phases[0].skills[0].onTimeout=Transition::Exit();
    c.defeated=S::MisdirectionDefeated;c.retreat=S::MisdirectionRetreat;c.defeatScore=100000;return c;
}
StartConfig SakuyaFinalConfig(ShotType shot) {
    return SakuyaFinalConfig(shot,native_combat::Difficulty::Normal);
}
StartConfig SakuyaFinalConfig(ShotType shot,native_combat::Difficulty difficulty) {
    const int d=native_combat::DifficultyIndex(difficulty);if(d<0)throw std::invalid_argument("Invalid Sakuya final difficulty");
    using S=SakuyaBoss::Behavior;
    auto c=Base({-10,0,4.28571415F},7604,kSakuya,native_sizes::kBoss56Radius,shot);c.intro=S::Entrance;c.controlsWorldTime=true;c.difficulty=difficulty;
    c.phases={
        {{S::FirstNonspell,d>=2?S::JackTheLudoBile:S::ClockCorpse},{{{1,14.0F/190,kSpellPink},{0,176.0F/190,WHITE}}}},
        {{S::SecondNonspell,d>=2?S::TheWorld:S::LunaClock},{{{1,14.0F/170,kSpellPink},{0,156.0F/170,WHITE}}}},
        {{S::ThirdNonspell,d>=2?S::KillingDoll:S::Marionette},{{{1,17.0F/160,kSpellPink},{0,143.0F/160,WHITE}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
namespace {
std::unique_ptr<Boss> CreateMidboss(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<SakuyaBoss>();boss->StartBoss(SakuyaMidbossConfig(shot,difficulty));return boss;
}
std::unique_ptr<Boss> CreateFinal(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<SakuyaBoss>();boss->StartBoss(SakuyaFinalConfig(shot,difficulty));return boss;
}
/// @brief 单技能工厂显式恢复前置非符留下的世界边界与姿态；角色继续独立解析原始生命和符卡准备时钟。
template<SakuyaBoss::Behavior Skill>
std::unique_ptr<Boss> CreateSpell(ShotType shot,native_combat::Difficulty difficulty) {
    using S=SakuyaBoss;constexpr bool mid=Skill==S::Behavior::Misdirection;
    auto c=Base({0,0,4.28571415F},mid?3272:7604,kSakuya,mid?native_sizes::kBoss40Radius:native_sizes::kBoss56Radius,shot);
    c.difficulty=difficulty;c.controlsWorldTime=true;c.phases={{{Skill},{{{0,1,kSpellPink}}}}};
    c.configure=mid?S::Behaviors::ConfigureMisdirectionPractice:S::Behaviors::ConfigureFinalPractice;
    c.defeated=mid?S::Behavior::MisdirectionDefeated:S::Behavior::Defeated;c.retreat=mid?S::Behavior::MisdirectionRetreat:S::Behavior::Defeated;
    c.defeatScore=c.retreatScore=mid?100000:200000;auto boss=std::make_unique<S>();boss->StartBoss(c);return boss;
}
template<SakuyaBoss::Behavior Skill> const Battle kSpellBattle{CreateSpell<Skill>};
}
const Battle& SakuyaSpellBattle(int spellCard,native_combat::Difficulty difficulty) {
    const int d=native_combat::DifficultyIndex(difficulty);
    if(d<0||spellCard<85||spellCard>100||(spellCard-85)%4!=d)throw std::invalid_argument("Stage 5 spell does not match difficulty");
    using S=SakuyaBoss::Behavior;
    switch((spellCard-85)/4) {
        case 0:return kSpellBattle<S::Misdirection>;
        case 1:return d>=2?kSpellBattle<S::JackTheLudoBile>:kSpellBattle<S::ClockCorpse>;
        case 2:return d>=2?kSpellBattle<S::TheWorld>:kSpellBattle<S::LunaClock>;
        case 3:return d>=2?kSpellBattle<S::KillingDoll>:kSpellBattle<S::Marionette>;
    }
    throw std::logic_error("Unreachable Stage 5 spell group");
}
const Battle kSakuyaMidboss{CreateMidboss};
const Battle kSakuyaFinal{CreateFinal};
}

// 本文件定义第 6 面阶段的技能组合、血条比例/颜色/排列与入退场参数；技能血量由角色内部定义。
#include "stages/stage6/boss_config.hpp"
#include "stages/stage6/behaviors.hpp"
#include "stages/stage6/ids.hpp"
#include "gameplay/bosses/sakuya.hpp"
#include "gameplay/bosses/remilia.hpp"
namespace koumakyou::stage6 {
namespace {
using namespace bosses;
constexpr Color kSpellPink{239,138,152,255};
StartConfig Base(Vector3 position,std::uint32_t seed,VisualId visual,float radius,ShotType shot) {
    StartConfig c;c.position=position;c.seed=seed;c.visual=visual;c.hitRadius=radius;c.shot=shot;
    c.defeatBase=kNativeDefeat;c.configure=ConfigureBehavior;return c;
}
} // namespace
using namespace bosses;
StartConfig SakuyaMidbossConfig(ShotType shot,native_combat::Difficulty difficulty) {
    using S=SakuyaBoss::Behavior;
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Sakuya shot");
    if(native_combat::DifficultyIndex(difficulty)<1)throw std::invalid_argument("Stage 6 midboss requires Normal, Hard or Lunatic");
    constexpr std::array<float,4> ratio{0.125F,1.0F/6,11.0F/60,11.0F/60};
    auto c=Base({-10,0,4.28571415F},2493,kSakuya,native_sizes::kBoss56Radius,shot);c.difficulty=difficulty;c.intro=S::EternalMeekEntrance;c.initialContact=false;
    c.phases={{{S::EternalMeekNonspell,S::EternalMeek},{{{1,ratio[index],kSpellPink},{0,1-ratio[index],WHITE}}}}};
    c.phases[0].skills[0].onTimeout=Transition::Exit();
    c.defeated=S::EternalMeekDefeated;c.retreat=S::EternalMeekRetreat;c.defeatScore=100000;return c;
}
StartConfig RemiliaFinalConfig(ShotType shot,native_combat::Difficulty difficulty) {
    using S=RemiliaBoss::Behavior;
    if(static_cast<unsigned>(shot)>3||native_combat::DifficultyIndex(difficulty)<1)
        throw std::invalid_argument("Stage 6 final requires valid shot and Normal, Hard or Lunatic");
    const bool high=difficulty!=native_combat::Difficulty::Normal;
    auto c=Base({2.85714293F,0,12.8571424F},3098,kRemilia,native_sizes::kBoss56Radius,shot);
    c.difficulty=difficulty;
    c.intro=S::Entrance;c.resumeSteps={S::Prelude};c.initialContact=false;
    c.phases={
        {{S::FirstNonspell,high?S::YoungDemonLord:S::StarOfVanities},{{{1,12.0F/130,kSpellPink},{0,118.0F/130,WHITE}}}},
        {{S::SecondNonspell,high?S::ThousandNeedles:S::ScarletNetherworld},{{{1,16.0F/155,kSpellPink},{0,139.0F/155,WHITE}}}},
        {{S::ThirdNonspell,high?S::VampireIllusion:S::VladTepesCurse},{{{1,16.0F/110,kSpellPink},{0,94.0F/110,WHITE}}}},
        {{S::BatNonspell,high?S::ScarletMeister:S::ScarletShoot},{{{1,0.1F,kSpellPink},{0,0.9F,WHITE}}}},
        {{high?S::ScarletGensokyo:S::RedMagic},{{{0,1,kSpellPink}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
namespace {
std::unique_ptr<Boss> CreateMidboss(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<SakuyaBoss>();boss->StartBoss(SakuyaMidbossConfig(shot,difficulty));return boss;
}
std::unique_ptr<Boss> CreateFinal(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<RemiliaBoss>();boss->StartBoss(RemiliaFinalConfig(shot,difficulty));return boss;
}
/// @brief 隔离目标符卡并恢复前置技能的不变配置；准备、宣告、Bomb反应及退场继续由角色执行。
template<RemiliaBoss::Behavior Skill>
std::unique_ptr<Boss> CreateRemiliaSpell(ShotType shot,native_combat::Difficulty difficulty) {
    auto c=RemiliaFinalConfig(shot,difficulty);c.position={0,0,5.71428585F};c.intro={};c.resumeSteps.clear();c.initialContact=true;
    c.phases={{{Skill},{{{0,1,kSpellPink}}}}};c.configure=RemiliaBoss::Behaviors::ConfigurePractice;
    auto boss=std::make_unique<RemiliaBoss>();boss->StartBoss(c);return boss;
}
template<RemiliaBoss::Behavior Skill> const Battle kRemiliaSpell{CreateRemiliaSpell<Skill>};
std::unique_ptr<Boss> CreateMeekSpell(ShotType shot,native_combat::Difficulty difficulty) {
    auto c=SakuyaMidbossConfig(shot,difficulty);c.position={0,0,4.28571415F};c.hitRadius=native_sizes::kBoss40Radius;c.intro={};c.initialContact=true;
    c.phases={{{SakuyaBoss::Behavior::EternalMeek},{{{0,1,kSpellPink}}}}};c.configure=SakuyaBoss::Behaviors::ConfigureEternalMeekPractice;
    auto boss=std::make_unique<SakuyaBoss>();boss->StartBoss(c);return boss;
}
const Battle kMeekSpell{CreateMeekSpell};
}
const Battle& Stage6SpellBattle(int spellCard,native_combat::Difficulty difficulty) {
    const int d=native_combat::DifficultyIndex(difficulty);
    if(d<1||spellCard<101||spellCard>118||(spellCard-101)%3!=d-1)
        throw std::invalid_argument("Stage 6 spell does not match difficulty");
    using S=RemiliaBoss::Behavior;
    switch((spellCard-101)/3) {
        case 0:return kMeekSpell;
        case 1:return d>=2?kRemiliaSpell<S::YoungDemonLord>:kRemiliaSpell<S::StarOfVanities>;
        case 2:return d>=2?kRemiliaSpell<S::ThousandNeedles>:kRemiliaSpell<S::ScarletNetherworld>;
        case 3:return d>=2?kRemiliaSpell<S::VampireIllusion>:kRemiliaSpell<S::VladTepesCurse>;
        case 4:return d>=2?kRemiliaSpell<S::ScarletMeister>:kRemiliaSpell<S::ScarletShoot>;
        case 5:return d>=2?kRemiliaSpell<S::ScarletGensokyo>:kRemiliaSpell<S::RedMagic>;
    }
    throw std::logic_error("Unreachable Stage 6 spell group");
}
const Battle kSakuyaMidboss{CreateMidboss};
const Battle kRemiliaFinal{CreateFinal};
}

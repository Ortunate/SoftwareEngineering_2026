// 本文件定义第 3 面阶段的技能组合、血条比例/颜色/排列与入退场参数；技能血量由角色内部定义。
#include "stages/stage3/boss_config.hpp"
#include "stages/stage3/behaviors.hpp"
#include "stages/stage3/ids.hpp"
#include "gameplay/bosses/meiling.hpp"
namespace koumakyou::stage3 {
namespace {
using namespace bosses;
constexpr Color kSpellPink{239,138,152,255};
StartConfig Base(Vector3 position,std::uint32_t seed,VisualId visual,float radius,ShotType shot) {
    StartConfig c;c.position=position;c.seed=seed;c.visual=visual;c.hitRadius=radius;c.shot=shot;
    c.defeatBase=kNativeDefeat;c.configure=ConfigureBehavior;return c;
}
} // namespace
using namespace bosses;
StartConfig MeilingMidbossConfig(ShotType shot) {return MeilingMidbossConfig(shot,native_combat::Difficulty::Normal);}
StartConfig MeilingMidbossConfig(ShotType shot,native_combat::Difficulty difficulty) {
    if(native_combat::DifficultyIndex(difficulty)<0)throw std::invalid_argument("Invalid Meiling difficulty");
    using S=MeilingBoss::Behavior;
    auto c=Base({7.14285707F,0,14.2857141F},3474,kMeiling,native_sizes::kBoss56Radius,shot);
    c.phases={{{S::Midboss,native_combat::DifficultyIndex(difficulty)>=2?S::Selaginella:S::GorgeousSweetFlower},{{{1,0.1F,kSpellPink},{0,0.9F,WHITE}}}}};
    c.phases[0].skills[0].onTimeout=Transition::Exit();
    c.difficulty=difficulty;c.defeated=S::MidbossDefeated;c.retreat=S::MidbossTimeout;c.defeatScore=100000;return c;
}
StartConfig MeilingFinalConfig(ShotType shot) {return MeilingFinalConfig(shot,native_combat::Difficulty::Normal);}
StartConfig MeilingFinalConfig(ShotType shot,native_combat::Difficulty difficulty) {
    if(native_combat::DifficultyIndex(difficulty)<0)throw std::invalid_argument("Invalid Meiling difficulty");
    using S=MeilingBoss::Behavior;
    auto c=Base({7.14285707F,0,14.2857141F},6254,kMeiling,native_sizes::kBoss56Radius,shot);c.intro=S::Entrance;
    c.phases={
        {{S::FirstNonspell,S::RainbowWindChime},{{{1,19.0F/140,kSpellPink},{0,121.0F/140,WHITE}}}},
        {{S::SummoningNonspell},{{{0,1,WHITE}}}},
        {{S::RainNonspell,S::ColorfulRain,S::DazzlingColorTyphoon},
            {{{2,20.0F/165,kSpellPink},{1,14.0F/165,kSpellPink},{0,131.0F/165,WHITE}}}},
    };
    if(difficulty==native_combat::Difficulty::Easy)
        c.phases[2]={{S::RainNonspell,S::ColorfulRain},{{{1,14.0F/165,kSpellPink},{0,151.0F/165,WHITE}}}};
    if(native_combat::DifficultyIndex(difficulty)>=2) {
        c.phases[1]={{S::SummoningNonspell,S::FlowerDream},{{{1,14.0F/120,kSpellPink},{0,106.0F/120,WHITE}}}};
        c.phases[2].skills[1]=S::ColorLightDance;
    }
    c.difficulty=difficulty;
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
namespace {
std::unique_ptr<Boss> CreateMidboss(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<MeilingBoss>();boss->StartBoss(MeilingMidbossConfig(shot,difficulty));return boss;
}
std::unique_ptr<Boss> CreateFinal(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<MeilingBoss>();boss->StartBoss(MeilingFinalConfig(shot,difficulty));return boss;
}
}
const Battle kMeilingMidboss{CreateMidboss};
const Battle kMeilingFinal{CreateFinal};
}

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
StartConfig MeilingMidbossConfig(ShotType shot) {
    using S=MeilingBoss::Behavior;
    auto c=Base({7.14285707F,0,14.2857141F},3474,kMeiling,native_sizes::kBoss56Radius,shot);
    c.phases={{{S::Midboss,S::GorgeousSweetFlower},{{{1,0.1F,kSpellPink},{0,0.9F,WHITE}}}}};
    c.phases[0].skills[0].onTimeout=Transition::Exit();
    c.defeated=S::MidbossDefeated;c.retreat=S::MidbossTimeout;c.defeatScore=100000;return c;
}
StartConfig MeilingFinalConfig(ShotType shot) {
    using S=MeilingBoss::Behavior;
    auto c=Base({7.14285707F,0,14.2857141F},6254,kMeiling,native_sizes::kBoss56Radius,shot);c.intro=S::Entrance;
    c.phases={
        {{S::FirstNonspell,S::RainbowWindChime},{{{1,19.0F/140,kSpellPink},{0,121.0F/140,WHITE}}}},
        {{S::SummoningNonspell},{{{0,1,WHITE}}}},
        {{S::RainNonspell,S::ColorfulRain,S::DazzlingColorTyphoon},
            {{{2,20.0F/165,kSpellPink},{1,14.0F/165,kSpellPink},{0,131.0F/165,WHITE}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
const Battle kMeilingMidboss{Create<MeilingBoss,MeilingMidbossConfig>};
const Battle kMeilingFinal{Create<MeilingBoss,MeilingFinalConfig>};
}

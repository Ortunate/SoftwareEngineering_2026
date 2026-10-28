// 本文件定义第 2 面阶段的技能组合、血条比例/颜色/排列与入退场参数；技能血量由角色内部定义。
#include "stages/stage2/boss_config.hpp"
#include "stages/stage2/behaviors.hpp"
#include "stages/stage2/ids.hpp"
#include "gameplay/bosses/daiyousei.hpp"
#include "gameplay/bosses/cirno.hpp"
namespace koumakyou::stage2 {
namespace {
using namespace bosses;
constexpr Color kSpellPink{239,138,152,255};
StartConfig Base(Vector3 position,std::uint32_t seed,VisualId visual,float radius,ShotType shot) {
    StartConfig c;c.position=position;c.seed=seed;c.visual=visual;c.hitRadius=radius;c.shot=shot;
    c.defeatBase=kNativeDefeat;c.configure=ConfigureBehavior;return c;
}
} // namespace
using namespace bosses;
StartConfig DaiyouseiMidbossConfig(ShotType shot) {
    using S=DaiyouseiBoss::Behavior;
    auto c=Base({0,0,11.4285717F},2498,kDaiyousei,native_sizes::kBoss45Radius,shot);
    c.phases={{{S::Midboss},{{{0,1,WHITE}}}}};
    c.defeated=S::Defeated;c.retreat=S::Timeout;c.defeatScore=100000;return c;
}
StartConfig CirnoFinalConfig(ShotType shot) {
    using S=CirnoBoss::Behavior;
    auto c=Base({0,0,5.71428585F},5894,kCirno,native_sizes::kBoss48Radius,shot);
    c.phases={
        {{S::FirstNonspell,S::IcicleFall},{{{1,0.15F,kSpellPink},{0,0.85F,WHITE}}}},
        {{S::SecondNonspell,S::PerfectFreeze,S::DiamondBlizzard},
            {{{2,14.0F/130,kSpellPink},{1,18.0F/130,kSpellPink},{0,98.0F/130,WHITE}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.musicOnEntrance=true;c.defeatScore=c.retreatScore=200000;return c;
}
const Battle kDaiyouseiMidboss{Create<DaiyouseiBoss,DaiyouseiMidbossConfig>};
const Battle kCirnoFinal{Create<CirnoBoss,CirnoFinalConfig>};
}

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
    using S=SakuyaBoss::Behavior;
    auto c=Base({-10,0,4.28571415F},3272,kSakuya,native_sizes::kBoss56Radius,shot);c.intro=S::Entrance;c.controlsWorldTime=true;
    c.phases={{{S::MisdirectionNonspell,S::Misdirection},{{{1,71.0F/1500,kSpellPink},{0,1429.0F/1500,WHITE}}}}};
    c.phases[0].skills[0].onTimeout=Transition::Exit();
    c.defeated=S::MisdirectionDefeated;c.retreat=S::MisdirectionRetreat;c.defeatScore=100000;return c;
}
StartConfig SakuyaFinalConfig(ShotType shot) {
    using S=SakuyaBoss::Behavior;
    auto c=Base({-10,0,4.28571415F},7604,kSakuya,native_sizes::kBoss56Radius,shot);c.intro=S::Entrance;c.controlsWorldTime=true;
    c.phases={
        {{S::FirstNonspell,S::ClockCorpse},{{{1,14.0F/190,kSpellPink},{0,176.0F/190,WHITE}}}},
        {{S::SecondNonspell,S::LunaClock},{{{1,14.0F/170,kSpellPink},{0,156.0F/170,WHITE}}}},
        {{S::ThirdNonspell,S::Marionette},{{{1,17.0F/160,kSpellPink},{0,143.0F/160,WHITE}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
const Battle kSakuyaMidboss{Create<SakuyaBoss,SakuyaMidbossConfig>};
const Battle kSakuyaFinal{Create<SakuyaBoss,SakuyaFinalConfig>};
}

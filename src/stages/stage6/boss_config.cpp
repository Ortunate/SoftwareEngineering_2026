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
StartConfig SakuyaMidbossConfig(ShotType shot) {
    using S=SakuyaBoss::Behavior;
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Sakuya shot");
    constexpr std::array<float,4> ratio{0.125F,1.0F/6,11.0F/60,11.0F/60};
    auto c=Base({-10,0,4.28571415F},2493,kSakuya,native_sizes::kBoss56Radius,shot);c.intro=S::EternalMeekEntrance;c.initialContact=false;
    c.phases={{{S::EternalMeekNonspell,S::EternalMeek},{{{1,ratio[index],kSpellPink},{0,1-ratio[index],WHITE}}}}};
    c.phases[0].skills[0].onTimeout=Transition::Exit();
    c.defeated=S::EternalMeekDefeated;c.retreat=S::EternalMeekRetreat;c.defeatScore=100000;return c;
}
StartConfig RemiliaFinalConfig(ShotType shot) {
    using S=RemiliaBoss::Behavior;
    auto c=Base({2.85714293F,0,12.8571424F},3098,kRemilia,native_sizes::kBoss56Radius,shot);
    c.intro=S::Entrance;c.resumeSteps={S::Prelude};c.initialContact=false;
    c.phases={
        {{S::FirstNonspell,S::StarOfVanities},{{{1,12.0F/130,kSpellPink},{0,118.0F/130,WHITE}}}},
        {{S::SecondNonspell,S::ScarletNetherworld},{{{1,16.0F/155,kSpellPink},{0,139.0F/155,WHITE}}}},
        {{S::ThirdNonspell,S::VladTepesCurse},{{{1,16.0F/110,kSpellPink},{0,94.0F/110,WHITE}}}},
        {{S::BatNonspell,S::ScarletShoot},{{{1,0.1F,kSpellPink},{0,0.9F,WHITE}}}},
        {{S::RedMagic},{{{0,1,kSpellPink}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
const Battle kSakuyaMidboss{Create<SakuyaBoss,SakuyaMidbossConfig>};
const Battle kRemiliaFinal{Create<RemiliaBoss,RemiliaFinalConfig>};
}

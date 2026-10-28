// 本文件定义第 1 面阶段的技能组合、血条比例/颜色/排列与入退场参数；技能血量由角色内部定义。
#include "stages/stage1/boss_config.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/bosses/rumia.hpp"
namespace koumakyou::stage1 {
namespace {
using namespace bosses;
constexpr Color kSpellPink{239,138,152,255};
StartConfig Base(Vector3 position,std::uint32_t seed,VisualId visual,float radius,ShotType shot) {
    StartConfig c;c.position=position;c.seed=seed;c.visual=visual;c.hitRadius=radius;c.shot=shot;
    c.defeatBase=kNativeDefeat;c.configure=ConfigureBehavior;return c;
}
} // namespace
using namespace bosses;
StartConfig RumiaMidbossConfig(ShotType shot) {
    using S=RumiaBoss::Behavior;
    auto c=Base({0,0,11.4285717F},1882,kRumia,native_sizes::kBoss48Radius,shot);
    c.phases={{{S::Midboss},{{{0,1,WHITE}}}}};
    c.defeated=S::MidbossDefeated;c.retreat=S::MidbossTimeout;c.defeatScore=100000;return c;
}
StartConfig RumiaFinalConfig(ShotType shot) {
    using S=RumiaBoss::Behavior;
    auto c=Base({0,0,5.71428585F},5093,kRumia,native_sizes::kBoss48Radius,shot);
    c.phases={
        {{S::FirstNonspell,S::NightBird},{{{1,9.0F/70,kSpellPink},{0,61.0F/70,WHITE}}}},
        {{S::SecondNonspell,S::Demarcation},{{{1,8.0F/75,kSpellPink},{0,67.0F/75,WHITE}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.musicOnEntrance=true;c.defeatScore=c.retreatScore=200000;return c;
}
const Battle kRumiaMidboss{Create<RumiaBoss,RumiaMidbossConfig>};
const Battle kRumiaFinal{Create<RumiaBoss,RumiaFinalConfig>};
}

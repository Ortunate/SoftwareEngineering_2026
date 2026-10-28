// 本文件定义第 4 面阶段的技能组合、血条比例/颜色/排列与入退场参数；技能血量由角色内部定义。
#include "stages/stage4/boss_config.hpp"
#include "stages/stage4/behaviors.hpp"
#include "stages/stage4/ids.hpp"
#include "gameplay/bosses/koakuma.hpp"
#include "gameplay/bosses/patchouli.hpp"
namespace koumakyou::stage4 {
namespace {
using namespace bosses;
constexpr Color kSpellPink{239,138,152,255};
StartConfig Base(Vector3 position,std::uint32_t seed,VisualId visual,float radius,ShotType shot) {
    StartConfig c;c.position=position;c.seed=seed;c.visual=visual;c.hitRadius=radius;c.shot=shot;
    c.defeatBase=kNativeDefeat;c.configure=ConfigureBehavior;return c;
}
} // namespace
using namespace bosses;
StartConfig KoakumaMidbossConfig(ShotType shot) {
    using S=KoakumaBoss::Behavior;
    auto c=Base({0,0,4.28571415F},4058,kKoakuma,native_sizes::kBoss40Radius,shot);
    c.phases={{{S::Encounter},{{{0,1,WHITE}}}}};
    c.initialDamageable=true;c.defeated=S::Defeated;c.retreat=S::Timeout;c.defeatScore=100000;return c;
}
StartConfig PatchouliFinalConfig(ShotType shot) {
    using S=PatchouliBoss::Behavior;
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Patchouli shot");
    // 四机体决定组合，血量与时限仍由所选角色技能定义解析。比例独立作为外观数据保存。
    constexpr std::array<S,4> first{S::AgniShine,S::PrincessUndine,S::SylphyHorn,S::LazyTrilithon};
    constexpr std::array<S,4> second{S::LazyTrilithonAdvanced,S::SylphyHornAdvanced,S::AgniShineAdvanced,S::MetalFatigue};
    constexpr std::array<S,4> combination{S::LavaCromlech,S::WaterElf,S::ForestBlaze,S::EmeraldMegalith};
    constexpr std::array<S,4> last{S::MercuryPoison,S::MercuryPoison,S::EmeraldMegalithFinal,S::WaterElfFinal};
    constexpr std::array<float,4> firstRatio{16.0F/170,16.0F/170,16.0F/170,18.0F/170};
    constexpr std::array<float,4> secondRatio{15.0F/160,16.0F/160,17.0F/160,18.0F/160};
    auto c=Base({-5.71428585F,0,11.4285717F},10511,kPatchouli,native_sizes::kBoss48Radius,shot);c.intro=S::Entrance;
    c.phases={
        {{S::FirstNonspell,first[index]},{{{1,firstRatio[index],kSpellPink},{0,1-firstRatio[index],WHITE}}}},
        {{S::SecondNonspell,second[index]},{{{1,secondRatio[index],kSpellPink},{0,1-secondRatio[index],WHITE}}}},
        {{combination[index],last[index]},{{{1,0.5F,kSpellPink},{0,0.5F,kSpellPink}}}},
    };
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
const Battle kKoakumaMidboss{Create<KoakumaBoss,KoakumaMidbossConfig>};
const Battle kPatchouliFinal{Create<PatchouliBoss,PatchouliFinalConfig>};
}

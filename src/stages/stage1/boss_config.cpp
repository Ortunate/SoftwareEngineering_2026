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
StartConfig RumiaMidbossConfig(ShotType shot) {return RumiaMidbossConfig(shot,native_combat::Difficulty::Normal);}
StartConfig RumiaMidbossConfig(ShotType shot,native_combat::Difficulty difficulty) {
    using S=RumiaBoss::Behavior;
    auto c=Base({0,0,11.4285717F},1882,kRumia,native_sizes::kBoss48Radius,shot);
    c.phases={{{S::Midboss},{{{0,1,WHITE}}}}};
    c.difficulty=difficulty;
    if(difficulty==native_combat::Difficulty::Hard||difficulty==native_combat::Difficulty::Lunatic) {
        // 0.9/0.1 是项目显式血条设计；源数据确认两技能和 500 HP 阈值，不证明原生显示比例。
        c.phases={{{S::Midboss,S::MoonlightRay},{{{1,0.1F,kSpellPink},{0,0.9F,WHITE}}}}};
        c.phases[0].skills[0].onTimeout=Transition::Exit();
        c.phases[0].skills[1].onTimeout=Transition::ExitDefeated();
    }
    c.defeated=S::MidbossDefeated;c.retreat=S::MidbossTimeout;c.defeatScore=100000;return c;
}
StartConfig RumiaFinalConfig(ShotType shot) {return RumiaFinalConfig(shot,native_combat::Difficulty::Normal);}
StartConfig RumiaFinalConfig(ShotType shot,native_combat::Difficulty difficulty) {
    using S=RumiaBoss::Behavior;
    auto c=Base({0,0,5.71428585F},5093,kRumia,native_sizes::kBoss48Radius,shot);
    c.phases={
        {{S::FirstNonspell,S::NightBird},{{{1,9.0F/70,kSpellPink},{0,61.0F/70,WHITE}}}},
        {{S::SecondNonspell,S::Demarcation},{{{1,8.0F/75,kSpellPink},{0,67.0F/75,WHITE}}}},
    };
    c.difficulty=difficulty;
    // Easy 原首非符受击和超时均直接跳转下一组，不保留夜符血量。
    if(difficulty==native_combat::Difficulty::Easy)c.phases[0]={{S::FirstNonspell},{{{0,1,WHITE}}}};
    c.defeated=c.retreat=S::Defeated;c.musicOnEntrance=true;c.defeatScore=c.retreatScore=200000;return c;
}
namespace {
std::unique_ptr<Boss> CreateMidboss(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<RumiaBoss>();boss->StartBoss(RumiaMidbossConfig(shot,difficulty));return boss;
}
std::unique_ptr<Boss> CreateFinal(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<RumiaBoss>();boss->StartBoss(RumiaFinalConfig(shot,difficulty));return boss;
}
}
const Battle kRumiaMidboss{CreateMidboss};
const Battle kRumiaFinal{CreateFinal};
}

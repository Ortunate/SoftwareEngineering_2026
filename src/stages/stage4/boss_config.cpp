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
    return KoakumaMidbossConfig(shot,native_combat::Difficulty::Normal);
}
StartConfig KoakumaMidbossConfig(ShotType shot,native_combat::Difficulty difficulty) {
    if(native_combat::DifficultyIndex(difficulty)<0)throw std::invalid_argument("Invalid Koakuma difficulty");
    using S=KoakumaBoss::Behavior;
    auto c=Base({0,0,4.28571415F},4058,kKoakuma,native_sizes::kBoss40Radius,shot);
    c.difficulty=difficulty;
    c.phases={{{S::Encounter},{{{0,1,WHITE}}}}};
    c.initialDamageable=true;c.defeated=S::Defeated;c.retreat=S::Timeout;c.defeatScore=100000;return c;
}
StartConfig PatchouliFinalConfig(ShotType shot) {
    return PatchouliFinalConfig(shot,native_combat::Difficulty::Normal);
}
StartConfig PatchouliFinalConfig(ShotType shot,native_combat::Difficulty difficulty) {
    using S=PatchouliBoss::Behavior;
    const auto index=static_cast<unsigned>(shot);if(index>3)throw std::invalid_argument("Invalid Patchouli shot");
    const int d=native_combat::DifficultyIndex(difficulty);if(d<0)throw std::invalid_argument("Invalid Patchouli difficulty");
    // 四机体决定元素顺序；比例仍是明确的演出参数，不在运行时由技能 HP 自动计算。
    constexpr std::array<S,4> ordinary{S::AgniShine,S::PrincessUndine,S::SylphyHorn,S::LazyTrilithon};
    constexpr std::array<S,4> enhanced{S::AgniShineAdvanced,S::BuryInLake,S::SylphyHornAdvanced,S::LazyTrilithonAdvanced};
    constexpr std::array<S,4> normalSecond{S::LazyTrilithonAdvanced,S::SylphyHornAdvanced,S::AgniShineAdvanced,S::MetalFatigue};
    constexpr std::array<S,4> hardSecond{S::TrilithonShake,S::GreenStorm,S::AgniRadiance,S::SilverDragon};
    constexpr std::array<S,4> combination{S::LavaCromlech,S::WaterElf,S::ForestBlaze,S::EmeraldMegalith};
    constexpr std::array<S,4> middle{S::MercuryPoisonMiddle,S::MercuryPoisonMiddle,S::EmeraldMegalithMiddle,S::WaterElfMiddle};
    constexpr std::array<S,4> normalLast{S::MercuryPoison,S::MercuryPoison,S::EmeraldMegalithFinal,S::WaterElfFinal};
    constexpr std::array<S,4> hardLast{S::ForestBlazeFinal,S::EmeraldMegalithFinal,S::LavaCromlechFinal,S::MercuryPoison};
    constexpr std::array<float,4> normalFirstRatio{16.0F/170,16.0F/170,16.0F/170,18.0F/170};
    constexpr std::array<float,4> hardFirstRatio{15.0F/170,16.0F/170,16.0F/170,18.0F/170};
    constexpr std::array<float,4> easySecondRatio{15.0F/160,16.0F/160,16.0F/160,18.0F/160};
    constexpr std::array<float,4> normalSecondRatio{15.0F/160,16.0F/160,17.0F/160,18.0F/160};
    constexpr std::array<float,4> hardSecondRatio{15.0F/160,16.0F/160,18.0F/160,18.0F/160};
    auto c=Base({-5.71428585F,0,11.4285717F},10511,kPatchouli,native_sizes::kBoss48Radius,shot);c.intro=S::Entrance;c.difficulty=difficulty;
    if(d==0) {
        c.phases={
            {{S::FirstNonspell},{{{0,1,WHITE}}}},
            {{S::SecondNonspell,ordinary[index]},{{{1,easySecondRatio[index],kSpellPink},{0,1-easySecondRatio[index],WHITE}}}},
            {{combination[index]},{{{0,1,kSpellPink}}}},
        };
    } else if(d==1) {
        c.phases={
            {{S::FirstNonspell,ordinary[index]},{{{1,normalFirstRatio[index],kSpellPink},{0,1-normalFirstRatio[index],WHITE}}}},
            {{S::SecondNonspell,normalSecond[index]},{{{1,normalSecondRatio[index],kSpellPink},{0,1-normalSecondRatio[index],WHITE}}}},
            {{combination[index],normalLast[index]},{{{1,0.5F,kSpellPink},{0,0.5F,kSpellPink}}}},
        };
    } else {
        c.phases={
            {{S::FirstNonspell,enhanced[index]},{{{1,hardFirstRatio[index],kSpellPink},{0,1-hardFirstRatio[index],WHITE}}}},
            {{S::SecondNonspell,hardSecond[index]},{{{1,hardSecondRatio[index],kSpellPink},{0,1-hardSecondRatio[index],WHITE}}}},
            {{combination[index],middle[index],hardLast[index]},{{{2,1.0F/3,kSpellPink},{1,1.0F/3,kSpellPink},{0,1.0F/3,kSpellPink}}}},
        };
    }
    c.defeated=c.retreat=S::Defeated;c.defeatScore=c.retreatScore=200000;return c;
}
namespace {
std::unique_ptr<Boss> CreateMidboss(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<KoakumaBoss>();boss->StartBoss(KoakumaMidbossConfig(shot,difficulty));return boss;
}
std::unique_ptr<Boss> CreateFinal(ShotType shot,native_combat::Difficulty difficulty) {
    auto boss=std::make_unique<PatchouliBoss>();boss->StartBoss(PatchouliFinalConfig(shot,difficulty));return boss;
}
}
namespace {
/// @brief 单技能练习独立建立继承边界，保留角色 HP、准备、宣告与退场；不借用整关机体分支定位符卡。
template<PatchouliBoss::Behavior Skill>
std::unique_ptr<Boss> CreateSpell(ShotType shot,native_combat::Difficulty difficulty) {
    using P=PatchouliBoss;
    auto c=Base({0,0,4.28571415F},10511,kPatchouli,native_sizes::kBoss48Radius,shot);c.difficulty=difficulty;
    c.configure=P::Behaviors::ConfigurePractice;c.phases={{{Skill},{{{0,1,kSpellPink}}}}};
    c.defeated=c.retreat=P::Behavior::Defeated;c.defeatScore=c.retreatScore=200000;
    auto boss=std::make_unique<P>();boss->StartBoss(c);return boss;
}
template<PatchouliBoss::Behavior Skill> const Battle kSpellBattle{CreateSpell<Skill>};
}
const Battle& PatchouliSpellBattle(int spellCard,native_combat::Difficulty difficulty) {
    using B=PatchouliBoss::Behavior;
    struct Entry {int cardBase,minDifficulty,maxDifficulty;const Battle* battle;};
    static const std::array<Entry,18> entries{{
        {38,0,1,&kSpellBattle<B::AgniShine>},{39,1,3,&kSpellBattle<B::AgniShineAdvanced>},
        {41,2,3,&kSpellBattle<B::AgniRadiance>},{45,0,1,&kSpellBattle<B::PrincessUndine>},
        {45,2,3,&kSpellBattle<B::BuryInLake>},{49,0,1,&kSpellBattle<B::SylphyHorn>},
        {50,1,3,&kSpellBattle<B::SylphyHornAdvanced>},{52,2,3,&kSpellBattle<B::GreenStorm>},
        {56,0,1,&kSpellBattle<B::LazyTrilithon>},{57,1,3,&kSpellBattle<B::LazyTrilithonAdvanced>},
        {59,2,3,&kSpellBattle<B::TrilithonShake>},{62,1,1,&kSpellBattle<B::MetalFatigue>},
        {62,2,3,&kSpellBattle<B::SilverDragon>},{66,0,3,&kSpellBattle<B::LavaCromlech>},
        {70,0,3,&kSpellBattle<B::ForestBlaze>},{74,0,3,&kSpellBattle<B::WaterElf>},
        {77,1,3,&kSpellBattle<B::MercuryPoison>},{81,0,3,&kSpellBattle<B::EmeraldMegalith>},
    }};
    const int d=native_combat::DifficultyIndex(difficulty);
    if(d<0)throw std::invalid_argument("Invalid Patchouli practice difficulty");
    for(const auto& entry:entries)if(d>=entry.minDifficulty&&d<=entry.maxDifficulty&&spellCard==entry.cardBase+d)return *entry.battle;
    throw std::invalid_argument("Stage 4 spell does not match difficulty");
}
const Battle kKoakumaMidboss{CreateMidboss};
const Battle kPatchouliFinal{CreateFinal};
}

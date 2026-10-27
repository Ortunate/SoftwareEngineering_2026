// 本文件同步四面生命目录、机体分支、血条和阶段回调；实际攻击与演出由命名行为编排。
#include "stages/stage4/boss.hpp"
#include "stages/stage4/behaviors.hpp"
#include "stages/stage4/ids.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace koumakyou::stage4 {
namespace {
float Setting(int sub,int opcode) {return static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(4,sub,opcode)));}
float Instruction(int sub,int index,int opcode) {
    const auto* r=native_combat::Instruction(4,sub,index);
    if(!r||r->opcode!=opcode)throw std::logic_error("Invalid Patchouli health provenance");
    return static_cast<float>(native_combat::RequireLiteral(r));
}
struct HealthGroups {float mid,first,second,combination,reserve;std::array<float,4> firstSpell,secondSpell;};
const HealthGroups& Health() {
    static const HealthGroups h{static_cast<float>(native_combat::RequireLiteral(native_combat::Spawn(4,0,4058,21))),
        Setting(27,111),Setting(37,111),Setting(39,111),Setting(39,113),
        {Instruction(27,21,113),Instruction(27,28,113),Instruction(27,35,113),Instruction(27,40,113)},
        {Instruction(37,49,113),Instruction(37,56,113),Instruction(37,63,113),Instruction(37,68,113)}};
    return h;
}
struct Spell {behavior::Entry entry;int preparation,card;const char* name;};
using namespace patchouli;
constexpr Spell kAgni{AgniShine,43,39,"火符「Agni Shine」"};
constexpr Spell kAgniAdvanced{AgniShineAdvanced,46,40,"火符「Agni Shine上级」"};
constexpr Spell kUndine{PrincessUndine,52,46,"水符「Princess Undine」"};
constexpr Spell kSylphy{SylphyHorn,58,50,"木符「Sylphy Horn」"};
constexpr Spell kSylphyAdvanced{SylphyHornAdvanced,61,51,"木符「Sylphy Horn上级」"};
constexpr Spell kTrilithon{LazyTrilithon,67,57,"土符「Lazy Trilithon」"};
constexpr Spell kTrilithonAdvanced{LazyTrilithonAdvanced,70,58,"土符「Lazy Trilithon上级」"};
constexpr Spell kMetal{MetalFatigue,76,63,"金符「Metal Fatigue」"};
constexpr Spell kLava{LavaCromlech,82,67,"火&土符「Lava Cromlech」"};
constexpr Spell kForest{ForestBlaze,85,71,"木&火符「Forest Blaze」"};
constexpr Spell kWater{WaterElf,88,75,"水&木符「Water Elf」"};
constexpr Spell kMercury{MercuryPoison,91,78,"金&水符「Mercury Poison」"};
constexpr Spell kEmerald{EmeraldMegalith,94,82,"土&金符「Emerald Megalith」"};
// 121(3) 的原生四机体表在 Normal 只取前两列组合；第三列属于更高难度，不作为本关额外阶段。
constexpr std::array<std::array<Spell,4>,4> kSpells{{
    {kAgni,kUndine,kSylphy,kTrilithon}, {kTrilithonAdvanced,kSylphyAdvanced,kAgniAdvanced,kMetal},
    {kLava,kWater,kForest,kEmerald}, {kMercury,kMercury,kEmerald,kWater}}};
const Spell* CurrentSpell(int phase,ShotType shot) {
    const int slot=phase==2 ? 0 : phase==4 ? 1 : phase==5 ? 2 : phase==6 ? 3 : -1;
    return slot<0 ? nullptr : &kSpells[slot][static_cast<int>(shot)];
}
}
Stage4Boss::Stage4Boss(bool final,behavior::UpdateRate rate,bool spatial):actor_(rate),final_(final),waiting_(final) {
    active=true;phase=final ? 0 : 1;visual=final ? kPatchouli : kKoakuma;presentation.Reset(ActorKind::Boss);
    health=maxHealth=final ? 1 : Health().mid;
    actor_.Reset(final ? Vector3{-5.71428585F,0,11.4285717F} : Vector3{0,0,4.28571415F},health,final ? 10511U : 4058U);
    ConfigureBehavior(actor_,spatial);actor_.damageable= !final;
    actor_.Start(final ? patchouli::Entrance(actor_) : koakuma::Encounter(actor_));position=actor_.Position();
}
void Stage4Boss::SetPlayerShot(ShotType shot) {
    if(static_cast<unsigned>(shot)>3)throw std::invalid_argument("Invalid Patchouli player shot");
    if(final_&&!waiting_)throw std::logic_error("Cannot change Patchouli branch during combat");
    shot_=shot;
}
void Stage4Boss::ResumeCombat() {
    if(!active||!final_||!waiting_)throw std::logic_error("Patchouli is not awaiting dialogue completion");
    waiting_=false;phase=1;StartPhase();
}
void Stage4Boss::StartPhase() {
    const auto& h=Health();const auto index=static_cast<int>(shot_);const auto* spell=CurrentSpell(phase,shot_);
    actor_.health=actor_.maxHealth=phase==1 ? h.first : phase==2 ? h.firstSpell[index] : phase==3 ? h.second :
        phase==4 ? h.secondSpell[index] : phase==5 ? h.combination : h.reserve;
    actor_.pendingCallback=spell ? spell->entry : phase==1 ? patchouli::FirstNonspell : patchouli::SecondNonspell;
    actor_.Transition();actor_.damageable=false;actor_.deathCallback=patchouli::PhaseComplete;actor_.timeoutCallback=patchouli::PhaseComplete;
    constexpr std::array<int,4> firstTimeout{19,26,33,38},secondTimeout{47,54,61,66};
    actor_.timeoutFrames=spell ? Setting(spell->preparation,115) :
        Instruction(phase==1 ? 27 : 37,(phase==1 ? firstTimeout : secondTimeout)[index],115);
    actor_.lifeThreshold=phase==1 ? h.firstSpell[index] : phase==3 ? h.secondSpell[index] : phase==5 ? h.reserve : -1;
    actor_.lifeCallback=patchouli::PhaseComplete;
    if(phase>=5) {
        const int duration=static_cast<int>(actor_.timeoutFrames);actor_.timeoutFrames=duration+120;
        actor_.Start(patchouli::Combination(actor_,spell->entry,duration));
    }
    health=actor_.health;maxHealth=actor_.maxHealth;phaseTimer=0;phaseElapsed_=0;
}
bool Stage4Boss::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!active)return false;
    presentation.Update(seconds);phaseElapsed_+=seconds;actor_.health=health;actor_.Update(seconds,player,bullets);
    health=actor_.health;maxHealth=actor_.maxHealth;position=actor_.Position();phaseTimer=static_cast<float>(actor_.phaseFrames/60);
    animationScript=actor_.animationScript;animationAge=actor_.animationAge;animationRevision=actor_.animationRevision;
    defeatVisual=static_cast<VisualId>(kNativeDefeat+std::clamp(actor_.deathEffect,0,2));
    return actor_.pendingCallback||actor_.finished;
}
bool Stage4Boss::ShouldAdvanceFromDamage() const {
    return CanTakeDamage()&&(health<=0||(actor_.lifeThreshold>0&&health<actor_.lifeThreshold));
}
bool Stage4Boss::PhaseDefeated() const {
    return health<=0||(final_&&phase==5&&(health<Health().reserve||actor_.callbackCause==behavior::CallbackCause::HealthThreshold));
}
bool Stage4Boss::AdvancePhase() {
    if(!active||waiting_)throw std::logic_error("Cannot advance inactive or waiting Patchouli");
    actor_.health=health;
    if(actor_.finished) {active=false;phase=0;return false;}
    if(!final_) {
        defeated_=health<=0||actor_.pendingCallback==koakuma::Defeated;
        if(!actor_.pendingCallback)actor_.pendingCallback=koakuma::Defeated;
        actor_.Transition();actor_.damageable=false;retiring_=true;
    } else if(phase<6) {++phase;StartPhase();}
    else {
        defeated_=health<=0;retiring_=true;actor_.pendingCallback=patchouli::Defeated;actor_.Transition();actor_.damageable=false;
    }
    health=actor_.health;maxHealth=actor_.maxHealth;phaseTimer=0;return true;
}
const char* Stage4Boss::SpellName() const {
    if(!active||!final_||waiting_||retiring_||(phase>=5&&phaseElapsed_+1e-6<2))return nullptr;
    const auto* spell=CurrentSpell(phase,shot_);return spell ? spell->name : nullptr;
}
int Stage4Boss::SpellCardNumber() const {return SpellName() ? CurrentSpell(phase,shot_)->card : 0;}
float Stage4Boss::TimeLimit() const {return waiting_||retiring_ ? -1 : actor_.timeoutFrames/60.0F;}
BossHealthBar Stage4Boss::HealthBar() const {
    if(!active||waiting_||retiring_)return {};
    if(!final_)return Boss::HealthBar();
    const auto& h=Health();const auto index=static_cast<int>(shot_);
    const float total=phase<=2 ? h.first : phase<=4 ? h.second : h.combination;
    const float reserve=phase<=2 ? h.firstSpell[index] : phase<=4 ? h.secondSpell[index] : h.reserve;
    const float current=std::clamp(health,0.0F,total);BossHealthBar bar;bar.count=2;bar.remainingBars=phase<=2 ? 2 : phase<=4 ? 1 : 0;
    bar.segments[0]={reserve/total,std::min(current,reserve)/total,BossHealthKind::Spell};
    bar.segments[1]={(total-reserve)/total,std::max(0.0F,current-reserve)/total,phase>=5 ? BossHealthKind::Spell : BossHealthKind::NonSpell};
    return bar;
}
}

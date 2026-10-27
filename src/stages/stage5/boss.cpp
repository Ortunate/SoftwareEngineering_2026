// 本文件同步咲夜 Normal 的原生生命目录、阈值、时停超时钟、符卡记录与完整退场通知。
#include "stages/stage5/boss.hpp"
#include "stages/stage5/behaviors.hpp"
#include "stages/stage5/ids.hpp"
#include "gameplay/data/native_combat.hpp"
#include <array>
#include <stdexcept>
namespace koumakyou::stage5 {
namespace {
float Setting(int sub,int opcode) {return static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(5,sub,opcode)));}
struct HealthGroups {float mid,midSpell;std::array<float,3> main,reserve;};
const HealthGroups& Health() {
    static const HealthGroups h{static_cast<float>(native_combat::RequireLiteral(native_combat::Spawn(5,0,3272,12))),
        Setting(13,113),{Setting(24,111),Setting(36,111),Setting(43,111)},
        {Setting(24,113),Setting(36,113),Setting(43,113)}};return h;
}
}
Stage5Boss::Stage5Boss(bool final,behavior::UpdateRate rate,bool spatial):actor_(rate),final_(final) {
    active=true;phase=0;visual=kSakuya;presentation.Reset(ActorKind::Boss);health=maxHealth=final ? 1 : Health().mid;
    actor_.Reset({-10,0,4.28571415F},health,final ? 7604U : 3272U);ConfigureBehavior(actor_,spatial);actor_.hitRadius=hitRadius=native_sizes::kBoss56Radius;
    actor_.Start(sakuya::Entrance(actor_));position=actor_.Position();
}
void Stage5Boss::ResumeCombat() {
    if(!active||!waiting_)throw std::logic_error("Sakuya is not awaiting dialogue completion");
    waiting_=false;phase=1;StartPhase();
}
void Stage5Boss::StartPhase() {
    // 道中入场 ECL 全宽 56，开战子程序改为 40，后续符卡继承。
    if(!final_)actor_.hitRadius=hitRadius=native_sizes::kBoss40Radius;
    const auto& h=Health();using namespace sakuya;
    constexpr std::array<behavior::Entry,6> entries{FirstNonspell,ClockCorpse,SecondNonspell,LunaClock,ThirdNonspell,Marionette};
    constexpr std::array<int,6> settings{24,47,36,53,43,61};
    const int group=(phase-1)/2;const bool spell=phase%2==0;
    actor_.health=actor_.maxHealth=final_ ? (spell ? h.reserve[group] : h.main[group]) : (spell ? h.midSpell : h.mid);
    actor_.pendingCallback=final_ ? entries[phase-1] : spell ? Misdirection : Midboss;
    actor_.Transition();actor_.phaseClockPaused=false;actor_.interactable=true;actor_.damageable=false;
    actor_.timeoutFrames=static_cast<int>(Setting(final_ ? settings[phase-1] : spell ? 19 : 13,115));
    actor_.lifeThreshold=spell ? -1 : final_ ? h.reserve[group] : h.midSpell;
    actor_.lifeCallback=actor_.deathCallback=actor_.timeoutCallback=PhaseComplete;
    health=actor_.health;maxHealth=actor_.maxHealth;phaseTimer=0;
}
bool Stage5Boss::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!active)return false;
    presentation.Update(seconds);actor_.health=health;actor_.Update(seconds,player,bullets);
    hitRadius=actor_.hitRadius;health=actor_.health;maxHealth=actor_.maxHealth;position=actor_.Position();phaseTimer=static_cast<float>(actor_.phaseFrames/60);
    animationScript=actor_.animationScript;animationAge=actor_.animationAge;animationRevision=actor_.animationRevision;
    defeatVisual=static_cast<VisualId>(kNativeDefeat+std::clamp(actor_.deathEffect,0,2));
    return actor_.pendingCallback||actor_.finished;
}
bool Stage5Boss::ShouldAdvanceFromDamage() const {return CanTakeDamage()&&(health<=0||(actor_.lifeThreshold>0&&health<actor_.lifeThreshold));}
bool Stage5Boss::PhaseDefeated() const {return phase%2==0&&health<=0;}
bool Stage5Boss::AdvancePhase() {
    if(!active||waiting_)throw std::logic_error("Cannot advance inactive or waiting Sakuya");
    if(retiring_) {active=false;phase=0;return false;}
    if(!final_&&phase==1&&actor_.callbackCause!=behavior::CallbackCause::Timeout) {++phase;StartPhase();return true;}
    if(final_&&phase<6) {++phase;StartPhase();return true;}
    retiring_=true;defeated_=health<=0;actor_.health=health;actor_.phaseClockPaused=false;
    actor_.pendingCallback=final_ ? sakuya::Defeated : defeated_ ? sakuya::MidbossDefeated : sakuya::MidbossRetreat;
    actor_.Transition();actor_.damageable=false;actor_.timeoutFrames=-1;actor_.lifeThreshold=-1;
    return true;
}
const char* Stage5Boss::SpellName() const {
    if(waiting_||retiring_||!active||phase%2)return nullptr;
    return !final_ ? "奇术「Misdirection」" : phase==2 ? "幻在「Clock Corpse」" : phase==4 ? "幻象「Luna Clock」" : "女仆秘技「提线玩偶」";
}
int Stage5Boss::SpellCardNumber() const {return !SpellName() ? 0 : !final_ ? 86 : phase==2 ? 90 : phase==4 ? 94 : 98;}
BossHealthBar Stage5Boss::HealthBar() const {
    if(!active||waiting_||retiring_)return {};
    const auto& h=Health();const int group=(phase-1)/2;
    const float total=final_ ? h.main[group] : h.mid,reserve=final_ ? h.reserve[group] : h.midSpell;
    const float current=std::clamp(health,0.0F,total);BossHealthBar bar;bar.remainingBars=final_ ? 2-group : 0;bar.count=2;
    bar.segments[0]={reserve/total,std::min(current,reserve)/total,BossHealthKind::Spell};
    bar.segments[1]={(total-reserve)/total,std::max(0.0F,current-reserve)/total,BossHealthKind::NonSpell};return bar;
}
}

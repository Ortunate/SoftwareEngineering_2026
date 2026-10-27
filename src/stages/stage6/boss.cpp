// 本文件将六面原生生命、机体阈值、两次对白通知、化蝠防 Bomb 和完整退场接入公共 Boss 生命周期。
#include "stages/stage6/boss.hpp"
#include "stages/stage6/ids.hpp"
#include "gameplay/data/native_combat.hpp"
#include <array>
#include <stdexcept>
namespace koumakyou::stage6 {
namespace {
float Setting(int sub,int opcode) {return static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(6,sub,opcode)));}
float Instruction(int sub,int index,int opcode) {
    const auto* r=native_combat::Instruction(6,sub,index);if(!r||r->opcode!=opcode)throw std::logic_error("Invalid Stage 6 health provenance");
    return static_cast<float>(native_combat::RequireLiteral(r));
}
struct HealthGroups {float mid,finalEntrance,last;std::array<float,4> midSpell,main,reserve;};
const HealthGroups& Health() {
    static const HealthGroups h{static_cast<float>(native_combat::RequireLiteral(native_combat::Spawn(6,0,2493,8))),
        static_cast<float>(native_combat::RequireLiteral(native_combat::Spawn(6,0,3098,17))),Setting(61,111),
        {Instruction(9,19,113),Instruction(9,24,113),Instruction(9,29,113),Instruction(9,34,113)},
        {Setting(19,111),Setting(21,111),Setting(25,111),Setting(28,111)},
        {Setting(19,113),Setting(21,113),Setting(25,113),Setting(28,113)}};return h;
}
}
Stage6Boss::Stage6Boss(bool final,behavior::UpdateRate rate,bool spatial):actor_(rate),final_(final) {
    active=true;phase=0;visual=final ? kRemilia : kSakuya;presentation.Reset(ActorKind::Boss);
    health=maxHealth=final ? Health().finalEntrance : Health().mid;
    actor_.Reset(final ? Vector3{2.85714293F,0,12.8571424F} : Vector3{-10,0,4.28571415F},health,final ? 3098U : 2493U);
    ConfigureBehavior(actor_,spatial);actor_.damageable=false;actor_.contactEnabled=false;
    actor_.Start(final ? remilia::Entrance(actor_) : sakuya::Entrance(actor_));position=actor_.Position();
}
void Stage6Boss::SetPlayerShot(ShotType shot) {
    if(static_cast<unsigned>(shot)>3||!waiting_)throw std::invalid_argument("Invalid Stage 6 shot selection");shot_=shot;
}
void Stage6Boss::ResumeCombat() {
    if(!active||!waiting_)throw std::logic_error("Stage 6 boss is not awaiting a dialogue notification");
    if(final_&&notifications_++==0) {
        actor_.pendingCallback=remilia::Prelude;actor_.Transition();actor_.damageable=false;return;
    }
    waiting_=false;phase=1;StartPhase();
}
void Stage6Boss::StartPhase() {
    const auto& h=Health();const bool spell=phase%2==0;const int group=(phase-1)/2;
    actor_.health=actor_.maxHealth=!final_ ? (spell ? h.midSpell[static_cast<int>(shot_)] : h.mid) :
        phase==9 ? h.last : spell ? h.reserve[group] : h.main[group];
    actor_.pendingCallback=remilia::PhaseComplete;actor_.Transition();actor_.damageable=false;actor_.interactable=true;
    if(!final_)actor_.Start(spell ? sakuya::EternalMeek(actor_) : sakuya::Midboss(actor_));
    else switch(phase) {
        case 1:actor_.Start(remilia::FirstNonspell(actor_));break;
        case 2:actor_.Start(remilia::StarOfVanities(actor_,memory_));break;
        case 3:actor_.Start(remilia::SecondNonspell(actor_,memory_));break;
        case 4:actor_.Start(remilia::ScarletNetherworld(actor_));break;
        case 5:actor_.Start(remilia::ThirdNonspell(actor_));break;
        case 6:actor_.Start(remilia::VladTepesCurse(actor_));break;
        case 7:actor_.Start(remilia::BatNonspell(actor_));break;
        case 8:actor_.Start(remilia::ScarletShoot(actor_));break;
        case 9:actor_.Start(remilia::RedMagic(actor_));break;
        default:throw std::logic_error("Invalid Remilia phase");
    }
    constexpr std::array<int,8> settings{19,33,21,39,25,45,28,53};
    actor_.timeoutFrames=final_&&phase==9 ? -1 : static_cast<int>(Setting(final_ ? settings[phase-1] : spell ? 15 : 9,115));
    actor_.lifeThreshold=spell||phase==9 ? -1 : final_ ? h.reserve[group] : h.midSpell[static_cast<int>(shot_)];
    actor_.lifeCallback=actor_.deathCallback=actor_.timeoutCallback=remilia::PhaseComplete;
    health=actor_.health;maxHealth=actor_.maxHealth;phaseTimer=0;
}
void Stage6Boss::UpdateBatTransformation(float seconds) {
    if(!final_||phase!=9||retiring_||health<=0)return;
    if(bombActive_) {
        batRecoveryFrames_=60;
        if(!batTransformed_||actor_.poses[3]>=0) {batTransformed_=true;actor_.SetPoses({-1,-1,-1,-1,-1});actor_.Animate(165);}
        actor_.interactable=false;
    } else if(batTransformed_) {
        batRecoveryFrames_=std::max(0.0,batRecoveryFrames_-seconds*60);
        if(batRecoveryFrames_<=1e-5) {
            batTransformed_=false;actor_.interactable=true;actor_.SetPoses({160,163,164,161,162});actor_.Animate(160);
        }
    }
}
bool Stage6Boss::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!active)return false;
    presentation.Update(seconds);actor_.health=health;actor_.Update(seconds,player,bullets);health=actor_.health;maxHealth=actor_.maxHealth;
    UpdateBatTransformation(seconds);position=actor_.Position();phaseTimer=static_cast<float>(actor_.phaseFrames/60);
    animationScript=actor_.animationScript;animationAge=actor_.animationAge;animationRevision=actor_.animationRevision;
    defeatVisual=static_cast<VisualId>(kNativeDefeat+std::clamp(actor_.deathEffect,0,2));return actor_.pendingCallback||actor_.finished;
}
bool Stage6Boss::ShouldAdvanceFromDamage() const {return CanTakeDamage()&&(health<=0||(actor_.lifeThreshold>0&&health<actor_.lifeThreshold));}
bool Stage6Boss::PhaseDefeated() const {return (phase%2==0||phase==9)&&health<=0;}
bool Stage6Boss::AdvancePhase() {
    if(!active||waiting_)throw std::logic_error("Cannot advance inactive or waiting Stage 6 boss");
    if(retiring_) {active=false;phase=0;return false;}
    if(!final_&&phase==1&&actor_.callbackCause!=behavior::CallbackCause::Timeout) {++phase;StartPhase();return true;}
    if(final_&&phase<9) {++phase;StartPhase();return true;}
    retiring_=true;defeated_=health<=0;actor_.health=health;
    actor_.pendingCallback=final_ ? remilia::Defeated : defeated_ ? sakuya::Defeated : sakuya::Retreat;
    actor_.Transition();actor_.damageable=false;actor_.contactEnabled=false;actor_.timeoutFrames=-1;actor_.lifeThreshold=-1;return true;
}
const char* Stage6Boss::SpellName() const {
    if(!active||waiting_||retiring_)return nullptr;
    if(!final_)return phase==2 ? "奇术「Eternal Meek」" : nullptr;
    return phase==2 ? "天罚「Star of Vanities」" : phase==4 ? "冥符「红色冥界」" : phase==6 ? "咒诅「弗拉德·采佩什的诅咒」" :
        phase==8 ? "红符「Scarlet Shoot」" : phase==9&&actor_.timeoutFrames>0 ? "「Red Magic」" : nullptr;
}
int Stage6Boss::SpellCardNumber() const {return !SpellName() ? 0 : !final_ ? 101 : phase==2 ? 104 : phase==4 ? 107 : phase==6 ? 110 : phase==8 ? 113 : 116;}
BossHealthBar Stage6Boss::HealthBar() const {
    if(!active||waiting_||retiring_)return {};const auto& h=Health();BossHealthBar bar;
    if(final_&&phase==9) {bar.count=1;bar.segments[0]={1,std::clamp(health/h.last,0.0F,1.0F),BossHealthKind::Spell};return bar;}
    const int group=(phase-1)/2;const float total=final_ ? h.main[group] : h.mid,reserve=final_ ? h.reserve[group] : h.midSpell[static_cast<int>(shot_)];
    const float current=std::clamp(health,0.0F,total);bar.remainingBars=final_ ? 4-group : 0;bar.count=2;
    bar.segments[0]={reserve/total,std::min(current,reserve)/total,BossHealthKind::Spell};
    bar.segments[1]={(total-reserve)/total,std::max(0.0F,current-reserve)/total,BossHealthKind::NonSpell};return bar;
}
}

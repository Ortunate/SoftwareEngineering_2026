// 本文件将红美铃的命名行为接入伤害阈值、分组血条、符卡记录、对话开战通知及完整退场。
#include "stages/stage3/boss.hpp"
#include "stages/stage3/ids.hpp"
#include "gameplay/data/native_combat.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace koumakyou::stage3 {
namespace {
float Setting(int sub,int opcode) {return static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(3,sub,opcode)));}
struct Groups {float mid,midSpell,first,firstSpell,second,third,rain,typhoon;};
const Groups& Health() {
    static const Groups data{static_cast<float>(native_combat::RequireLiteral(native_combat::Spawn(3,0,3474,9))),
        Setting(9,113),Setting(22,111),Setting(22,113),Setting(24,111),Setting(30,111),Setting(30,113),Setting(39,113)};
    return data;
}
}
Stage3Boss::Stage3Boss(bool final,behavior::UpdateRate rate,bool spatial):actor_(rate),final_(final),waiting_(final) {
    active=true;phase=final ? 0 : 1;visual=kMeiling;presentation.Reset(ActorKind::Boss);
    health=maxHealth=final ? 1 : Health().mid;
    actor_.Reset({7.14285707F,0,14.2857141F},health,final ? 6254U : 3474U);
    ConfigureBehavior(actor_,spatial);actor_.hitRadius=hitRadius=native_sizes::kBoss56Radius;actor_.damageable=false;
    actor_.Start(final ? meiling::Entrance(actor_) : meiling::Midboss(actor_));position=actor_.Position();
}
void Stage3Boss::ResumeCombat() {
    if(!active||!waiting_||!final_)throw std::logic_error("Meiling is not awaiting dialogue completion");
    waiting_=false;phase=1;health=maxHealth=actor_.health=actor_.maxHealth=Health().first;
    actor_.pendingCallback=meiling::FirstNonspell;actor_.Transition();actor_.damageable=false;phaseTimer=0;
}
bool Stage3Boss::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!active)return false;
    presentation.Update(seconds);actor_.health=health;actor_.Update(seconds,player,bullets);
    hitRadius=actor_.hitRadius;health=actor_.health;maxHealth=actor_.maxHealth;position=actor_.Position();phaseTimer=static_cast<float>(actor_.phaseFrames/60);
    animationScript=actor_.animationScript;animationAge=actor_.animationAge;animationRevision=actor_.animationRevision;
    defeatVisual=static_cast<VisualId>(kNativeDefeat+std::clamp(actor_.deathEffect,0,2));
    return actor_.pendingCallback||actor_.finished;
}
bool Stage3Boss::ShouldAdvanceFromDamage() const {
    if(!CanTakeDamage())return false;
    const auto& h=Health();
    return health<=0||(!final_&&phase==1&&health<h.midSpell)||
        (final_&&((phase==1&&health<h.firstSpell)||(phase==4&&health<h.rain)||(phase==5&&health<h.typhoon)));
}
bool Stage3Boss::PhaseDefeated() const {
    return health<=0||(final_&&phase==5&&(health<Health().typhoon||actor_.callbackCause==behavior::CallbackCause::HealthThreshold));
}
bool Stage3Boss::AdvancePhase() {
    if(!active||waiting_)throw std::logic_error("Cannot advance inactive or waiting Meiling");
    actor_.health=health;
    if(actor_.finished||(retiring_&&actor_.pendingCallback==meiling::End)) {active=false;phase=0;return false;}
    const auto& h=Health();
    if(!final_) {
        if(!actor_.pendingCallback)actor_.pendingCallback=phase==1 ? meiling::GorgeousSweetFlower : meiling::MidbossDefeated;
        // 非符超时直接退场；生命阈值才进入道中符卡，不能用 phase+1 把两条路径混合。
        if(phase==1&&actor_.pendingCallback==meiling::GorgeousSweetFlower) {
            phase=2;actor_.health=actor_.maxHealth=h.midSpell;
        } else {retiring_=true;defeated_=actor_.pendingCallback==meiling::MidbossDefeated;}
    } else {
        constexpr std::array<behavior::Entry,6> entries{meiling::RainbowWindChime,meiling::SummoningNonspell,
            meiling::RainNonspell,meiling::ColorfulRain,meiling::DazzlingColorTyphoon,meiling::Defeated};
        if(phase<1||phase>6)throw std::logic_error("Invalid Meiling phase");
        if(!actor_.pendingCallback)actor_.pendingCallback=entries[phase-1];
        if(phase==6) {retiring_=true;defeated_=health<=0;}
        else {
            ++phase;actor_.health=actor_.maxHealth=phase==2 ? h.firstSpell : phase==3 ? h.second : phase==4 ? h.third : phase==5 ? h.rain : h.typhoon;
        }
    }
    actor_.damageable=false;actor_.Transition();health=actor_.health;maxHealth=actor_.maxHealth;phaseTimer=0;return true;
}
const char* Stage3Boss::SpellName() const {
    if(!active||waiting_||retiring_)return nullptr;
    if(!final_)return phase==2 ? "华符「芳华绚烂」" : nullptr;
    return phase==2 ? "虹符「彩虹的风铃」" : phase==5 ? "彩符「彩雨」" : phase==6 ? "彩符「极彩台风」" : nullptr;
}
int Stage3Boss::SpellCardNumber() const {
    if(!SpellName())return 0;return !final_ ? 22 : phase==2 ? 26 : phase==5 ? 32 : 35;
}
BossHealthBar Stage3Boss::HealthBar() const {
    if(!active||waiting_||retiring_)return {};
    const auto& h=Health();BossHealthBar bar;
    const float total=!final_ ? h.mid : phase<=2 ? h.first : phase==3 ? h.second : h.third;
    const float reserve=!final_ ? h.midSpell : phase<=2 ? h.firstSpell : phase==3 ? 0 : h.rain;
    const float current=std::clamp(health,0.0F,total);
    bar.remainingBars=final_ ? (phase<=2 ? 2 : phase==3 ? 1 : 0) : 0;
    if(final_&&phase>=4) {
        bar.count=3;bar.segments[0]={h.typhoon/total,std::min(current,h.typhoon)/total,BossHealthKind::Spell};
        bar.segments[1]={(h.rain-h.typhoon)/total,std::clamp(current-h.typhoon,0.0F,h.rain-h.typhoon)/total,BossHealthKind::Spell};
        bar.segments[2]={(total-h.rain)/total,std::max(0.0F,current-h.rain)/total,BossHealthKind::NonSpell};
    } else if(reserve>0) {
        bar.count=2;bar.segments[0]={reserve/total,std::min(current,reserve)/total,BossHealthKind::Spell};
        bar.segments[1]={(total-reserve)/total,std::max(0.0F,current-reserve)/total,BossHealthKind::NonSpell};
    } else {bar.count=1;bar.segments[0]={1,current/total,BossHealthKind::NonSpell};}
    return bar;
}
}

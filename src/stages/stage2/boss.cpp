// 本文件同步二面 Boss 的伤害、阈值、分组血条与真实遭遇结束；不按全局固定帧拆分战斗阶段。
#include "stages/stage2/boss.hpp"
#include "stages/stage2/ids.hpp"
#include "gameplay/data/native_combat.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace koumakyou::stage2 {
namespace {
float Setting(int sub,int opcode) { return static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(2,sub,opcode))); }
/// @brief 原始 HP 分组；第二组包含 Perfect Freeze 与 Diamond Blizzard 两个连续预留段。
struct HealthGroups { float first,firstSpell,second,freeze,blizzard; };
const HealthGroups& Groups() {
    static const HealthGroups data{Setting(22,111),Setting(22,113),Setting(25,111),Setting(25,113),Setting(36,113)};
    return data;
}
}
Stage2Boss::Stage2Boss(bool final,behavior::UpdateRate rate,bool spatial):actor_(rate),final_(final) {
    active=true;phase=1;presentation.Reset(ActorKind::Boss);visual=final ? kCirno : kDaiyousei;
    health=maxHealth=final ? Groups().first : static_cast<float>(native_combat::RequireLiteral(native_combat::Spawn(2,0,2498,20)));
    actor_.Reset(final ? Vector3{0,0,5.71428585F} : Vector3{0,0,11.4285717F},health,final ? 5894U : 2498U);
    ConfigureBehavior(actor_,spatial);actor_.hitRadius=hitRadius=final ? native_sizes::kBoss48Radius : native_sizes::kBoss45Radius;actor_.damageable=false;
    actor_.Start(final ? cirno::FirstNonspell(actor_) : daiyousei::Midboss(actor_));position=actor_.Position();
}
bool Stage2Boss::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!active)return false;
    presentation.Update(seconds);actor_.health=health;actor_.Update(seconds,player,bullets);
    hitRadius=actor_.hitRadius;health=actor_.health;maxHealth=actor_.maxHealth;position=actor_.Position();phaseTimer=static_cast<float>(actor_.phaseFrames/60);
    animationScript=actor_.animationScript;animationAge=actor_.animationAge;animationRevision=actor_.animationRevision;
    defeatVisual=static_cast<VisualId>(kNativeDefeat+std::clamp(actor_.deathEffect,0,2));
    return actor_.pendingCallback||actor_.finished;
}
bool Stage2Boss::ShouldAdvanceFromDamage() const {
    if(!CanTakeDamage())return false;
    if(health<=0)return true;
    if(!final_)return false;
    const auto& g=Groups();return phase==1 ? health<g.firstSpell : phase==3 ? health<g.freeze : phase==4 ? health<g.blizzard : false;
}
bool Stage2Boss::PhaseDefeated() const {
    return health<=0||(final_&&phase==4&&(health<Groups().blizzard||actor_.callbackCause==behavior::CallbackCause::HealthThreshold));
}
bool Stage2Boss::AdvancePhase() {
    if(!active)return false;
    actor_.health=health;
    if(actor_.finished||(retiring_&&actor_.pendingCallback==cirno::End)) {
        active=false;phase=0;return false;
    }
    if(!actor_.pendingCallback) {
        if(!final_)actor_.pendingCallback=daiyousei::Defeated;
        else {
            constexpr std::array<behavior::Entry,5> entries{cirno::IcicleFall,cirno::SecondNonspell,
                cirno::PerfectFreeze,cirno::DiamondBlizzard,cirno::Defeated};
            if(phase<1||phase>5)throw std::logic_error("Invalid Cirno phase");
            actor_.pendingCallback=entries[phase-1];
        }
    }
    if(!final_||phase==5) { retiring_=true;defeated_=health<=0; }
    else {
        ++phase;const auto& g=Groups();
        // 碰撞发生在行为之后；在本帧原子交接生命与保护，不能让旧零 HP 在下一 Tick 前再次切阶段。
        actor_.health=actor_.maxHealth=phase==2 ? g.firstSpell : phase==3 ? g.second : phase==4 ? g.freeze : g.blizzard;
    }
    actor_.damageable=false;actor_.Transition();health=actor_.health;maxHealth=actor_.maxHealth;phaseTimer=0;
    return true;
}
const char* Stage2Boss::SpellName() const {
    if(!active||!final_||retiring_)return nullptr;
    return phase==2 ? "冰符「Icicle Fall」" : phase==4 ? "冻符「Perfect Freeze」" : phase==5 ? "雪符「Diamond Blizzard」" : nullptr;
}
BossHealthBar Stage2Boss::HealthBar() const {
    if(!active||retiring_)return {};
    if(!final_)return Boss::HealthBar();
    const auto& g=Groups();BossHealthBar bar;
    if(phase<=2) {
        bar.count=2;bar.remainingBars=1;
        const float remaining=std::clamp(health,0.0F,g.first);
        bar.segments[0]={g.firstSpell/g.first,std::min(remaining,g.firstSpell)/g.first,BossHealthKind::Spell};
        bar.segments[1]={(g.first-g.firstSpell)/g.first,std::max(0.0F,remaining-g.firstSpell)/g.first,BossHealthKind::NonSpell};
    } else {
        bar.count=3;bar.remainingBars=0;
        const float remaining=std::clamp(health,0.0F,g.second);
        bar.segments[0]={g.blizzard/g.second,std::min(remaining,g.blizzard)/g.second,BossHealthKind::Spell};
        bar.segments[1]={(g.freeze-g.blizzard)/g.second,std::clamp(remaining-g.blizzard,0.0F,g.freeze-g.blizzard)/g.second,BossHealthKind::Spell};
        bar.segments[2]={(g.second-g.freeze)/g.second,std::max(0.0F,remaining-g.freeze)/g.second,BossHealthKind::NonSpell};
    }
    return bar;
}
}

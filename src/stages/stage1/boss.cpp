// 本文件将带参移动/技能的阶段回调接入露米娅公共生命周期，HP 与计时保持原始单位。
#include "stages/stage1/boss.hpp"
#include "stages/stage1/ids.hpp"
#include "gameplay/data/native_combat.hpp"
#include <algorithm>
namespace koumakyou::stage1 {
namespace {
// 与 Normal Sub14/Sub19 的 ins_111、ins_113 保持一致，单位是原作 HP。
// total 包含后续符卡；spell 是非符结束后保留的血量，同时供伤害阈值与 HUD 使用，不能按伤害倍率加权。
struct HealthGroup { float total, spell; int remainingBars; };
const std::array<HealthGroup,2>& HealthGroups() {
    static const std::array<HealthGroup,2> groups{{
        {static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(1,14,111))),
         static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(1,14,113))),1},
        {static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(1,19,111))),
         static_cast<float>(native_combat::RequireLiteral(native_combat::Setting(1,19,113))),0},
    }};
    return groups;
}
}
void RumiaBoss::StartMidboss() {
    encounter_=Encounter::Midboss; active=true; phase=1; retiring_=defeated_=false;
    presentation.Reset(ActorKind::Boss); visual=kRumia; phaseTimer=0;
    const float initialHealth=native_combat::RequireLiteral(native_combat::Spawn(1,0,1882,9));
    behavior_.Reset({0,0,11.4285717F},initialHealth,1882);
    ConfigureBehavior(behavior_);behavior_.Start(rumia::Midboss(behavior_));
    behavior_.damageable=false;
    position=behavior_.Position(); health=maxHealth=initialHealth;
}
void RumiaBoss::StartFinalBoss() {
    encounter_=Encounter::Final; active=true; phase=1; retiring_=defeated_=false;
    presentation.Reset(ActorKind::Boss); visual=kRumia; phaseTimer=0;
    behavior_.Reset({0,0,5.71428585F},HealthGroups()[0].total,5093);
    ConfigureBehavior(behavior_);behavior_.Start(rumia::FirstNonspell(behavior_));
    behavior_.damageable=false;
    position=behavior_.Position(); health=maxHealth=HealthGroups()[0].total;
}
bool RumiaBoss::Update(float dt, Vector3 player, BulletSystem& bullets) {
    if(!active) return false;
    presentation.Update(dt);
    behavior_.health=health;
    behavior_.Update(dt,player,bullets);
    defeatVisual=static_cast<VisualId>(kNativeDefeat+std::clamp(behavior_.deathEffect,0,2));
    animationScript=behavior_.animationScript;animationAge=behavior_.animationAge;animationRevision=behavior_.animationRevision;
    health=behavior_.health; maxHealth=behavior_.maxHealth;
    position=behavior_.Position(); phaseTimer=behavior_.phaseFrames/60.0F;
    return behavior_.pendingCallback!=nullptr || behavior_.finished;
}
bool RumiaBoss::ShouldAdvanceFromDamage() const {
    // 碰撞与行为共用 120 Hz 战斗步；准备期不能用上次攻击的残余生命再次推进。
    return CanTakeDamage() && (health<=0 || (!IsMidboss() && (phase==1 || phase==3) &&
        health<HealthGroups()[phase==1 ? 0 : 1].spell));
}
bool RumiaBoss::AdvancePhase() {
    if(!active) return false;
    // 玩家碰撞发生在行为 Update 之后；回调必须看到本帧的新生命，而不是上一帧的副本。
    behavior_.health=health;
    if(behavior_.finished || (retiring_ && behavior_.pendingCallback==rumia::End)) {
        active=false; phase=0; return false;
    }
    if(!behavior_.pendingCallback) {
        if(IsMidboss())behavior_.pendingCallback=rumia::MidbossDefeated;
        else if(phase==1) { behavior_.pendingCallback=rumia::NightBird; behavior_.health=HealthGroups()[0].spell; }
        else if(phase==2)behavior_.pendingCallback=rumia::SecondNonspell;
        else if(phase==3) { behavior_.pendingCallback=rumia::Demarcation; behavior_.health=HealthGroups()[1].spell; }
        else behavior_.pendingCallback=rumia::Defeated;
    }
    if(IsMidboss() || phase==4) { defeated_=health<=0; retiring_=true; }
    else ++phase;
    // SecondNonspell 的生命初始化要等下一次 120 Hz Tick 才执行。先原子交接下一组生命，避免击破后的 1 HP
    // 占位在高帧率下跳过非符，也让同一帧的 HUD 立即显示第二组完整白段与粉段。
    if(!retiring_ && phase==3)behavior_.health=behavior_.maxHealth=HealthGroups()[1].total;
    behavior_.SetDeathCallback(IsMidboss() ? rumia::MidbossDefeated : phase<=2 ? rumia::SecondNonspell : rumia::Defeated);
    // 小于 1/120 秒的帧可能还未执行回调首条受伤开关；准备期保护必须在交接时立即生效。
    behavior_.damageable=false;
    behavior_.Transition(); health=behavior_.health;
    // maxHealth 保留当前攻击的战斗上限；HUD 使用独立的整组快照，不能在符卡回调处将预留段铺满。
    if(phase==2 || phase==4)behavior_.maxHealth=behavior_.health;
    maxHealth=behavior_.maxHealth; phaseTimer=0;
    return true;
}
const char* RumiaBoss::SpellName() const {
    if(!active || IsMidboss() || retiring_) return nullptr;
    return phase==2 ? "夜符「Night Bird」" : phase==4 ? "暗符「Demarcation」" : nullptr;
}

BossHealthBar RumiaBoss::HealthBar() const {
    if (!active || retiring_ || phase < 1 || phase > 4) return {};
    if (IsMidboss()) return Boss::HealthBar();
    const auto group = HealthGroups()[phase <= 2 ? 0 : 1];
    const bool spell = phase == 2 || phase == 4;
    const float remaining = std::clamp(health, 0.0F,
                                       spell ? group.spell : group.total);
    BossHealthBar bar;
    bar.count = 2;
    bar.remainingBars = group.remainingBars;
    bar.segments[0] = {group.spell / group.total, std::min(remaining, group.spell) / group.total,
                       BossHealthKind::Spell};
    bar.segments[1] = {(group.total - group.spell) / group.total,
                       std::max(0.0F, remaining - group.spell) / group.total, BossHealthKind::NonSpell};
    return bar;
}
} // namespace koumakyou::stage1

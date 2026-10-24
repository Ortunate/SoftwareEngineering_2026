// 本文件将新典原始脚本的 HP/超时回调接入公共 Boss 生命周期；不重新编写弹幕或移动路径。
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
    script_.Reset(9,{192,-32,0},initialHealth,1882);
    script_.damageable=false;
    position=script_.Position(); health=maxHealth=initialHealth/kReferenceHealthUnit;
}
void RumiaBoss::StartFinalBoss() {
    encounter_=Encounter::Final; active=true; phase=1; retiring_=defeated_=false;
    presentation.Reset(ActorKind::Boss); visual=kRumia; phaseTimer=0;
    script_.Reset(14,{192,96,0},HealthGroups()[0].total,5093);
    script_.damageable=false;
    position=script_.Position(); health=maxHealth=HealthGroups()[0].total/kReferenceHealthUnit;
}
bool RumiaBoss::Update(float dt, Vector3 player, BulletSystem& bullets) {
    if(!active) return false;
    presentation.Update(dt);
    script_.health=health*kReferenceHealthUnit;
    script_.Update(dt,player,bullets);
    defeatVisual=static_cast<VisualId>(kNativeDefeat+std::clamp(script_.deathEffect,0,2));
    animationScript=script_.animationScript;animationAge=script_.animationAge;animationRevision=script_.animationRevision;
    health=script_.health/kReferenceHealthUnit; maxHealth=script_.maxHealth/kReferenceHealthUnit;
    position=script_.Position(); phaseTimer=script_.phaseFrames/60.0F;
    return script_.pendingCallback>=0 || script_.finished;
}
bool RumiaBoss::ShouldAdvanceFromDamage() const {
    // Game 每个渲染帧都检查碰撞，但脚本只在 60 Hz 执行；准备期不能用上次攻击的残余生命再次推进。
    return CanTakeDamage() && (health<=0 || (!IsMidboss() && (phase==1 || phase==3) &&
        health<HealthGroups()[phase==1 ? 0 : 1].spell/kReferenceHealthUnit));
}
bool RumiaBoss::AdvancePhase() {
    if(!active) return false;
    // 玩家碰撞发生在脚本 Update 之后；回调必须看到本帧的新生命，而不是上一帧的副本。
    script_.health=health*kReferenceHealthUnit;
    if(script_.finished || (retiring_ && script_.pendingCallback==21)) {
        active=false; phase=0; return false;
    }
    if(script_.pendingCallback<0) {
        if(IsMidboss())script_.pendingCallback=7;
        else if(phase==1) { script_.pendingCallback=26; script_.health=HealthGroups()[0].spell; }
        else if(phase==2)script_.pendingCallback=19;
        else if(phase==3) { script_.pendingCallback=29; script_.health=HealthGroups()[1].spell; }
        else script_.pendingCallback=20;
    }
    if(IsMidboss() || phase==4) { defeated_=health<=0; retiring_=true; }
    else ++phase;
    // Sub19 的 ins_111 要等下一次 60 Hz Tick 才执行。先原子交接下一组生命，避免击破后的 1 HP
    // 占位在高帧率下跳过非符，也让同一帧的 HUD 立即显示第二组完整白段与粉段。
    if(!retiring_ && phase==3)script_.health=script_.maxHealth=HealthGroups()[1].total;
    script_.SetDeathCallback(IsMidboss() ? 7 : phase<=2 ? 19 : 20);
    // 小于 1/60 秒的帧可能还未执行回调首条 ins_105；准备期保护必须在交接时立即生效。
    script_.damageable=false;
    script_.Transition(); health=script_.health/kReferenceHealthUnit;
    // maxHealth 保留当前攻击的战斗上限；HUD 使用独立的整组快照，不能在符卡回调处将预留段铺满。
    if(phase==2 || phase==4)script_.maxHealth=script_.health;
    maxHealth=script_.maxHealth/kReferenceHealthUnit; phaseTimer=0;
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
    const float remaining = std::clamp(health * kReferenceHealthUnit, 0.0F,
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

// 本文件将新典帧脚本的 HP/超时回调接入 Boss 生命周期，并启用保留 Z 轨迹的三维走位与弹幕。
#include "stages/stage1/boss.hpp"
#include "stages/stage1/ids.hpp"
namespace redmist::stage1 {
void RumiaBoss::StartMidboss() {
    encounter_=Encounter::Midboss; active=true; phase=1; retiring_=defeated_=false;
    presentation.Reset(ActorKind::Boss); visual=kRumia; phaseTimer=0;
    script_.Reset(9,{192,-32,0},6000,1882,false,ReferenceGeometry::Spatial);
    script_.damageable=false;
    position=script_.Position(); health=maxHealth=6000/kReferenceHealthUnit;
}
void RumiaBoss::StartFinalBoss() {
    encounter_=Encounter::Final; active=true; phase=1; retiring_=defeated_=false;
    presentation.Reset(ActorKind::Boss); visual=kRumia; phaseTimer=0;
    script_.Reset(14,{192,96,0},7000,5093,false,ReferenceGeometry::Spatial);
    script_.damageable=false;
    position=script_.Position(); health=maxHealth=7000/kReferenceHealthUnit;
}
bool RumiaBoss::Update(float dt, Vector3 player, BulletSystem& bullets) {
    if(!active) return false;
    presentation.Update(dt);
    script_.health=health*kReferenceHealthUnit;
    script_.Update(dt,player,bullets);
    health=script_.health/kReferenceHealthUnit; maxHealth=script_.maxHealth/kReferenceHealthUnit;
    position=script_.Position(); phaseTimer=script_.phaseFrames/60.0F;
    return script_.pendingCallback>=0 || script_.finished;
}
bool RumiaBoss::ShouldAdvanceFromDamage() const {
    return !retiring_ && (health<=0 || (!IsMidboss() && (phase==1 ? health<900/kReferenceHealthUnit :
                                     phase==3 ? health<800/kReferenceHealthUnit : false)));
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
        else if(phase==1) { script_.pendingCallback=26; script_.health=900; }
        else if(phase==2)script_.pendingCallback=19;
        else if(phase==3) { script_.pendingCallback=29; script_.health=800; }
        else script_.pendingCallback=20;
    }
    if(IsMidboss() || phase==4) { defeated_=health<=0; retiring_=true; }
    else ++phase;
    script_.SetDeathCallback(IsMidboss() ? 7 : phase<=2 ? 19 : 20);
    // 小于 1/60 秒的帧可能还未执行回调首条 ins_105；准备期保护必须在交接时立即生效。
    script_.damageable=false;
    script_.Transition(); health=script_.health/kReferenceHealthUnit;
    // 每张符卡继承前一非符的剩余 HP；HUD 在回调处重新以该阶段生命上限绘制。
    if(phase==2 || phase==4)script_.maxHealth=script_.health;
    maxHealth=script_.maxHealth/kReferenceHealthUnit; phaseTimer=0;
    return true;
}
const char* RumiaBoss::SpellName() const {
    if(!active || IsMidboss() || retiring_) return nullptr;
    return phase==2 ? "夜符「Night Bird」" : phase==4 ? "暗符「Demarcation」" : nullptr;
}
} // namespace redmist::stage1

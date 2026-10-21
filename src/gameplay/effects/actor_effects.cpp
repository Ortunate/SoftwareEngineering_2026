// 本文件负责统一角色入退场时长、缩放透明度曲线及退场快照回收。

#include "gameplay/effects/actor_effects.hpp"

#include <algorithm>

namespace koumakyou {
namespace {
float Smooth(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}
float EntryDuration(ActorKind kind) {
    return kind == ActorKind::Boss ? 0.6F : 0.25F;
}
float DefeatDuration(ActorKind kind) {
    return kind == ActorKind::Boss ? 0.8F : 0.3F;
}
}

void ActorPresentation::Reset(ActorKind kind) {
    kind_ = kind;
    age_ = departure_ = boundaryDeparture_ = 0.0F;
}

void ActorPresentation::Update(float deltaTime) {
    age_ += std::max(0.0F, deltaTime);
    boundaryDeparture_ = 0.0F;
}

float ActorPresentation::ExitDuration(ActorKind kind) {
    return kind == ActorKind::Boss ? 0.5F : 0.25F;
}

void ActorPresentation::PrepareExit(float remainingSeconds) {
    departure_ = std::max(departure_, Smooth(
        1.0F - remainingSeconds / ExitDuration(kind_)));
}

void ActorPresentation::PreviewBoundaryExit(float remainingSeconds) {
    boundaryDeparture_ = std::max(boundaryDeparture_, Smooth(
        1.0F - remainingSeconds / ExitDuration(kind_)));
}

ActorAppearance ActorPresentation::Sample() const {
    const float entry = Smooth(age_ / EntryDuration(kind_));
    const float departure = std::max(departure_, boundaryDeparture_);
    const float visible = entry * (1.0F - departure);
    return {0.65F + 0.35F * visible, visible,
            1.3F + 1.1F * entry - 1.2F * departure,
            std::max(1.0F - entry, departure * (1.0F - departure)) * 0.8F};
}

ActorAppearance ActorExitEffect::Sample() const {
    if(nativeDefeat)return {}; // 透明度和缩放完全由原 ANM 决定。
    const float t = std::clamp(age / duration, 0.0F, 1.0F);
    const float fade = 1.0F - Smooth(t);
    ActorAppearance result = initial;
    result.alpha *= fade;
    result.scale *= 1.0F - 0.4F * Smooth(t);
    result.ringRadius = reason == ActorExitReason::Defeated
        ? 1.0F + 4.0F * t : 1.8F - 1.1F * t;
    result.ringAlpha = fade * (reason == ActorExitReason::Defeated ? 1.0F
        : std::max(initial.ringAlpha, initial.alpha * 0.5F));
    if (reason == ActorExitReason::Defeated) {
        // 即使出生当帧被击破也有反馈；快照只影响绘制，不能恢复任何判定。
        result.alpha = std::max(initial.alpha, 0.4F) * fade;
        result.burst = t;
        result.burstAlpha = fade;
    }
    return result;
}

bool ActorEffectSystem::Spawn(VisualId visual, Vector3 position, float hitRadius,
                             const ActorPresentation& presentation,
                             ActorExitReason reason,const anm::Frame* animation,VisualId defeatVisual) {
    ActorExitEffect* effect = effects_.Acquire();
    if (!effect) return false;
    effect->nativeDefeat=reason==ActorExitReason::Defeated && defeatVisual!=kInvalidVisualId;
    effect->visual = effect->nativeDefeat ? defeatVisual : visual;
    effect->position = position;
    effect->hitRadius = hitRadius;
    effect->kind = presentation.Kind();
    effect->reason = reason;
    effect->initial = presentation.Sample();
    if(animation && !effect->nativeDefeat)effect->animation=*animation;
    effect->duration = reason == ActorExitReason::Defeated
        ? DefeatDuration(effect->kind) : ActorPresentation::ExitDuration(effect->kind);
    if(effect->nativeDefeat)effect->duration=1.0F; // 无删除指令的错误绑定也必须回收；正常 ANM 结束即失活。
    return true;
}

void ActorEffectSystem::Update(float deltaTime) {
    effects_.ForEachActive([deltaTime](ActorExitEffect& effect) {
        effect.age += std::max(0.0F, deltaTime);
        if (effect.age >= effect.duration) effect.active = false;
    });
}

}  // namespace koumakyou

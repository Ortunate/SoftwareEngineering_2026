// 本文件按实例频率管理演员与协程；默认 120 Hz，保持移动/行为/速度顺序及 60 Hz 内容时钟。
#include "gameplay/actors/behavior.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>
namespace koumakyou::behavior {
void Routine::Awaiter::await_suspend(Handle parent) { parent.promise().owner->Attach(child); }
bool AtFrame::await_ready() const { return owner.Frame()>=frame; }
Actor::Actor(UpdateRate rate):updateHz_(static_cast<int>(rate)) {
    if(rate!=UpdateRate::Hz60&&rate!=UpdateRate::Hz120)throw std::invalid_argument("Unsupported behavior update rate");
}
Actor::~Actor() { ClearRoutines(); }
void Actor::ClearRoutines() { for(auto i=stack_.rbegin();i!=stack_.rend();++i)i->destroy();stack_.clear(); }
void Actor::Attach(Routine::Handle handle) {
    if(!handle)throw std::invalid_argument("Empty behavior routine");
    if(stack_.size()>=32) { handle.destroy();throw std::runtime_error("Behavior call depth exceeded"); }
    handle.promise().owner=this;
    try { stack_.push_back(handle); }
    catch(...) { handle.destroy();throw; }
}
void Actor::Reset(Vector3 initialPosition,float hp,std::uint32_t seed,bool mirrored) {
    ClearRoutines();ClearBeamSlots();motion={};skills={};motion.position=initialPosition;motion.mirrored=mirrored;liftedMotion.reset();spatialIndex=spawnIndex=seed ? seed : 1;
    health=maxHealth=hp;lifeThreshold=-1;damageable=interactable=visible=contactEnabled=true;finished=retiring=false;
    rotateAnimation=false;
    emitterShot_={};emitterSkills_={};emitterPeriod_=0;emitterElapsed_=0;emitterConfigured_=false;emitterSound_=-1;
    pendingCallback=lifeCallback=deathCallback=timeoutCallback=nullptr;
    callbackCause=CallbackCause::None;
    phaseFrames=0;timeoutFrames=-1;scheduledTimeout=timeoutArmFrame=-1;deathEffect=0;soundEvents.clear();dropEvents.clear();itemEvents.clear();spawnEvents.clear();defeatActors=false;
    animationScript=-1;animationAge=0;animationRevision=0;animationInterrupt.reset();animationInterruptSequence_=0;poses={-1,-1,-1,-1,-1};pose_=-1;
    random_=seed ? seed : 1;remainingTicks_=0;bullets_=nullptr;
    active=true;phaseClockPaused=false;visual=defeatVisual=kInvalidVisualId;position=origin=Position();age=0;hitRadius=native_sizes::kDefaultEnemyRadius;
    scoreValue=0;sourceItem=-2;presentation.Reset();animation={};entered_=false;retireOutside_=false;retirementHeights_.reset();defeatBase_=kInvalidVisualId;
}
void Actor::Activate(const Spawn& spawn) {
    if(!Valid(spawn))throw std::invalid_argument("Invalid behavior actor spawn");
    Reset(spawn.position,spawn.sourceHealth,spawn.seed,spawn.mirrored);
    skills.bulletBase=spawn.bulletBase;skills.laserBase=spawn.laserBase;skills.fallback=spawn.fallback;
    if(spawn.randomX) {
        const float fraction=static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F;
        motion.position.x=spawn.randomXRange.x+fraction*(spawn.randomXRange.y-spawn.randomXRange.x);
    }
    position=origin=Position();visual=spawn.visual;hitRadius=spawn.hitRadius;
    scoreValue=spawn.sourceScore;sourceItem=spawn.sourceItem;retireOutside_=spawn.retireOutside;defeatBase_=spawn.defeatBase;
    damageable=spawn.damageable;retirementBounds_=spawn.retirementBounds;retirementHeights_=spawn.retirementHeights;
    // 场内出生已满足入场条件；否则一次较大的首帧更新越界后会被误认为尚未入场。
    entered_=retireOutside_&&retirementBounds_.Contains(motion.position)&&
        (!retirementHeights_||(motion.position.y>=retirementHeights_->x&&motion.position.y<=retirementHeights_->y));
    deathCallback=spawn.pattern.onDefeated;
    rotateAnimation=spawn.pattern.rotateAnimation;
    auto pattern=spawn.pattern;if(spawn.initialAnimation>=0)pattern.initialAnimation=spawn.initialAnimation;
    Start(Follow(*this,pattern));
}
void Actor::Start(Routine routine) { ClearRoutines();Attach(routine.Release()); }
void Actor::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("Invalid behavior delta time");
    soundEvents.clear();dropEvents.clear();itemEvents.clear();spawnEvents.clear();defeatActors=false;player_=player;bullets_=&bullets;
    age+=seconds;presentation.Update(seconds);
    remainingTicks_+=static_cast<double>(seconds)*updateHz_;
    while(remainingTicks_+0.00001>=1&&!finished&&!pendingCallback) { remainingTicks_-=1;Tick(); }
    bullets_=nullptr;
    position=Position();
    if(defeatBase_!=kInvalidVisualId)defeatVisual=static_cast<VisualId>(defeatBase_+std::clamp(deathEffect,0,2));
    // 场外出生缓冲带不能在第一帧回收；入场后才按同一世界边界判断退场。
    if(retireOutside_) {
        const bool inside=retirementBounds_.Contains(motion.position)&&
            (!retirementHeights_||(motion.position.y>=retirementHeights_->x&&motion.position.y<=retirementHeights_->y));
        if(inside)entered_=true;
        if(entered_&&!inside)active=false;
    }
    if(finished)active=false;
}
void Actor::Transition() {
    if(!pendingCallback)return;
    const auto next=std::exchange(pendingCallback,nullptr);
    callbackCause=CallbackCause::None;
    if(health<=0)health=1;
    timeoutCallback=deathCallback;lifeThreshold=-1;timeoutFrames=-1;scheduledTimeout=timeoutArmFrame=-1;phaseFrames=0;
    Start(next(*this));
}
bool Actor::Defeat() {
    if(retiring||!active)return false;
    retiring=true;damageable=false;
    if(deathCallback) { pendingCallback=deathCallback;Transition(); }
    else active=false;
    return true;
}
void Actor::FireAt(const skill::Shot& shot,Vector3 position,const skill::State* configuration) {
    if(!bullets_)throw std::logic_error("Fire requires a running behavior update");
    (configuration ? *configuration : skills).Fire(shot,position,player_,*bullets_,[this](float range){return Random(range);});
    if((shot.flags&skill::PlaySound)&&emitterSound_>=0)soundEvents.push_back(emitterSound_);
}
void Actor::FireBeam(int slot,const skill::Laser& laser) {
    if(!bullets_)throw std::logic_error("Beam creation requires a running behavior update");
    if(slot<0||slot>=static_cast<int>(beams_.size()))throw std::invalid_argument("Beam slot outside actor capacity");
    beams_[slot]=bullets_->MakeHandle(skills.Fire(laser,motion.position,player_,*bullets_));
}
Bullet* Actor::Beam(int slot) {
    if(!bullets_)throw std::logic_error("Beam access requires a running behavior update");
    if(slot<0||slot>=static_cast<int>(beams_.size()))throw std::invalid_argument("Beam slot outside actor capacity");
    return bullets_->Resolve(beams_[slot]);
}
void Actor::ConfigureEmitter(const skill::Shot& shot) {
    if(!skill::Valid({0,shot}))throw std::invalid_argument("Invalid emitter shot");
    emitterShot_=shot;emitterSkills_=skills;emitterConfigured_=true;
}
void Actor::SetEmitterPeriod(int period,int initial) {
    if(period<0||initial<0||(period>0&&(!emitterConfigured_||initial>=period))||(period==0&&initial!=0))
        throw std::invalid_argument("Invalid emitter period or initial counter");
    emitterPeriod_=period;emitterElapsed_=initial;
}
void Actor::FireEmitterAt(Vector3 worldMuzzle) {
    if(!emitterConfigured_)throw std::logic_error("Emitter has no shot configuration");
    auto configuration=emitterSkills_;configuration.offset={};FireAt(emitterShot_,worldMuzzle,&configuration);
}
void Actor::SetEmitterSound(int sound) {
    // 原生关闭操作保留音效身份；下一轮显式带 PlaySound 的发射仍可再次使用它。
    if(sound>=0) {emitterSound_=sound;emitterShot_.flags|=skill::PlaySound;}
    else emitterShot_.flags&=~skill::PlaySound;
}
void Actor::ModifyEnemyProjectiles(const std::function<void(Bullet&)>& visitor) {
    if(!bullets_)throw std::logic_error("Projectile mutation requires a running behavior update");
    bullets_->Pool().ForEachActive([&](Bullet& bullet) {
        if(bullet.owner==BulletOwner::Enemy&&!bullet.IsBeam())visitor(bullet);
    });
}
float Actor::Random(float range) {
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return static_cast<float>(random_>>8)/16777216.0F*range;
}
std::uint32_t Actor::RandomIndex(std::uint32_t bound) {
    if(!bound)throw std::invalid_argument("Random index bound must be positive");
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return random_%bound;
}
void Actor::RandomDirection(float minimum,float maximum,bool reflect) {
    auto& path=liftedMotion ? liftedMotion->planar : motion;
    constexpr float pi=std::numbers::pi_v<float>;
    float angle=Random(maximum-minimum)+minimum;
    if(reflect) {
        if(path.position.x<path.bounds.lower.x+path.reflectionMargin.x&&std::abs(angle)>pi/2)angle=std::copysign(pi,angle)-angle;
        if(path.position.x>path.bounds.upper.x-path.reflectionMargin.x&&std::abs(angle)<pi/2)angle=std::copysign(pi,angle)-angle;
        if(path.position.z>path.bounds.upper.y-path.reflectionMargin.y&&angle<0)angle=-angle;
        if(path.position.z<path.bounds.lower.y+path.reflectionMargin.y&&angle>0)angle=-angle;
    }
    path.Polar(angle,path.speed);
}
void Actor::Animate(int script) { animationScript=script;animationAge=0;++animationRevision;animationInterrupt.reset(); }
void Actor::InterruptAnimation(int label) {
    if(animationScript<0)throw std::logic_error("Animation interrupt requires an active script");
    animationInterrupt=battle::AnimationInterrupt{animationAge,label,animationRevision,++animationInterruptSequence_};
}
void Actor::ClearBullets() { bullets_->ClearEnemyBullets();skills.rankInfluence=skill::kDefaultRankInfluence; }
void Actor::ScheduleTimeout(int frames,int armFrame) {
    if(frames< -1||armFrame<0)throw std::invalid_argument("Invalid scheduled timeout");
    scheduledTimeout=frames;timeoutArmFrame=armFrame;
    timeoutFrames=armFrame==0 ? frames : -1;
}
void Actor::Tick() {
    if(timeoutArmFrame>=0&&phaseFrames>=timeoutArmFrame) {
        timeoutFrames=scheduledTimeout;timeoutArmFrame=-1;
    }
    if(liftedMotion)liftedMotion->BeginFrame(motion,updateHz_);else motion.BeginFrame(updateHz_);
    if(health<lifeThreshold) { health=lifeThreshold;pendingCallback=lifeCallback;callbackCause=CallbackCause::HealthThreshold;return; }
    if(health<=0&&deathCallback) { pendingCallback=deathCallback;callbackCause=CallbackCause::Defeat;return; }
    if(timeoutFrames>=0&&phaseFrames>=timeoutFrames) {
        if(lifeThreshold>0)health=lifeThreshold;
        pendingCallback=timeoutCallback;callbackCause=CallbackCause::Timeout;return;
    }
    // 子行为返回后父行为必须同帧恢复；仅当前最深层行为在帧末增加局部时间。
    int budget=2048;
    while(!stack_.empty()&&!finished&&budget-->0) {
        auto current=stack_.back();
        if(current.done()) {
            const auto failure=current.promise().failure;
            current.destroy();stack_.pop_back();
            if(failure)std::rethrow_exception(failure);
            continue;
        }
        if(current.promise().due>current.promise().frame)break;
        current.resume();
    }
    if(budget<=0)throw std::runtime_error("Behavior resume budget exhausted");
    // 周期计数独立于最深协程时钟：子调用和回拨不能停止或重置自动发射。
    // 本步行为先更新配方/开关，再递增内容计数并发弹，保留源周期的首发边界与 120 Hz 半帧。
    if(emitterPeriod_>0&&!finished&&!pendingCallback) {
        emitterElapsed_+=60.0/updateHz_;
        while(emitterElapsed_>=emitterPeriod_) {
            emitterElapsed_-=emitterPeriod_;
            FireEmitterAt({motion.position.x+emitterSkills_.offset.x,motion.position.y+emitterSkills_.offset.y,motion.position.z+emitterSkills_.offset.z});
        }
    }
    if(liftedMotion)liftedMotion->EndFrame(motion,updateHz_);else motion.EndFrame(updateHz_);
    if(poses[3]>=0) {
        const float horizontal=motion.velocity.x*(motion.mirrored ? -1 : 1);
        const int next=horizontal<0 ? -2 : horizontal>0 ? 2 : 0;
        if(next!=pose_) {
            Animate(next<0 ? poses[3] : next>0 ? poses[4] : pose_==-2 ? poses[1] : pose_==2 ? poses[2] : poses[0]);
            pose_=next;
        }
    }
    // 内容仍以 60 Hz 帧编号；提高更新频率只细分时间，不使技能、子行为或阶段超时加速。
    const double contentFrames=60.0/updateHz_;
    animationAge+=1.0/updateHz_;
    if(!stack_.empty())stack_.back().promise().frame+=contentFrames;
    if(!phaseClockPaused)phaseFrames+=contentFrames;
}
Routine Follow(Actor& actor,Pattern pattern) {
    actor.skills.spatial=pattern.spatialShots;
    actor.motion.Polar(pattern.movement.angle,pattern.movement.speed);
    actor.Animate(pattern.initialAnimation);actor.deathEffect=pattern.deathEffect;actor.skills.offset=pattern.muzzleOffset;
    if(pattern.routine) { co_await pattern.routine(actor);co_return; }
    int movement=0;
    bool fired=!pattern.skill,animated=pattern.stoppedAnimation<0;
    while(movement<pattern.movement.count||!fired||!animated) {
        int next=pattern.endFrame;
        if(movement<pattern.movement.count)next=std::min(next,pattern.movement.keys[movement].frame);
        if(!fired)next=std::min(next,pattern.skill->frame);
        if(!animated)next=std::min(next,pattern.stoppedAnimationFrame);
        co_await actor.At(next);
        if(!animated&&next==pattern.stoppedAnimationFrame) { actor.Animate(pattern.stoppedAnimation);animated=true; }
        while(movement<pattern.movement.count&&pattern.movement.keys[movement].frame==next)actor.motion.Apply(pattern.movement.keys[movement++]);
        if(!fired&&pattern.skill->frame==next) { actor.Fire(pattern.skill->shot);fired=true; }
    }
    co_await actor.At(pattern.endFrame);actor.finished=true;
}
bool Valid(const Pattern& pattern) {
    if(pattern.routine&&(pattern.movement.count!=0||pattern.skill||pattern.stoppedAnimation>=0))return false;
    if(!movement::Valid(pattern.movement)||pattern.endFrame<0||
       (pattern.movement.count>0&&pattern.movement.keys[pattern.movement.count-1].frame>pattern.endFrame))return false;
    return (!pattern.skill||(skill::Valid(*pattern.skill)&&pattern.skill->frame<=pattern.endFrame))&&
        pattern.initialAnimation>=0&&pattern.stoppedAnimation>=-1&&pattern.stoppedAnimationFrame>=0&&
        (pattern.stoppedAnimation<0||pattern.stoppedAnimationFrame<=pattern.endFrame)&&
        pattern.deathEffect>=0&&pattern.deathEffect<=2&&
        std::isfinite(pattern.muzzleOffset.x)&&std::isfinite(pattern.muzzleOffset.y)&&std::isfinite(pattern.muzzleOffset.z);
}
bool Valid(const Spawn& spawn) {
    const auto& bounds=spawn.retirementBounds;
    return (!spawn.retirementHeights||(std::isfinite(spawn.retirementHeights->x)&&std::isfinite(spawn.retirementHeights->y)&&
        spawn.retirementHeights->x<=spawn.retirementHeights->y))&&
        Valid(spawn.pattern)&&spawn.initialAnimation>=-1&&std::isfinite(spawn.position.x)&&std::isfinite(spawn.position.y)&&std::isfinite(spawn.position.z)&&
        std::isfinite(spawn.sourceHealth)&&spawn.sourceHealth>0&&spawn.sourceScore>=0&&
        spawn.sourceItem>=-2&&spawn.sourceItem<=2&&spawn.visual!=kInvalidVisualId&&
        std::isfinite(spawn.hitRadius)&&spawn.hitRadius>0&&
        std::isfinite(spawn.randomXRange.x)&&std::isfinite(spawn.randomXRange.y)&&spawn.randomXRange.x<=spawn.randomXRange.y&&
        std::isfinite(spawn.randomXRange.y-spawn.randomXRange.x)&&
        std::isfinite(bounds.lower.x)&&std::isfinite(bounds.lower.y)&&std::isfinite(bounds.upper.x)&&std::isfinite(bounds.upper.y)&&
        bounds.lower.x<=bounds.upper.x&&bounds.lower.y<=bounds.upper.y;
}
} // namespace koumakyou::behavior

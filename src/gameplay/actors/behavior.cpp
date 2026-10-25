// 本文件直接管理行为演员的出生、协程、战斗/视觉状态与回收，保持 60 Hz 的移动/行为/速度更新顺序。
#include "gameplay/actors/behavior.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>
namespace koumakyou::behavior {
void Routine::Awaiter::await_suspend(Handle parent) { parent.promise().owner->Attach(child); }
bool AtFrame::await_ready() const { return owner.Frame()>=frame; }
Actor::~Actor() { ClearRoutines(); }
void Actor::ClearRoutines() { for(auto i=stack_.rbegin();i!=stack_.rend();++i)i->destroy();stack_.clear(); }
void Actor::Attach(Routine::Handle handle) {
    if(!handle)throw std::invalid_argument("Empty behavior routine");
    if(stack_.size()>=32) { handle.destroy();throw std::runtime_error("Behavior call depth exceeded"); }
    handle.promise().owner=this;
    try { stack_.push_back(handle); }
    catch(...) { handle.destroy();throw; }
}
void Actor::Reset(Vector3 source,float hp,std::uint32_t seed,bool mirrored) {
    ClearRoutines();motion={};skills={};motion.position=source;motion.mirrored=mirrored;
    health=maxHealth=hp;lifeThreshold=-1;damageable=true;finished=false;
    pendingCallback=lifeCallback=deathCallback=timeoutCallback=nullptr;
    phaseFrames=0;timeoutFrames=-1;deathEffect=0;soundEvents.clear();dropEvents.clear();
    animationScript=-1;animationAge=0;animationRevision=0;poses={-1,-1,-1,-1,-1};pose_=-1;
    random_=seed ? seed : 1;remainingFrames_=0;bullets_=nullptr;
    active=true;visual=defeatVisual=kInvalidVisualId;position=origin=Position();age=0;hitRadius=0.55F;
    scoreValue=0;sourceItem=-2;presentation.Reset();animation={};entered_=false;retireOutside_=false;defeatBase_=kInvalidVisualId;
}
void Actor::Activate(const Spawn& spawn) {
    if(!Valid(spawn))throw std::invalid_argument("Invalid behavior actor spawn");
    Reset(spawn.space.Source(spawn.position),spawn.sourceHealth,spawn.seed,spawn.mirrored);
    skills.space=spawn.space;skills.bulletBase=spawn.bulletBase;skills.laserBase=spawn.laserBase;skills.fallback=spawn.fallback;
    if(spawn.randomX)motion.position.x=static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F*384;
    position=origin=Position();visual=spawn.visual;hitRadius=spawn.hitRadius;
    scoreValue=spawn.sourceScore;sourceItem=spawn.sourceItem;retireOutside_=spawn.retireOutside;defeatBase_=spawn.defeatBase;
    damageable=spawn.damageable;
    // 场内出生已满足入场条件；否则一次较大的首帧更新越界后会被误认为尚未入场。
    entered_=retireOutside_&&motion.position.x>=-16&&motion.position.x<=400&&motion.position.y>=-16&&motion.position.y<=464;
    Start(Follow(*this,spawn.pattern));
}
void Actor::Start(Routine routine) { ClearRoutines();Attach(routine.Release()); }
void Actor::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("Invalid behavior delta time");
    soundEvents.clear();dropEvents.clear();player_=player;bullets_=&bullets;
    age+=seconds;presentation.Update(seconds);
    remainingFrames_+=static_cast<double>(seconds)*60;
    while(remainingFrames_+0.00001>=1&&!finished&&!pendingCallback) { remainingFrames_-=1;Tick(); }
    bullets_=nullptr;
    position=Position();
    if(defeatBase_!=kInvalidVisualId)defeatVisual=static_cast<VisualId>(defeatBase_+std::clamp(deathEffect,0,2));
    // 场外出生缓冲带不能在第一帧回收；入场后才允许按原逻辑像素边界退场。
    const auto p=motion.position;
    if(retireOutside_) {
        if(p.x>=-16&&p.x<=400&&p.y>=-16&&p.y<=464)entered_=true;
        if(entered_&&(p.x<-16||p.x>400||p.y<-16||p.y>464))active=false;
    }
    if(finished)active=false;
}
void Actor::Transition() {
    if(!pendingCallback)return;
    const auto next=std::exchange(pendingCallback,nullptr);
    if(health<=0)health=1;
    timeoutCallback=deathCallback;lifeThreshold=-1;timeoutFrames=-1;phaseFrames=0;
    Start(next(*this));
}
float Actor::Random(float range) {
    random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
    return static_cast<float>(random_>>8)/16777216.0F*range;
}
void Actor::RandomDirection(float minimum,float maximum,bool reflect) {
    constexpr float pi=std::numbers::pi_v<float>;
    float angle=Random(maximum-minimum)+minimum;
    if(reflect) {
        if(motion.position.x<motion.lower.x+96&&std::abs(angle)>pi/2)angle=std::copysign(pi,angle)-angle;
        if(motion.position.x>motion.upper.x-96&&std::abs(angle)<pi/2)angle=std::copysign(pi,angle)-angle;
        if(motion.position.y<motion.lower.y+48&&angle<0)angle=-angle;
        if(motion.position.y>motion.upper.y-48&&angle>0)angle=-angle;
    }
    motion.Polar(angle,motion.speed);
}
void Actor::Animate(int script) { animationScript=script;animationAge=0;++animationRevision; }
void Actor::ClearBullets() { bullets_->ClearEnemyBullets();skills.rankInfluence={-0.5F,0.5F,0,0,0,0}; }
void Actor::Tick() {
    motion.BeginFrame();
    if(health<lifeThreshold) { health=lifeThreshold;pendingCallback=lifeCallback;return; }
    if(health<=0&&deathCallback) { pendingCallback=deathCallback;return; }
    if(timeoutFrames>=0&&phaseFrames>=timeoutFrames) {
        if(lifeThreshold>0)health=lifeThreshold;
        pendingCallback=timeoutCallback;return;
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
    motion.EndFrame();
    if(poses[3]>=0) {
        const float horizontal=motion.velocity.x*(motion.mirrored ? -1 : 1);
        const int next=horizontal<0 ? -2 : horizontal>0 ? 2 : 0;
        if(next!=pose_) {
            Animate(next<0 ? poses[3] : next>0 ? poses[4] : pose_==-2 ? poses[1] : pose_==2 ? poses[2] : poses[0]);
            pose_=next;
        }
    }
    animationAge+=1.0/60;
    if(!stack_.empty())++stack_.back().promise().frame;
    ++phaseFrames;
}
Routine Follow(Actor& actor,Pattern pattern) {
    actor.motion.Polar(pattern.movement.angle,pattern.movement.speed);
    actor.Animate(pattern.initialAnimation);actor.deathEffect=pattern.deathEffect;actor.skills.offset=pattern.muzzleOffset;
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
    if(!movement::Valid(pattern.movement)||pattern.endFrame<0||
       (pattern.movement.count>0&&pattern.movement.keys[pattern.movement.count-1].frame>pattern.endFrame))return false;
    return (!pattern.skill||(skill::Valid(*pattern.skill)&&pattern.skill->frame<=pattern.endFrame))&&
        pattern.initialAnimation>=0&&pattern.stoppedAnimation>=-1&&pattern.stoppedAnimationFrame>=0&&
        (pattern.stoppedAnimation<0||pattern.stoppedAnimationFrame<=pattern.endFrame)&&
        pattern.deathEffect>=0&&pattern.deathEffect<=2&&
        std::isfinite(pattern.muzzleOffset.x)&&std::isfinite(pattern.muzzleOffset.y)&&std::isfinite(pattern.muzzleOffset.z);
}
bool Valid(const Spawn& spawn) {
    return Valid(spawn.pattern)&&std::isfinite(spawn.position.x)&&std::isfinite(spawn.position.y)&&std::isfinite(spawn.position.z)&&
        std::isfinite(spawn.sourceHealth)&&spawn.sourceHealth>0&&spawn.sourceScore>=0&&
        spawn.sourceItem>=-2&&spawn.sourceItem<=2&&spawn.visual!=kInvalidVisualId&&
        std::isfinite(spawn.hitRadius)&&spawn.hitRadius>0&&std::isfinite(spawn.space.scale)&&spawn.space.scale>0;
}
} // namespace koumakyou::behavior

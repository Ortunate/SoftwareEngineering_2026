// 本文件解析角色技能和阶段血条配置，负责技能交接、Boss 自身血条绘制、对白入口与完整退场。
#include "gameplay/bosses/configured_boss.hpp"
#include <algorithm>
#include <cmath>
namespace koumakyou::bosses {
behavior::Routine SkillComplete(behavior::Actor&) {co_return;}
namespace {
void Validate(const StartConfig& c) {
    if(native_combat::DifficultyIndex(c.difficulty)<0||c.phases.empty()||!c.configure||!c.defeated||!c.retreat||static_cast<unsigned>(c.shot)>3||
       !std::isfinite(c.hitRadius)||c.hitRadius<=0||!std::isfinite(c.position.x)||!std::isfinite(c.position.y)||!std::isfinite(c.position.z))
        throw std::invalid_argument("Invalid Boss start configuration");
    for(const auto entry:c.resumeSteps)if(!entry||!c.intro)throw std::invalid_argument("Invalid Boss resume step");
    for(const auto& p:c.phases) {
        if(p.skills.empty()||p.healthBar.segments.size()!=static_cast<std::size_t>(std::ranges::count_if(p.skills,[](const SkillSlot& slot){return !slot.omitHealthBarSegment;})))throw std::invalid_argument("Invalid Boss phase composition");
        std::vector<bool> used(p.skills.size());double sum=0;
        for(const auto& segment:p.healthBar.segments) {
            if(segment.skillIndex>=p.skills.size()||p.skills[segment.skillIndex].omitHealthBarSegment||used[segment.skillIndex]||!std::isfinite(segment.ratio)||segment.ratio<=0||segment.ratio>1)
                throw std::invalid_argument("Invalid Boss health bar segment");
            used[segment.skillIndex]=true;sum+=segment.ratio;
        }
        if(std::abs(sum-1)>1e-5)throw std::invalid_argument("Boss health bar ratios must sum to one");
        for(const auto& slot:p.skills) {
            if(!slot.id)throw std::invalid_argument("Missing Boss skill");
            for(const auto transition:{slot.onDamage,slot.onTimeout,slot.onDefeat.value_or(slot.onDamage)})switch(transition.kind) {
                case Transition::Kind::Next:case Transition::Kind::Exit:case Transition::Kind::ExitDefeated:break;
                case Transition::Kind::Jump:
                    if(transition.phase>=c.phases.size()||transition.skill>=c.phases[transition.phase].skills.size())
                        throw std::invalid_argument("Invalid Boss skill transition");
                    break;
                default:throw std::invalid_argument("Invalid Boss transition kind");
            }
        }
    }
}
void Validate(const SkillDefinition& p) {
    if(p.bombRecoveryPoses&&std::ranges::any_of(*p.bombRecoveryPoses,[](int script){return script< -1;}))
        throw std::invalid_argument("Invalid Boss Bomb recovery poses");
    if(!p.behavior||!std::isfinite(p.health)||p.health<0||(p.health==0&&p.reserveHealth<=0)||!std::isfinite(p.reserveHealth)||p.reserveHealth<0||
       !std::isfinite(p.NativeHealth())||!std::isfinite(p.entranceHealth)||p.entranceHealth<0||
       p.timeoutFrames< -1||p.timeoutArmFrame<0||!std::isfinite(p.initialFrame)||p.initialFrame<0||
       !std::isfinite(p.hitRadius)||p.hitRadius<0||p.spellRevealFrame<0||p.bombReactionFrame<0)
        throw std::invalid_argument("Invalid Boss skill definition");
}
}
ConfiguredBoss::~ConfiguredBoss() {actor_.Reset({},1,1);}
void ConfiguredBoss::StartBoss(const StartConfig& config) {
    Validate(config);auto copy=config;std::vector<ResolvedPhase> parsed;
    // 在更换旧状态前完成引用解析；外部容器、机体参数和血条颜色后续变化都不能污染活动战斗。
    for(const auto& p:copy.phases) {
        ResolvedPhase phase;
        for(const auto& slot:p.skills) {auto definition=ResolveSkillForDifficulty(slot.id,copy.shot,copy.difficulty);Validate(definition);phase.skills.push_back(std::move(definition));}
        parsed.push_back(std::move(phase));
    }
    auto intro=copy.intro ? ResolveBehavior(copy.intro) : Attack{};
    auto defeated=ResolveBehavior(copy.defeated),retreat=ResolveBehavior(copy.retreat);std::vector<Attack> resume;
    for(const auto id:copy.resumeSteps)resume.push_back(ResolveBehavior(id));
    if(!defeated||!retreat||(copy.intro&&!intro)||std::ranges::any_of(resume,[](const auto& a){return !a;}))
        throw std::invalid_argument("Missing Boss behavior implementation");
    const auto& first=parsed.front().skills.front();const float hp=first.entranceHealth>0 ? first.entranceHealth : first.NativeHealth();
    actor_.Reset(copy.position,hp,copy.seed); // 旧协程先于旧技能闭包销毁，保留借用期。
    config_=std::move(copy);resolved_=std::move(parsed);intro_=std::move(intro);defeat_=std::move(defeated);retreat_=std::move(retreat);resumeSteps_=std::move(resume);
    // 子演员可显式继承角色外观；Actor与Boss共用当前关卡内的VisualId借用期。
    actor_.difficulty=config_.difficulty;actor_.visual=config_.visual;
    config_.configure(actor_,spatial_);active=true;phase=skill=attack=0;waiting_=static_cast<bool>(intro_);retiring_=defeated_=retirementTimedOut_=false;notifications_=0;
    stage_=skill_=0;presentation.Reset(ActorKind::Boss);animation={};animationScript=-1;animationAge=0;animationRevision=0;
    skillTimer=0;skillElapsed_=0;visual=config_.visual;hitRadius=actor_.hitRadius=config_.hitRadius;
    actor_.damageable=config_.initialDamageable;actor_.contactEnabled=config_.initialContact;
    if(waiting_)actor_.Start(intro_(actor_,*this));else {StartSkill(0,0);actor_.damageable=config_.initialDamageable;}
    Sync();
}
const SkillDefinition* ConfiguredBoss::CurrentSkill() const {
    return active&&!waiting_&&!retiring_ ? &resolved_.at(stage_).skills.at(skill_) : nullptr;
}
void ConfiguredBoss::StartSkill(std::size_t stage,std::size_t index) {
    stage_=stage;skill_=index;phase=static_cast<int>(stage+1);skill=static_cast<int>(index+1);attack=skill;
    for(std::size_t i=0;i<stage;++i)attack+=static_cast<int>(resolved_[i].skills.size());
    const auto& p=resolved_[stage].skills[index];
    // 复用 Transition 清除旧回调并保留尚未消费的固定步，不重置运动、随机流或入场动画。
    actor_.pendingCallback=SkillComplete;actor_.Transition();actor_.Start(p.behavior(actor_,*this));
    actor_.health=actor_.maxHealth=p.NativeHealth();actor_.lifeThreshold=p.reserveHealth>0 ? p.reserveHealth : -1;
    actor_.lifeCallback=actor_.deathCallback=actor_.timeoutCallback=SkillComplete;
    actor_.ScheduleTimeout(p.timeoutFrames,p.timeoutArmFrame);actor_.phaseFrames=p.initialFrame;actor_.phaseClockPaused=false;
    actor_.interactable=true;actor_.damageable=false;actor_.finished=false;
    if(p.hitRadius>0)actor_.hitRadius=p.hitRadius;
    skillElapsed_=0;skillTimer=0;Sync(); // 当帧碰撞前必须交接生命与准备期保护。
}
void ConfiguredBoss::ResumeCombat() {
    if(!active||!waiting_)throw std::logic_error("Boss is not awaiting dialogue notification");
    if(notifications_<resumeSteps_.size()) {
        actor_.pendingCallback=SkillComplete;actor_.Transition();actor_.Start(resumeSteps_[notifications_++](actor_,*this));actor_.damageable=false;return;
    }
    waiting_=false;StartSkill(0,0);
}
void ConfiguredBoss::Sync() {
    health=actor_.health;maxHealth=actor_.maxHealth;position=actor_.Position();hitRadius=actor_.hitRadius;
    skillTimer=static_cast<float>(actor_.phaseFrames/60);animationScript=actor_.animationScript;animationAge=actor_.animationAge;animationRevision=actor_.animationRevision;
    if(config_.defeatBase!=kInvalidVisualId)defeatVisual=static_cast<VisualId>(config_.defeatBase+std::clamp(actor_.deathEffect,0,2));
}
bool ConfiguredBoss::Update(float seconds,Vector3 player,BulletSystem& bullets) {
    if(!active)return false;
    presentation.Update(seconds);skillElapsed_+=seconds;actor_.health=health;actor_.Update(seconds,player,bullets);health=actor_.health;
    AfterUpdate(seconds);Sync();return actor_.pendingCallback||actor_.finished;
}
bool ConfiguredBoss::ShouldAdvanceFromDamage() const {
    const auto* p=CurrentSkill();return p&&CanTakeDamage()&&(health<=0||(p->reserveHealth>0&&health<p->reserveHealth));
}
bool ConfiguredBoss::SkillDefeated() const {
    const auto* p=CurrentSkill();return health<=0||(p&&p->thresholdDefeatsSpell&&
        (health<p->reserveHealth||actor_.callbackCause==behavior::CallbackCause::HealthThreshold));
}
bool ConfiguredBoss::SpellSucceeded() const {
    const auto* p=CurrentSkill();
    return p&&p->survival ? actor_.callbackCause==behavior::CallbackCause::Timeout : SkillDefeated();
}
bool ConfiguredBoss::ClearsBulletsOnSkillEnd() const {
    const auto* p=CurrentSkill();
    return !(p&&p->survival&&actor_.callbackCause==behavior::CallbackCause::Timeout);
}
bool ConfiguredBoss::AdvanceSkill() {
    if(!active)return false;
    if(waiting_)throw std::logic_error("Cannot advance Boss before dialogue notification");
    actor_.health=health;
    if(retiring_||actor_.finished) {active=false;phase=skill=attack=0;return false;}
    const bool timedOut=actor_.callbackCause==behavior::CallbackCause::Timeout;
    // 碰撞可能在下一次Actor::Tick前交接；生命归零/超时释放附着，正生命预留阈值继续跟随。
    if(timedOut||health<=0||actor_.callbackCause==behavior::CallbackCause::Defeat)actor_.ReleaseAttachedEffects();
    const auto& slot=config_.phases[stage_].skills[skill_];
    // 原生命归零可跳过当前预留符卡；普通阈值与超时仍进入各自后继，不能统一成一个伤害出口。
    const auto next=timedOut ? slot.onTimeout : (health<=0||actor_.callbackCause==behavior::CallbackCause::Defeat) ? slot.onDefeat.value_or(slot.onDamage) : slot.onDamage;
    std::size_t stage=stage_,index=skill_+1;
    if(next.kind==Transition::Kind::Jump) {stage=next.phase;index=next.skill;}
    else if(index>=resolved_[stage].skills.size()) {++stage;index=0;}
    if(next.kind!=Transition::Kind::Exit&&next.kind!=Transition::Kind::ExitDefeated&&stage<resolved_.size()) {StartSkill(stage,index);return true;}
    // 最后一张可在正生命寄存器阈值处完成；击破依据技能实际剩余 HP，不能只检查原寄存器是否归零。
    const auto& definition=resolved_[stage_].skills[skill_];
    defeated_=next.kind==Transition::Kind::ExitDefeated||
        (!timedOut&&(health<=definition.reserveHealth||actor_.callbackCause==behavior::CallbackCause::HealthThreshold));
    retiring_=true;retirementTimedOut_=timedOut;actor_.phaseClockPaused=false;
    actor_.pendingCallback=SkillComplete;actor_.Transition();actor_.Start((defeated_ ? defeat_ : retreat_)(actor_,*this));
    actor_.damageable=false;actor_.contactEnabled=false;actor_.lifeThreshold=-1;actor_.timeoutFrames=-1;Sync();return true;
}
const char* ConfiguredBoss::SpellName() const {
    const auto* p=CurrentSkill();return p&&skillElapsed_*60+1e-4>=p->spellRevealFrame&&(!p->spellNeedsTimer||actor_.timeoutFrames>0) ? p->spellName : nullptr;
}
int ConfiguredBoss::SpellCardNumber() const {const auto* p=CurrentSkill();return SpellName()&&p ? p->spellCard : 0;}
float ConfiguredBoss::TimeLimit() const {return CurrentSkill() ? actor_.timeoutFrames/60.0F : -1;}
BossHealthBar ConfiguredBoss::HealthBar() const {
    if(!CurrentSkill())return {};
    BossHealthBar result;result.remainingBars=static_cast<int>(resolved_.size()-stage_-1);
    const auto& p=config_.phases[stage_];result.background=p.healthBar.background;
    for(const auto& segment:p.healthBar.segments) {
        const auto& definition=resolved_[stage_].skills[segment.skillIndex];
        const float fill=segment.skillIndex<skill_||definition.health==0 ? 0 : segment.skillIndex>skill_ ? 1 :
            std::clamp((health-definition.reserveHealth)/definition.health,0.0F,1.0F);
        result.segments.push_back({segment.ratio,segment.ratio*fill,segment.color});
    }
    result.count=result.segments.size();return result;
}
void ConfiguredBoss::DrawHealthBar(Rectangle area) const {DrawBossHealthBar(HealthBar(),area);}
} // namespace koumakyou::bosses

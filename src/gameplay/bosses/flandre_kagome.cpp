// 本文件实现芙兰朵露Kagome Kagome的大小弹邻域解放；世界单位计算与角色本体编排分离。
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <cmath>
#include <numbers>

namespace koumakyou::bosses {
namespace {
// 本符只生成原生普通弹；nativeType保证有对应ANM，尺寸取其主体精灵，而非独立显现特效的缩放。
// EnemyBulletLogicalSize已用真实ANM逐色核验，30像素的等号归大弹；不能按碰撞半径或type9特判。
bool CageProjectile(const Bullet& bullet) {
    return !bullet.pendingExpiration&&bullet.nativeType>=0&&bullet.nativeType<=9;
}
constexpr float kLargeWidth=1.33928571F,kReleaseRadius=2.85714286F,kAcceleration=1.60714281F;
using behavior::Actor;using behavior::Routine;
constexpr float pi=std::numbers::pi_v<float>;
// 动画167固定引用64×80逻辑精灵；按源可见矩形加半尺寸得到此中心范围，不能使用56×56碰撞盒。
constexpr movement::Bounds kEmitterBounds{{-10,-11.7857143F},{10,11.7857143F}};
movement::State& Motion(Actor& actor) {
    if(actor.skills.spatial&&!actor.liftedMotion)actor.liftedMotion.emplace(actor.motion);
    return actor.liftedMotion?actor.liftedMotion->planar:actor.motion;
}
void Height(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Aim(actor.motion,-1.25F+2.5F*spatial::RadicalInverse(actor.spatialIndex++,2));
}
void Wander(Actor& actor) {
    if(actor.liftedMotion)actor.liftedMotion->Rebase(actor.motion);
    actor.RandomDirection(-pi,pi);Motion(actor).Apply({.speed=6.69642878F});Motion(actor).Decelerate(90);Height(actor);
}
// 原生122安装的回调在移动参数和姿态更新后运行；根行为切换由Actor自动解除，不能泄漏到后继非符。
void ReleaseCageStep(Actor& actor) {
    static_cast<void>(FlandreBoss::Behaviors::ReleaseCageBullets(actor));
}
Routine BoundaryEmitter(Actor& actor) {
    actor.SetPoses({-1,-1,-1,-1,-1});actor.Animate(167);
    actor.damageable=actor.contactEnabled=actor.interactable=actor.visible=false;
    actor.skills.offset={0,0,0.535714269F};actor.skills.rankInfluence={};actor.skills.projectile.sampleAtStepStart=true;
    actor.SetEmitterSound(22);int nextShot=0;
    // 全部已编排出生点均在精灵扩展范围内，满足原生先入场条件；穿出时直接回收，不结算击破/掉落。
    // 每半内容帧检查，可防止Actor::Update一次补算多个固定步时在离场后继续留下静止弹。
    for(;;) {
        if(!kEmitterBounds.Contains(actor.Position())) {actor.finished=true;co_return;}
        if(actor.Frame()>=nextShot) {
            actor.Fire({skill::Formation::FixedFan,1,9,1,1,0,0,0,0,516});nextShot+=6;
        }
        co_await actor.At(actor.Frame()+0.5);
    }
}
void SpawnBoundaryEmitter(Actor& actor,Vector3 position,float angle,int item) {
    const behavior::Pattern pattern{.movement={.angle=angle,.speed=8.5714283F},.initialAnimation=167,
        .spatialShots=actor.skills.spatial,.routine=BoundaryEmitter};
    actor.spawnEvents.push_back({.position=position,.pattern=pattern,.difficulty=actor.difficulty,
        .sourceHealth=1200,.sourceScore=10,.seed=++actor.spawnIndex,.sourceItem=item,.visual=actor.visual,
        .hitRadius=1.25F,.retirementBounds=kEmitterBounds,.bulletBase=actor.skills.bulletBase,
        .laserBase=actor.skills.laserBase,.defeatBase=stage_assets::kNativeDefeat,.fallback=actor.skills.fallback,
        .retireOutside=true,.damageable=false,.visible=false,.stageClearResponse=behavior::StageClearResponse::Remove});
}
void FireLarge(Actor& actor,bool triple=false) {
    // 每次都读取方向；三发配置随后覆盖为零角，也必须保留这次未采用的随机读取。
    const float angle=actor.Random(pi/2)-pi/4;
    actor.Fire({skill::Formation::AimedFan,9,3,triple?3:1,1,9.64285755F,0,triple?0:angle,triple?1.04719758F:0,516});
}
}
int FlandreBoss::Behaviors::ReleaseCageBullets(behavior::Actor& actor) {
    int largeCount=0;const auto center=actor.Position();
    // 两层访问均沿物理槽序；首个大弹写入非零加速度后，其余大弹必须跳过该小弹。
    actor.ModifyEnemyProjectiles([&](Bullet& large) {
        if(!CageProjectile(large)||battle::EnemyBulletLogicalSize(large.nativeType,large.nativeColor).y<kLargeWidth)return;
        ++largeCount;
        const float beta=std::atan2(center.z-large.position.z,large.position.x-center.x);
        actor.ModifyEnemyProjectiles([&](Bullet& small) {
            if(!CageProjectile(small)||battle::EnemyBulletLogicalSize(small.nativeType,small.nativeColor).y>=kLargeWidth||
               small.acceleration.x!=0||small.acceleration.y!=0||small.acceleration.z!=0||small.speedAcceleration!=0)return;
            const Vector3 gap{small.position.x-large.position.x,actor.skills.spatial?small.position.y-large.position.y:0,
                              small.position.z-large.position.z};
            if(!(BulletSystem::Length(gap)<kReleaseRadius))return;
            const Vector3 radial{small.position.x-center.x,small.position.y-center.y,small.position.z-center.z};
            const float theta=std::atan2(-radial.z,radial.x);
            // 原生不对theta-beta取最短角；跨±π时先归一化会改变整片弹幕的方向。
            const float angle=beta+2.200000047683716F*(theta-beta);
            Vector3 direction{std::cos(angle),0,-std::sin(angle)};
            if(actor.skills.spatial) {
                const float length=BulletSystem::Length(radial);
                const float vertical=length>0?radial.y/length:0;
                const float horizontal=std::sqrt(std::max(0.0F,1-vertical*vertical));
                direction={horizontal*direction.x,vertical,horizontal*direction.z};
            }
            small.acceleration=spatial::Scale(direction,kAcceleration);small.accelerationSeconds=2;
            small.motionClock=0;small.sampleMotionAtStart=true;
            // 更换主体色会由Cursor::Sync重启ANM；显现时窗、视觉年龄及既有速度不重置。
            ++small.nativeColor;
            small.visual=static_cast<VisualId>(actor.skills.bulletBase+small.nativeType*16+small.nativeColor);
        });
    });
    return largeCount;
}
Routine FlandreBoss::Behaviors::KagomeKagome(Actor& actor) {
    actor.ClearBullets();actor.SetEmitterPeriod(0);actor.SetEmitterSound(-1);actor.defeatActors=true;
    actor.SetPoses({160,166,166,161,162});actor.damageable=false;actor.contactEnabled=true;
    actor.skills.offset={};actor.skills.projectile.sampleAtStepStart=true;
    Motion(actor).Interpolate(120,{0,0,6.42857143F},2);Height(actor);
    co_await actor.At(120);actor.SetAmbientEffects(effects::AmbientMode::Living);actor.damageable=true;actor.soundEvents.push_back(5);actor.SetStepEffect(ReleaseCageStep);
    // 每3内容帧发出一组独立蓄力粒子；保持216帧才放行攻击与步末原生15的执行顺序。
    for(int n=0;n<32;++n) {co_await actor.At(120+n*3);actor.EmitEffect(effects::Kind::Charge,6,{128,128,255,255});}
    co_await actor.At(216);
    for(int cycle=0;;++cycle) {
        const int start=213+cycle*690;
        SpawnBoundaryEmitter(actor,{-5.71428585F,0,10},pi/2,-2);
        co_await actor.At(start+13);SpawnBoundaryEmitter(actor,{9.28571415F,0,-4.28571415F},pi,2);
        co_await actor.At(start+23);SpawnBoundaryEmitter(actor,{-9.28571415F,0,2.85714293F},0,2);
        co_await actor.At(start+33);SpawnBoundaryEmitter(actor,{5.71428585F,0,-10.7142859F},-pi/2,2);
        co_await actor.At(start+43);SpawnBoundaryEmitter(actor,{-2.85714293F,0,10},pi/2,-2);
        co_await actor.At(start+53);SpawnBoundaryEmitter(actor,{9.28571415F,0,-1.42857146F},pi,2);
        co_await actor.At(start+63);SpawnBoundaryEmitter(actor,{-9.28571415F,0,1.42857146F},0,2);
        co_await actor.At(start+73);SpawnBoundaryEmitter(actor,{2.85714293F,0,-10.7142859F},-pi/2,2);
        co_await actor.At(start+83);SpawnBoundaryEmitter(actor,{9.28571415F,0,7.14285707F},pi,2);
        co_await actor.At(start+93);SpawnBoundaryEmitter(actor,{-9.28571415F,0,4.28571415F},0,2);
        co_await actor.At(start+253);FireLarge(actor);
        co_await actor.At(start+313);FireLarge(actor);Wander(actor);
        co_await actor.At(start+353);SpawnBoundaryEmitter(actor,{9.28571415F,0,10},2.356194496F,-2);
        co_await actor.At(start+368);SpawnBoundaryEmitter(actor,{9.28571415F,0,4.28571415F},2.356194496F,-2);
        co_await actor.At(start+383);SpawnBoundaryEmitter(actor,{9.28571415F,0,-1.42857146F},2.356194496F,-2);
        co_await actor.At(start+443);FireLarge(actor);
        co_await actor.At(start+473);FireLarge(actor,true);
        co_await actor.At(start+513);SpawnBoundaryEmitter(actor,{-9.28571415F,0,10},0.785398185F,-2);
        co_await actor.At(start+528);SpawnBoundaryEmitter(actor,{-9.28571415F,0,4.28571415F},0.785398185F,-2);
        co_await actor.At(start+543);SpawnBoundaryEmitter(actor,{-9.28571415F,0,-1.42857146F},0.785398185F,-2);Wander(actor);
        co_await actor.At(start+603);FireLarge(actor);
        co_await actor.At(start+633);FireLarge(actor,true);
        co_await actor.At(start+693);
    }
}
}

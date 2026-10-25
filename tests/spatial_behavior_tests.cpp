// 本文件独立检查球面/球冠的面积密度、队列保持、等路程移动和实际 Stage 1 阶段，不使用生产采样器构造期望值。
#include "actor_fixture.hpp"
#include "gameplay/spatial.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/content.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>
#include <set>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>;
void Near(float a,float b,float error=1e-4F) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>error) {std::cerr<<"Spatial mismatch "<<a<<" vs "<<b<<" error "<<std::abs(a-b)<<'\n';std::abort();}
}
void Near(Vector3 a,Vector3 b) {Near(a.x,b.x);Near(a.y,b.y);Near(a.z,b.z);}
float Dot(Vector3 a,Vector3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
Vector3 Unit(Vector3 v) {const float n=std::sqrt(Dot(v,v));assert(n>0);return {v.x/n,v.y/n,v.z/n};}
std::vector<Bullet> Snapshot(const BulletSystem& bullets) {
    std::vector<Bullet> result;bullets.Pool().ForEachActive([&](const Bullet& bullet){result.push_back(bullet);});return result;
}
skill::State SpatialSkill() {skill::State state;state.spatial=true;state.rankInfluence={};return state;}
void TestSphereDensity() {
    auto bullets=std::make_unique<BulletSystem>();auto state=SpatialSkill();state.offset={0.25F,1,-0.5F};
    const skill::Shot shell{skill::Formation::FixedRing,2,6,16,2,6,3,0,0,0,384};
    state.Fire(shell,{2,2,4},{9,8,-6},*bullets);const auto emitted=Snapshot(*bullets);assert(emitted.size()==768);
    std::array<int,12> equalAreaBands{};std::array<int,8> octants{};std::vector<Vector3> directions;
    for(int n=0;n<384;++n) {
        const auto& bullet=emitted[n];const auto direction=Unit(bullet.velocity);directions.push_back(direction);
        Near(bullet.position,{2.25F,3,3.5F});Near(BulletSystem::Length(bullet.velocity),6);
        Near(BulletSystem::Length(emitted[n+384].velocity),4.5F);Near(Unit(emitted[n+384].velocity),direction);
        assert(bullet.nativeColor==6&&bullet.nativeType==2);
        // 球面面积与轴向坐标成正比，等高球带的独立期望都是 384/12=32。
        ++equalAreaBands[std::clamp(static_cast<int>((direction.z+1)*6),0,11)];
        ++octants[(direction.x>0 ? 1 : 0)+(direction.y>0 ? 2 : 0)+(direction.z>0 ? 4 : 0)];
    }
    for(int n:equalAreaBands)assert(n==32);
    for(int n:octants)assert(n>=44&&n<=52);
    float closest=2;
    for(std::size_t i=0;i<directions.size();++i)for(std::size_t j=0;j<i;++j)
        closest=std::min(closest,std::sqrt(std::max(0.0F,2-2*Dot(directions[i],directions[j]))));
    assert(closest>2/std::sqrt(384.0F)); // 排除轴线重叠和极区拥挤，不能只检查平均方向。
    // rank 对总方向预算应用一次，不再按复制份数叠加修正。
    for(const auto& [rank,expected]:{std::pair{10,47},std::pair{16,49},std::pair{32,54}}) {
        bullets->Reset();state.rank=rank;state.rankInfluence={0,0,-3,6,0,0};auto shot=shell;shot.layers=1;shot.spatialCount=48;
        state.Fire(shot,{}, {},*bullets);assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
    }
    std::cout<<"Sphere density: 12 equal-area bands each contain 32 samples; minimum chord "<<closest<<'\n';
}
void TestConeAndStreamDensity() {
    auto bullets=std::make_unique<BulletSystem>();auto state=SpatialSkill();
    const float opening=0.9F,range=1-std::cos(opening);const Vector3 axis{0.6F,0.8F,0};
    skill::Shot cone{skill::Formation::AimedFan,1,2,7,1,5,0,0,0,0,96,opening};
    state.Fire(cone,{1,2,3},{4,6,3},*bullets);
    std::array<int,8> rings{};std::array<int,4> sectors{};
    for(const auto& bullet:Snapshot(*bullets)) {
        const Vector3 direction=Unit(bullet.velocity);Near(BulletSystem::Length(bullet.velocity),5);
        const float area=(1-Dot(direction,axis))/range;assert(area>=0&&area<=1);
        ++rings[std::clamp(static_cast<int>(area*8),0,7)];
        float angle=std::atan2(direction.z,0.8F*direction.x-0.6F*direction.y);if(angle<0)angle+=2*pi;
        ++sectors[std::clamp(static_cast<int>(angle/(pi/2)),0,3)];
    }
    for(int n:rings)assert(n==12);for(int n:sectors)assert(n>=20&&n<=28);
    // 单弹瞄准保持中心；逐发扫描的球冠面积分位则均衡覆盖，而非落在几张平面上。
    bullets->Reset();cone.count=1;cone.spatialCount=1;state.Fire(cone,{1,2,3},{4,6,3},*bullets);
    Near(Snapshot(*bullets)[0].velocity,{3,4,0});
    bullets->Reset();std::array<int,4> streamBands{};
    for(int index=0;index<256;++index) {cone.sampleIndex=index;state.Fire(cone,{1,2,3},{4,6,3},*bullets);}
    for(const auto& bullet:Snapshot(*bullets)) {
        const float area=(1-Dot(Unit(bullet.velocity),axis))/range;
        ++streamBands[std::clamp(static_cast<int>(area*4+1e-5F),0,3)];
    }
    for(int n:streamBands)assert(n>=63&&n<=65);
    // 竖直瞄准和重合目标均须有限，且不放大速率。
    for(Vector3 target:{Vector3{1,8,3},Vector3{1,2,3},Vector3{1,2,-8},Vector3{1,2.001F,-8}}) {
        bullets->Reset();cone.sampleIndex=-1;state.Fire(cone,{1,2,3},target,*bullets);
        Near(BulletSystem::Length(Snapshot(*bullets)[0].velocity),5);
    }
    auto invalid=cone;invalid.spatialCount=-1;assert(!skill::Valid({0,invalid}));
    invalid=cone;invalid.coneHalfAngle=0;assert(!skill::Valid({0,invalid}));
    invalid.coneHalfAngle=pi+0.1F;assert(!skill::Valid({0,invalid}));
    invalid=cone;invalid.sampleIndex=-2;assert(!skill::Valid({0,invalid}));
}
void TestProjectileMotion() {
    auto bullets=std::make_unique<BulletSystem>();auto state=SpatialSkill();
    state.projectile={.delayFrames=2,.turnAngle=pi/2,.turnSpeed=4};
    skill::Shot shell{skill::Formation::FixedRing,1,2,4,1,6,0,0,0,skill::DelayTurn,48};
    state.Fire(shell,{1,2,3},{},*bullets);const auto before=Snapshot(*bullets);assert(before.size()==48);
    state.projectile={.delayFrames=999,.turnSpeed=99};bullets->Update(1.0F/30);
    const auto after=Snapshot(*bullets);
    for(std::size_t i=0;i<before.size();++i) {
        const auto& b=after[i];assert(b.turnApplied);Near(b.turnSpeed,4);Near(b.turnDelaySeconds,2.0F/60);
        const Vector3 a=Unit(before[i].velocity),c=Unit(b.velocity);Near(Dot(a,b.rotationAxis),0);Near(Dot(a,c),0);
        Near(BulletSystem::Length(b.velocity),4);
    }
    // 零初速的显现/启动加速环必须沿各自方向展开，不能全部退化到 +Z。
    bullets->Reset();state.projectile={.acceleration=2};shell.firstSpeed=0;shell.flags=skill::LaunchBoost|skill::Accelerate;
    state.Fire(shell,{}, {},*bullets);const auto zero=Snapshot(*bullets);bullets->Update(1.0F/120);
    const auto boosted=Snapshot(*bullets);
    for(std::size_t i=0;i<zero.size();++i) {
        Near(Unit(boosted[i].velocity),Unit(zero[i].acceleration));
        Near(BulletSystem::Length(boosted[i].launchDirection),1);
    }
    // 重新瞄准使用转向时的真实玩家，而非采样球面的虚拟目标。
    bullets->Reset();state.projectile={.delayFrames=1,.turnSpeed=5};shell.firstSpeed=2;shell.flags=skill::DelayAim;
    state.Fire(shell,{0,0,3},{0,0,-6},*bullets);const Vector3 target{3,4,-3};bullets->Update(1.0F/60,{},target);
    for(const auto& b:Snapshot(*bullets)) {
        const Vector3 old{b.position.x-b.velocity.x/60,b.position.y-b.velocity.y/60,b.position.z-b.velocity.z/60};
        Near(Unit(b.velocity),Unit({target.x-old.x,target.y-old.y,target.z-old.z}));Near(BulletSystem::Length(b.velocity),5);
    }
    bullets->Reset();while(bullets->Spawn(BulletOwner::Player,{}, {},0.1F,WHITE,kInvalidVisualId)){}
    state.Fire(shell,{}, {},*bullets);assert(bullets->Pool().DroppedSpawns()==49);
}
void TestConservedMovement() {
    movement::State world;movement::LiftedPath lifted;
    lifted.planar.Interpolate(60,{4,0,3},2);lifted.Aim(world,1.25F);
    float oldDistance=0,newDistance=0;
    for(int tick=0;tick<120;++tick) {
        const Vector3 old=lifted.planar.position,point=world.position;
        lifted.BeginFrame(world);lifted.EndFrame(world);
        oldDistance+=BulletSystem::Length({lifted.planar.position.x-old.x,0,lifted.planar.position.z-old.z});
        newDistance+=BulletSystem::Length({world.position.x-point.x,world.position.y-point.y,world.position.z-point.z});
    }
    Near(oldDistance,5);Near(newDistance,5);Near(world.position.y,1.25F);
    // 原地等待不会因目标高度不同而新增移动。
    lifted.planar.Interpolate(120,lifted.planar.position,2);lifted.Aim(world,-1.25F);
    const Vector3 stopped=world.position;
    for(int tick=0;tick<240;++tick) {lifted.BeginFrame(world);lifted.EndFrame(world);}Near(world.position,stopped);
    // Actor 重开释放跨阶段的移动状态，不能沿用上一场的高度或编排进度。
    behavior::Actor actor;actor.Reset({},1,5093);stage1::ConfigureBehavior(actor);assert(!actor.liftedMotion);
    auto bullets=std::make_unique<BulletSystem>();actor.Start(stage1::rumia::FirstNonspell(actor));
    actor.Update(1.0F/120,{},*bullets);assert(actor.liftedMotion);
    actor.Reset({},1,1);assert(!actor.liftedMotion&&actor.spatialIndex==1);
}
void TestBossCadenceAndDistance() {
    auto plainBullets=std::make_unique<BulletSystem>(),spatialBullets=std::make_unique<BulletSystem>();
    float maximumDifference=0;int caseIndex=0;
    for(const auto entry:{stage1::rumia::Midboss,stage1::rumia::MidbossDefeated,stage1::rumia::MidbossTimeout,
            stage1::rumia::FirstNonspell,stage1::rumia::SecondNonspell,stage1::rumia::NightBird,
            stage1::rumia::Demarcation,stage1::rumia::Defeated})for(unsigned seed:{1882U,5093U,12345U}) {
        ++caseIndex;behavior::Actor plain,lifted;plain.Reset({0,0,5.71428585F},7000,seed);lifted.Reset({0,0,5.71428585F},7000,seed);
        stage1::ConfigureBehavior(plain,false);stage1::ConfigureBehavior(lifted,true);
        plain.Start(entry(plain));lifted.Start(entry(lifted));
        double plainDistance=0,liftedDistance=0;
        for(int tick=0;tick<3000;++tick) {
            plainBullets->Reset();spatialBullets->Reset();const auto a=plain.Position(),b=lifted.Position();
            plain.Update(1.0F/120,{2,0,-6},*plainBullets);lifted.Update(1.0F/120,{2,0,-6},*spatialBullets);
            const auto canonical=lifted.liftedMotion ? lifted.liftedMotion->planar.position : lifted.motion.position;
            if(BulletSystem::Length({canonical.x-plain.Position().x,canonical.y-plain.Position().y,canonical.z-plain.Position().z})>0.0001F)
                std::cerr<<"Boss canonical case "<<caseIndex<<" tick "<<tick<<" planar "<<canonical.x<<","<<canonical.y<<","<<canonical.z<<" old "<<plain.Position().x<<","<<plain.Position().y<<","<<plain.Position().z<<'\n';
            Near(canonical,plain.Position());
            const auto c=plain.Position(),d=lifted.Position();
            const float oldStep=BulletSystem::Length({c.x-a.x,c.y-a.y,c.z-a.z});
            const float newStep=BulletSystem::Length({d.x-b.x,d.y-b.y,d.z-b.z});
            maximumDifference=std::max(maximumDifference,std::abs(oldStep-newStep));
            if(std::abs(oldStep-newStep)>0.0001F) {
                std::cerr<<"Boss distance mismatch tick "<<tick<<": "<<oldStep<<" vs "<<newStep<<'\n';std::abort();
            }
            plainDistance+=oldStep;liftedDistance+=newStep;
            assert(plain.phaseFrames==lifted.phaseFrames&&plain.pendingCallback==lifted.pendingCallback);
            assert(plain.damageable==lifted.damageable&&plain.finished==lifted.finished&&plain.health==lifted.health);
            assert(plain.soundEvents==lifted.soundEvents&&plain.dropEvents==lifted.dropEvents);
            std::set<std::pair<int,int>> oldShots,newShots;int oldBeams=0,newBeams=0;
            plainBullets->Pool().ForEachActive([&](const Bullet& bullet){oldShots.insert({bullet.nativeType,bullet.nativeColor});oldBeams+=bullet.beamLength>0;});
            spatialBullets->Pool().ForEachActive([&](const Bullet& bullet){newShots.insert({bullet.nativeType,bullet.nativeColor});newBeams+=bullet.beamLength>0;});
            assert(oldShots==newShots&&oldBeams==newBeams); // 精确到每个 120 Hz 步的发射、光束和空档。
            if(entry==stage1::rumia::FirstNonspell&&tick<200)Near(lifted.Position(),b);
            if(plain.finished||plain.pendingCallback)break;
        }
        assert(std::abs(plainDistance-liftedDistance)<0.0005);
    }
    std::cout<<"Boss attack/movement cadence preserved; maximum per-step distance difference "<<maximumDifference<<'\n';
}
void TestStageEncounters() {
    auto bullets=std::make_unique<BulletSystem>();std::size_t peak=0;
    for(const behavior::Entry entry:{stage1::rumia::Midboss,stage1::rumia::MidbossDefeated,stage1::rumia::MidbossTimeout,
            stage1::rumia::FirstNonspell,stage1::rumia::SecondNonspell,stage1::rumia::NightBird,
            stage1::rumia::Demarcation,stage1::rumia::Defeated}) {
        behavior::Actor actor;actor.Reset({0,0,6},7000,5093);stage1::ConfigureBehavior(actor);actor.Start(entry(actor));
        bullets->Reset();float low=0,high=0;bool verticalBullet=false;
        for(int tick=0;tick<2400&&!actor.pendingCallback&&!actor.finished;++tick) {
            actor.Update(1.0F/120,{2,3,-6},*bullets);low=std::min(low,actor.position.y);high=std::max(high,actor.position.y);
            bullets->Pool().ForEachActive([&](const Bullet& b){verticalBullet=verticalBullet||std::abs(b.velocity.y)>0.05F;});
            peak=std::max(peak,bullets->Pool().ActiveCount());bullets->Update(1.0F/120,{}, {2,3,-6});
        }
        assert(high-low>0.05F);
        if(entry!=stage1::rumia::MidbossDefeated&&entry!=stage1::rumia::MidbossTimeout&&entry!=stage1::rumia::Defeated)assert(verticalBullet);
        assert(bullets->Pool().DroppedSpawns()==0);
    }
    std::cout<<"Uniform spatial Stage 1 encounters validated; sampled peak "<<peak<<" / "<<BulletSystem::kCapacity<<'\n';
}
} // namespace
/// @brief 独立验证面积密度、最小间距、队列保持、等路程运动及原有生命周期约束。
void RunSpatialBehaviorTests() {
    TestSphereDensity();TestConeAndStreamDensity();TestProjectileMotion();
    TestConservedMovement();TestBossCadenceAndDistance();TestStageEncounters();
}

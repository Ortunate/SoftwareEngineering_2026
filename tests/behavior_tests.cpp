// 本文件保留独立旧行为差分，并验证编队轨迹、三维插值及固定内容时钟。
#include "actor_fixture.hpp"
#include "reference_script.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/content.hpp"
#include "gameplay/data/native_combat.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <memory>
#include <limits>
#include <tuple>
#include <vector>
namespace {
using namespace koumakyou;
using namespace koumakyou::stage1;
int currentSub=0,currentStep=0;
std::array<bool,38> covered{};
float maxPositionError=0;
// 原编号只用于定位差分测试的旧指令；生产代码直接引用这五个具名常量。
constexpr std::array kPatternsBySub{kTurningFairy,kGentleTurningFairy,kLateTurningFairy,
                                        kShootingDarkFairy,kPassingDarkFairy};
/// @brief 仅关闭有意新增的空间适配；其余生产配方继续接受独立旧解释器的逐帧检查。
behavior::Pattern PlanarRecipe(behavior::Pattern pattern) {
    pattern.spatialShots=false;return pattern;
}

void Equal(float a,float b,const char* field,float tolerance=0.00001F) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {
        std::cerr<<std::setprecision(9)<<"behavior mismatch Sub"<<currentSub<<" step "<<currentStep<<" "<<field<<": "<<a<<" != "<<b<<" (error "<<std::abs(a-b)<<")\n";
        std::abort();
    }
}
// 世界单位积分改变舍入位置，速度/加速度向量允许 1e-4；位置单独约束累积误差。
void Equal(Vector3 a,Vector3 b,const char* field) { Equal(a.x,b.x,field,0.0001F);Equal(a.y,b.y,field,0.0001F);Equal(a.z,b.z,field,0.0001F); }
void EqualPosition(Vector3 a,Vector3 b,const char* field) {
    // 5e-4 世界单位约为 0.0112 原作像素；该上限只覆盖单位迁移的累计舍入，不放宽时序或离散状态。
    maxPositionError=std::max({maxPositionError,std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)});
    Equal(a.x,b.x,field,0.0005F);Equal(a.y,b.y,field,0.0005F);Equal(a.z,b.z,field,0.0005F);
}
behavior::Entry Entry(int sub) {
    switch(sub) {
    case 7:return rumia::MidbossDefeated;case 8:return rumia::MidbossTimeout;case 9:return rumia::Midboss;
    case 14:return rumia::FirstNonspell;case 19:return rumia::SecondNonspell;case 20:return rumia::Defeated;
    case 21:return rumia::End;case 26:return rumia::NightBird;case 29:return rumia::Demarcation;
    case -1:return nullptr;
    default:std::abort();
    }
}
/// @brief 比较实际弹池槽序及所有敌弹运动/表现参数，不依赖重构后的配方来构造期望值。
void EqualBullets(const BulletSystem& original,const BulletSystem& replacement) {
    std::vector<const Bullet*> a,b;
    original.Pool().ForEachActive([&](const Bullet& v){a.push_back(&v);});
    replacement.Pool().ForEachActive([&](const Bullet& v){b.push_back(&v);});
    if(a.size()!=b.size()) {
        std::cerr<<"bullet count Sub"<<currentSub<<" step "<<currentStep<<": "<<a.size()<<" != "<<b.size()<<'\n';std::abort();
    }
    assert(original.Pool().DroppedSpawns()==replacement.Pool().DroppedSpawns());
    for(std::size_t i=0;i<a.size();++i) {
        const auto& x=*a[i];const auto& y=*b[i];
        if(x.owner!=y.owner||x.visual!=y.visual||x.nativeType!=y.nativeType||x.nativeColor!=y.nativeColor) {
            std::cerr<<"bullet visual Sub"<<currentSub<<" step "<<currentStep<<": "<<x.nativeType<<"/"<<x.nativeColor<<" != "<<y.nativeType<<"/"<<y.nativeColor<<'\n';std::abort();
        }
        assert(x.turnAimed==y.turnAimed&&x.turnApplied==y.turnApplied&&x.Dangerous()==y.Dangerous());
        assert(x.color.r==y.color.r&&x.color.g==y.color.g&&x.color.b==y.color.b&&x.color.a==y.color.a);
        EqualPosition(x.position,y.position,"bullet.position");Equal(x.velocity,y.velocity,"bullet.velocity");
        Equal(x.baseVelocity,y.baseVelocity,"bullet.baseVelocity");Equal(x.launchDirection,y.launchDirection,"bullet.launchDirection");Equal(x.acceleration,y.acceleration,"bullet.acceleration");
        Equal(x.rotationAxis,y.rotationAxis,"bullet.axis");Equal(x.beamDirection,y.beamDirection,"beam.direction");
        const float xs[]={x.radius,x.age,x.maxAge,x.angularVelocity,x.homingTurnRate,x.damage,x.beamLength,x.warningSeconds,
            x.turnDelaySeconds,x.turnAngle,x.turnSpeed,x.launchBoost,x.launchBoostSeconds,x.spawnVisualSeconds,x.spawnMoveScale,
            x.beamGrowSeconds,x.beamFadeSeconds,x.dangerEndSeconds};
        const float ys[]={y.radius,y.age,y.maxAge,y.angularVelocity,y.homingTurnRate,y.damage,y.beamLength,y.warningSeconds,
            y.turnDelaySeconds,y.turnAngle,y.turnSpeed,y.launchBoost,y.launchBoostSeconds,y.spawnVisualSeconds,y.spawnMoveScale,
            y.beamGrowSeconds,y.beamFadeSeconds,y.dangerEndSeconds};
        for(std::size_t n=0;n<std::size(xs);++n)Equal(xs[n],ys[n],"bullet.scalar");
    }
}
struct Pair {
    ReferenceScript original;
    behavior::Actor replacement{behavior::UpdateRate::Hz60};
    BulletSystem originalBullets,replacementBullets;
    Pair(int sub,unsigned seed=1,bool mirrored=false) {
        currentSub=sub;
        const float hp=sub<5 ? 256 : sub==9 ? 6000 : sub==26 ? 900 : sub==29 ? 800 : 7000;
        const Vector3 pos=sub<5||sub==9 ? Vector3{60,-32,0} : Vector3{192,96,0};
        original.Reset(sub,pos,hp,seed,mirrored);replacement.Reset(ReferencePosition(pos),hp,seed,mirrored);ConfigureBehavior(replacement,false);
        if(sub<5)replacement.Start(behavior::Follow(replacement,PlanarRecipe(kPatternsBySub.at(sub))));
        else replacement.Start(Entry(sub)(replacement));
        if(sub==26||sub==29) {
            original.SetDeathCallback(sub==26 ? 19 : 20);
            replacement.SetDeathCallback(sub==26 ? rumia::SecondNonspell : rumia::Defeated);
        }
    }
    void Step(float dt,int step,int rank) {
        currentStep=step;
        // 同时覆盖随时间变化的 X/Z 瞄准与正负高度，保证不把动态瞄准偷换成固定方向。
        const Vector3 player{std::sin(step*0.017F)*3,std::cos(step*0.013F)*2,-6+std::sin(step*0.007F)};
        original.SetRank(rank);replacement.SetRank(rank);
        original.Update(dt,player,originalBullets);replacement.Update(dt,player,replacementBullets);
        for(std::size_t i=0;i<covered.size();++i)covered[i]=covered[i]||original.visited[i];
        EqualPosition(original.Position(),replacement.motion.position,"actor.position");
        Equal(original.health,replacement.health,"actor.health");Equal(original.maxHealth,replacement.maxHealth,"actor.maxHealth");
        assert(original.finished==replacement.finished&&original.damageable==replacement.damageable);
        assert(original.pendingCallback==-1 ? !replacement.pendingCallback : Entry(original.pendingCallback)==replacement.pendingCallback);
        assert(original.phaseFrames==replacement.phaseFrames&&original.timeoutFrames==replacement.timeoutFrames);
        if(original.animationScript!=replacement.animationScript||original.animationRevision!=replacement.animationRevision) {
            std::cerr<<"animation Sub"<<currentSub<<" step "<<step<<": "<<original.animationScript<<"/"<<original.animationRevision<<" != "<<replacement.animationScript<<"/"<<replacement.animationRevision<<'\n';std::abort();
        }
        Equal(original.animationAge,replacement.animationAge,"animation.age");assert(original.deathEffect==replacement.deathEffect);
        assert(original.soundEvents==replacement.soundEvents&&original.dropEvents==replacement.dropEvents);
        EqualBullets(originalBullets,replacementBullets);
        originalBullets.Update(dt,{},player);replacementBullets.Update(dt,{},player);
    }
};
/// @brief 不同显示帧率、rank、镜像与随机种子的逐帧差分；包括完整循环与超时前的随机分支链。
void TestRecipes() {
    for(const auto [fps,rank,seed]:{std::tuple{60,16,1U},std::tuple{30,10,1882U},std::tuple{144,32,5093U}}) {
        for(int sub:{0,1,2,3,4,7,8,9,14,19,20,21,26,29})for(bool mirrored:{false,true}) {
            if(sub>4&&mirrored)continue;
            auto pair=std::make_unique<Pair>(sub,seed,mirrored);
            const int duration=sub<5 ? 5 : sub==14 ? 36 : sub==19 ? 31 : sub==9 ? 25 : 27;
            for(int step=0;step<duration*fps;++step) {
                pair->Step(step%113==0 ? 0 : 1.0F/fps,step,rank);
                if(pair->original.finished||pair->original.pendingCallback>=0)break;
            }
        }
    }
}
/// @brief 同一演员跨完整关底阶段，覆盖受击阈值、清弹、保留速度/特效上下文和回调后的剩余时间。
void TestTransitions() {
    for(bool damage:{false,true}) {
        auto pair=std::make_unique<Pair>(14,5093);
        int transitions=0;
        for(int step=0;step<10000&&transitions<5;++step) {
            if(damage&&pair->original.phaseFrames==340)pair->original.health=pair->replacement.health=0;
            pair->Step(1.0F/60,step,16);
            if(pair->original.pendingCallback>=0) {
                const int callback=pair->original.pendingCallback;
                pair->original.Transition();pair->replacement.Transition();++transitions;
                // 游戏在阶段交接时补齐下一阶段默认死亡回调；两个实现接收完全相同的边界输入。
                const int death=callback==26 ? 19 : callback==19||callback==29 ? 20 : 21;
                pair->original.SetDeathCallback(death);pair->replacement.SetDeathCallback(Entry(death));
            }
        }
        assert(transitions>=3);
    }
}
/// @brief 逐条核对 StageEvent 到配方的映射与实际演员实例，旧坐标仅在参考侧转换为世界坐标。
void TestBirths() {
    Stage1Content content;std::vector<const native_combat::HealthRecord*> births;
    assert(content.Events().size()==226);
    for(const auto& r:native_combat::Stage(1)->health)if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub>=0&&r.targetSub<=4)births.push_back(&r);
    std::size_t index=0;
    for(std::size_t eventIndex=0;eventIndex<content.Events().size();++eventIndex) {
        const auto& event=content.Events()[eventIndex];
        const auto* spawn=std::get_if<SpawnActors>(&event.action);
        if(!spawn)continue;
        const auto& r=*births.at(index++);const auto& p=spawn->actor;
        assert(event.atFrame==r.frame&&p.sourceHealth==*r.value&&p.sourceScore==r.score&&p.sourceItem==r.item);
        assert(p.mirrored==(r.opcode==2||r.opcode==6)&&p.randomX==(r.position.x==-999));
        // 旧实现的种子含已插入的道中 Boss 事件索引；显式表必须保留这些固定值。
        assert(p.seed==static_cast<unsigned>(r.frame*31+eventIndex));
        const Vector3 expectedSource{r.position.x==-999 ? 192 : r.position.x,r.position.y,r.position.z};
        Equal(ReferencePosition(expectedSource),{p.position.x,0,p.position.z},"spawn.initialFormation");
        assert(behavior::Valid(p.pattern));
        auto planar=spawn->actor;planar.position=ReferencePosition(expectedSource);planar.randomX=(r.position.x==-999);planar.pattern=PlanarRecipe(planar.pattern);planar.retirementHeights.reset();
        auto enemy=testing::MakeActor(planar,behavior::UpdateRate::Hz60);
        auto production=testing::MakeActor(p,behavior::UpdateRate::Hz60);
        auto productionBullets=std::make_unique<BulletSystem>();
        auto original=std::make_unique<ReferenceScript>();
        Vector3 source=expectedSource;
        if(planar.randomX)source.x=static_cast<float>((p.seed*1664525U+1013904223U)>>8)/16777216.0F*384;
        original->Reset(r.targetSub,source,p.sourceHealth,p.seed,p.mirrored);
        auto a=std::make_unique<BulletSystem>(),b=std::make_unique<BulletSystem>();
        bool entered=false;
        for(int step=0;step<260&&enemy->active;++step) {
            original->Update(1.0F/60,{2,3,-6},*a);enemy->Update(1.0F/60,{2,3,-6},*b);
            production->Update(1.0F/60,{2,3,-6},*productionBullets);
            currentSub=r.targetSub;currentStep=step;
            EqualPosition({original->Position().x,p.position.y,original->Position().z},production->position,"formation.productionPath");
            assert(production->active==enemy->active);
            productionBullets->Reset();
            EqualPosition(original->Position(),enemy->position,"spawn.position");EqualBullets(*a,*b);
            const auto pos=original->sourcePosition;
            if(pos.x>=-16&&pos.x<=400&&pos.y>=-16&&pos.y<=464)entered=true;
            const bool expired=original->finished||(entered&&(pos.x<-16||pos.x>400||pos.y<-16||pos.y>464));
            assert(enemy->active==!expired);
            assert(enemy->visual==(r.targetSub<3 ? kFairy : kDarkFairy));
            a->Reset();b->Reset();
        }
    }
    assert(index==222);
}
/// @brief 验证配方确实可独立组合，以及挂起的父/子行为在重开和销毁时释放各自资源。
void TestCompositionAndOwnership() {
    auto params=testing::SpawnAt();
    params.position={0,0,5.71428585F};
    params.pattern.movement.speed=0;
    params.pattern.movement.keys[0]={.frame=3,.speed=1};params.pattern.movement.count=1;
    params.pattern.skill=skill::TimedShot{5,{skill::Formation::FixedRing,2,6,9,2,2,1}};
    params.pattern.endFrame=8;
    Stage1Content content;
    auto enemy=testing::MakeActor(params);assert(enemy);
    auto bullets=std::make_unique<BulletSystem>();
    for(int i=0;i<5;++i)enemy->Update(1.0F/60,{},*bullets);
    assert(bullets->Pool().ActiveCount()==0);
    enemy->Update(1.0F/60,{},*bullets);assert(bullets->Pool().ActiveCount()==18);
    for(int i=6;i<=8;++i)enemy->Update(1.0F/60,{},*bullets);
    assert(!enemy->active);
    auto invalid=params;invalid.pattern.movement.count=5;assert(!behavior::Valid(invalid));
    invalid=params;invalid.pattern.movement.keys[0].frame=9;assert(!behavior::Valid(invalid));
    invalid=params;invalid.pattern.skill->frame=9;assert(!behavior::Valid(invalid));
    invalid=params;invalid.pattern.movement.speed=std::numeric_limits<float>::infinity();assert(!behavior::Valid(invalid));
    invalid=params;invalid.pattern.skill->shot.count=0;assert(!behavior::Valid(invalid));

    auto retreat=testing::SpawnAt({0,0,-10.5357141F});
    retreat.pattern=kLateTurningFairy;retreat.retireOutside=true;
    auto exiting=testing::MakeActor(retreat);exiting->Update(0.1F,{},*bullets);
    assert(!exiting->active); // 首帧从场内越界也必须回收，不能依赖之前已有更新样本。

    struct Guard { int& live; explicit Guard(int& count):live(count) { ++live; } ~Guard() { --live; } };
    const auto child=[](behavior::Actor& actor,int& live)->behavior::Routine {
        Guard guard(live);co_await actor.At(100);
    };
    const auto parent=[&child](behavior::Actor& actor,int& live)->behavior::Routine {
        Guard guard(live);co_await child(actor,live);
    };
    int live=0;
    {
        behavior::Actor actor;actor.Reset({},1,1);actor.Start(parent(actor,live));
        actor.Update(1.0F/60,{},*bullets);assert(live==2);
        actor.Reset({},1,1);assert(live==0);
        actor.Start(parent(actor,live));actor.Update(1.0F/60,{},*bullets);assert(live==2);
    }
    assert(live==0);
}
/// @brief 独立世界单位契约：秒速率、XYZ 插值、X/Z 边界、发射偏移与自定义随机/回收范围均不依赖原作映射。
void TestWorldCoordinates() {
    movement::State motion;
    motion.position={2,3,4};motion.velocity={60,120,-180};motion.BeginFrame(60);
    Equal(motion.position,{3,5,1},"world.integrate");
    motion.mirrored=true;motion.BeginFrame(60);Equal(motion.position,{2,7,-2},"world.mirror");
    motion={};motion.Polar(0,60);
    for(int frame=0;frame<60;++frame) { motion.EndFrame(60);motion.BeginFrame(60); }
    Equal(motion.position,{60,0,0},"world.speedPerSecond");
    motion={};motion.position={2,3,4};motion.Interpolate(60,{-4,9,-8},2);
    for(int frame=0;frame<60;++frame) { motion.BeginFrame(60);motion.EndFrame(60); }
    Equal(motion.position,{-4,9,-8},"world.interpolateXYZ");
    motion={};motion.position={2,7,4};motion.bounds={{-1,-2},{1,2}};motion.clamped=true;
    motion.BeginFrame(60);Equal(motion.position,{1,7,2},"world.clampXZ");

    auto bullets=std::make_unique<BulletSystem>();
    skill::State skill;skill.offset={0.25F,1,-0.5F};
    skill.Fire(skill::Shot{skill::Formation::FixedFan,2,6,1,1,60}, {13,7,-4},{14,8,-4.5F},*bullets);
    assert(bullets->Pool().ActiveCount()==1);
    bullets->Pool().ForEachActive([](const Bullet& bullet) {
        Equal(bullet.position,{13.25F,8,-4.5F},"world.muzzleOffset");
        Equal(bullet.velocity,{60,0,0},"world.shotSpeed");
    });
    bullets->Reset();
    skill.Fire(skill::Laser{.length=30,.width=2},{13,7,-4},{14,8,-4.5F},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& beam) {
        Equal(beam.position,{13.25F,8,-4.5F},"world.beamOrigin");
        Equal(beam.beamLength,30,"world.beamLength");Equal(beam.radius,1,"world.beamRadius");
    });

    auto config=testing::SpawnAt({11,20,1});
    config.retireOutside=true;config.retirementBounds={{10,-5},{14,5}};
    config.pattern.movement={.angle=0,.speed=60};
    auto actor=testing::MakeActor(config);
    assert(actor->Position().x==11&&actor->Position().y==20&&actor->Position().z==1);
    actor->Update(7.0F/120,{},*bullets);assert(actor->active);
    actor->Update(1.0F/120,{},*bullets);assert(!actor->active);
    config.pattern.movement.speed=0;config.randomX=true;config.randomXRange={20,24};
    actor->Activate(config);auto duplicate=testing::MakeActor(config);
    assert(actor->position.x>=20&&actor->position.x<=24&&actor->position.y==20&&actor->position.z==1);
    assert(actor->position.x==duplicate->position.x);
    auto invalid=config;invalid.randomXRange={24,20};assert(!behavior::Valid(invalid));
    invalid=config;invalid.retirementBounds.lower.x=15;assert(!behavior::Valid(invalid));
}
/// @brief 生产默认 120 Hz：半个内容帧就更新位置，技能、子行为与超时仍按原有 60 Hz 帧号等待。
void TestRuntime120Hz() {
    auto bullets=std::make_unique<BulletSystem>();
    behavior::Pattern pattern{.movement={.angle=0,.speed=120},
        .skill=skill::TimedShot{2,{skill::Formation::FixedRing,2,6,4,1,3}}};
    behavior::Actor actor;
    actor.Reset({10,5,0},1,1);actor.Start(behavior::Follow(actor,pattern));
    actor.Update(1.0F/240,{},*bullets);assert(actor.phaseFrames==0);
    actor.Update(1.0F/240,{},*bullets);assert(actor.phaseFrames==0.5);
    Equal(actor.Position(),{10,5,0},"120.initialStep");
    actor.Update(1.0F/120,{},*bullets);Equal(actor.Position(),{11,5,0},"120.halfFrameMovement");
    assert(actor.phaseFrames==1&&bullets->Pool().ActiveCount()==0);
    actor.Update(2.0F/120,{},*bullets);assert(actor.phaseFrames==2&&bullets->Pool().ActiveCount()==0);
    actor.Update(1.0F/120,{},*bullets);assert(bullets->Pool().ActiveCount()==4);
    Equal(static_cast<float>(actor.animationAge),5.0F/120,"120.animationSeconds");

    // 父内容时钟在子行为期间冻结；半帧更新不能把等待 1 帧误解为等待 1 次更新。
    const auto child=[](behavior::Actor& a)->behavior::Routine {
        co_await a.At(1);a.soundEvents.push_back(2);
    };
    const auto parent=[&child](behavior::Actor& a)->behavior::Routine {
        co_await a.At(1);a.soundEvents.push_back(1);co_await child(a);
        co_await a.At(2);a.soundEvents.push_back(3);a.finished=true;
    };
    actor.Reset({},1,1);actor.Start(parent(actor));
    for(int tick=1;tick<=7;++tick) {
        actor.Update(1.0F/120,{},*bullets);
        if(tick==3||tick==5||tick==7)assert(actor.soundEvents==std::vector{(tick-1)/2});
        else assert(actor.soundEvents.empty());
    }
    assert(actor.finished);

    const behavior::Entry end=[](behavior::Actor& a)->behavior::Routine { a.finished=true;co_return; };
    actor.Reset({},1,1);actor.Start(behavior::Follow(actor,{}));actor.timeoutFrames=2;actor.timeoutCallback=end;
    actor.Update(4.0F/120,{},*bullets);assert(actor.phaseFrames==2&&!actor.pendingCallback);
    actor.Update(5.0F/120,{},*bullets);assert(actor.pendingCallback==end);
    actor.Transition();actor.Update(0,{},*bullets);assert(actor.finished); // 回调之后剩余的更新步须保留。

    behavior::ActorPool<1> pool;auto config=testing::SpawnAt();
    auto* pooled=pool.Spawn(config);pooled->Update(1.0F/120,{},*bullets);assert(pooled->phaseFrames==0.5);
    pooled->active=false;pooled=pool.Spawn(config);
    pooled->Update(1.0F/120,{},*bullets);assert(pooled->phaseFrames==0.5); // 复用仍为 120 Hz，时钟从零开始。

    // 不同显示帧率必须执行相同的 120 Hz 步序列，不能由显示帧长改变曲线或技能次数。
    for(const auto& recipe:kPatternsBySub) {
        Vector3 expectedPosition{},expectedVelocity{};
        int expectedBullets=0;
        for(int fps:{120,30,60,144,240}) {
            bullets->Reset();actor.Reset({2,3,4},32,123);ConfigureBehavior(actor);
            actor.Start(behavior::Follow(actor,recipe));
            for(int frame=0;frame<4*fps;++frame)actor.Update(1.0F/fps,{0,1,-6},*bullets);
            assert(actor.phaseFrames==240);
            if(fps==120) {
                expectedPosition=actor.Position();expectedVelocity=actor.motion.velocity;
                expectedBullets=static_cast<int>(bullets->Pool().ActiveCount());
            } else {
                Equal(actor.Position(),expectedPosition,"120.renderFpsPosition");
                Equal(actor.motion.velocity,expectedVelocity,"120.renderFpsVelocity");
                assert(static_cast<int>(bullets->Pool().ActiveCount())==expectedBullets);
            }
        }
    }
    movement::State interpolated;interpolated.Interpolate(60,{12,6,-3},1);
    for(int tick=0;tick<60;++tick) { interpolated.BeginFrame();interpolated.EndFrame(); }
    assert(interpolated.position.x>5&&interpolated.position.x<7); // 60 次 120 Hz 更新只经过半秒。
    for(int tick=0;tick<60;++tick) { interpolated.BeginFrame();interpolated.EndFrame(); }
    Equal(interpolated.position,{12,6,-3},"120.interpolationDuration");
    std::cout<<"Validated default 120 Hz behavior at 30/60/120/144/240 FPS; content timing remains 60 Hz\n";
}
} // namespace
/// @brief 运行无窗口行为差分，失败输出原子程序与首次不一致帧；不读取原游戏安装或解包缓存。
void RunBehaviorTests() {
    TestWorldCoordinates();TestRuntime120Hz();TestRecipes();TestTransitions();TestBirths();TestCompositionAndOwnership();
    for(int sub:{0,1,2,3,4,5,6,7,8,9,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}) {
        if(!covered[sub]) { std::cerr<<"Uncovered original Sub"<<sub<<'\n';std::abort(); }
    }
    std::cout<<"Validated 28 reachable Normal subprograms and 222 StageEvent births against the original interpreter; max world position error "<<maxPositionError<<'\n';
}

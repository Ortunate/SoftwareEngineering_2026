// 本文件使用重构前 ECL 解释器作为独立基准，逐帧对比组合行为的轨迹、动画、技能和阶段边界。
#include "actor_fixture.hpp"
#include "reference_script.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage1/behaviors.hpp"
#include "gameplay/data/native_combat.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <limits>
#include <tuple>
#include <vector>
namespace {
using namespace koumakyou;
using namespace koumakyou::stage1;
int currentSub=0,currentStep=0;
std::array<bool,38> covered{};
// 原编号只用于定位差分测试的旧指令；生产代码直接引用这五个具名常量。
constexpr std::array kPatternsBySub{kTurningFairy,kGentleTurningFairy,kLateTurningFairy,
                                        kShootingDarkFairy,kPassingDarkFairy};
void Equal(float a,float b,const char* field) {
    if(std::abs(a-b)>0.00001F) {
        std::cerr<<"behavior mismatch Sub"<<currentSub<<" step "<<currentStep<<" "<<field<<": "<<a<<" != "<<b<<'\n';
        std::abort();
    }
}
void Equal(Vector3 a,Vector3 b,const char* field) { Equal(a.x,b.x,field);Equal(a.y,b.y,field);Equal(a.z,b.z,field); }
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
        Equal(x.position,y.position,"bullet.position");Equal(x.velocity,y.velocity,"bullet.velocity");
        Equal(x.baseVelocity,y.baseVelocity,"bullet.baseVelocity");Equal(x.acceleration,y.acceleration,"bullet.acceleration");
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
    behavior::Actor replacement;
    BulletSystem originalBullets,replacementBullets;
    Pair(int sub,unsigned seed=1,bool mirrored=false) {
        currentSub=sub;
        const float hp=sub<5 ? 256 : sub==9 ? 6000 : sub==26 ? 900 : sub==29 ? 800 : 7000;
        const Vector3 pos=sub<5||sub==9 ? Vector3{60,-32,0} : Vector3{192,96,0};
        original.Reset(sub,pos,hp,seed,mirrored);replacement.Reset(pos,hp,seed,mirrored);ConfigureBehavior(replacement);
        if(sub<5)replacement.Start(behavior::Follow(replacement,kPatternsBySub.at(sub)));
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
        Equal(original.sourcePosition,replacement.motion.position,"actor.position");
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
/// @brief 逐条核对 StageEvent 到配方的映射与工厂实际实例，保留原始出生参数和独立随机横坐标。
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
        Equal(ReferencePosition(expectedSource),p.position,"spawn.initialPosition");
        assert(behavior::Valid(p.pattern));
        auto enemy=testing::MakeActor(spawn->actor);
        auto original=std::make_unique<ReferenceScript>();
        Vector3 source=ReferenceSourcePosition(ReferencePosition(expectedSource));
        if(p.randomX)source.x=static_cast<float>((p.seed*1664525U+1013904223U)>>8)/16777216.0F*384;
        original->Reset(r.targetSub,source,p.sourceHealth,p.seed,p.mirrored);
        auto a=std::make_unique<BulletSystem>(),b=std::make_unique<BulletSystem>();
        bool entered=false;
        for(int step=0;step<260&&enemy->active;++step) {
            original->Update(1.0F/60,{2,3,-6},*a);enemy->Update(1.0F/60,{2,3,-6},*b);
            currentSub=r.targetSub;currentStep=step;
            Equal(original->Position(),enemy->position,"spawn.position");EqualBullets(*a,*b);
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
    params.position=ReferencePosition({192,96,0});
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

    auto retreat=testing::SpawnAt(ReferencePosition({192,460,0}));
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
} // namespace
/// @brief 运行无窗口行为差分，失败输出原子程序与首次不一致帧；不读取原游戏安装或解包缓存。
void RunBehaviorTests() {
    TestRecipes();TestTransitions();TestBirths();TestCompositionAndOwnership();
    for(int sub:{0,1,2,3,4,5,6,7,8,9,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}) {
        if(!covered[sub]) { std::cerr<<"Uncovered original Sub"<<sub<<'\n';std::abort(); }
    }
    std::cout<<"Validated 28 reachable Normal subprograms and 222 StageEvent births against the original interpreter\n";
}

// 本文件以源像素公式、原始etama4脚本及独立随机输入验证外部粒子的世界轨迹、半帧时钟与池生命周期。
#include "gameplay/effects/external_effects.hpp"
#include "gameplay/actors/behavior.hpp"
#include "resources/battle_catalog.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "reference_external_effects.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <limits>
#include <numbers>
#include <vector>

/// @brief 运行独立附着源矩阵及生命周期回归，与外部粒子专项共用入口。
void RunAttachedEffectTests();
namespace {
using namespace koumakyou;
using Kind=effects::Kind;
constexpr float scale=20.0F/448;
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
float Random(std::uint32_t& seed) {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return static_cast<float>(seed>>8)/16777216.0F;}
void Near(float a,float b) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>5e-4F)std::cerr<<"External particle value "<<a<<" vs "<<b<<'\n';
    assert(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=5e-4F);
}
anm::Archive Archive() {return anm::LoadArchive(std::filesystem::path(KOUMAKYOU_ASSET_SOURCE_ROOT)/battle::Describe(battle::Atlas::BulletEffects).anm);}
void TestSourceMotion() {
    const auto archive=Archive();
    for(Kind kind:{Kind::Burst,Kind::Charge,Kind::SlowCharge,Kind::Drift,Kind::Graze})for(std::uint32_t seed:{1U,7U,0x9e3779b9U}) {
        effects::ExternalEffectSystem pool(60);pool.Clear(seed);pool.Bind(archive,1);
        assert(pool.Spawn({kind,World({160,90,-20}),1,{200,40,80,240}})==1);
        pool.ForEachActive([](const auto& p){assert(p.animation.Sample().color.a==240&&p.animation.Sample().color.r==200);});
        auto random=seed;const int lifetime=kind==Kind::Burst?40:kind==Kind::Charge?60:kind==Kind::Drift?120:kind==Kind::Graze?30:240;
        float x=160,y=90,vx=0,vy=0,ax=0,ay=0,angle=0;
        if(kind==Kind::Burst) {vx=(Random(random)*256-128)*4/33;vy=(Random(random)*256-128)*4/33;ax=-vx*.05F;ay=-vy*.05F;}
        else if(kind==Kind::Drift||kind==Kind::Graze) {
            Random(random); // 创建时spriteRand消费一个池种子，运动初始化仍在后续首步读取。
            const float divisor=kind==Kind::Graze?12:60,deceleration=kind==Kind::Graze?19:95;
            vx=(Random(random)*256-128)/divisor;vy=(Random(random)*256-128)/divisor;ax=-vx/deceleration;ay=-vy/deceleration;
        } else angle=Random(random)*2*std::numbers::pi_v<float>-std::numbers::pi_v<float>;
        for(int frame=0;frame<lifetime;++frame) {
            if(kind==Kind::Burst||kind==Kind::Drift||kind==Kind::Graze) {x+=vx;y+=vy;vx+=ax;vy+=ay;}
            else {const float radius=256-static_cast<float>(frame)*256/lifetime;x=160+std::cos(angle)*radius;y=90+std::sin(angle)*radius;}
            pool.Update(1.0F/60);
            if(frame==lifetime-1) {assert(pool.ActiveCount()==0);break;}
            assert(pool.ActiveCount()==1);
            pool.ForEachActive([&](const auto& p) {
                const auto point=World({x,y,-20});Near(p.position.x,point.x);Near(p.position.y,point.y);Near(p.position.z,point.z);
                assert(p.frame==frame+1&&p.animation.Sample().color.r==200&&p.animation.Sample().color.g==40&&p.animation.Sample().color.b==80);
                if(kind==Kind::Burst||kind==Kind::Drift||kind==Kind::Graze) {Near(p.velocity.x,vx*60*scale);Near(p.velocity.z,-vy*60*scale);}
            });
        }
        assert(pool.Source()==&archive);pool.Clear();assert(pool.Source()==nullptr);
    }
}
void TestHalfFramesAndFps() {
    const auto archive=Archive();
    for(Kind kind:{Kind::Charge,Kind::SlowCharge}) {
        effects::ExternalEffectSystem pool;pool.Bind(archive,1);pool.Spawn({kind,{0,2,3},1,WHITE});
        pool.Update(1.0F/120);Vector3 first{};pool.ForEachActive([&](const auto& p){first=p.position;assert(p.frame==.5&&p.animation.Sample().color.a==255);});
        pool.Update(1.0F/120);pool.ForEachActive([&](const auto& p){
            assert(p.frame==1&&p.position.y==2&&p.animation.Sample().color.a==(kind==Kind::Charge?6:0));
            assert(std::hypot(p.position.x,p.position.z-3)<std::hypot(first.x,first.z-3));
        });
        const int duration=kind==Kind::Charge?120:480;
        for(int n=2;n<duration;++n)pool.Update(1.0F/120);
        assert(pool.ActiveCount()==0);
    }
    std::vector<Vector3> expected;
    for(int fps:{30,60,120,144,240}) {
        effects::ExternalEffectSystem pool;pool.Clear(7);pool.Bind(archive,1);pool.Spawn({Kind::Charge,{1,2,3},6,WHITE});
        for(int n=0;n<fps/2;++n)pool.Update(1.0F/fps);
        std::vector<Vector3> actual;pool.ForEachActive([&](const auto& p){actual.push_back(p.position);assert(p.frame==30);});
        assert(actual.size()==6);
        if(expected.empty())expected=actual;
        else for(std::size_t n=0;n<actual.size();++n) {Near(actual[n].x,expected[n].x);Near(actual[n].z,expected[n].z);}
    }
}
/// @brief 漂散按半内容帧积分、整数ANM时钟旋转淡出；颜色覆盖不能抹掉脚本已建立的alpha插值基准。
void TestDriftHalfFramesAndFps() {
    const auto archive=Archive();
    for(int hz:{60,120})for(std::uint32_t seed:{1U,7U,0x9e3779b9U}) {
        effects::ExternalEffectSystem pool(hz);pool.Clear(seed);pool.Bind(archive,1);
        pool.Spawn({Kind::Drift,World({160,90,-20}),1,{0,0,1,0}});
        auto random=seed;Random(random);float vx=(Random(random)*256-128)/60,vy=(Random(random)*256-128)/60;
        const float ax=-vx/95,ay=-vy/95,step=60.0F/hz;float x=160,y=90;
        int sprite=-1;pool.ForEachActive([&](const auto& p){assert(!p.initialized&&p.animation.Sample().color.a==0);sprite=p.animation.Sample().sprite;});
        for(int n=0;n<hz*2;++n) {
            x+=vx*step;y+=vy*step;vx+=ax*step;vy+=ay*step;pool.Update(1.0F/hz);
            if(n==hz*2-1) {assert(!pool.ActiveCount());break;}
            pool.ForEachActive([&](const auto& p) {
                const auto point=World({x,y,-20});Near(p.position.x,point.x);Near(p.position.z,point.z);Near(p.position.y,point.y);
                assert(p.animation.ScriptId()==21&&p.animation.Sample().sprite==sprite);
                const auto color=p.animation.Sample().color;assert(color.r==0&&color.g==0&&color.b==1);
                const int frame=static_cast<int>(p.frame);
                assert(color.a==(frame?static_cast<unsigned char>(128*(1-(frame+1)/120.0F)):0));
            });
        }
    }
    std::vector<Vector3> expected;
    for(int fps:{120,30,60,144,240}) {
        effects::ExternalEffectSystem pool;pool.Clear(7);pool.Bind(archive,1);pool.Spawn({Kind::Drift,{1,2,3},32,{0,0,1,0}});
        for(int n=0;n<fps;++n)pool.Update(1.0F/fps);
        std::vector<Vector3> actual;std::array<bool,4> seen{};
        pool.ForEachActive([&](const auto& p){assert(p.frame==60);actual.push_back(p.position);seen.at(p.animation.Sample().sprite-7)=true;});
        assert(actual.size()==32);for(bool value:seen)assert(value);
        if(fps==120)expected=actual;
        else for(std::size_t n=0;n<actual.size();++n){Near(actual[n].x,expected[n].x);Near(actual[n].z,expected[n].z);}
    }
}
/// @brief 擦弹的原始像素公式独立积分；检查半帧、30帧寿命、固定加速度反向及创建后颜色覆盖。
void TestGrazeHalfFramesAndFps() {
    const auto archive=Archive();
    for(int hz:{60,120})for(std::uint32_t seed:{1U,7U,0x9e3779b9U}) {
        effects::ExternalEffectSystem pool(hz);pool.Clear(seed);pool.Bind(archive,1);
        pool.Spawn({Kind::Graze,World({160,90,-20}),1,WHITE});
        auto random=seed;Random(random);
        float vx=(Random(random)*256-128)/12,vy=(Random(random)*256-128)/12;
        const float ax=-vx/19,ay=-vy/19,step=60.0F/hz;float x=160,y=90;
        int sprite=-1;pool.ForEachActive([&](const auto& p){
            assert(!p.initialized&&p.frame==0&&p.animation.Sample().color.a==255);
            sprite=p.animation.Sample().sprite;assert(sprite>=7&&sprite<=10);
        });
        for(int n=0;n<hz/2;++n) {
            x+=vx*step;y+=vy*step;vx+=ax*step;vy+=ay*step;pool.Update(1.0F/hz);
            if(n==hz/2-1) {assert(!pool.ActiveCount());break;}
            assert(pool.ActiveCount()==1);
            pool.ForEachActive([&](const auto& p){
                const auto point=World({x,y,-20});Near(p.position.x,point.x);Near(p.position.y,point.y);Near(p.position.z,point.z);
                Near(p.velocity.x,vx*60*scale);Near(p.velocity.z,-vy*60*scale);
                const auto& sample=p.animation.Sample();const int frame=static_cast<int>(p.frame);
                assert(p.animation.ScriptId()==13&&sample.sprite==sprite&&sample.additive&&sample.disableZWrite);
                assert(sample.color.r==255&&sample.color.g==255&&sample.color.b==255);
                assert(sample.color.a==(frame?static_cast<unsigned char>(128*(1-(frame+1)/30.0F)):255));
                Near(sample.scale.x,.7F-.023333333432674408F*(frame+1));
                Near(sample.rotation.z,std::remainder(.6F*(frame+1),2*std::numbers::pi_v<float>));
            });
        }
        // 失活槽复用不能残留上次的速度、动画年龄或颜色。
        assert(pool.Spawn({Kind::Graze,{1,2,3},1,WHITE})==1);
        pool.ForEachActive([](const auto& p){assert(!p.initialized&&p.frame==0&&p.velocity.x==0&&p.animation.Sample().color.a==255);});
        pool.Clear();assert(!pool.Source()&&!pool.ActiveCount());
    }
    std::vector<Vector3> expected;
    for(int fps:{120,30,60,144,240}) {
        effects::ExternalEffectSystem pool;pool.Clear(7);pool.Bind(archive,1);pool.Spawn({Kind::Graze,{1,2,3},32,WHITE});
        // 共同采样点为1/6秒；五种FPS均落在整数显示帧与20个120Hz步上。
        for(int n=0;n<fps/6;++n)pool.Update(1.0F/fps);
        std::vector<Vector3> actual;std::array<bool,4> seen{};
        pool.ForEachActive([&](const auto& p){assert(p.frame==10);actual.push_back(p.position);seen.at(p.animation.Sample().sprite-7)=true;});
        assert(actual.size()==32);for(bool value:seen)assert(value);
        if(fps==120)expected=actual;
        else for(std::size_t n=0;n<actual.size();++n){Near(actual[n].x,expected[n].x);Near(actual[n].z,expected[n].z);}
        for(int n=fps/6;n<fps/2;++n)pool.Update(1.0F/fps);
        assert(!pool.ActiveCount()&&!pool.DroppedSpawns());
    }
}
void TestPoolAndFailures() {
    const auto archive=Archive();effects::ExternalEffectSystem pool;
    bool rejected=false;try {pool.Spawn({});}catch(const std::logic_error&) {rejected=true;}assert(rejected);
    pool.Bind(archive,1);assert(pool.Spawn({Kind::Burst,{},600,WHITE})==512);assert(pool.ActiveCount()==512&&pool.DroppedSpawns()==88);
    assert(pool.Spawn({Kind::Flash,{},1,WHITE})==0&&pool.DroppedSpawns()==89);
    rejected=false;try {pool.Bind(archive,1);}catch(const std::logic_error&) {rejected=true;}assert(rejected);
    for(int n=0;n<80;++n)pool.Update(1.0F/120);assert(pool.ActiveCount()==0);
    assert(pool.Spawn({Kind::Flash,{1,2,3},1,WHITE})==1);
    pool.ForEachActive([](const auto& p){assert(p.kind==Kind::Flash&&p.frame==0&&!p.initialized&&p.velocity.x==0&&p.animation.Sample().sprite==23);});
    for(float dt:{-1.0F,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        rejected=false;try {pool.Update(dt);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
    for(effects::Request request:{effects::Request{Kind::Charge,{},0},effects::Request{static_cast<Kind>(99),{},1},
                                  effects::Request{Kind::Flash,{NAN,0,0},1}}) {
        rejected=false;try {pool.Spawn(request);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected&&pool.ActiveCount()==1&&pool.DroppedSpawns()==89);
    }
    pool.Clear();assert(pool.Source()==nullptr&&pool.ActiveCount()==0&&pool.DroppedSpawns()==0);
    behavior::Actor actor;rejected=false;try {actor.EmitEffect(Kind::Charge,6,WHITE);}catch(const std::logic_error&) {rejected=true;}assert(rejected);
}
void TestActorRequestFrameRates() {
    for(int fps:{30,60,120,144,240}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,5.71428585F},6000,7);
        bosses::FlandreBoss::Behaviors::ConfigurePractice(actor,false);
        actor.Start(bosses::FlandreBoss::Behaviors::RipplesOf495Years(actor));
        std::vector<effects::Request> requests;
        for(int tick=0;tick<fps*4;++tick) {
            actor.Update(1.0F/fps,{2,0,-6},*bullets);
            for(const auto& request:actor.effectEvents)if(request.kind==Kind::Charge)requests.push_back(request);
        }
        assert(requests.size()==30);
        for(int n=0;n<30;++n) {
            assert(requests[n].contentFrame==120+n*2&&requests[n].count==6&&requests[n].kind==Kind::Charge);
            assert(requests[n].color.r==128&&requests[n].color.g==128&&requests[n].color.b==255&&requests[n].color.a==255);
        }
        actor.Reset({},1,7);assert(actor.effectEvents.empty());
    }
}
void TestAmbientMotion() {
    const auto archive=Archive();
    for(int hz:{60,120}) {
        effects::ExternalEffectSystem pool(hz);pool.Bind(archive,1);
        const Vector3 sourcePosition{160,90,-20},velocity{.23F,-.75F,.125F},acceleration{-.004F,.013F,-.002F};
        const effects::Request request{Kind::Ambient,World(sourcePosition),1,{48,48,255,255},0,
            {velocity.x*60*scale,-velocity.z*60*scale,-velocity.y*60*scale},
            {acceleration.x*3600*scale,-acceleration.z*3600*scale,-acceleration.y*3600*scale}};
        assert(pool.Spawn(request)==1);
        std::vector<int> sprites;
        pool.ForEachActive([&](const auto& p){assert(p.velocity.y!=0&&p.animation.ScriptId()==19);sprites.push_back(p.animation.Sample().sprite);Near(p.animation.Sample().scale.x,2.975F);});
        auto point=sourcePosition,v=velocity;const float dt=60.0F/hz;
        for(int frame=0;frame<hz*2;++frame) {
            point.x+=v.x*dt;point.y+=v.y*dt;point.z+=v.z*dt;
            v.x+=acceleration.x*dt;v.y+=acceleration.y*dt;v.z+=acceleration.z*dt;
            pool.Update(1.0F/hz);
            if(frame==hz*2-1) {assert(pool.ActiveCount()==0);break;}
            assert(pool.ActiveCount()==1);
            pool.ForEachActive([&](const auto& p){
                const auto expected=World(point);Near(p.position.x,expected.x);Near(p.position.y,expected.y);Near(p.position.z,expected.z);
                Near(p.velocity.x,v.x*60*scale);Near(p.velocity.y,-v.z*60*scale);Near(p.velocity.z,-v.y*60*scale);
                const auto& anm=p.animation.Sample();assert(anm.sprite==sprites[0]&&anm.sprite>=7&&anm.sprite<=10);
                assert(anm.additive&&anm.disableZWrite&&anm.color.r==48&&anm.color.g==48&&anm.color.b==255);
                const auto wholeFrame=static_cast<int>(p.frame);
                // Start已执行内容帧0的scaleSpeed，之后只在整60Hz动画帧再缩一次。
                Near(anm.scale.x,2.975F-wholeFrame*.025F);Near(anm.scale.y,2.975F-wholeFrame*.025F);
            });
        }
    }
    // spriteRand必须逐实例重放并覆盖四个原始精灵；只比较视觉局部随机流，不声称原生全局RNG等价。
    effects::ExternalEffectSystem a,b;a.Bind(archive,1);b.Bind(archive,1);
    a.Spawn({Kind::Ambient,{},64});b.Spawn({Kind::Ambient,{},64});
    std::vector<int> left,right;std::array<bool,4> seen{};
    a.ForEachActive([&](const auto& p){left.push_back(p.animation.Sample().sprite);seen.at(left.back()-7)=true;});
    b.ForEachActive([&](const auto& p){right.push_back(p.animation.Sample().sprite);});assert(left==right);
    for(bool value:seen)assert(value);
    auto invalid=effects::Request{Kind::Ambient};invalid.acceleration.y=INFINITY;
    bool rejected=false;try {a.Spawn(invalid);}catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected&&a.ActiveCount()==64&&!a.DroppedSpawns());
}
void TestAmbientSource() {
    using Boss=bosses::FlandreBoss;
    struct Case {behavior::Entry entry;int source;};
    const Case cases[]{{Boss::Behaviors::FirstNonspell,38},{Boss::Behaviors::SecondNonspell,43},
        {Boss::Behaviors::ThirdNonspell,48},{Boss::Behaviors::FourthNonspell,55},{Boss::Behaviors::FifthNonspell,61},
        {Boss::Behaviors::SixthNonspell,65},{Boss::Behaviors::SeventhNonspell,71},{Boss::Behaviors::EighthNonspell,76},
        {Boss::Behaviors::CranberryTrap,39},{Boss::Behaviors::Laevatein,44},{Boss::Behaviors::MazeOfLove,62},{Boss::Behaviors::ClockOfThePast,77}};
    for(const auto& c:cases)for(std::uint32_t seed:{7U,0x9e3779b9U}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
        actor.Reset(World({192,96,0}),6000,seed);Boss::Behaviors::ConfigurePractice(actor,false);actor.SetRank(18);actor.Start(c.entry(actor));
        stage_extra::reference::FlandreReference source(c.source,{192,96,0},seed,18,6000);
        int ambient=0;
        for(int n=0;n<420;++n) {
            source.Tick({240,350,0});actor.Update(1.0F/60,World({240,350,0}),*bullets);
            stage_extra::reference::CompareEffects(actor,source);ambient+=static_cast<int>(source.ambientEffects.size());bullets->Reset();
        }
        assert(ambient>300&&actor.Random(1)==source.Random(1));
    }
}
behavior::Routine AmbientRoutine(behavior::Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Active);
    actor.SetStepEffect([](behavior::Actor& a){a.soundEvents.push_back(99);});
    co_await actor.At(1000);
}
behavior::Routine AmbientWait(behavior::Actor& actor) {co_await actor.At(1000);}
behavior::Routine LivingAmbientRoutine(behavior::Actor& actor) {
    actor.SetAmbientEffects(effects::AmbientMode::Living);co_await actor.At(1000);
}
void TestAmbientClocksAndLifecycle() {
    std::vector<effects::Request> expected;
    for(int fps:{30,60,120,144,240}) {
        auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({1,2,3},1,7);actor.Start(AmbientRoutine(actor));
        std::vector<effects::Request> actual;int steps=0;
        for(int n=0;n<fps*5/2;++n) {
            actor.Update(1.0F/fps,{},*bullets);steps+=static_cast<int>(actor.soundEvents.size());
            actual.insert(actual.end(),actor.effectEvents.begin(),actor.effectEvents.end());
        }
        assert(steps==300&&actual.size()==140&&actor.phaseFrames==150);
        if(expected.empty())expected=actual;
        else for(std::size_t i=0;i<actual.size();++i) {
            assert(actual[i].contentFrame==expected[i].contentFrame);
            Near(actual[i].position.x,expected[i].position.x);Near(actual[i].position.z,expected[i].position.z);
            Near(actual[i].velocity.x,expected[i].velocity.x);Near(actual[i].velocity.z,expected[i].velocity.z);
        }
        actor.Reset({},1,7);actor.Start(AmbientWait(actor));actor.Update(1.0F/120,{},*bullets);assert(actor.effectEvents.empty());
    }
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60),baseline(behavior::UpdateRate::Hz60);
    actor.Reset({},1,7);baseline.Reset({},1,7);actor.Start(AmbientRoutine(actor));baseline.Start(AmbientRoutine(baseline));
    for(int i=0;i<4;++i) {actor.Update(1.0F/60,{},*bullets);baseline.Update(1.0F/60,{},*bullets);}
    // 隐藏时连相位和视觉随机流一起冻结；恢复后与没有推进的独立实例一致。
    actor.visible=false;for(int i=0;i<20;++i) {actor.Update(1.0F/60,{},*bullets);assert(actor.effectEvents.empty());}
    actor.visible=true;actor.Start(AmbientWait(actor));
    for(int i=0;i<10;++i) {
        actor.Update(1.0F/60,{},*bullets);baseline.Update(1.0F/60,{},*bullets);
        assert(actor.soundEvents.empty()&&actor.effectEvents.size()==baseline.effectEvents.size());
        for(std::size_t j=0;j<actor.effectEvents.size();++j) {
            Near(actor.effectEvents[j].position.x,baseline.effectEvents[j].position.x);Near(actor.effectEvents[j].position.z,baseline.effectEvents[j].position.z);
            assert(actor.effectEvents[j].velocity.x==baseline.effectEvents[j].velocity.x);
        }
    }
    assert(actor.Random(1)==baseline.Random(1));
    actor.Start(LivingAmbientRoutine(actor));actor.health=0;
    for(int i=0;i<10;++i) {actor.Update(1.0F/60,{},*bullets);assert(actor.effectEvents.empty());}
    actor.stageClearResponse=behavior::StageClearResponse::Remove;actor.ClearFromStage();assert(!actor.active&&actor.finished);
    actor.Reset({},1,7);actor.Start(AmbientRoutine(actor));actor.Update(1.0F/60,{},*bullets);assert(actor.effectEvents.empty());
    for(int i=1;i<=8;++i)actor.Update(1.0F/60,{},*bullets);
    assert(actor.effectEvents.size()==2&&actor.effectEvents[0].contentFrame==8);
    effects::AmbientEmitter emitter,copy;
    bool rejected=false;try {static_cast<void>(emitter.Step(.25,{},0));}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    assert(emitter.Step(1,{},0).empty()&&copy.Step(1,{},0).empty());
    for(int i=1;i<8;++i) {assert(emitter.Step(1,{},i).empty());assert(copy.Step(1,{},i).empty());}
    const auto a=emitter.Step(1,{},8),b=copy.Step(1,{},8);assert(a.size()==2&&b.size()==2&&a[0].velocity.x==b[0].velocity.x);
}
}
/// @brief 核心与专项入口共享真实素材和独立源公式，不启动窗口或读取原作安装路径。
void RunExternalEffectTests() {TestSourceMotion();TestHalfFramesAndFps();TestDriftHalfFramesAndFps();TestGrazeHalfFramesAndFps();TestPoolAndFailures();TestActorRequestFrameRates();TestAmbientMotion();TestAmbientSource();TestAmbientClocksAndLifecycle();RunAttachedEffectTests();}

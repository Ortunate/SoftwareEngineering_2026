// 本文件以源像素四元数矩阵独立核对附着效果13的三维轨道、ANM覆盖、释放和演员/池复用。
#include "gameplay/actors/behavior.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "resources/battle_catalog.hpp"
#include "reference_external_effects.hpp"
#include "stages/stage_extra/boss_config.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>

namespace {
using namespace koumakyou;
constexpr float scale=20.0F/448,pi=std::numbers::pi_v<float>;
Vector3 World(Vector3 p) {return {(p.x-192)*scale,-p.z*scale,10-p.y*scale};}
void Near(float a,float b) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>5e-4F) {
        std::cerr<<"Attached source value "<<a<<" vs "<<b<<'\n';std::abort();
    }
}
anm::Archive Archive() {return anm::LoadArchive(std::filesystem::path(KOUMAKYOU_ASSET_SOURCE_ROOT)/battle::Describe(battle::Atlas::BulletEffects).anm);}
// 独立保留源XYZ、行向量矩阵及源Z放大；不调用生产旋转/坐标工具或读取其轨道计算结果。
Vector3 SourcePosition(Vector3 center,Vector3 axis,float radius,float phase) {
    const float length=std::sqrt(axis.x*axis.x+axis.y*axis.y+axis.z*axis.z);
    if(length>1e-6F)axis={axis.x/length,axis.y/length,axis.z/length};else axis={};
    const float sine=std::sin(phase);float x=axis.x*sine,y=axis.y*sine,z=axis.z*sine,w=std::cos(phase);
    const float norm=std::sqrt(x*x+y*y+z*z+w*w);
    if(norm>1e-6F) {x/=norm;y/=norm;z/=norm;w/=norm;}
    const float matrix[3][3]{{1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w)},
        {2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w)},
        {2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y)}};
    float rx=axis.y,ry=-axis.x;
    const float squared=rx*rx+ry*ry;
    if(squared>=1e-5F) {const float inverse=1/std::sqrt(squared);rx*=inverse;ry*=inverse;}
    rx*=radius;ry*=radius;
    return {center.x+rx*matrix[0][0]+ry*matrix[1][0],center.y+rx*matrix[0][1]+ry*matrix[1][1],
        center.z+6*(rx*matrix[0][2]+ry*matrix[1][2])};
}
// 原始ANM18在60设255，120/180开始相反方向的60帧插值，240跳回偏移36的60帧指令。
struct SourceAlpha {
    int clock=1,time=0,duration=0,start=0,target=0;unsigned char alpha=255;
    void Tick() {
        if(clock==240)clock=60;
        if(clock==60)alpha=255;
        if(clock==120||clock==180) {start=alpha;target=clock==120?64:255;duration=60;time=0;}
        if(duration) {const float t=static_cast<float>(++time)/duration;alpha=static_cast<unsigned char>(start+(target-start)*t);if(time==duration)duration=0;}
        ++clock;
    }
};
effects::Request Attached(const std::shared_ptr<effects::AttachmentAnchor>& anchor,Vector3 sourceAxis) {
    effects::Request request{effects::Kind::Attached,anchor->Position(),1,{224,224,255,255}};
    request.axis={sourceAxis.x,-sourceAxis.z,-sourceAxis.y};request.attachment=anchor;return request;
}
void TestSourceOrbitAndFade() {
    const auto archive=Archive();
    for(int hz:{60,120})for(Vector3 axis: {Vector3{-.3F,-.7F,-.3F},Vector3{-1,-.7F,-.3F},Vector3{.4F,-.7F,-.3F},
            Vector3{1.1F,-.7F,-.3F},Vector3{},Vector3{0,0,1},Vector3{.001F,0,1}})for(int release:{0,119,150,190,240,500}) {
        effects::ExternalEffectSystem pool(hz);pool.Bind(archive,1);
        auto anchor=std::make_shared<effects::AttachmentAnchor>();anchor->Follow(World({192,96,-12}),48*scale);
        pool.Spawn(Attached(anchor,axis));SourceAlpha animation;
        float radius=0,phase=0;Vector3 center{192,96,-12};const float step=60.0F/hz;const int factor=hz/60;
        pool.ForEachActive([](const auto& p) {assert(p.animation.ScriptId()==18&&p.animation.Sample().sprite==24);Near(p.animation.Sample().rotation.z,-.15F);});
        for(int n=0;n<(release+16)*factor;++n) {
            const bool fading=n>=release*factor;
            if(!fading) {
                center={192+30*std::sin(n*.01F),96+20*std::cos(n*.013F),-12+6*std::sin(n*.03F)};
                const float target=n<100*factor?48:n<200*factor?24:80;
                anchor->Follow(World(center),target*scale);
                if(radius<target)radius+=.30000001192092896F*step;
                phase+=.03141592815518379F*step;if(phase>pi)phase-=2*pi;
            } else {
                anchor.reset();animation.alpha=static_cast<unsigned char>((1-(n-release*factor+1)*step/16)*255);
            }
            if((n+1)%factor==0)animation.Tick();
            pool.Update(1.0F/hz);
            if(n==(release+16)*factor-1) {assert(!pool.ActiveCount());break;}
            assert(pool.ActiveCount()==1);
            const auto expected=World(SourcePosition(center,axis,radius,phase));
            pool.ForEachActive([&](const auto& p) {
                Near(p.position.x,expected.x);Near(p.position.y,expected.y);Near(p.position.z,expected.z);
                Near(p.orbitRadius,radius*scale);Near(p.orbitPhase,phase);
                const auto& frame=p.animation.Sample();
                if(frame.color.a!=animation.alpha)std::cerr<<"Attached alpha hz "<<hz<<" step "<<n<<" release "<<release<<": "<<int(frame.color.a)<<" vs "<<int(animation.alpha)<<'\n';
                assert(frame.color.a==animation.alpha&&frame.color.r==224&&frame.color.g==224&&frame.color.b==255);
                assert(frame.additive&&frame.disableZWrite&&frame.visible&&p.released==fading);
                const float size=fading?1+(n-release*factor+1)*step/16:1;Near(frame.scale.x,size);Near(frame.scale.y,size);
            });
        }
    }
}
void TestSlotReuseAndInvalidInput() {
    const auto archive=Archive();effects::ExternalEffectSystem pool(60);pool.Bind(archive,1);
    auto old=std::make_shared<effects::AttachmentAnchor>();old->Follow({1,2,3},1);
    const auto request=Attached(old,{-.3F,-.7F,-.3F});
    for(int n=0;n<512;++n)assert(pool.Spawn(request)==1);
    assert(!pool.Spawn(request)&&pool.DroppedSpawns()==1);pool.Update(1.0F/60);
    old.reset();for(int n=0;n<16;++n)pool.Update(1.0F/60);assert(!pool.ActiveCount());
    auto current=std::make_shared<effects::AttachmentAnchor>();current->Follow({4,5,6},0);
    for(int n=0;n<512;++n)assert(pool.Spawn(Attached(current,{-.3F,-.7F,-.3F}))==1);
    pool.Update(1.0F/60);pool.ForEachActive([](const auto& p) {
        Near(p.orbitRadius,.3F*scale);Near(p.orbitPhase,2*pi/100);assert(!p.released&&p.fadeFrames==0);
    });
    const auto invalid=[&](effects::Request r) {bool rejected=false;try {pool.Spawn(r);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected&&pool.ActiveCount()==512&&pool.DroppedSpawns()==1);};
    auto r=Attached(current,{});r.count=2;invalid(r);r.count=1;r.axis.x=NAN;invalid(r);
    bool rejected=false;try {current->Follow({NAN,0,0},1);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected&&current->Position().x==4&&current->Radius()==0);
    rejected=false;try {current->Follow({},-1);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected&&current->Position().x==4);
    pool.Clear();pool.Bind(archive,1);pool.Spawn(Attached(current,{}));pool.Update(1.0F/60);
    pool.ForEachActive([](const auto& p){assert(p.orbitRadius==0);Near(p.orbitPhase,pi/100);assert(p.position.x==4&&p.position.y==5&&p.position.z==6);});
}
behavior::Routine Hold(behavior::Actor& actor) {co_await actor.At(1000);}
behavior::Routine Attach(behavior::Actor& actor) {
    actor.AttachEffect({-.3F,.3F,.7F},2.14285714F,{224,224,255,255});co_await actor.At(1000);
}
void TestActorLifecycleAndRequests() {
    const auto archive=Archive();auto bullets=std::make_unique<BulletSystem>();
    for(int route=0;route<6;++route) {
        effects::ExternalEffectSystem pool;pool.Bind(archive,1);auto actor=std::make_unique<behavior::Actor>();
        actor->Reset({1,2,3},100,7);actor->Start(Attach(*actor));actor->Update(1.0F/120,{},*bullets);
        const auto weak=actor->effectEvents[0].attachment;pool.Spawn(actor->effectEvents[0]);pool.Update(1.0F/120);
        actor->Start(Hold(*actor));actor->motion.position={3,4,5};actor->Update(1.0F/120,{},*bullets);pool.Update(1.0F/120);
        assert(!weak.expired());pool.ForEachActive([](const auto& p){assert(p.origin.x==3&&p.origin.y==4&&p.origin.z==5&&!p.released);});
        switch(route) {
            case 0:actor->Reset({},1,7);break;
            case 1:actor->stageClearResponse=behavior::StageClearResponse::Remove;actor->ClearFromStage();break;
            case 2:actor->visible=false;actor->Update(1.0F/120,{},*bullets);break;
            case 3:assert(actor->Defeat());break;
            case 4:actor->timeoutFrames=0;actor->timeoutCallback=[](behavior::Actor& a)->behavior::Routine{co_await a.At(1);};actor->Update(1.0F/120,{},*bullets);break;
            case 5:actor.reset();break;
        }
        assert(weak.expired());pool.Update(1.0F/120);pool.ForEachActive([](const auto& p){assert(p.released&&p.fadeFrames==.5);});
        for(int n=1;n<32;++n)pool.Update(1.0F/120);assert(!pool.ActiveCount());
    }
    for(const auto entry:{bosses::FlandreBoss::Behaviors::FirstNonspell,bosses::FlandreBoss::Behaviors::FinalPrelude}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset(World({192,96,0}),10000,7);
        bosses::FlandreBoss::Behaviors::ConfigurePractice(actor,false);actor.SetRank(18);actor.Start(entry(actor));
        const bool first=entry==bosses::FlandreBoss::Behaviors::FirstNonspell;
        stage_extra::reference::FlandreReference source(first?38:91,{192,96,0},7,18,10000);int count=0;
        for(int n=0;n<90;++n) {
            actor.Update(1.0F/60,{},*bullets);source.Tick({192,448,0});CompareEffects(actor,source);
            for(const auto& e:actor.effectEvents)if(e.kind==effects::Kind::Attached) {assert(e.contentFrame==(first?30+count*10:30));++count;}
            bullets->Reset();
        }
        assert(count==5);
    }
}
void TestBossThresholdAndDefeat() {
    const auto archive=Archive();auto bullets=std::make_unique<BulletSystem>();
    for(int route=0;route<3;++route) {
        effects::ExternalEffectSystem pool;pool.Bind(archive,1);bosses::FlandreBoss boss(behavior::UpdateRate::Hz120,false);
        auto config=stage_extra::FlandreMainConfig();config.intro={};boss.StartBoss(config);
        std::weak_ptr<const effects::AttachmentAnchor> anchor;
        for(int n=0;n<170;++n) {boss.Update(1.0F/120,{},*bullets);for(const auto& request:boss.EffectEvents()) {
            pool.Spawn(request);if(request.kind==effects::Kind::Attached)anchor=request.attachment;
        }pool.Update(1.0F/120);bullets->Reset();}
        assert(!anchor.expired());
        if(route==0)boss.health=1799; // 正生命预留阈值跳符卡，继续跟随同一附着状态。
        else if(route==1)boss.health=0;
        else {bool ready=false;for(int n=0;n<4000&&!ready;++n) {ready=boss.Update(1.0F/120,{},*bullets);bullets->Reset();}assert(ready);}
        assert(boss.AdvanceSkill());assert(anchor.expired()==(route!=0));
    }
}
}
/// @brief 由外部粒子专项/全量CTest调用，不启动图形窗口或读取本机原作路径。
void RunAttachedEffectTests() {TestSourceOrbitAndFade();TestSlotReuseAndInvalidInput();TestActorLifecycleAndRequests();TestBossThresholdAndDefeat();}

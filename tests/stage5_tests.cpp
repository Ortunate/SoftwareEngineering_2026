// 本文件检查五面出生、世界轨迹、Boss 源时钟、时停与随机顺序；完整渲染和资源生命周期另由图形目标验证。
#include "stages/stage5/data.hpp"
#include "stages/stage5/behaviors.hpp"
#include "stages/stage5/ids.hpp"
#include "stages/stage5/boss.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>;
void Near(float actual,float expected) {
    if(!std::isfinite(actual)||std::abs(actual-expected)>5e-4F) {std::cerr<<"Stage 5 mismatch "<<actual<<" vs "<<expected<<'\n';std::abort();}
}
unsigned Next(unsigned& random) {random^=random<<13;random^=random>>17;random^=random<<5;return random;}
int DialogueFrames(std::span<const DialogueLine> lines) {
    int frames=0;for(const auto& line:lines)frames+=static_cast<int>(std::lround(line.duration*60));return frames;
}
std::vector<const native_combat::HealthRecord*> Births() {
    std::vector<const native_combat::HealthRecord*> births;
    for(const auto& record:native_combat::Stage(5)->health)
        if(record.scope==native_combat::HealthScope::TimelineSpawn&&record.targetSub!=12&&record.targetSub!=23)births.push_back(&record);
    return births;
}
void TestBirthsMovementAndAttacks() {
    const auto births=Births();assert(births.size()==227&&stage5::Stage5Timeline().size()==234);
    auto bullets=std::make_unique<BulletSystem>();std::size_t index=0;std::array<bool,12> covered{};
    for(const auto& event:stage5::Stage5Timeline()) {
        const auto* spawn=std::get_if<SpawnActors>(&event.action);if(!spawn)continue;
        const auto& record=*births.at(index++);const int sub=record.targetSub;covered[sub]=true;auto parameters=spawn->actor;
        assert(behavior::Valid(parameters)&&spawn->count==1&&spawn->interval==0);
        assert(parameters.sourceHealth==*record.value&&parameters.sourceScore==record.score&&parameters.sourceItem==record.item);
        assert(parameters.mirrored==(record.opcode==2||record.opcode==6)&&parameters.randomX==(record.position.x==-999));
        if(record.frame<3272)assert(event.atFrame==record.frame);else assert(!event.atFrame);
        parameters.pattern.spatialShots=false;parameters.retireOutside=false;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(parameters);
        constexpr float scale=20.0F/448;
        float x=parameters.randomX ? static_cast<float>((parameters.seed*1664525U+1013904223U)>>8)/16777216.0F*384 : record.position.x;
        float y=record.position.y,speed=sub==1||sub>=9 ? 2.5F : 2,angle=sub==1||sub>=9 ? 0 : pi/2,acceleration=0,vx=0,vy=0;
        unsigned random=parameters.seed;int next=-1,interval=0,count=0;
        if(sub==1||sub>=9) {interval=sub==1 ? 60 : sub==9 ? 40 : sub==10 ? 30 : 10;next=interval-static_cast<int>(Next(random)%interval)-1;count=sub==1 ? 6 : sub==9 ? 6 : 4;}
        for(int frame=0;frame<220;++frame) {
            x+=vx*(parameters.mirrored ? -1 : 1);y+=vy;
            const int retreat=sub==2 ? 90 : sub==6 ? 190 : 110;
            if(sub!=1&&sub<9) {
                if(frame==40)acceleration=-0.0666666701F;if(frame==70)acceleration=0;
                if(frame==retreat) {angle=pi/2;speed=1.79999995F;}
            }
            speed+=acceleration;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,parameters.position.y);
            int expected=0;
            if(frame==next) {expected=count;next+=interval;}
            if(sub==0&&frame>=70&&frame<=105&&(frame-70)%5==0)expected=30;
            if(sub==2&&frame>=70&&frame<=86&&(frame-70)%4==0)expected=24;
            if(sub>=3&&sub<=5&&frame>=70&&frame<=105&&(frame-70)%5==0)expected=24;
            if(sub==6&&frame>=70&&frame<=187&&(frame-70)%3==0)expected=5;
            assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
            assert(actor.soundEvents.size()==(expected>0 ? 1U : 0U));if(expected>0)assert(actor.soundEvents.front()==22);
        }
        if(sub>=2&&sub<=5) {
            assert(actor.Defeat()&&!actor.Defeat());actor.Update(1.0F/60,{},*bullets);
            assert(actor.finished&&actor.dropEvents.size()==1&&actor.dropEvents.front()==(sub==2 ? 4 : 6));
        }
    }
    int recipes=0;for(bool found:covered)recipes+=found;assert(recipes==10&&index==227);
}
void TestDialogueWaitAndRelativeClock() {
    const auto births=Births();StageDirector director(stage5::Stage5Timeline());std::size_t index=0;
    double dialogueEnd=-1,offset=0;bool resumed=false;int bossCount=0;
    for(int step=1;step<=9000;++step) {
        director.Update(1.0F/60,[&](const StageEvent& event) {
            if(std::holds_alternative<SpawnActors>(event.action)) {
                assert(index<births.size());const auto& birth=*births[index++];
                assert(std::lround(director.Elapsed()*60)==birth.frame+offset);return true;
            }
            if(std::holds_alternative<StartBoss>(event.action)) {++bossCount;return false;}
            if(const auto* dialogue=std::get_if<StartDialogue>(&event.action)) {
                if(dialogue->scene==DialogueScene::Midboss)dialogueEnd=step+180;return false;
            }
            if(std::holds_alternative<ResumeBoss>(event.action)) {
                assert(!resumed);resumed=true;offset=step-3274;return true;
            }
            return true;
        });
        if(step==dialogueEnd)director.EndEvent(2);
    }
    assert(index==227&&bossCount==2&&resumed);
    for(bool marisa:{false,true}) {
        const auto mid=stage5::Stage5Dialogue(DialogueScene::Midboss,false,marisa);
        assert(!mid.empty()&&stage5::Stage5Dialogue(DialogueScene::Midboss,true,marisa).empty());
        assert(DialogueFrames(mid)==240);
        assert(DialogueFrames(stage5::Stage5Dialogue(DialogueScene::FinalBoss,false,marisa))==(marisa ? 8120 : 8121));
        assert(DialogueFrames(stage5::Stage5Dialogue(DialogueScene::FinalBoss,true,marisa))==1383);
    }
}
behavior::Routine ClockCompleted(behavior::Actor&) {co_return;}
behavior::Routine ClockPauseProbe(behavior::Actor& actor) {
    actor.timeoutFrames=50;actor.timeoutCallback=ClockCompleted;actor.motion.Polar(0,1);
    co_await actor.At(10);actor.phaseClockPaused=true;
    co_await actor.At(25);actor.Fire({skill::Formation::FixedFan,1,2,1,1,1});
    co_await actor.At(40);actor.phaseClockPaused=false;co_await actor.At(1000);
}
void TestWorldTimeClockPrimitives() {
    auto bullets=std::make_unique<BulletSystem>();
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        behavior::Actor actor(rate);actor.Reset({},100,7);actor.Start(ClockPauseProbe(actor));
        for(int frame=0;frame<80;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);assert(!actor.pendingCallback);
            const int expected=frame<10 ? frame+1 : frame<40 ? 10 : frame-29;
            Near(static_cast<float>(actor.phaseFrames),expected);
            assert(bullets->Pool().ActiveCount()==(frame==25 ? 1U : 0U));
            Near(actor.position.x,(frame+(rate==behavior::UpdateRate::Hz120 ? 0.5F : 0))/60);
        }
        actor.Update(1.0F/60,{},*bullets);assert(actor.pendingCallback==ClockCompleted&&actor.callbackCause==behavior::CallbackCause::Timeout);
        actor.phaseClockPaused=true;actor.Reset({},100,7);assert(!actor.phaseClockPaused);
    }
    bullets->Reset();auto* enemy=bullets->Spawn(BulletOwner::Enemy,{1,2,3},{2,0,-1},0.1F,WHITE,stage5::kBlueBullet);
    auto* player=bullets->Spawn(BulletOwner::Player,{1,2,3},{0,0,1},0.1F,WHITE,stage5::kBlueBullet);
    player->damagePerSecond=60;player->age=0.5F;enemy->age=0.5F;
    for(int step=0;step<240;++step) {
        bullets->Update(1.0F/120,{}, {},true);
        Near(enemy->position.x,1);Near(enemy->position.z,3);Near(enemy->age,0.5F);
        Near(player->position.z,3);Near(player->age,0.5F);Near(player->ContactDamage(),0.5F);
    }
    bullets->Update(1.0F/60);Near(enemy->position.x,1+2.0F/60);Near(player->position.z,3+1.0F/60);
}
void TestBounceAndLockedSpatialAxis() {
    auto bullets=std::make_unique<BulletSystem>();skill::State state;state.rankInfluence={};
    state.projectile={.sampleAtStepStart=true,.bounceCount=2,.bounceLower={-8.57142857F,-10,-10},.bounceUpper={8.57142857F,10,10},
        .bounceHalfExtents={0.714285714F,0.714285714F,0.714285714F}};
    state.Fire({skill::Formation::FixedFan,8,3,1,1,10.7142859F,0,0.31F,0,2048},{0,0,4.28571415F},{},*bullets);
    Bullet* bullet=nullptr;bullets->Pool().ForEachActive([&](Bullet& b){bullet=&b;});assert(bullet);
    // 新典 0x1162a：可见半尺寸是 逻辑尺寸 32×32 的一半，而非弹的碰撞半径；独立原坐标积分。
    constexpr float scale=20.0F/448;float x=192,y=128,vx=4*std::cos(0.31F),vy=4*std::sin(0.31F);int remaining=2;
    for(int frame=0;frame<250;++frame) {
        if(remaining>0&&(x+16<0||x-16>384||y+16<0||y-16>448)) {
            if(x<0||x>=384)vx=-vx;if(y<0)vy=-vy;--remaining;
        }
        x+=vx;y+=vy;bullets->Update(1.0F/60);
        assert(bullet->active&&bullet->bouncesRemaining==remaining);Near(bullet->position.x,(x-192)*scale);Near(bullet->position.z,10-y*scale);
    }
    for(bool all:{false,true}) {
        bullets->Reset();state.projectile.bounceCount=1;
        state.Fire({skill::Formation::FixedFan,8,3,1,1,2,0,pi/2,0,all ? 1024 : 2048},{0,0,-11.8F},{},*bullets);
        bullets->Pool().ForEachActive([&](Bullet& b){bullet=&b;});bullets->Update(1.0F/60);
        assert(bullet->bouncesRemaining==0&&(all ? bullet->velocity.z>0 : bullet->velocity.z<0));
    }
    bullets->Reset();state.Fire({skill::Formation::FixedFan,8,3,1,1,2,0,-pi/4,0,2048},{10.3F,0,11.8F},{},*bullets);
    bullets->Pool().ForEachActive([&](Bullet& b){bullet=&b;});bullets->Update(1.0F/60);
    assert(bullet->bouncesRemaining==0&&bullet->velocity.x<0&&bullet->velocity.z<0&&bullet->position.x>8.57142857F&&bullet->position.z>10);
    bullets->Reset();state.projectile={};state.spatial=true;
    state.Fire({.formation=skill::Formation::FixedFan,.count=1,.firstSpeed=2,.angle=pi,.spatialAxis=Vector3{1,2,-3}},{},{0,0,8},*bullets);
    const float length=std::sqrt(14.0F);bullets->Pool().ForEachActive([&](const Bullet& b){Near(b.velocity.x,2/length);Near(b.velocity.y,4/length);Near(b.velocity.z,-6/length);});
    assert(!skill::Valid({0,skill::Shot{.spatialAxis=Vector3{}}}));
    bullets->Reset();state.spatial=false;state.projectile={.delayFrames=60,.turnAngle=0.7F,.preserveTurnSpeed=true};state.rank=32;
    state.Fire({skill::Formation::FixedFan,4,2,1,3,6,3,0,0,64},{},{},*bullets);
    std::vector<float> speeds;bullets->Pool().ForEachActive([&](const Bullet& b){speeds.push_back(BulletSystem::Length(b.velocity));Near(b.turnSpeed,speeds.back());});
    for(int frame=0;frame<91;++frame)bullets->Update(1.0F/60);
    std::size_t index=0;bullets->Pool().ForEachActive([&](const Bullet& b){assert(b.turnApplied);Near(BulletSystem::Length(b.velocity),speeds[index++]);});assert(index==3);
    bullets->Reset();state.spatial=true;state.projectile={};state.rank=16;
    state.Fire({.formation=skill::Formation::RandomAngleSpeed,.count=128,.firstSpeed=2,.lastSpeed=2,.angle=1,.spacing=-1,.spatialAxis=Vector3{0,1,0}},
        {},{},*bullets,[](float range){return range*0.5F;});
    bullets->Pool().ForEachActive([](const Bullet& b){assert(b.velocity.y>=2*std::cos(1.0F)-1e-5F);Near(BulletSystem::Length(b.velocity),2);});

}
/// @brief 从源跳转及调用栈独立展开的帧表；不读取新协程或以其输出生成期望值。
void TestBossSourceClocks() {
    using namespace stage5::sakuya;
    struct Case {behavior::Entry entry;int first,cycle,stop,unstop,initialCount;};
    const std::array<Case,3> cases{{{ClockCorpse,288,400,338,490,128},{LunaClock,268,418,318,438,128},
        {Marionette,120,0,195,273,8}}};
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto& c:cases) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},10000,49);
        stage5::ConfigureBehavior(actor,false);actor.skills.rankInfluence={};actor.Start(c.entry(actor));
        int paused=0;
        for(int frame=0;frame<560;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{0,0,-6},*bullets);
            if(frame==c.first)assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(c.initialCount));
            if(frame==c.first-1)assert(bullets->Pool().ActiveCount()==0);
            if(frame>=c.stop&&frame<c.unstop) {
                assert(actor.phaseClockPaused&&!actor.CanBeTargeted()&&!actor.CanTakeDamage());++paused;
            } else if(frame<c.stop||frame<c.unstop+1)assert(!actor.phaseClockPaused);
            if(c.entry!=Marionette&&frame>=c.stop&&frame<c.stop+(c.entry==ClockCorpse ? 90 : 60))
                assert(bullets->Pool().ActiveCount()==((frame-c.stop)%9==0 ? 9U : 0U));
            if(frame==c.unstop) {
                assert(!actor.phaseClockPaused&&actor.interactable);assert(actor.animationScript==(actor.Position().x<0 ? 140 : 139));Near(static_cast<float>(actor.phaseFrames),frame+1-paused);
            }
        }
    }
    behavior::Actor mid(behavior::UpdateRate::Hz60);mid.Reset({0,0,4.28571415F},15000,49);
    stage5::ConfigureBehavior(mid,false);mid.Start(Midboss(mid));
    for(int frame=0;frame<620;++frame) {
        bullets->Reset();mid.Update(1.0F/60,{},*bullets);
        const int local=frame<311 ? frame : (frame-311)%281+30;
        const bool kunai=(local>=30&&local<=102&&(local-30)%8==0)||(local>=200&&local<=272&&(local-200)%8==0);
        const bool daggers=(local>=110&&local<=137&&(local-110)%3==0)||(local>=280&&local<=307&&(local-280)%3==0);
        if(bullets->Pool().ActiveCount()!=(kunai||daggers ? 8U : 0U))std::cerr<<"Mid source clock frame "<<frame<<" local "<<local<<" actual "<<bullets->Pool().ActiveCount()<<"\n";
        assert(bullets->Pool().ActiveCount()==(kunai||daggers ? 8U : 0U));assert(mid.interactable==(frame>=30));
    }
    // 瞬移两侧的物理周期均为 284 帧，首轮从 120 帧准备结束起算。
    behavior::Actor misdirection(behavior::UpdateRate::Hz60);misdirection.Reset({0,0,4.28571415F},710,49);
    stage5::ConfigureBehavior(misdirection,false);misdirection.Start(Misdirection(misdirection));
    for(int frame=0;frame<730;++frame) {
        bullets->Reset();misdirection.Update(1.0F/60,{0,0,-6},*bullets);
        const int local=(frame-120)%284;int expected=0;
        if(frame>=120&&local<60&&(local+1)%6==0)expected=72;
        if(frame>=120&&(local==80||local==98||local==116))expected=44;
        assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
        if(frame>=120&&local==254) {Near(misdirection.position.x,0);Near(misdirection.position.z,3.57142854F);}
    }
}

void TestMainNonspellSourceClocks() {
    using namespace stage5::sakuya;auto bullets=std::make_unique<BulletSystem>();
    for(int phase=0;phase<3;++phase) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},19000,49);stage5::ConfigureBehavior(actor,false);
        const std::array<behavior::Entry,3> entries{FirstNonspell,SecondNonspell,ThirdNonspell};actor.Start(entries[phase](actor));
        for(int frame=0;frame<1100;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{0,0,-6},*bullets);
            const bool summoned=phase<2&&frame>=(phase==0 ? 100 : 60)&&(frame-(phase==0 ? 100 : 60))%(phase==0 ? 586 : 420)==0;
            assert(actor.spawnEvents.size()==(summoned ? 4U : 0U));
            if(phase==0) {
                const int local=frame<100 ? frame : (frame-100)%586+100;
                if(local>=450&&local<=538&&(local-450)%8==0)assert(bullets->Pool().ActiveCount()==16);
                if(local==606)assert(actor.animationInterrupt.has_value());
            } else if(phase==1) {
                const int local=frame<60 ? frame : (frame-60)%420+60;
                if(local==340)assert(bullets->Pool().ActiveCount()==24);
            } else {
                const int local=frame<60 ? frame : (frame-60)%370+60;
                const bool first=local>=60&&local<=95&&(local-60)%5==0,second=local>=220&&local<=255&&(local-220)%5==0;
                assert(bullets->Pool().ActiveCount()==(first ? 10U : second ? 12U : 0U));
            }
        }
    }
}
/// @brief 时停扩展的平面期望独立使用原始像素矩阵；三维版另检不同高度的瞄准与九枚真实世界出生点。
behavior::Routine ArcProbe(behavior::Actor& actor) {for(int pulse=0;pulse<10;++pulse)stage5::sakuya::EmitClockArc(actor,pulse);co_await actor.At(100);}
void TestClockArcCoordinates() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({0,0,4.28571415F},100,49);stage5::ConfigureBehavior(actor,false);actor.Start(ArcProbe(actor));
    actor.Update(1.0F/60,{2,0,-6},*bullets);assert(bullets->Pool().ActiveCount()==90);
    constexpr float scale=20.0F/448;const float dx=2/scale,dy=(4.28571415F+6)/scale,length=std::hypot(dx,dy);
    int index=0;bullets->Pool().ForEachActive([&](const Bullet& b) {
        const int pulse=index/9,point=index%9;const float radius=pulse%2 ? -256.0F : 256.0F;
        const float baseX=dx*(0.5F-pulse*0.5F/9)+dx/length*radius,baseY=dy*(0.5F-pulse*0.5F/9)+dy/length*radius;
        const float phi=pi/4-(point+1)*pi/18;
        const float armX=-dx/length*radius,armY=-dy/length*radius;
        const float x=baseX+armX*std::cos(phi)+armY*std::sin(phi),y=baseY-armX*std::sin(phi)+armY*std::cos(phi);
        Near(b.position.x,x*scale);Near(b.position.z,4.28571415F-y*scale);Near(b.position.y,0);
        Near(BulletSystem::Length(b.velocity),5.35714293F);assert(b.nativeType==8&&b.nativeColor==3);++index;
    });
}
void TestBossLifecycleAndSummons() {
    auto bullets=std::make_unique<BulletSystem>();
    for(bool final:{false,true})for(bool timeout:{false,true}) {
        stage5::Stage5Boss boss(final,behavior::UpdateRate::Hz60,false);
        for(int frame=0;frame<65;++frame) {assert(!boss.Update(1.0F/60,{},*bullets));assert(!boss.CanTakeDamage()&&!boss.CanContactPlayer()&&boss.CanBeTargeted());}
        boss.ResumeCombat();const int phases=final ? 6 : timeout ? 1 : 2;
        for(int phase=1;phase<=phases;++phase) {
            assert(boss.phase==phase);bool ready=false;
            for(int frame=0;frame<10000&&!ready;++frame) {
                bullets->Reset();ready=boss.Update(1.0F/60,{},*bullets);
                if(!timeout&&boss.CanTakeDamage()) {boss.health=0;ready=boss.ShouldAdvanceFromDamage();}
            }
            assert(ready);if(!timeout&&phase%2==0)assert(boss.PhaseDefeated());
            assert(boss.AdvancePhase());assert(!boss.WorldTimeStopped()&&!boss.CanTakeDamage());
        }
        bool ready=false;for(int frame=0;frame<200&&!ready;++frame)ready=boss.Update(1.0F/60,{},*bullets);
        assert(ready&&!boss.AdvancePhase()&&!boss.active);assert(boss.Defeated()==!timeout);
    }
    for(bool ricochet:{false,true}) {
        behavior::Actor boss(behavior::UpdateRate::Hz60);boss.Reset({0,0,4.28571415F},19000,49);stage5::ConfigureBehavior(boss,false);
        boss.Start(ricochet ? stage5::sakuya::SecondNonspell(boss) : stage5::sakuya::FirstNonspell(boss));
        std::vector<behavior::Spawn> summons;const int born=ricochet ? 60 : 100;
        for(int frame=0;frame<=born;++frame) {boss.Update(1.0F/60,{},*bullets);if(frame==born)summons=boss.spawnEvents;}
        assert(summons.size()==4);
        for(const auto& spawn:summons) {
            assert(behavior::Valid(spawn)&&spawn.sourceHealth==100&&spawn.sourceScore==0&&spawn.sourceItem==0);
            behavior::Actor familiar(behavior::UpdateRate::Hz60);familiar.Activate(spawn);
            const int repeats=ricochet ? 17 : 40,interval=ricochet ? 14 : 6,fade=50+repeats*interval;
            for(int frame=0;frame<=fade+60;++frame) {
                bullets->Reset();familiar.Update(1.0F/60,{},*bullets);
                const bool fire=frame>=50&&frame<fade&&(frame-50)%interval==0;
                assert(bullets->Pool().ActiveCount()==(fire ? (ricochet ? 6U : 5U) : 0U));
                assert(!familiar.CanBeTargeted()&&!familiar.CanContactPlayer());
                assert(familiar.animationInterrupt.has_value()==(frame>=fade));assert(familiar.active==(frame<fade+60));
                if(fire&&ricochet)bullets->Pool().ForEachActive([](const Bullet& b){assert(b.bouncesRemaining==1&&!b.reflectLowerZ);});
            }
        }
    }
}

behavior::Routine RedirectProbe(behavior::Actor& actor) {stage5::sakuya::RedirectStoppedBullets(actor);co_await actor.At(100);}
void TestRedirectRandomOrder() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({},100,49);stage5::ConfigureBehavior(actor,false);actor.Start(RedirectProbe(actor));
    struct Expected {Vector3 velocity;int color;};std::vector<Expected> expected;unsigned random=49;int changed=0;
    for(int i=0;i<200;++i) {
        const int type=i%4==0 ? 0 : i%3==0 ? 3 : 8,color=i%5==0 ? 5 : 3;const Vector3 position{i%2 ? 1.0F : 8.0F,0,0};
        auto* b=bullets->Spawn(BulletOwner::Enemy,position,{3,0,-4},0.1F,WHITE,stage5::kBlueBullet);assert(b);b->nativeType=type;b->nativeColor=color;
        Expected result{{3,0,-4},color};
        if(changed<14&&type>=6&&color!=5&&Next(random)%4==0) {
            const float uniform=static_cast<float>(Next(random)>>8)/16777216.0F;
            const float angle=position.x>5.71428571F ? uniform*(3*pi/4)+pi/4 : pi/2+uniform*2*pi;
            result={{5*std::cos(angle),0,-5*std::sin(angle)},5};++changed;
        }
        expected.push_back(result);
    }
    assert(changed==14);actor.Update(1.0F/60,{},*bullets);int index=0;
    bullets->Pool().ForEachActive([&](const Bullet& b){const auto& value=expected[index++];assert(b.nativeColor==value.color);Near(b.velocity.x,value.velocity.x);Near(b.velocity.z,value.velocity.z);});
    assert(actor.Random(1)==static_cast<float>(Next(random)>>8)/16777216.0F);
}
void TestSpatialDisplayFrameRates() {
    using namespace stage5::sakuya;
    const std::array<behavior::Entry,8> entries{Midboss,Misdirection,FirstNonspell,ClockCorpse,SecondNonspell,LunaClock,ThirdNonspell,Marionette};
    struct Snapshot {Vector3 position,velocity;int type,color,bounces;};
    for(auto entry:entries) {
        std::vector<Snapshot> expected;Vector3 position{};float random=0;double phase=0;bool stopped=false;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,4},19000,7);
            stage5::ConfigureBehavior(actor,true);actor.Start(entry(actor));
            for(int frame=0;frame<fps*12;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Snapshot> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back({b.position,b.velocity,b.nativeType,b.nativeColor,b.bouncesRemaining});});
            const float nextRandom=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=nextRandom;phase=actor.phaseFrames;stopped=actor.phaseClockPaused;}
            else {
                assert(actual.size()==expected.size()&&nextRandom==random&&phase==actor.phaseFrames&&stopped==actor.phaseClockPaused);
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                for(std::size_t i=0;i<actual.size();++i) {
                    const auto& a=actual[i];const auto& b=expected[i];assert(a.type==b.type&&a.color==b.color&&a.bounces==b.bounces);
                    Near(a.position.x,b.position.x);Near(a.position.y,b.position.y);Near(a.position.z,b.position.z);
                    Near(a.velocity.x,b.velocity.x);Near(a.velocity.y,b.velocity.y);Near(a.velocity.z,b.velocity.z);
                }
            }
            assert(bullets->Pool().DroppedSpawns()==0);
        }
    }
}

}
/// @brief 运行五面出生、攻击时钟、改向随机顺序、Boss 生命周期与显示帧率一致性；图形由集成目标验证。
void RunStage5Tests() {TestBirthsMovementAndAttacks();TestDialogueWaitAndRelativeClock();TestWorldTimeClockPrimitives();TestBounceAndLockedSpatialAxis();TestBossSourceClocks();TestMainNonspellSourceClocks();TestClockArcCoordinates();TestBossLifecycleAndSummons();TestRedirectRandomOrder();TestSpatialDisplayFrameRates();}

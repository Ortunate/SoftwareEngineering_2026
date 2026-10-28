// 本文件以独立三面源参数和局部时钟见证记录，检查全部出生、普通轨迹、实际攻击、召唤和遭遇衔接。
#include "stages/stage3/content.hpp"
#include "stages/stage3/boss_config.hpp"
#include "stages/stage3/behaviors.hpp"
#include "gameplay/bosses/meiling.hpp"
#include "app/spell_practice.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>,scale=20.0F/448;
void Near(float a,float b,float tolerance=5e-4F) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {std::cerr<<"Stage 3 mismatch "<<a<<" vs "<<b<<'\n';std::abort();}
}
unsigned Next(unsigned& state) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
float Random(unsigned& state,float range) {return static_cast<float>(Next(state)>>8)/16777216.0F*range;}
void TestBirthsAndMovement() {
    stage3::Stage3Content content;assert(content.Events().size()==443);
    std::vector<const native_combat::HealthRecord*> births;
    for(const auto& r:native_combat::Stage(3)->health)
        if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=9&&r.targetSub!=21)births.push_back(&r);
    assert(births.size()==437);std::size_t index=0;auto bullets=std::make_unique<BulletSystem>();
    for(const auto& event:content.Events()) {
        const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
        const auto& source=*births.at(index++);auto spawn=request->actor;
        assert(request->count==1&&request->interval==0&&spawn.sourceHealth==*source.value&&spawn.sourceScore==source.score&&spawn.sourceItem==source.item);
        assert(spawn.mirrored==(source.opcode==2||source.opcode==6)&&spawn.randomX==(source.position.x==-999));
        if(source.frame<3474)assert(event.atFrame==source.frame);else assert(!event.atFrame);
        spawn.retireOutside=false;spawn.pattern.spatialShots=false;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
        unsigned random=spawn.seed;const int sub=source.targetSub;
        float x=source.position.x,y=source.position.y,vx=0,vy=0,angle=pi/2,speed=1.5F,accel=0,angular=0;
        if(spawn.randomX)x=static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F*384;
        if(sub<=1) {angle=0.52359879F;speed=4.5F;}
        if(sub==2||sub==3) {angle=-1.04719758F;speed=4;}
        int nextShot=-1;if(sub==6) {speed=2;nextShot=190-static_cast<int>(Next(random)%190)-1;}
        if(sub==8)speed=2.5F;
        float retreatAngle=0;
        for(int frame=0;frame<500;++frame) {
            x+=vx*(spawn.mirrored ? -1 : 1);y+=vy;
            if(sub<=3) {
                if(frame==30)angular=sub<=1 ? -0.0654498488F : 0.0349065848F;
                if(frame==(sub<=1 ? 115 : 90))angular=0;
            } else if(sub==6) {
                if(frame==40)angular=-0.0245436933F;
                if(frame==120)angular=0.0196349546F;
                if(frame==220)angular=0;
            } else {
                if(frame==40)accel=sub==8 ? -0.0833333358F : -0.0500000007F;
                if(frame==70) {
                    accel=0;
                    if(sub==5) {Random(random,1);retreatAngle=Random(random,pi/2)+pi/4;}
                    if(sub==8) {for(int i=0;i<28;++i)Random(random,1);angle=Random(random,pi/2)+pi/4;speed=1.5F;}
                }
                if(sub==4&&frame==130) {angle=Random(random,pi/2)+pi/4;speed=1.5F;}
                if(sub==5&&frame==100) {angle=retreatAngle;speed=-1.5F;}
                if(sub==7&&frame==102) {angle=Random(random,pi/2)+pi/4;speed=-1.5F;}
            }
            angle+=angular;speed+=accel;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,spawn.position.y);
            std::size_t count=0;
            if(sub==4&&frame>=70&&frame<130&&(frame-70)%2==0)count=1;
            if(sub==5&&frame==70)count=16;
            if(sub==6&&frame==nextShot&&frame<220) {count=3;nextShot+=190;}
            if(sub==7&&frame>=70&&frame<102&&(frame-70)%2==0)count=1;
            if(sub==8&&frame==70)count=14;
            if(bullets->Pool().ActiveCount()!=count) {std::cerr<<"Stage 3 ordinary shot frame "<<frame<<" sub "<<sub<<'\n';std::abort();}
        }
        // 全部实际种子也走正式三维首轮；高位为 1 的种子不能经有符号转换变成非法采样编号。
        auto spatial=spawn;spatial.pattern.spatialShots=true;behavior::Actor production;production.Activate(spatial);
        bullets->Reset();production.Update(1.2F,{2,3,-6},*bullets);
    }
    assert(index==437);
}
/// @brief 帧号独立由源调用/回拨时钟展开：两个普通循环、三面非符两段循环及雨弹阶段。
void TestAttackWitnesses() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    const auto start=[&](behavior::Entry entry,float hp) {actor.Reset({},hp,7);stage3::ConfigureBehavior(actor,false);actor.Start(entry(actor));};
    start(bosses::MeilingBoss::Behaviors::Midboss,13000);
    for(int frame=0;frame<=365;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const bool rain=frame>=150&&frame<=206&&(frame-150)%8==0;
        assert(bullets->Pool().ActiveCount()==(rain ? 16U : frame==364 ? 76U : 0U));
    }
    start(bosses::MeilingBoss::Behaviors::GorgeousSweetFlower,1300);
    for(int frame=0;frame<1000;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const std::size_t count=frame<120 ? 0 : ((frame-120)%6==0 ? 10 : 0)+(frame%80==0 ? 43 : 0);
        assert(bullets->Pool().ActiveCount()==count);
    }
    start(bosses::MeilingBoss::Behaviors::FirstNonspell,14000);
    for(int frame=0;frame<=324;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const bool first=frame>=50&&frame<170&&(frame-50)%4==0;
        const bool second=frame>=170&&frame<202&&(frame-170)%2==0;
        const bool next=frame==322;
        assert(bullets->Pool().ActiveCount()==(first||second ? 5U : next ? 6U : 0U));
    }
    start(bosses::MeilingBoss::Behaviors::RainNonspell,16500);
    for(int frame=0;frame<=340;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const bool rings=(frame>=160&&frame<=165)||(frame>=236&&frame<=241);
        const bool rain=frame>=262&&frame<=340&&(frame-262)%2==0;
        assert(bullets->Pool().ActiveCount()==(rings ? 24U : rain ? 1U : 0U));
        if(rain)bullets->Pool().ForEachActive([](const Bullet& b){Near(b.acceleration.x,0);Near(b.acceleration.z,-4.33928585F);});
    }
    start(bosses::MeilingBoss::Behaviors::ColorfulRain,3400);
    for(int frame=0;frame<=365;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const bool first=(frame>=120&&frame<=196&&(frame-120)%4==0)||(frame>=360&&(frame-360)%4==0);
        const bool second=frame>=300&&frame<=357&&(frame-300)%3==0;
        assert(bullets->Pool().ActiveCount()==(first||second ? 4U : 0U));
    }
    start(bosses::MeilingBoss::Behaviors::DazzlingColorTyphoon,2000);
    for(int frame=0;frame<200;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const std::size_t expected=frame<180 ? 0 : (frame-180)%6==0||(frame-180)%6==3 ? 2 : 1;
        assert(bullets->Pool().ActiveCount()==expected);
    }
    start(bosses::MeilingBoss::Behaviors::SummoningNonspell,12000);std::vector<behavior::Spawn> summons;
    actor.Animate(71);actor.SetPoses({71,71,71,67,68});
    for(int frame=0;frame<=220;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        assert(actor.animationScript==64); // 源 98 更换组时重选本体，不能保留上一阶段的翻滚动作或辅助槽 66。
        assert(actor.spawnEvents.size()==(frame==160||frame==190||frame==220 ? 2U : 0U));
        summons.insert(summons.end(),actor.spawnEvents.begin(),actor.spawnEvents.end());
    }
    assert(summons.size()==6);const std::array<float,6> hp{2000,1000,500,2000,1000,500};
    for(std::size_t i=0;i<summons.size();++i)assert(behavior::Valid(summons[i])&&summons[i].sourceHealth==hp[i]&&summons[i].sourceScore==2000&&summons[i].sourceItem==1);
    std::array<bool,3> choices{};
    for(unsigned seed:{1U,48U,49U}) {
        unsigned random=seed;const int choice=Next(random)%3;choices[choice]=true;
        actor.Reset({},12000,seed);stage3::ConfigureBehavior(actor,false);actor.Start(bosses::MeilingBoss::Behaviors::SummoningNonspell(actor));
        for(int frame=0;frame<=220;++frame) {bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);}
        const std::size_t count=choice==0 ? 36 : choice==1 ? 16 : 26;
        assert(bullets->Pool().ActiveCount()==count);
        // 召唤的独立种子不读取 Boss 攻击随机流；源发射按分支保留角度/速率读取的实际次数。
        if(choice==0)Random(random,1);
        else {if(choice==2)Random(random,1);for(std::size_t i=0;i<count*2;++i)Random(random,1);}
        assert(actor.Random(1)==Random(random,1));
    }
    for(bool covered:choices)assert(covered);
    auto child=summons.front();child.retireOutside=false;actor.Activate(child);
    for(int frame=0;frame<=1039;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        const bool fires=frame>=70&&(frame-70)%250<60&&(frame-70)%2==0;
        assert(bullets->Pool().ActiveCount()==(fires ? 1U : 0U));
    }
}
void TestBossTransitions() {
    auto bullets=std::make_unique<BulletSystem>();bosses::MeilingBoss boss(behavior::UpdateRate::Hz60,false);boss.StartBoss(stage3::MeilingFinalConfig());
    assert(boss.attack==0&&!boss.CanTakeDamage()&&!boss.MusicOnEntrance());
    boss.Update(2,{},*bullets);boss.ResumeCombat();assert(boss.attack==1&&boss.health==14000&&!boss.CanTakeDamage());
    constexpr std::array<int,6> health{14000,1900,12000,16500,3400,2000},seconds{30,30,40,40,36,36};
    for(int phase=1;phase<=6;++phase) {
        assert(boss.attack==phase&&boss.health==health[phase-1]);bool ended=false;
        if(phase==2||phase==5||phase==6) {
            const auto* card=spell::Find(2,phase==2 ? 1 : phase==5 ? 3 : 4,1);
            assert(card&&boss.SpellCardNumber()==card->number&&std::string_view(boss.SpellName())==card->name);
        }
        for(int frame=0;frame<=seconds[phase-1]*60+1&&!ended;++frame) {
            bullets->Reset();ended=boss.Update(1.0F/60,{2,3,-6},*bullets);
        }
        assert(ended&&boss.health>0&&!boss.SkillDefeated());assert(boss.AdvanceSkill());
    }
    for(int i=0;i<65&&boss.active;++i)if(boss.Update(1.0F/60,{},*bullets))boss.AdvanceSkill();
    assert(!boss.active&&!boss.Defeated());
    bosses::MeilingBoss mid(behavior::UpdateRate::Hz60,false);mid.StartBoss(stage3::MeilingMidbossConfig());
    for(int i=0;i<=100;++i)mid.Update(1.0F/60,{},*bullets);assert(mid.CanTakeDamage());
    mid.health=1299;assert(mid.ShouldAdvanceFromDamage());assert(mid.AdvanceSkill()&&mid.attack==2&&mid.health==1300&&!mid.CanTakeDamage());
    for(int i=0;i<=120;++i)mid.Update(1.0F/60,{},*bullets);assert(mid.CanTakeDamage()&&mid.SpellCardNumber()==22);
    mid.health=0;assert(mid.AdvanceSkill());int drops=0;
    for(int i=0;i<182&&mid.active;++i) {const bool ended=mid.Update(1.0F/60,{},*bullets);for(int n:mid.DropEvents())drops+=n;if(ended)mid.AdvanceSkill();}
    assert(!mid.active&&mid.Defeated()&&drops==5);
}
void TestSceneAndTimelineWaits() {
    stage3::Stage3Content content;const std::array<int,6> frames{5026,1382,270,3944,1382,270};std::size_t index=0;
    for(auto shot:{ShotType::HomingAmulet,ShotType::MagicMissile}) {
        for(auto branch:{0,1,2}) {
            const auto scene=branch==2 ? DialogueScene::Midboss : DialogueScene::FinalBoss;
            float duration=0;for(const auto& line:content.SceneDialogue(scene,branch!=0,shot))duration+=line.duration;
            Near(duration*60,static_cast<float>(frames[index++]),0.002F);
        }
    }
    const auto lines=content.Dialogue();assert(std::string_view(lines[4].secondLine)=="可管不了这么多吧"&&lines[4].line2Delay==1.0F/60);
    const auto cues=content.DialogueCues(false,ShotType::HomingAmulet);
    assert(DialogueCueExpressionsAt(cues,1384.0F/60,{})[1]==0);
    assert(DialogueCueExpressionsAt(cues,1385.0F/60,{})[1]==1);
    StageDirector director(content.Events());int spawns=0,bosses=0,dialogues=0,resumes=0;
    const auto handle=[&](const StageEvent& event) {
        if(std::holds_alternative<SpawnActors>(event.action)){++spawns;return true;}
        if(std::holds_alternative<StartBoss>(event.action))++bosses;
        if(std::holds_alternative<StartDialogue>(event.action))++dialogues;
        if(std::holds_alternative<ResumeBoss>(event.action)){++resumes;return true;}
        return false;
    };
    director.Update(100,handle);assert(bosses==1&&dialogues==0);const int before=spawns;
    director.Update(50,handle);assert(spawns==before&&bosses==1&&dialogues==0);
    assert(director.EndEvent(1));director.Update(1.0F/60,handle);assert(dialogues==1);
    assert(director.EndEvent(2));director.Update(1.0F,handle);assert(spawns==before+2);
    for(int i=0;i<2800;++i)director.Update(1.0F/60,handle);
    assert(bosses==2&&dialogues==2&&resumes==0&&spawns==437);
    assert(director.EndEvent(4));director.Update(1.0F/60,handle);assert(resumes==1);
    assert(director.EndEvent(3));director.Update(1.0F/60,handle);assert(dialogues==3);
}
/// @brief 每种真实配方在默认 120 Hz 下跨显示 FPS 对照实际弹幕与随机流；随机单发须跨轮覆盖高度。
void TestSpatialAndDisplayRates() {
    stage3::Stage3Content content;std::vector<const behavior::Spawn*> examples;
    for(const auto& event:content.Events())if(const auto* request=std::get_if<SpawnActors>(&event.action)) {
        bool duplicate=false;for(const auto* prior:examples)duplicate|=prior->pattern.routine==request->actor.pattern.routine&&
            prior->pattern.initialAnimation==request->actor.pattern.initialAnimation&&prior->pattern.movement.angle==request->actor.pattern.movement.angle;
        if(!duplicate)examples.push_back(&request->actor);
    }
    assert(examples.size()==8);auto bullets=std::make_unique<BulletSystem>();
    for(const auto* example:examples) {
        Vector3 position{};float random=0;std::vector<Bullet> expected;
        for(int fps:{120,30,60,144,240}) {
            auto spawn=*example;spawn.retireOutside=false;behavior::Actor actor;actor.Activate(spawn);bullets->Reset();
            for(int i=0;i<fps*4;++i)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(b);});const float next=actor.Random(1);
            if(fps==120) {position=actor.Position();random=next;expected=shots;}
            else {
                Near(actor.Position().x,position.x,1e-5F);Near(actor.Position().z,position.z,1e-5F);assert(next==random&&shots.size()==expected.size());
                for(std::size_t i=0;i<shots.size();++i) {
                    assert(shots[i].nativeType==expected[i].nativeType&&shots[i].nativeColor==expected[i].nativeColor);
                    Near(shots[i].position.x,expected[i].position.x,1e-5F);Near(shots[i].position.z,expected[i].position.z,1e-5F);
                    Near(shots[i].velocity.x,expected[i].velocity.x,1e-5F);Near(shots[i].velocity.y,expected[i].velocity.y,1e-5F);Near(shots[i].velocity.z,expected[i].velocity.z,1e-5F);
                }
            }
        }
    }
    behavior::Actor rain(behavior::UpdateRate::Hz60);rain.Reset({},3400,7);stage3::ConfigureBehavior(rain);rain.Start(bosses::MeilingBoss::Behaviors::ColorfulRain(rain));
    std::array<int,8> bands{};
    for(int frame=0;frame<400;++frame) {
        bullets->Reset();rain.Update(1.0F/60,{2,3,-6},*bullets);
        bullets->Pool().ForEachActive([&](const Bullet& b) {
            const float cosine=b.velocity.x/BulletSystem::Length(b.velocity);
            ++bands[std::clamp(static_cast<int>((cosine+1)*4),0,7)];
        });
    }
    for(int count:bands)assert(count>0);
}
void TestEntranceAndDepartureSourceBoundaries() {
    auto bullets=std::make_unique<BulletSystem>();
    for(bool middle:{false,true}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({7.14285707F,0,14.2857141F},13000,7);
        stage3::ConfigureBehavior(actor,false);actor.Start(middle ? bosses::MeilingBoss::Behaviors::Midboss(actor) : bosses::MeilingBoss::Behaviors::Entrance(actor));
        for(int frame=0;frame<110;++frame) {
            actor.Update(1.0F/60,{},*bullets);const float fraction=frame>=99 ? 1 : std::pow(frame/100.0F,2.0F);
            Near(actor.position.x,(352-160*fraction-192)*20/448);
            const float z=10-(-96+246*fraction)*20/448;
            Near(actor.position.z,middle&&frame>=101 ? 10-144.0F*20/448 : z);
            assert(actor.CanContactPlayer()==(middle&&frame>=100));
        }
    }
    behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({2,0,4},100,7);stage3::ConfigureBehavior(actor,false);
    actor.motion.clamped=true;actor.Start(bosses::MeilingBoss::Behaviors::MidbossTimeout(actor));
    for(int frame=0;frame<180;++frame)actor.Update(1.0F/60,{},*bullets);
    assert(actor.position.z>12&&!actor.motion.clamped);
}
}
/// @brief 独立三面测试入口，不依赖本机原典安装或一面旧解释器。
void RunStage3Tests() {TestBirthsAndMovement();TestAttackWitnesses();TestBossTransitions();TestSceneAndTimelineWaits();TestSpatialAndDisplayRates();TestEntranceAndDepartureSourceBoundaries();}

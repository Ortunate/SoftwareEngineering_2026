// 本文件以独立三面源参数和局部时钟见证记录，检查全部出生、普通轨迹、实际攻击、召唤和遭遇衔接。
#include "stages/stage3/content.hpp"
#include "stages/stage3/boss_config.hpp"
#include "stages/stage3/behaviors.hpp"
#include "gameplay/bosses/meiling.hpp"
#include "actor_fixture.hpp"
#include "stage3_reference_program.hpp"
#include "stage3_spell_reference.hpp"
#include "app/spell_practice.hpp"
#include <algorithm>
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
/// @brief 请求精确比较身份、数量及ARGB；位置与附着轴只在独立参考侧转换为世界坐标。
void CompareStage3Effects(const behavior::Actor& actor,const std::vector<stage3::reference::ExternalEffect>& expected) {
    if(actor.effectEvents.size()!=expected.size())std::cerr<<"Stage3 effect requests "<<actor.effectEvents.size()<<" vs "<<expected.size()<<" phase "<<actor.phaseFrames<<'\n';
    assert(actor.effectEvents.size()==expected.size());
    for(std::size_t n=0;n<expected.size();++n) {
        const auto& a=actor.effectEvents[n];const auto& e=expected[n];assert(e.type==3||e.type==12||e.type==13||e.type==20);
        const auto kind=e.type==3?effects::Kind::Burst:e.type==12?effects::Kind::Flash:e.type==13?effects::Kind::Attached:effects::Kind::Drift;
        assert(a.kind==kind&&a.count==e.count&&a.color.r==((e.color>>16)&255)&&a.color.g==((e.color>>8)&255)&&a.color.b==(e.color&255)&&a.color.a==(e.color>>24));
        Near(a.position.x,(e.position.x-192)*scale);Near(a.position.y,-e.position.z*scale);Near(a.position.z,10-e.position.y*scale);
        if(e.type==13) {
            assert(a.axis.x==e.axis.x&&a.axis.y==-e.axis.z&&a.axis.z==-e.axis.y);
            const auto anchor=a.attachment.lock();assert(anchor);Near(anchor->Radius(),e.radius*scale,1e-5F);
        }
    }
}
/// @brief 源99只重启指定辅助槽，精确比较编号/次数；年龄使用源帧独立换秒，不从生产时钟构造期望。
void CompareStage3Auxiliary(const behavior::Actor& actor,const stage3::reference::SpellReference& source) {
    for(std::size_t n=0;n<source.auxiliary.size();++n) {
        const auto& actual=actor.auxiliaryAnimations[n];const auto& expected=source.auxiliary[n];
        if(actual.script!=expected.script||actual.revision!=expected.revision)
            std::cerr<<"Stage3 auxiliary slot "<<n<<" phase "<<actor.phaseFrames<<" source phase "<<source.phaseFrames
                <<" script "<<actual.script<<'/'<<expected.script<<" revision "<<actual.revision<<'/'<<expected.revision<<'\n';
        assert(actual.script==expected.script&&actual.revision==expected.revision);
        assert(std::abs(actual.age-expected.frames/60.0)<1e-6);
        assert(actual.visual==kInvalidVisualId&&!actual.interrupt);
    }
}
/// @brief 入场有限窗及两路道中/最终退场独立验证请求、移动、声音与清弹存活；不扩大道中攻击解释范围。
void TestMeilingEntranceAndRetirementEffects() {
    using B=bosses::MeilingBoss::Behaviors;
    struct Case {int source;behavior::Entry entry;int frames;};
    const Case cases[]{{9,B::Midboss,130},{21,B::Entrance,130},{19,B::MidbossDefeated,181},
        {20,B::MidbossTimeout,181},{48,B::Defeated,61}};
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(const auto& c:cases) {
        stage3::reference::SpellReference source(d,c.source,{192,112,0},seed);
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},13000,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);stage3::ConfigureBehavior(actor,false);
        actor.motion.bounds={{-7.14285714F,3.57142857F},{7.14285714F,7.85714286F}};actor.motion.clamped=true;
        if(c.source==9||c.source==21) {actor.Reset({7.14285714F,0,14.2857143F},13000,seed);actor.difficulty=*native_combat::DifficultyFromIndex(d);stage3::ConfigureBehavior(actor,false);}
        actor.Start(c.entry(actor));int requests=0,particles=0;
        for(int frame=0;frame<c.frames;++frame) {
            bullets->Reset();auto* sentinel=bullets->Spawn(BulletOwner::Enemy,{}, {},.1F,WHITE,stage3::kBlueBullet);assert(sentinel);
            source.Tick({240,360,0});actor.Update(1.0F/60,{},*bullets);
            CompareStage3Effects(actor,source.effects);CompareStage3Auxiliary(actor,source);
            assert(sentinel->active==!source.clearBullets&&actor.soundEvents==source.sounds);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            for(const auto& e:actor.effectEvents){++requests;particles+=e.count;}
        }
        if(c.source==48) {
            assert(requests==30&&particles==60&&actor.pendingCallback==B::End&&!actor.finished);
            actor.Transition();actor.Update(1.0F/60,{},*bullets);assert(actor.finished&&actor.effectEvents.empty());
        }
        if(c.source==19||c.source==20)assert(requests==5&&particles==10&&actor.finished);
        if(c.source==9) {
            assert(requests==13&&particles==23);bool rejected=false;
            try {source.Tick({});}catch(const std::logic_error&) {rejected=true;}assert(rejected);
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief 两逻辑频率分别核对五显示FPS的请求时钟、同帧顺序和位置，涵盖低/高难准备、附着与退场。
void TestMeilingEffectDisplayRates() {
    using B=bosses::MeilingBoss::Behaviors;
    const behavior::Entry entries[]{B::Midboss,B::Entrance,B::GorgeousSweetFlower,B::Selaginella,B::FirstNonspell,
        B::RainbowWindChime,B::SummoningNonspell,B::FlowerDream,B::RainNonspell,B::ColorfulRain,B::ColorLightDance,
        B::DazzlingColorTyphoon,B::MidbossTimeout,B::Defeated};
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120})for(std::size_t route=0;route<std::size(entries);++route) {
        std::vector<effects::Request> expected;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor(rate);actor.Reset({0,0,5},13000,7);stage3::ConfigureBehavior(actor,false);
            actor.difficulty=route==3||route==7||route==10?native_combat::Difficulty::Hard:native_combat::Difficulty::Normal;
            actor.Start(entries[route](actor));auto bullets=std::make_unique<BulletSystem>();std::vector<effects::Request> actual;
            for(int frame=0;frame<fps*12;++frame) {
                bullets->Reset();actor.Update(1.0F/fps,{2,0,-6},*bullets);
                actual.insert(actual.end(),actor.effectEvents.begin(),actor.effectEvents.end());
            }
            assert(!actual.empty());
            if(route==0||route==4) {
                std::vector<effects::Request> attached;
                for(const auto& r:actual)if(r.kind==effects::Kind::Attached)attached.push_back(r);
                assert(attached.size()==4);
                const auto program=stage3::reference::ProgramsForDifficulty(1)[route==0?9:22];
                std::size_t index=0;
                for(const auto& ins:program)if(ins.opcode==102) {
                    const auto& r=attached.at(index++);const auto& a=ins.args;
                    assert(r.contentFrame==ins.frame+(route==0?100:0));
                    assert(r.axis.x==a[1]&&r.axis.y==-a[3]&&r.axis.z==-a[2]);
                    const auto anchor=r.attachment.lock();assert(anchor);Near(anchor->Radius(),a[4]*scale,1e-5F);
                    const auto color=int(a[0])==5?Color{255,224,224,255}:int(a[0])==6?Color{255,224,255,255}:Color{224,224,255,255};
                    assert(r.color.r==color.r&&r.color.g==color.g&&r.color.b==color.b&&r.color.a==color.a);
                }
                assert(index==4);
            }
            if(route==13)assert(actual.size()==30);
            if(fps==120)expected=actual;
            else {
                assert(actual.size()==expected.size());
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& e=expected[n];assert(a.kind==e.kind&&a.count==e.count&&a.contentFrame==e.contentFrame);
                    assert(a.color.r==e.color.r&&a.color.g==e.color.g&&a.color.b==e.color.b&&a.color.a==e.color.a);
                    Near(a.position.x,e.position.x);Near(a.position.y,e.position.y);Near(a.position.z,e.position.z);
                }
            }
        }
    }
}
void TestBirthsAndMovement() {
    for(int difficultyIndex=0;difficultyIndex<4;++difficultyIndex) {
        stage3::Stage3Content content(*native_combat::DifficultyFromIndex(difficultyIndex));assert(content.Events().size()==443);
        const auto programs=stage3::reference::ProgramsForDifficulty(difficultyIndex);
        std::vector<const native_combat::HealthRecord*> births;
        for(const auto& r:native_combat::Stage(3)->health)
            if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=9&&r.targetSub!=21)births.push_back(&r);
        assert(births.size()==437);std::size_t index=0;auto bullets=std::make_unique<BulletSystem>();
        for(const auto& event:content.Events()) {
            const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
            const auto& source=*births.at(index++);auto spawn=content.ConfigureSpawn(request->actor);
            assert(request->count==1&&request->interval==0&&spawn.sourceHealth==*source.value&&spawn.sourceScore==source.score&&spawn.sourceItem==source.item);
            assert(spawn.mirrored==(source.opcode==2||source.opcode==6)&&spawn.randomX==(source.position.x==-999));
            if(source.frame<3474)assert(event.atFrame==source.frame);else assert(!event.atFrame);
            spawn.retireOutside=false;spawn.pattern.spatialShots=false;behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
            unsigned random=spawn.seed;const int sub=source.targetSub;
            float x=source.position.x,y=source.position.y,vx=0,vy=0,angle=pi/2,speed=1.5F,accel=0,angular=0;
            if(spawn.randomX)x=static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F*384;
            if(sub<=1) {angle=0.52359879F;speed=4.5F;}
            if(sub==2||sub==3) {angle=-1.04719758F;speed=4;}
            // 发射见证直接读取本关筛选后的源参数；120 Hz 生产路径另测，不从新配方反推答案。
            int interval=0,nextShot=-1,automaticCount=0,streamCount=0,streamFan=0,openingRing=0,singleCount=0;
            for(const auto& op:programs[sub]) {
                if(op.frame==0&&(op.opcode==76||op.opcode==77)&&op.args[0]>0) {
                    const int base=static_cast<int>(op.args[0]);interval=base+base/5-(base/5*2*16)/32;
                    nextShot=interval-(op.opcode==77?static_cast<int>(Next(random)%interval):0)-1;
                }
                if(op.frame==0&&op.opcode>=67&&op.opcode<=75)automaticCount=static_cast<int>(op.args[2]*op.args[3]);
                if(op.frame==70&&op.opcode==4&&op.args[0]==-10009)streamCount=static_cast<int>(op.args[1]);
                if(op.frame==70&&op.opcode==68)streamFan=static_cast<int>(op.args[2]);
                if(op.frame==70&&op.opcode==69)openingRing=static_cast<int>(op.args[2]*op.args[3]);
                if(op.frame==70&&(op.opcode==67||op.opcode==75))singleCount=static_cast<int>(op.args[2]*op.args[3]);
            }
            if(sub==6)speed=2;
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
                        if(sub==8) {for(int i=0;i<singleCount*2;++i)Random(random,1);angle=Random(random,pi/2)+pi/4;speed=1.5F;}
                    }
                    if(sub==4&&frame==70+streamCount*2) {angle=Random(random,pi/2)+pi/4;speed=1.5F;}
                    if(sub==5&&frame==100) {angle=retreatAngle;speed=-1.5F;}
                    if(sub==7&&frame==70+streamCount*2) {angle=Random(random,pi/2)+pi/4;speed=-1.5F;}
                }
                angle+=angular;speed+=accel;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
                bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
                Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,spawn.position.y);
                std::size_t count=0;
                if((sub==4||sub==7)&&frame>=70&&frame<70+streamCount*2&&(frame-70)%2==0)count=streamFan;
                if((sub==4||sub==7)&&frame==70)count+=openingRing;
                if((sub==5||sub==8)&&frame==70)count=singleCount;
                const int stop=sub==6?220:sub<=1?115:90;
                if(frame==nextShot&&frame<stop) {count=automaticCount;nextShot+=interval;}
                if(bullets->Pool().ActiveCount()!=count) {std::cerr<<"Stage 3 difficulty "<<difficultyIndex<<" ordinary shot frame "<<frame<<" sub "<<sub<<'\n';std::abort();}
            }
            assert(actor.Random(1)==Random(random,1));
            // 全部实际种子也走正式三维首轮；高位为 1 的种子不能经有符号转换变成非法采样编号。
            auto spatial=spawn;spatial.pattern.spatialShots=true;behavior::Actor production;production.Activate(spatial);
            bullets->Reset();production.Update(1.2F,{2,3,-6},*bullets);
        }
        assert(index==437);
    }
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
        if(choice==0) {Random(random,1);Random(random,1);}
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
    for(int difficulty=0;difficulty<4;++difficulty) {
        stage3::Stage3Content variant(*native_combat::DifficultyFromIndex(difficulty));
        for(const auto* example:examples) {
            Vector3 position{};float random=0;std::vector<Bullet> expected;
            for(int fps:{120,30,60,144,240}) {
                auto spawn=variant.ConfigureSpawn(*example);spawn.retireOutside=false;behavior::Actor actor;actor.Activate(spawn);bullets->Reset();
                for(int i=0;i<fps*4;++i)actor.Update(1.0F/fps,{2,3,-6},*bullets);
                std::vector<Bullet> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(b);});const float next=actor.Random(1);
                if(fps==120) {position=actor.Position();random=next;expected=shots;}
                else {
                    Near(actor.Position().x,position.x,1e-5F);Near(actor.Position().z,position.z,1e-5F);assert(next==random&&shots.size()==expected.size());
                    for(std::size_t i=0;i<shots.size();++i) {
                        assert(shots[i].nativeType==expected[i].nativeType&&shots[i].nativeColor==expected[i].nativeColor);
                        Near(shots[i].position.x,expected[i].position.x,1e-5F);Near(shots[i].position.z,expected[i].position.z,1e-5F);
                        Near(shots[i].Velocity().x,expected[i].Velocity().x,1e-5F);Near(shots[i].Velocity().y,expected[i].Velocity().y,1e-5F);Near(shots[i].Velocity().z,expected[i].Velocity().z,1e-5F);
                    }
                }
            }
        }
    }
    behavior::Actor rain(behavior::UpdateRate::Hz60);rain.Reset({},3400,7);stage3::ConfigureBehavior(rain);rain.Start(bosses::MeilingBoss::Behaviors::ColorfulRain(rain));
    std::array<int,8> bands{};
    for(int frame=0;frame<400;++frame) {
        bullets->Reset();rain.Update(1.0F/60,{2,3,-6},*bullets);
        bullets->Pool().ForEachActive([&](const Bullet& b) {
            const float cosine=b.Velocity().x/BulletSystem::Length(b.Velocity());
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
/// @brief 四难度首轮直接对照源 ECL 弹数，源世界采样保持 Hz60，生产频率不改变内容帧。
void TestOrdinaryDifficultyFirstShots() {
    using D=native_combat::Difficulty;
    constexpr std::array difficulties{D::Easy,D::Normal,D::Hard,D::Lunatic};
    constexpr std::array streams{1U,1U,19U,21U},rings{8U,16U,24U,72U},rain{8U,14U,20U,22U};
    constexpr std::array sourceCounts{830,838,875,874};
    for(std::size_t d=0;d<difficulties.size();++d) {
      const auto programs=stage3::reference::ProgramsForDifficulty(d);
      std::size_t total=0;for(const auto sub:programs)total+=sub.size();assert(total==sourceCounts[d]);
      for(int pattern=0;pattern<3;++pattern) {
        const auto sub=pattern==0?4:pattern==1?5:8;
        std::size_t rawCount=0;
        for(const auto& op:programs[sub])if(op.frame==70 && op.opcode>=67&&op.opcode<=75)
            rawCount+=static_cast<std::size_t>(op.args[2]*op.args[3]);
        assert(rawCount==(pattern==0?streams[d]:pattern==1?rings[d]:rain[d]));
        auto spawn=testing::SpawnAt();spawn.difficulty=difficulties[d];
        spawn.pattern=pattern==0?stage3::kAimedStreamOrb:pattern==1?stage3::kRetreatingRingOrb:stage3::kRainOrb;
        spawn.pattern.spatialShots=false;auto actor=testing::MakeActor(spawn,behavior::UpdateRate::Hz60);
        auto bullets=std::make_unique<BulletSystem>();
        for(int frame=0;frame<=70;++frame) {bullets->Reset();actor->Update(1.0F/60,{2,3,-6},*bullets);}
        assert(bullets->Pool().ActiveCount()==(pattern==0?streams[d]:pattern==1?rings[d]:rain[d]));
      }
      // 同一 120 Hz 行为步由不同显示频率分批交付，不能加速内容时钟或改变随机读取。
      std::size_t expected=0;
      for(int fps:{30,60,120,144,240}) {
          auto spawn=testing::SpawnAt();spawn.difficulty=difficulties[d];spawn.pattern=stage3::kRainOrb;
          spawn.pattern.spatialShots=false;auto actor=testing::MakeActor(spawn);
          auto bullets=std::make_unique<BulletSystem>();
          for(int frame=0;frame<fps*2;++frame)actor->Update(1.0F/fps,{2,3,-6},*bullets);
          const auto actual=bullets->Pool().ActiveCount();
          if(fps==30)expected=actual;else assert(actual==expected);
          assert(actual==rain[d]);
      }
    }
}
/// @brief 四难度组合、正生命阈值和四机体击破/超时退场直接对照本关源指令。
void TestDifficultyBossComposition() {
    using S=bosses::MeilingBoss::Behavior;
    auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);
        const auto programs=stage3::reference::ProgramsForDifficulty(d);
        const auto instruction=[&](int sub,int opcode,int frame=0) {
            for(const auto& op:programs[sub])if(op.frame==frame&&op.opcode==opcode)return op.args[0];
            throw std::logic_error("Missing Stage 3 source instruction");
        };
        const bool easy=d==0,hard=d>=2;
        std::vector<float> health{instruction(22,111),instruction(22,113),instruction(24,111)};
        std::vector<int> limits{int(instruction(22,115)),int(instruction(34,115)),int(instruction(24,115))};
        std::vector<int> cards{0,25+d,0};
        std::vector<S> skills{S::FirstNonspell,S::RainbowWindChime,S::SummoningNonspell};
        if(hard) {
            health.push_back(instruction(24,113));limits.push_back(int(instruction(37,115,1)));
            cards.push_back(27+d);skills.push_back(S::FlowerDream);
        }
        health.insert(health.end(),{instruction(30,111),instruction(30,113)});
        limits.insert(limits.end(),{int(instruction(30,115)),int(instruction(hard?43:40,115))});
        cards.insert(cards.end(),{0,31+d});skills.insert(skills.end(),{S::RainNonspell,hard?S::ColorLightDance:S::ColorfulRain});
        if(!easy) {
            health.push_back(instruction(hard?42:39,113));limits.push_back(int(instruction(45,115)));
            cards.push_back(34+d);skills.push_back(S::DazzlingColorTyphoon);
        }
        stage3::Stage3Content content(difficulty);
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
            auto encounter=content.CreateBoss(stage3::kMeilingFinal,static_cast<ShotType>(shot));encounter->ResumeCombat();
            for(std::size_t attack=0;attack<skills.size();++attack) {
                assert(encounter->attack==int(attack)+1&&encounter->health==health[attack]);
                bosses::MeilingBoss definitions;
                const auto definition=definitions.DescribeSkill(skills[attack],static_cast<ShotType>(shot),difficulty);
                assert(definition.NativeHealth()==health[attack]&&definition.spellCard==cards[attack]);
                assert(definition.timeoutFrames==limits[attack]);
                // 华想梦葛在第 1 内容帧才设置计时器，刚交接时 HUD 仍无时限。
                assert(encounter->TimeLimit()==(definition.timeoutArmFrame>0?-1.0F/60:limits[attack]/60.0F));
                if(skills[attack]==S::ColorfulRain||skills[attack]==S::ColorLightDance)
                    assert(definition.thresholdDefeatsSpell==!easy&&definition.reserveHealth==(easy?0:2000));
                bool complete=false;
                for(int frame=0;frame<limits[attack]+180&&!complete;++frame) {
                    bullets->Reset();complete=encounter->Update(1.0F/60,{2,3,-6},*bullets);
                    if(damage&&encounter->CanTakeDamage()&&encounter->skillTimer>3) {
                        encounter->health=definition.reserveHealth>0?definition.reserveHealth-1:0;
                        assert(encounter->ShouldAdvanceFromDamage());complete=true;
                    }
                }
                const bool defeated=damage&&(definition.reserveHealth==0||definition.thresholdDefeatsSpell);
                assert(complete&&encounter->SkillDefeated()==defeated);
                if(cards[attack]>0)assert(encounter->SpellCardNumber()==cards[attack]);
                assert(encounter->AdvanceSkill()&&!encounter->CanTakeDamage());
            }
            for(int frame=0;frame<70&&encounter->active;++frame) {
                bullets->Reset();if(encounter->Update(1.0F/60,{},*bullets))encounter->AdvanceSkill();
            }
            assert(!encounter->active&&encounter->Defeated()==damage);
        }
    }
}
/// @brief 独立展开 Sub18 的双方向与原样角度补偿，逐帧比较两种高难的实际弹数、速度和延迟转向。
void TestSelaginellaSourceCycle() {
    using S=bosses::MeilingBoss::Behavior;
    auto bullets=std::make_unique<BulletSystem>();
    for(int d:{2,3}) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);
        const auto program=stage3::reference::ProgramsForDifficulty(d);
        const auto& source=program[18];
        std::vector<stage3::reference::Instruction> rings,turns;
        for(const auto& op:source) {
            if(op.opcode==70)rings.push_back(op);
            if(op.opcode==82)turns.push_back(op);
        }
        assert(rings.size()==2&&turns.size()==2);
        const auto aimed=std::find_if(source.begin(),source.end(),[](const auto& op){return op.opcode==69;});
        assert(aimed!=source.end());
        // 0x1f24 的目的寄存器确实为左方向；该非对称不变量不能由生产配方生成。
        const auto compensation=std::find_if(source.begin(),source.end(),[](const auto& op){return op.offset==0x1f24;});
        assert(compensation!=source.end()&&compensation->opcode==20&&compensation->args[0]==-10005);
        bosses::MeilingBoss definitions;
        const auto skill=definitions.DescribeSkill(S::Selaginella,ShotType::HomingAmulet,difficulty);
        assert(skill.NativeHealth()==1300&&skill.timeoutFrames==1200&&skill.spellCard==d+21);
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({},1300,7);actor.difficulty=difficulty;
        stage3::ConfigureBehavior(actor,false);actor.Start(bosses::MeilingBoss::Behaviors::Selaginella(actor));
        float left=0,right=0;
        for(int frame=0;frame<1200;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool burst=frame>=120&&(frame-120)%6==0;
            const bool circle=frame>=120&&frame%120==0;
            const int count=static_cast<int>(rings[0].args[2]);
            assert(bullets->Pool().ActiveCount()==std::size_t((burst?count*2:0)+(circle?aimed->args[2]:0)));
            int emitted=0;
            bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                if(bullet.nativeColor!=13)return;
                const int side=emitted/count,n=emitted%count;const auto& op=rings[side];
                const float angle=(side?right:left)+n*2*pi/count;
                Near(bullet.Velocity().x,std::cos(angle)*op.args[4]*(60*scale),1e-5F);
                Near(bullet.Velocity().z,-std::sin(angle)*op.args[4]*(60*scale),1e-5F);
                Near(bullet.turnDelaySeconds,turns[side].args[0]/60,1e-6F);
                Near(bullet.turnAngle,turns[side].args[4],1e-6F);
                Near(bullet.turnSpeed,turns[side].args[5]*(60*scale),1e-5F);++emitted;
            });
            assert(emitted==(burst?count*2:0));
            if(burst) {
                left+=0.1483529806137085F;right-=0.16580627858638763F;
                if(left>pi)left-=2*pi;
                if(right<pi)left+=2*pi;
            }
        }
        // 初始方向全是常量，整张符卡不得消费演员随机流。
        unsigned random=7;assert(actor.Random(1)==Random(random,1));
    }
}
/// @brief 高难移动弹的固定周期从配置帧的步末开始计数，停止帧不得再次发射。
void TestMovingEmitter() {
    stage3::Stage3Content content(native_combat::Difficulty::Hard);
    auto spawn=testing::SpawnAt();spawn.pattern=stage3::kBlueHookOrb;spawn.pattern.spatialShots=false;
    spawn=content.ConfigureSpawn(spawn);
    auto actor=testing::MakeActor(spawn,behavior::UpdateRate::Hz60);
    auto bullets=std::make_unique<BulletSystem>();
    for(int frame=0;frame<125;++frame) {
        bullets->Reset();actor->Update(1.0F/60,{2,3,-6},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==59?1U:0U));
    }
}
/// @brief 四难度弹流、非符及符卡按独立源上下文逐帧比较生成快照；星形另核对原生扩展几何。
void TestHighSpellSourceExecution() {
    using Entry=behavior::Entry;
    struct Case {int source;Entry production;int frames;unsigned difficulties=15;};
    const std::array cases{
        Case{4,stage3::AcceleratingStream,500},
        Case{7,stage3::RisingStream,500},
        Case{22,bosses::MeilingBoss::Behaviors::FirstNonspell,1800},
        Case{24,bosses::MeilingBoss::Behaviors::SummoningNonspell,2400},
        Case{29,stage3::SummonedStream,1200},
        Case{30,bosses::MeilingBoss::Behaviors::RainNonspell,2300},
        Case{33,bosses::MeilingBoss::Behaviors::RainbowWindChime,1800},
        Case{36,bosses::MeilingBoss::Behaviors::FlowerDream,1800,12},
        Case{39,bosses::MeilingBoss::Behaviors::ColorfulRain,1800,3},
        Case{42,bosses::MeilingBoss::Behaviors::ColorLightDance,1800,12},
        Case{45,bosses::MeilingBoss::Behaviors::DazzlingColorTyphoon,2100,14}};
    auto bullets=std::make_unique<BulletSystem>();
    std::array<std::array<bool,61>,4> visited{};
    for(int difficulty:{0,1,2,3})for(const auto& entry:cases)if(entry.difficulties&(1U<<difficulty))for(unsigned seed:{7U,0x9e3779b9U}) {
        stage3::reference::SpellReference original(difficulty,entry.source,{192,96,0},seed);
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,10-96*scale},1400,seed);
        // 独立技能入口继承前一非符的战斗边界；源执行器使用同一已进入战斗的初始上下文。
        actor.motion.bounds={{(32-192)*scale,10-144*scale},{(352-192)*scale,10-48*scale}};actor.motion.clamped=true;
        actor.difficulty=*native_combat::DifficultyFromIndex(difficulty);stage3::ConfigureBehavior(actor,false);actor.Start(entry.production(actor));
        if(entry.source==4||entry.source==7||entry.source==29) {
            auto spawn=testing::SpawnAt(actor.Position());spawn.seed=seed;spawn.difficulty=actor.difficulty;
            spawn.pattern=entry.source==4?stage3::kAimedStreamOrb:entry.source==7?stage3::kRisingStreamOrb:stage3::kSummonedStream;
            spawn.pattern.spatialShots=false;spawn.retireOutside=false;actor.Activate(spawn);
        }
        for(int frame=0;frame<entry.frames;++frame) {
            bullets->Reset();original.Tick({240,360,0});actor.Update(1.0F/60,{48*scale,0,10-360*scale},*bullets);
            CompareStage3Effects(actor,original.effects);
            const auto fail=[&](const char* field) {
                std::cerr<<"Stage 3 spell difficulty "<<difficulty<<" source "<<entry.source<<" frame "<<frame<<" field "<<field<<'\n';std::abort();
            };
            CompareStage3Auxiliary(actor,original);
            if(bullets->Pool().ActiveCount()!=original.emissions.size())fail("emission count");
            const auto close=[&](float actual,float expected,float tolerance,const char* field) {
                if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
                    std::cerr<<"actual "<<actual<<" expected "<<expected<<" difference "<<actual-expected<<'\n';fail(field);
                }
            };
            close(actor.Position().x,(original.position.x-192)*scale,5e-4F,"actor X");
            close(actor.Position().z,10-original.position.y*scale,5e-4F,"actor Z");
            std::size_t index=0;
            bullets->Pool().ForEachActive([&](const Bullet& actual) {
                const auto& expected=original.emissions.at(index++);
                if(actual.nativeType!=expected.type||actual.nativeColor!=expected.color)fail("bullet identity");
                close(actual.position.x,(expected.position.x-192)*scale,5e-4F,"bullet X");
                close(actual.position.z,10-expected.position.y*scale,5e-4F,"bullet Z");
                close(actual.Velocity().x,expected.velocity.x*(60*scale),1e-4F,"velocity X");
                close(actual.Velocity().z,-expected.velocity.y*(60*scale),1e-4F,"velocity Z");
                close(actual.acceleration.x,expected.acceleration.x*(3600*scale),1e-4F,"acceleration X");
                close(actual.acceleration.z,-expected.acceleration.y*(3600*scale),1e-4F,"acceleration Z");
                if(expected.flags&64) {
                    close(actual.turnAngle,expected.turnAngle,1e-5F,"turn angle");
                    close(actual.turnSpeed,expected.turnSpeed*(60*scale),1e-4F,"turn speed");
                    close(actual.turnDelaySeconds,expected.turnDelay/60,1e-5F,"turn delay");
                }
            });
            if(actor.spawnEvents.size()!=original.spawns.size())fail("summon count");
            for(std::size_t i=0;i<original.spawns.size();++i) {
                const auto& source=original.spawns[i];const auto& actual=actor.spawnEvents[i];
                close(actual.position.x,(source.position.x-192)*scale,5e-4F,"summon X");
                close(actual.position.z,10-source.position.y*scale,5e-4F,"summon Z");
                if(actual.sourceHealth!=source.health||actual.sourceItem!=source.item||actual.sourceScore!=source.score)fail("summon metadata");
            }
            if(frame>1&&actor.damageable!=original.damageable)fail("damage switch");
            if(actor.soundEvents!=original.sounds)fail("sound sequence");
        }
        for(std::size_t sub=0;sub<original.visited.size();++sub)visited[difficulty][sub]|=original.visited[sub];
        if(actor.Random(1)!=original.Random(1)) {
            std::cerr<<"Stage 3 spell difficulty "<<difficulty<<" source "<<entry.source<<" final random state differs\n";std::abort();
        }
    }
    for(int d=0;d<4;++d) {
        for(int sub:{26,27,28})assert(visited[d][sub]);
        std::cout<<"Stage 3 difficulty "<<d<<" combat reference visited "<<std::count(visited[d].begin(),visited[d].end(),true)<<" subprograms\n";
    }
}
/// @brief 新符卡在默认 120 Hz 下保持 60 Hz 发射节奏，并跨五种显示频率比较三维快照和随机流。
void TestBossSpellDisplayRates() {
    using Entry=behavior::Entry;
    const std::array<Entry,3> entries{bosses::MeilingBoss::Behaviors::FlowerDream,
        bosses::MeilingBoss::Behaviors::ColorLightDance,bosses::MeilingBoss::Behaviors::DazzlingColorTyphoon};
    auto bullets=std::make_unique<BulletSystem>();
    for(int d:{2,3})for(const auto entry:entries) {
        Vector3 position{};float random=0;std::vector<Bullet> expected;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;actor.Reset({0,0,5.71428585F},1400,7);actor.difficulty=*native_combat::DifficultyFromIndex(d);
            actor.motion.bounds={{-7.14285714F,3.57142857F},{7.14285714F,7.85714286F}};actor.motion.clamped=true;
            stage3::ConfigureBehavior(actor);actor.Start(entry(actor));bullets->Reset();
            for(int frame=0;frame<fps*8;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            assert(!bullets->Pool().DroppedSpawns());
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& bullet){actual.push_back(bullet);});
            const float next=actor.Random(1);
            if(fps==120) {position=actor.Position();random=next;expected=actual;}
            else {
                Near(actor.Position().x,position.x,1e-5F);Near(actor.Position().y,position.y,1e-5F);Near(actor.Position().z,position.z,1e-5F);
                assert(next==random&&actual.size()==expected.size());
                for(std::size_t i=0;i<actual.size();++i) {
                    assert(actual[i].nativeType==expected[i].nativeType&&actual[i].nativeColor==expected[i].nativeColor);
                    Near(actual[i].position.x,expected[i].position.x,1e-5F);Near(actual[i].position.y,expected[i].position.y,1e-5F);Near(actual[i].position.z,expected[i].position.z,1e-5F);
                    Near(actual[i].Velocity().x,expected[i].Velocity().x,1e-5F);Near(actual[i].Velocity().y,expected[i].Velocity().y,1e-5F);Near(actual[i].Velocity().z,expected[i].Velocity().z,1e-5F);
                }
            }
        }
        assert(!expected.empty());
    }
}
/// @brief 三面效果专项覆盖独立源请求、完整攻击及双频率显示节奏。
void RunStage3EffectTests() {TestMeilingEntranceAndRetirementEffects();TestMeilingEffectDisplayRates();TestHighSpellSourceExecution();}
void RunStage3Tests() {TestMeilingEntranceAndRetirementEffects();TestMeilingEffectDisplayRates();TestBirthsAndMovement();TestAttackWitnesses();TestBossTransitions();TestSceneAndTimelineWaits();TestSpatialAndDisplayRates();TestEntranceAndDepartureSourceBoundaries();TestOrdinaryDifficultyFirstShots();TestDifficultyBossComposition();TestSelaginellaSourceCycle();TestMovingEmitter();TestHighSpellSourceExecution();TestBossSpellDisplayRates();}

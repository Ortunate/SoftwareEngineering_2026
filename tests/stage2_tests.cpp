// 本文件以独立源参数检查二面全部出生、60 Hz 普通演员轨迹、发射边界、Boss 血量/超时与新公共能力。
#include "boss_fixture.hpp"
#include "stage2_reference_program.hpp"
#include "stage2_effect_reference.hpp"
#include "stages/stage2/content.hpp"
#include "stages/stage2/behaviors.hpp"
#include "stages/stage2/boss_config.hpp"
#include "gameplay/bosses/cirno.hpp"
#include "gameplay/bosses/daiyousei.hpp"
#include "gameplay/items/drop_system.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <memory>
#include <numbers>
#include <vector>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>,scale=20.0F/448;
void Near(float a,float b,float tolerance=5e-4F) {
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) {
        std::cerr<<"Stage 2 mismatch: "<<a<<" vs "<<b<<'\n';std::abort();
    }
}
/// @brief 独立源参考的 xorshift 序列，仅对照本项目约定的出生种子；不宣称等同新典原生 RNG。
float Random(unsigned& state,float range) {
    state^=state<<13;state^=state>>17;state^=state<<5;
    return static_cast<float>(state>>8)/16777216.0F*range;
}
/// @brief 二面有限源窗口逐步核对102/118、音效和延后清弹；源坐标/颜色换算只留测试侧。
void TestStage2EffectReference() {
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int sub:{20,22,28,39}) {
        const Vector3 point=sub==20?Vector3{192,-32,0}:Vector3{192,112,0};
        stage2::reference::EffectReference source(d,sub,point,seed);
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({(point.x-192)*scale,0,10-point.y*scale},10000,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);stage2::ConfigureBehavior(actor,false);
        actor.motion.bounds={{-7.14285714F,4.01785714F},{7.14285714F,7.85714286F}};actor.motion.clamped=sub!=20;
        const auto entry=sub==20?bosses::DaiyouseiBoss::Behaviors::Midboss:sub==22?bosses::CirnoBoss::Behaviors::FirstNonspell:
            sub==28?bosses::CirnoBoss::Behaviors::Defeated:bosses::CirnoBoss::Behaviors::DiamondBlizzard;
        actor.Start(entry(actor));int requests=0,particles=0;const int end=sub==20?129:sub==22?69:sub==28?60:59;
        for(int frame=0;frame<=end;++frame) {
            bullets->Reset();auto* retained=bullets->Spawn(BulletOwner::Enemy,{}, {},.1F,WHITE,stage2::kBlueBullet);assert(retained);
            source.Tick();actor.Update(1.0F/60,{},*bullets);
            assert(actor.effectEvents.size()==source.effects.size()&&actor.soundEvents==source.sounds);
            assert(actor.damageable==source.damageable&&retained->active==!source.clearBullets);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            for(std::size_t n=0;n<source.effects.size();++n) {
                const auto& a=actor.effectEvents[n];const auto& e=source.effects[n];++requests;particles+=a.count;
                assert(e.type==3||e.type==12||e.type==13);
                assert(a.kind==(e.type==3?effects::Kind::Burst:e.type==12?effects::Kind::Flash:effects::Kind::Attached)&&a.count==e.count);
                assert(a.color.r==((e.color>>16)&255)&&a.color.g==((e.color>>8)&255)&&a.color.b==(e.color&255)&&a.color.a==(e.color>>24));
                Near(a.position.x,(e.position.x-192)*scale);Near(a.position.y,-e.position.z*scale);Near(a.position.z,10-e.position.y*scale);
                if(e.type==13) {
                    assert(a.axis.x==e.axis.x&&a.axis.y==-e.axis.z&&a.axis.z==-e.axis.y);
                    const auto anchor=a.attachment.lock();assert(anchor);Near(anchor->Radius(),e.radius*scale,1e-5F);
                }
            }
        }
        assert(requests==(sub==28?30:sub==39?1:4)&&particles==(sub==28?60:sub==39?1:4));
    }
}
/// @brief 双固定更新频率和五显示FPS保持同一内容请求窗口；同频率比较全部位置，不放宽时序。
void TestStage2EffectDisplayRates() {
    for(const auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120})for(int sub:{20,22,28,39}) {
        std::vector<effects::Request> expected;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor(rate);actor.Reset({0,0,5},10000,7);stage2::ConfigureBehavior(actor,false);
            const auto entry=sub==20?bosses::DaiyouseiBoss::Behaviors::Midboss:sub==22?bosses::CirnoBoss::Behaviors::FirstNonspell:
                sub==28?bosses::CirnoBoss::Behaviors::Defeated:bosses::CirnoBoss::Behaviors::DiamondBlizzard;
            actor.Start(entry(actor));auto bullets=std::make_unique<BulletSystem>();std::vector<effects::Request> actual;
            for(int n=0;n<fps*3;++n) {bullets->Reset();actor.Update(1.0F/fps,{},*bullets);actual.insert(actual.end(),actor.effectEvents.begin(),actor.effectEvents.end());}
            assert(actual.size()==(sub==28?30U:sub==39?1U:4U));
            for(std::size_t n=0;n<actual.size();++n)assert(actual[n].contentFrame==(sub==28?n/5*10+(n%5+1)*2:sub==39?0:(sub==20?60:20)+n*10));
            if(fps==120)expected=actual;
            else for(std::size_t n=0;n<actual.size();++n) {
                const auto& a=actual[n];const auto& e=expected[n];assert(a.kind==e.kind&&a.count==e.count&&a.contentFrame==e.contentFrame);
                assert(a.color.r==e.color.r&&a.color.g==e.color.g&&a.color.b==e.color.b&&a.color.a==e.color.a);
                Near(a.position.x,e.position.x);Near(a.position.y,e.position.y);Near(a.position.z,e.position.z);
            }
        }
    }
}
void TestDifficultySourceShots() {
    // 对照 ECL Sub12 第 70 帧及 Sub5 击破发射，不从生产配方推导期望。
    for(unsigned index=0;index<4;++index) {
        const auto difficulty=*native_combat::DifficultyFromIndex(index);
        const auto programs=stage2::reference::ProgramsForDifficulty(index);
        const stage2::reference::Instruction* source=nullptr;
        for(const auto& instruction:programs[12])if(instruction.opcode==67&&instruction.frame==70)source=&instruction;
        assert(source);
        behavior::Spawn spawn{};spawn.visual=stage2::kOrb;spawn.pattern=stage2::kShootingDarkFairy;
        stage2::Stage2Content content(difficulty);
        const auto configured=content.ConfigureSpawn(spawn);
        assert(configured.difficulty==difficulty&&configured.pattern.skill);
        const auto& shot=configured.pattern.skill->shot;
        assert(shot.count==source->args[2]&&shot.layers==source->args[3]);
        Near(shot.firstSpeed,source->args[4]*60*scale,1e-5F);Near(shot.spacing,source->args[7],1e-6F);
        spawn.pattern=stage2::kLeftRedOrb;
        const auto red=content.ConfigureSpawn(spawn);
        assert((red.pattern.routine!=nullptr)==(index==3));
        assert(behavior::Valid(red));
    }
}
void TestBirthsAndSourceMovement() {
    stage2::Stage2Content content;assert(content.Events().size()==468);
    std::vector<const native_combat::HealthRecord*> source;
    for(const auto& r:native_combat::Stage(2)->health)
        if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=20&&r.targetSub!=21)source.push_back(&r);
    assert(source.size()==464);
    std::size_t birth=0;auto bullets=std::make_unique<BulletSystem>();
    for(const auto& event:content.Events()) {
        const auto* request=std::get_if<SpawnActors>(&event.action);if(!request)continue;
        const auto& original=*source.at(birth++);const auto& spawn=request->actor;
        assert(request->count==1&&request->interval==0&&event.atFrame==original.frame);
        assert(spawn.sourceHealth==*original.value&&spawn.sourceScore==original.score&&spawn.sourceItem==original.item);
        assert(spawn.randomX==(original.position.x==-999)&&spawn.mirrored==(original.opcode==2||original.opcode==6));
        assert(behavior::Valid(spawn));
        auto neutral=spawn;neutral.pattern.spatialShots=false;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(neutral);bullets->Reset();
        const int sub=original.targetSub;unsigned random=spawn.seed;
        assert(actor.rotateAnimation==(sub>=8&&sub<=11));
        float x=spawn.randomX ? static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F*384 : original.position.x;
        float y=original.position.y,vx=0,vy=0,angle=pi/2,speed=0,acceleration=0,angular=0;
        int nextShot=-1,cutoff=0;
        // 下列源参数独立转录自 ecldata2 的普通出生入口，不从新 Pattern 读取运动或技能答案。
        if(sub<=4) {
            angle=std::array{0.785398185F,1.17809725F,1.57079637F,1.96349537F,2.3561945F}[sub];
            speed=3;acceleration=-0.0149999997F;
            if(sub%2==0) {
                random^=random<<13;random^=random>>17;random^=random<<5;
                nextShot=180-static_cast<int>(random%180)-1;cutoff=sub==0 ? 180 : 200;
            }
        } else if(sub==6) {
            const float playerX=2/scale+192,playerY=16/scale;
            angle=std::atan2(playerY-y,playerX-x);speed=2.400000095F;
        } else if(sub>=7&&sub<=11) {
            angle=Random(random,1.57079637F)+0.785398185F;speed=sub==7 ? 3 : 5;
        } else { assert(sub==12);speed=2; }
        // 源像素累计用于轨迹差分；回收是项目世界边界契约，另以独立世界积分核对离散状态。
        // 不能把接近源边界时的两种浮点累计混用，也不能给回收比较添加容差。
        float wx=spawn.randomX ? -8.57142857F+static_cast<float>((spawn.seed*1664525U+1013904223U)>>8)/16777216.0F*17.1428571F : spawn.position.x;
        float wz=spawn.position.z,wvx=0,wvz=0,wa=angle,ws=speed*60*scale,wacc=acceleration*3600*scale,wangular=0;
        if(sub==6)wa=std::atan2(wz+6,2-wx);
        const auto inside=[](float x,float z) {return x>=-9.28571429F&&x<=9.28571429F&&z>=-10.7142857F&&z<=10.7142857F;};
        bool entered=inside(wx,wz);std::size_t expectedShots=0;
        for(int frame=0;frame<500&&actor.active;++frame) {
            x+=vx*(spawn.mirrored ? -1 : 1);y+=vy;
            wx+=wvx/60*(spawn.mirrored ? -1 : 1);wz+=wvz/60;
            if(sub==6) {
                if(frame==180) {angular=-0.0245436933F;wangular=angular*60;}
                if(frame==280)angular=wangular=0;
            }
            if(sub==12) {
                if(frame==60)speed=ws=0;
                if(frame==130) {acceleration=0.0500000007F;angular=0.052359879F;wacc=acceleration*3600*scale;wangular=angular*60;}
                if(frame==190)angular=wangular=0;
                if(frame==70)expectedShots+=14;
            }
            if(nextShot>=0&&frame==nextShot&&frame<cutoff) { expectedShots+=4;nextShot+=180; }
            angle+=angular;speed+=acceleration;vx=std::cos(angle)*speed;vy=std::sin(angle)*speed;
            wa+=wangular/60;ws+=wacc/60;wvx=std::cos(wa)*ws;wvz=-std::sin(wa)*ws;
            actor.Update(1.0F/60,{2,3,-6},*bullets);
            Near(actor.position.x,(x-192)*scale);Near(actor.position.z,10-y*scale);Near(actor.position.y,spawn.position.y);
            assert(bullets->Pool().ActiveCount()==expectedShots);
            Near(actor.position.x,wx);Near(actor.position.z,wz);
            if(inside(wx,wz))entered=true;
            const bool expectedActive=!(entered&&!inside(wx,wz));
            if(actor.active!=expectedActive) {
                std::cerr<<std::setprecision(12)<<"Retirement mismatch sub "<<sub<<" frame "<<frame<<" source "<<x<<","<<y
                         <<" world "<<actor.position.x<<","<<actor.position.z<<" entered "<<entered
                         <<" birth "<<original.frame<<" seed "<<spawn.seed<<" source origin "<<original.position.x<<","<<original.position.y<<'\n';
                std::abort();
            }
        }
    }
    assert(birth==464);
    // 异步依赖必须等待实际完成；不能因原始出生帧为 5894 就在对白尚未结束时启动关底。
    StageDirector director(content.Events());bool finalStarted=false,dialogueStarted=false;
    director.Update(100, [&](const StageEvent& event) {
        if(const auto* boss=std::get_if<StartBoss>(&event.action))finalStarted|=boss->battle==&stage2::kCirnoFinal;
        if(std::holds_alternative<StartDialogue>(event.action))dialogueStarted=true;
        return std::holds_alternative<SpawnActors>(event.action);
    });
    assert(dialogueStarted&&!finalStarted);assert(director.EndEvent(2));
    director.Update(1.0F/60,[&](const StageEvent& event) { finalStarted=std::holds_alternative<StartBoss>(event.action);return false; });
    assert(finalStarted);
}
void TestActorDefeatAndRandomSkills() {
    behavior::Spawn spawn;spawn.visual=stage2::kOrb;spawn.pattern=stage2::kCenterBlueOrb;spawn.retireOutside=false;
    spawn.pattern.spatialShots=false;auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;
    actor.Activate(spawn);actor.health=0;assert(actor.Defeat());assert(!actor.Defeat());
    assert(!actor.CanContactPlayer()&&!actor.CanTakeDamage());
    actor.Update(1.0F/60,{},*bullets);assert(actor.active&&bullets->Pool().ActiveCount()==0);
    actor.Update(1.0F/120,{},*bullets);assert(!actor.active&&bullets->Pool().ActiveCount()==4);
    actor.Activate(spawn);assert(!actor.retiring&&actor.CanTakeDamage());
    skill::State state;state.rankInfluence={};bullets->Reset();std::size_t reads=0;
    const std::array<float,4> samples{0.25F,0.75F,0.5F,0.25F};
    state.Fire({skill::Formation::RandomAngleSpeed,1,6,2,1,8,4,1,-1}, {}, {},*bullets,
        [&](float range) {return samples.at(reads++)*range;});
    assert(reads==4);std::vector<Bullet> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(b);});
    Near(shots[0].Velocity().x,std::cos(-0.5F)*7);Near(shots[0].Velocity().z,-std::sin(-0.5F)*7);
    Near(shots[1].Velocity().x,5);Near(shots[1].Velocity().z,0);
    // 固定扇形的轴必须来自显式世界方向；单方向不能忽略 angle 而全部射向 -Z。
    bullets->Reset();state.spatial=true;state.Fire({skill::Formation::FixedFan,1,6,1,1,5,0,pi/2}, {},{},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& b) {Near(b.Velocity().x,0);Near(b.Velocity().z,-5);});
    bullets->Reset();state.Fire(skill::Laser{.angle=pi/2,.speed=6}, {},{1,0,0},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& b) {Near(b.Direction().z,-1);Near(b.Velocity().z,-6);});
}
/// @brief 新典随机周期使用整数取模，rank 项在加 216 前截断；覆盖合并截断会出错的 rank。
void TestRandomShotInterval() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int rank:{10,11,15,16,17,31,32}) {
        behavior::Spawn spawn;spawn.pattern=stage2::kCenterBlueOrb;spawn.pattern.spatialShots=false;
        spawn.retireOutside=false;spawn.seed=73;spawn.visual=stage2::kOrb;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.skills.rank=rank;
        unsigned random=73;random^=random<<13;random^=random>>17;random^=random<<5;
        const int period=216-static_cast<int>(72*rank/32);
        int next=period-static_cast<int>(random%period)-1;
        for(int frame=0;frame<=201;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=frame==next&&frame<200;
            assert((bullets->Pool().ActiveCount()>0)==fires);
            if(fires)next+=period;
        }
    }
}
void TestDifficultyPeriodicOrbs() {
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned index=0;index<4;++index)for(int sub:{0,1}) {
        const auto difficulty=*native_combat::DifficultyFromIndex(index);
        stage2::Stage2Content content(difficulty);behavior::Spawn spawn;
        spawn.visual=stage2::kOrb;spawn.retireOutside=false;spawn.seed=73;
        spawn.pattern=sub==0 ? stage2::kLeftBlueOrb : stage2::kLeftRedOrb;
        spawn.pattern.spatialShots=false;spawn=content.ConfigureSpawn(spawn);
        const auto program=stage2::reference::ProgramsForDifficulty(index)[sub];
        const stage2::reference::Instruction* sourceShot=nullptr;bool periodic=false;
        for(const auto& instruction:program) {
            if(instruction.opcode==69||instruction.opcode==71)sourceShot=&instruction;
            if(instruction.opcode==77)periodic=true;
        }
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
        unsigned random=73;random^=random<<13;random^=random>>17;random^=random<<5;
        int next=180-static_cast<int>(random%180)-1;
        for(int frame=0;frame<=200;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            const bool fires=periodic&&frame==next&&frame<(sub==0 ? 180 : 200);
            const auto expected=fires ? static_cast<std::size_t>(sourceShot->args[2]*sourceShot->args[3]) : 0;
            assert(bullets->Pool().ActiveCount()==expected);
            if(fires)next+=180;
        }
    }
}
void TestAimedFairyDifficultyPeriod() {
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned index=0;index<4;++index) {
        const auto difficulty=*native_combat::DifficultyFromIndex(index);
        stage2::Stage2Content content(difficulty);behavior::Spawn spawn;
        spawn.visual=stage2::kOrb;spawn.retireOutside=false;spawn.seed=73;
        spawn.pattern=stage2::kAimedFairy;spawn.pattern.spatialShots=false;spawn=content.ConfigureSpawn(spawn);
        const auto program=stage2::reference::ProgramsForDifficulty(index)[6];
        bool periodic=false;const stage2::reference::Instruction* sourceShot=nullptr;
        for(const auto& instruction:program) {
            if(instruction.opcode==77)periodic=true;
            if(instruction.opcode==67)sourceShot=&instruction;
        }
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);
        unsigned random=73;random^=random<<13;random^=random>>17;random^=random<<5;
        int next=120-static_cast<int>(random%120)-1;
        for(int frame=0;frame<300;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,0,-6},*bullets);
            const bool fires=periodic&&frame==next;
            assert(bullets->Pool().ActiveCount()==(fires ? static_cast<std::size_t>(sourceShot->args[2]) : 0));
            if(fires)next+=120;
        }
    }
}
void TestDifficultyOrbDeath() {
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned index=0;index<4;++index) {
        const auto difficulty=*native_combat::DifficultyFromIndex(index);
        const auto programs=stage2::reference::ProgramsForDifficulty(index);
        const stage2::reference::Instruction* original=nullptr;
        for(const auto& instruction:programs[5])if(instruction.opcode==71)original=&instruction;
        assert(original);
        behavior::Spawn spawn;spawn.visual=stage2::kOrb;spawn.pattern=stage2::kCenterBlueOrb;
        spawn.pattern.spatialShots=false;spawn.retireOutside=false;spawn.difficulty=difficulty;
        behavior::Actor actor(behavior::UpdateRate::Hz60);bullets->Reset();actor.Activate(spawn);
        actor.health=0;assert(actor.Defeat());
        for(int frame=0;frame<3&&actor.active;++frame)actor.Update(1.0F/60,{},*bullets);
        assert(!actor.active&&bullets->Pool().ActiveCount()==static_cast<std::size_t>(original->args[2]));
        bullets->Pool().ForEachActive([&](const Bullet& bullet) {
            // 默认 rank=16 的速度修正为零，按源像素/帧转换为世界单位/秒。
            Near(BulletSystem::Length(bullet.Velocity()),original->args[4]*60*scale,1e-5F);
        });
    }
}
void TestDifficultyBossDefinitions() {
    using B=bosses::CirnoBoss::Behavior;
    bosses::CirnoBoss boss(behavior::UpdateRate::Hz60,false);
    for(unsigned index=0;index<4;++index) {
        const auto difficulty=*native_combat::DifficultyFromIndex(index);
        const auto source=stage2::reference::ProgramsForDifficulty(index);
        const auto literal=[&](int sub,int opcode) {
            const stage2::reference::Instruction* found=nullptr;
            for(const auto& instruction:source[sub])if(instruction.frame==0&&instruction.opcode==opcode) {
                assert(!found);found=&instruction;
            }
            assert(found);return found->args[0];
        };
        const auto first=boss.DescribeSkill(B::FirstNonspell,ShotType::HomingAmulet,difficulty);
        const auto ice=boss.DescribeSkill(B::IcicleFall,ShotType::HomingAmulet,difficulty);
        const auto second=boss.DescribeSkill(B::SecondNonspell,ShotType::HomingAmulet,difficulty);
        const auto freeze=boss.DescribeSkill(B::PerfectFreeze,ShotType::HomingAmulet,difficulty);
        assert(first.NativeHealth()==literal(22,111)&&first.reserveHealth==literal(22,113));
        assert(ice.timeoutFrames==literal(index<2 ? 31 : 34,115));
        assert(second.NativeHealth()==literal(25,111)&&second.reserveHealth==literal(25,113));
        assert(freeze.NativeHealth()==literal(25,113)&&freeze.reserveHealth==(index==0 ? 0 : literal(36,113)));
        assert(ice.spellCard==static_cast<int>(source[index<2 ? 31 : 34][0].args[1])+1);
        assert(freeze.spellCard==static_cast<int>(source[37][0].args[1])+1);
        const auto config=stage2::CirnoFinalConfig(ShotType::HomingAmulet,difficulty);
        assert(config.phases[1].skills.size()==(index==0 ? 2 : 3));
        boss.StartBoss(config);assert(boss.health==literal(22,111));
        boss.AdvanceSkill();assert(boss.SpellCardNumber()==ice.spellCard);
        boss.AdvanceSkill();assert(boss.health==literal(25,111));
        boss.AdvanceSkill();assert(boss.health==literal(25,113));
        if(index==0) {
            assert(boss.HealthBar().count==2);
            bool rejected=false;try {boss.DescribeSkill(B::DiamondBlizzard,ShotType::HomingAmulet,difficulty);}
            catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
        } else {
            const auto diamond=boss.DescribeSkill(B::DiamondBlizzard,ShotType::HomingAmulet,difficulty);
            assert(diamond.NativeHealth()==literal(36,113)&&diamond.timeoutFrames==literal(40,115));
            boss.AdvanceSkill();assert(boss.health==literal(36,113));
        }
    }
}
void TestEasyPerfectFreezeTimeoutExit() {
    // EXE 交接会将死亡回调 Sub28 留给 Easy 的超时回调，不能回到 Sub36 或进入最后一符。
    auto bullets=std::make_unique<BulletSystem>();
    bosses::CirnoBoss boss(behavior::UpdateRate::Hz60,false);
    boss.StartBoss(stage2::CirnoFinalConfig(ShotType::HomingAmulet,native_combat::Difficulty::Easy));
    for(int i=0;i<3;++i)boss.AdvanceSkill();
    assert(boss.attack==4&&boss.health==1400);
    bool timeout=false;
    for(int frame=0;frame<2402&&!timeout;++frame) {
        bullets->Reset();timeout=boss.Update(1.0F/60,{2,3,-6},*bullets);
    }
    assert(timeout&&!boss.SkillDefeated());boss.AdvanceSkill();
    for(int frame=0;frame<100&&boss.active;++frame) {
        bullets->Reset();if(boss.Update(1.0F/60,{2,3,-6},*bullets))boss.AdvanceSkill();
    }
    assert(!boss.active&&!boss.Defeated()&&boss.attack!=5);
}
void TestBossHealthAndTimeouts() {
    auto bullets=std::make_unique<BulletSystem>();bosses::CirnoBoss boss(behavior::UpdateRate::Hz120,true);boss.StartBoss(stage2::CirnoFinalConfig());
    assert(boss.health==10000&&boss.attack==1&&!boss.CanTakeDamage());
    for(int i=0;i<42;++i)boss.Update(1.0F/120,{2,3,-6},*bullets);
    assert(boss.CanTakeDamage());boss.health=0;assert(boss.ShouldAdvanceFromDamage());assert(boss.AdvanceSkill());
    assert(boss.attack==2&&boss.health==1500&&boss.maxHealth==1500&&!boss.CanTakeDamage());
    assert(boss.SpellCardNumber()==11);
    boss.Update(1.0F/240,{},*bullets);assert(!boss.CanTakeDamage()&&!boss.ShouldAdvanceFromDamage());
    boss.AdvanceSkill();assert(boss.attack==3&&boss.health==13000);
    boss.AdvanceSkill();assert(boss.attack==4&&boss.health==3200);
    auto bar=boss.HealthBar();assert(bar.count==3&&bar.remainingBars==0);
    assert(boss.SpellCardNumber()==15);
    Near(bar.segments[0].remaining,1400.0F/13000);Near(bar.segments[1].remaining,1800.0F/13000);Near(bar.segments[2].remaining,0);
    boss.health=1399;assert(boss.SkillDefeated()); // Perfect Freeze 完成时还要保留下一张符卡的 1400 HP。
    boss.AdvanceSkill();assert(boss.attack==5&&boss.health==1400);
    assert(boss.SpellCardNumber()==18);
    bosses::CirnoBoss timed(behavior::UpdateRate::Hz120,true);timed.StartBoss(stage2::CirnoFinalConfig());std::array<double,6> phaseLengths{};std::array<int,6> counts{};
    double elapsed=0;int previous=1;std::size_t peak=0;
    while(timed.active&&elapsed<185) {
        const bool transition=timed.Update(1.0F/120,{2,3,-6},*bullets);
        phaseLengths[previous]+=1.0/120;++counts[previous];elapsed+=1.0/120;
        peak=std::max(peak,bullets->Pool().ActiveCount());bullets->Update(1.0F/120,{}, {2,3,-6});
        if(transition) { bullets->ClearEnemyBullets();timed.AdvanceSkill();previous=std::max(1,timed.attack); }
    }
    assert(!timed.active&&!timed.Defeated());
    // 时间来自独立 ECL 初始化常量：1500/1800/3000/2400/1980 帧，末阶段另有 60 帧完整退场。
    Near(static_cast<float>(phaseLengths[1]),25,0.02F);Near(static_cast<float>(phaseLengths[2]),30,0.02F);
    Near(static_cast<float>(phaseLengths[3]),50,0.02F);Near(static_cast<float>(phaseLengths[4]),40,0.02F);
    Near(static_cast<float>(phaseLengths[5]),34,0.04F);assert(bullets->Pool().DroppedSpawns()==0);
    bosses::DaiyouseiBoss mid(behavior::UpdateRate::Hz120,true);mid.StartBoss(stage2::DaiyouseiMidbossConfig());assert(mid.health==10000);int bombDrops=0;
    mid.health=0;mid.AdvanceSkill();
    for(int i=0;i<4&&mid.active;++i) {
        const bool ended=mid.Update(1.0F/120,{},*bullets);
        for(int item:mid.ItemEvents()) {assert(item==3);++bombDrops;}
        if(ended)mid.AdvanceSkill();
    }
    assert(!mid.active&&bombDrops==1&&mid.Defeated());
    std::cout<<"Stage 2 timeout phases validated; peak projectiles "<<peak<<'\n';
}
/// @brief 攻击期望来自二面 Normal 原指令的父/子时钟展开；不从新协程或配方重新生成答案。
void TestDaiyouseiEightCycles() {
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned index=0;index<4;++index) {
        const auto difficulty=*native_combat::DifficultyFromIndex(index);
        const auto programs=stage2::reference::ProgramsForDifficulty(index);
        const auto sourceShot=[&](int sub,int occurrence) -> const stage2::reference::Instruction& {
            for(const auto& instruction:programs[sub])if((instruction.opcode==67||instruction.opcode==70)&&occurrence--==0)return instruction;
            throw std::logic_error("Missing Daiyousei source shot");
        };
        struct Witness {int frame;std::size_t count;int color;};
        std::vector<Witness> shots;std::vector<std::pair<int,float>> teleports;
        int start=130;unsigned random=7;
        for(int cycle=0;cycle<8;++cycle) {
            const bool aimed=cycle%3==2;
            if(aimed) {
                const auto& first=sourceShot(16,0);const auto& second=sourceShot(16,1);
                const auto count=static_cast<std::size_t>(first.args[2]*first.args[3]+second.args[2]*second.args[3]);
                for(int n=0;n<16;++n)shots.push_back({start+40+n*10,count,-1});
            } else {
                const auto& source=sourceShot(cycle%3==0 ? 14 : 15,0);
                for(int n=0;n<48;++n)shots.push_back({start+40+n,static_cast<std::size_t>(source.args[2]*source.args[3]),cycle%3==0 ? 10 : 2});
            }
            const int attackEnd=aimed ? 200 : 88;
            teleports.emplace_back(start+attackEnd+40,(Random(random,320)+32-192)*scale);
            start+=attackEnd+160;
        }
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,11.4285717F},10000,7);
        actor.difficulty=difficulty;stage2::ConfigureBehavior(actor,false);
        actor.Start(bosses::DaiyouseiBoss::Behaviors::Midboss(actor));
        std::size_t shotIndex=0,teleportIndex=0;
        for(int frame=0;frame<=teleports.back().first;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,0,-6},*bullets);
            const bool fires=shotIndex<shots.size()&&shots[shotIndex].frame==frame;
            assert(bullets->Pool().ActiveCount()==(fires ? shots[shotIndex].count : 0));
            if(fires) {
                const int color=shots[shotIndex].color;
                if(color>=0)bullets->Pool().ForEachActive([&](const Bullet& bullet) {assert(bullet.nativeColor==color);});
                ++shotIndex;
            }
            if(teleportIndex<teleports.size()&&teleports[teleportIndex].first==frame) {
                assert(actor.interactable);Near(actor.Position().x,teleports[teleportIndex].second);
                Near(actor.Position().z,5.71428585F);++teleportIndex;
            }
        }
        assert(shotIndex==shots.size()&&teleportIndex==teleports.size());
    }
}
void TestHailstormMultiCycle() {
    auto bullets=std::make_unique<BulletSystem>();
    for(auto difficulty:{native_combat::Difficulty::Hard,native_combat::Difficulty::Lunatic}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5.71428585F},1500,7);
        actor.difficulty=difficulty;stage2::ConfigureBehavior(actor,false);
        actor.Start(bosses::CirnoBoss::Behaviors::IcicleFall(actor));
        for(int frame=0;frame<=3080;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,0,-6},*bullets);
            const int relative=frame-120;
            const int cycle=relative>=0 ? relative/280 : -1;
            const int within=relative>=0 ? relative%280 : -1;
            const bool fires=cycle>=0&&cycle<11&&within<=140&&within%20==0;
            const int count=cycle+(difficulty==native_combat::Difficulty::Hard ? 8 : 13);
            assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(fires ? count*3 : 0));
            if(fires)bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                Near(bullet.turnDelaySeconds,1);Near(bullet.turnAngle,cycle%2==0 ? 0.392699093F : -0.392699093F);
                Near(bullet.turnSpeed,(1.4F+cycle*0.15F)*60*scale,1e-5F);assert(bullet.turnAbsolute);
            });
        }
        assert(actor.phaseFrames>=99999);
    }
}
void TestDifficultyBossShots() {
    auto bullets=std::make_unique<BulletSystem>();
    behavior::Actor actor(behavior::UpdateRate::Hz60);
    for(unsigned index=0;index<4;++index) {
        const auto difficulty=*native_combat::DifficultyFromIndex(index);
        const auto programs=stage2::reference::ProgramsForDifficulty(index);
        const auto shot=[&](int sub,int opcode,int frame) -> const stage2::reference::Instruction& {
            for(const auto& instruction:programs[sub])
                if(instruction.opcode==opcode&&instruction.frame==frame)return instruction;
            throw std::logic_error("Missing stage 2 source shot");
        };
        const auto start=[&](behavior::Entry entry,float hp) {
            actor.Reset({0,0,5.71428585F},hp,7);actor.difficulty=difficulty;
            stage2::ConfigureBehavior(actor,false);actor.Start(entry(actor));bullets->Reset();
        };
        start(bosses::DaiyouseiBoss::Behaviors::Midboss,10000);
        for(int frame=0;frame<=170;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            if(frame==170) {
                const auto& original=shot(14,70,40);
                assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(original.args[2]*original.args[3]));
            }
        }
        start(bosses::CirnoBoss::Behaviors::IcicleFall,1500);
        for(int frame=0;frame<=120;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            if(frame==120) {
                // H/L 源 Sub35 首轮 8/13 方向、3 层；E/N 同时两侧各 3 层。
                const std::size_t expected=index<2 ? 6 : (index==2 ? 24 : 39);
                assert(bullets->Pool().ActiveCount()==expected);
            }
        }
        start(bosses::CirnoBoss::Behaviors::PerfectFreeze,index==0 ? 1400 : 3200);
        for(int frame=0;frame<=125;++frame) {
            bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
            if(frame==125) {
                // Sub38 的源基数加 rank=16 时截断得到的两发修正。
                const auto& original=shot(38,13,0);
                assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(original.args[2]+2));
            }
        }
    }
}
/// @brief 攻击期望来自二面 Normal 原指令的父/子时钟展开；不从新协程或配方重新生成答案。
void TestSourceAttackFrames() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    const auto start=[&](behavior::Entry entry,float health) {
        actor.Reset({0,0,5.71428585F},health,7);stage2::ConfigureBehavior(actor,false);actor.Start(entry(actor));bullets->Reset();
    };
    start(bosses::CirnoBoss::Behaviors::FirstNonspell,10000);
    for(int frame=0;frame<=342;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);int expected=0;
        for(int base:{111,181,251})for(int index=0;index<6;++index)if(frame==base+index*2)expected=index+2;
        if(frame==342)expected=18;
        assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
        if(frame==111)bullets->Pool().ForEachActive([](const Bullet& bullet) {
            assert(bullet.nativeType==5&&bullet.nativeColor==6);Near(std::hypot(bullet.Velocity().x,bullet.Velocity().z),14.0625F);
            Near(bullet.spawnVisualSeconds,16.0F/60);
        });
    }
    start(bosses::CirnoBoss::Behaviors::IcicleFall,1500);
    unsigned random=7;const float jitter=Random(random,0.0490873866F);
    for(int frame=0;frame<=147;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==120 ? 6U : frame==147 ? 11U : 0U));
        if(frame==120) {
            int index=0;bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                const float angle=index<3 ? -0.196349546F+jitter : -2.94524312F-jitter;
                const float speed=17.4107151F-(17.4107151F-1.33928573F)*(index%3)/3;
                Near(bullet.Velocity().x,std::cos(angle)*speed);Near(bullet.Velocity().z,-std::sin(angle)*speed);
                Near(bullet.turnDelaySeconds,1);Near(bullet.turnAngle,index<3 ? pi/2 : -pi/2);
                ++index;
            });
        }
    }
    start(bosses::CirnoBoss::Behaviors::PerfectFreeze,3200);Bullet* friendly=nullptr;Bullet* beam=nullptr;
    for(int frame=0;frame<=570;++frame) {
        actor.Update(1.0F/60,{2,3,-6},*bullets);
        if(frame==329) {
            friendly=bullets->Spawn(BulletOwner::Player,{}, {1,0,0},0.1F,WHITE,stage2::kPlayerGoldCard);
            beam=bullets->SpawnBeam({}, {0,0,1},1,0.1F,0,10,WHITE);assert(friendly&&beam);
            friendly->maxAge=10;
        }
        if(frame==330) {
            int frozen=0;bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                if(bullet.owner==BulletOwner::Enemy&&bullet.beamLength==0) {
                    ++frozen;Near(BulletSystem::Length(bullet.Velocity()),0);assert(bullet.nativeColor==15);
                }
            });
            assert(frozen>0&&friendly->Velocity().x==1&&beam->active);
        }
        if(frame==570) {
            int released=0;bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                if(bullet.owner==BulletOwner::Enemy&&bullet.beamLength==0) {
                    ++released;Near(BulletSystem::Length(bullet.acceleration),1.60714281F);
                    Near(bullet.accelerationSeconds,220.0F/60);assert(bullet.nativeColor==15);
                }
            });
            assert(released>0&&friendly->Velocity().x==1&&beam->active);
        }
        bullets->Update(1.0F/60,{}, {2,3,-6});
    }
    start(bosses::CirnoBoss::Behaviors::DiamondBlizzard,1400);
    for(int frame=0;frame<=190;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==190 ? 12U : 0U));
    }
    start(bosses::DaiyouseiBoss::Behaviors::Midboss,10000);
    unsigned teleportRandom=7;const float teleportX=(Random(teleportRandom,320)+32-192)*scale;
    for(int frame=0;frame<=258;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        if(frame<170)assert(bullets->Pool().ActiveCount()==0);
        if(frame>=170&&frame<218)assert(bullets->Pool().ActiveCount()==2);
        if(frame==170)bullets->Pool().ForEachActive([](const Bullet& bullet) {assert(bullet.nativeType==4&&bullet.nativeColor==10);});
        if(frame==217)assert(actor.interactable);
        if(frame==218)assert(!actor.interactable&&!actor.CanTakeDamage()&&!actor.CanContactPlayer());
        if(frame==258) {assert(actor.interactable);Near(actor.position.x,teleportX);Near(actor.position.z,5.71428585F);}
    }
}
void TestDialogueAndItems() {
    stage2::Stage2Content content;
    const std::array<int,4> frames{2987,1320,3228,921};int branch=0;
    for(ShotType shot:{ShotType::HomingAmulet,ShotType::MagicMissile})for(bool after:{false,true}) {
        float duration=0;for(const auto& line:content.Dialogue(after,shot))duration+=line.duration;
        Near(duration*60,static_cast<float>(frames[branch++]),0.001F);
        assert(!content.DialogueCues(after,shot).empty());
    }
    DropSystem drops(1);drops.SpawnNativeItem({},3,false);drops.SpawnNativeItem({},5,false);drops.SpawnNativeItem({},4,false);
    const auto reward=drops.Update(0,{});assert(reward.bombs==1&&reward.lives==1&&reward.power==128&&reward.score==0);
    Player player;player.Reset();player.CollectResources(100,100);assert(player.RemainingLives()==9&&player.Bombs()==8);
    assert(content.ItemVisual(3)==stage2::kBombPoint&&content.ItemVisual(5)==stage2::kLifePoint);
    assert(content.Assets().visuals.size()==stage2::kVisualCount);
    assert(content.BossPortraitVisual(2)==content.BossPortraitVisual(0));
    assert(content.Assets().visuals[content.BossPortraitVisual(0)].path=="stages/stage2/portraits/cirno/face05a_0.png");
}
void TestDifficultyBossDisplayRates() {
    auto bullets=std::make_unique<BulletSystem>();
    for(unsigned index=0;index<4;++index) {
        std::vector<Vector3> expected;Vector3 position{};
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor;actor.Reset({0,0,5.71428585F},10000,7);
            actor.difficulty=*native_combat::DifficultyFromIndex(index);
            stage2::ConfigureBehavior(actor,false);
            actor.Start(bosses::CirnoBoss::Behaviors::FirstNonspell(actor));bullets->Reset();
            for(int frame=0;frame<fps*3;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Vector3> velocities;
            bullets->Pool().ForEachActive([&](const Bullet& bullet){velocities.push_back(bullet.Velocity());});
            if(fps==120) {expected=velocities;position=actor.Position();}
            else {
                assert(velocities.size()==expected.size());
                Near(actor.Position().x,position.x,1e-5F);Near(actor.Position().z,position.z,1e-5F);
                for(std::size_t n=0;n<velocities.size();++n) {
                    Near(velocities[n].x,expected[n].x,1e-5F);Near(velocities[n].z,expected[n].z,1e-5F);
                }
            }
        }
    }
}
void Test120HzAcrossDisplayRates() {
    stage2::Stage2Content content;std::vector<const native_combat::HealthRecord*> source;
    for(const auto& r:native_combat::Stage(2)->health)
        if(r.scope==native_combat::HealthScope::TimelineSpawn&&r.targetSub!=20&&r.targetSub!=21)source.push_back(&r);
    std::array<const behavior::Spawn*,13> examples{};std::size_t index=0;
    for(const auto& event:content.Events())if(const auto* spawn=std::get_if<SpawnActors>(&event.action)) {
        const int sub=source.at(index++)->targetSub;if(!examples[sub])examples[sub]=&spawn->actor;
    }
    auto bullets=std::make_unique<BulletSystem>();int covered=0;
    for(const auto* example:examples) {
        if(!example)continue;++covered;
        Vector3 expected{};std::vector<Bullet> expectedShots;int expectedAnimation=-1;float expectedRandom=0;
        for(int fps:{120,30,60,144,240}) {
            auto spawn=*example;spawn.retireOutside=false;behavior::Actor actor;actor.Activate(spawn);bullets->Reset();
            for(int frame=0;frame<fps*4;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            assert(actor.phaseFrames==240);const float random=actor.Random(1);
            std::vector<Bullet> shots;bullets->Pool().ForEachActive([&](const Bullet& bullet){shots.push_back(bullet);});
            if(fps==120) {expected=actor.Position();expectedShots=shots;expectedAnimation=actor.animationScript;expectedRandom=random;}
            else {
                Near(actor.Position().x,expected.x,1e-5F);Near(actor.Position().y,expected.y,1e-5F);Near(actor.Position().z,expected.z,1e-5F);
                assert(shots.size()==expectedShots.size()&&actor.animationScript==expectedAnimation&&random==expectedRandom);
                for(std::size_t i=0;i<shots.size();++i) {
                    assert(shots[i].nativeColor==expectedShots[i].nativeColor&&shots[i].nativeType==expectedShots[i].nativeType);
                    Near(shots[i].position.x,expectedShots[i].position.x,1e-5F);Near(shots[i].position.z,expectedShots[i].position.z,1e-5F);
                    Near(shots[i].Velocity().x,expectedShots[i].Velocity().x,1e-5F);Near(shots[i].Velocity().y,expectedShots[i].Velocity().y,1e-5F);
                    Near(shots[i].Velocity().z,expectedShots[i].Velocity().z,1e-5F);
                }
            }
        }
    }
    assert(covered==12);
}
}
/// @brief 核对本关独立静态事实及真实公共组件执行，不依赖原游戏安装目录。
void RunStage2Tests() {
    TestStage2EffectReference();TestStage2EffectDisplayRates();
    TestDifficultySourceShots();TestBirthsAndSourceMovement();TestActorDefeatAndRandomSkills();TestDifficultyPeriodicOrbs();TestAimedFairyDifficultyPeriod();TestDifficultyOrbDeath();TestDifficultyBossDefinitions();TestEasyPerfectFreezeTimeoutExit();TestBossHealthAndTimeouts();TestDaiyouseiEightCycles();TestHailstormMultiCycle();TestDifficultyBossShots();TestSourceAttackFrames();
    TestDialogueAndItems();TestDifficultyBossDisplayRates();Test120HzAcrossDisplayRates();TestRandomShotInterval();
}
/// @brief 独立运行二面全部102/118源窗口和双频率显示时钟验收。
void RunStage2EffectTests() {TestStage2EffectReference();TestStage2EffectDisplayRates();}

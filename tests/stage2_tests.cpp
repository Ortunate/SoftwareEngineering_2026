// 本文件以独立源参数检查二面全部出生、60 Hz 普通演员轨迹、发射边界、Boss 血量/超时与新公共能力。
#include "stages/stage2/content.hpp"
#include "stages/stage2/behaviors.hpp"
#include "stages/stage2/boss.hpp"
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
        if(const auto* boss=std::get_if<StartBoss>(&event.action))finalStarted|=boss->encounter==BossEncounter::Final;
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
    Near(shots[0].velocity.x,std::cos(-0.5F)*7);Near(shots[0].velocity.z,-std::sin(-0.5F)*7);
    Near(shots[1].velocity.x,5);Near(shots[1].velocity.z,0);
    // 固定扇形的轴必须来自显式世界方向；单方向不能忽略 angle 而全部射向 -Z。
    bullets->Reset();state.spatial=true;state.Fire({skill::Formation::FixedFan,1,6,1,1,5,0,pi/2}, {},{},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& b) {Near(b.velocity.x,0);Near(b.velocity.z,-5);});
    bullets->Reset();state.Fire(skill::Laser{.angle=pi/2,.speed=6}, {},{1,0,0},*bullets);
    bullets->Pool().ForEachActive([](const Bullet& b) {Near(b.beamDirection.z,-1);Near(b.velocity.z,-6);});
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
void TestBossHealthAndTimeouts() {
    auto bullets=std::make_unique<BulletSystem>();stage2::Stage2Boss boss(true);
    assert(boss.health==10000&&boss.phase==1&&!boss.CanTakeDamage());
    for(int i=0;i<42;++i)boss.Update(1.0F/120,{2,3,-6},*bullets);
    assert(boss.CanTakeDamage());boss.health=0;assert(boss.ShouldAdvanceFromDamage());assert(boss.AdvancePhase());
    assert(boss.phase==2&&boss.health==1500&&boss.maxHealth==1500&&!boss.CanTakeDamage());
    assert(boss.SpellCardNumber()==11);
    boss.Update(1.0F/240,{},*bullets);assert(!boss.CanTakeDamage()&&!boss.ShouldAdvanceFromDamage());
    boss.AdvancePhase();assert(boss.phase==3&&boss.health==13000);
    boss.AdvancePhase();assert(boss.phase==4&&boss.health==3200);
    auto bar=boss.HealthBar();assert(bar.count==3&&bar.remainingBars==0);
    assert(boss.SpellCardNumber()==15);
    Near(bar.segments[0].remaining,1400.0F/13000);Near(bar.segments[1].remaining,1800.0F/13000);Near(bar.segments[2].remaining,0);
    boss.health=1399;assert(boss.PhaseDefeated()); // Perfect Freeze 完成时还要保留下一张符卡的 1400 HP。
    boss.AdvancePhase();assert(boss.phase==5&&boss.health==1400);
    assert(boss.SpellCardNumber()==18);
    stage2::Stage2Boss timed(true);std::array<double,6> phaseLengths{};std::array<int,6> counts{};
    double elapsed=0;int previous=1;std::size_t peak=0;
    while(timed.active&&elapsed<185) {
        const bool transition=timed.Update(1.0F/120,{2,3,-6},*bullets);
        phaseLengths[previous]+=1.0/120;++counts[previous];elapsed+=1.0/120;
        peak=std::max(peak,bullets->Pool().ActiveCount());bullets->Update(1.0F/120,{}, {2,3,-6});
        if(transition) { bullets->ClearEnemyBullets();timed.AdvancePhase();previous=std::max(1,timed.phase); }
    }
    assert(!timed.active&&!timed.Defeated());
    // 时间来自独立 ECL 初始化常量：1500/1800/3000/2400/1980 帧，末阶段另有 60 帧完整退场。
    Near(static_cast<float>(phaseLengths[1]),25,0.02F);Near(static_cast<float>(phaseLengths[2]),30,0.02F);
    Near(static_cast<float>(phaseLengths[3]),50,0.02F);Near(static_cast<float>(phaseLengths[4]),40,0.02F);
    Near(static_cast<float>(phaseLengths[5]),34,0.04F);assert(bullets->Pool().DroppedSpawns()==0);
    stage2::Stage2Boss mid(false);assert(mid.health==10000);int bombDrops=0;
    mid.health=0;mid.AdvancePhase();
    for(int i=0;i<4&&mid.active;++i) {
        const bool ended=mid.Update(1.0F/120,{},*bullets);
        for(int item:mid.ItemEvents()) {assert(item==3);++bombDrops;}
        if(ended)mid.AdvancePhase();
    }
    assert(!mid.active&&bombDrops==1&&mid.Defeated());
    std::cout<<"Stage 2 timeout phases validated; peak projectiles "<<peak<<'\n';
}
/// @brief 攻击期望来自二面 Normal 原指令的父/子时钟展开；不从新协程或配方重新生成答案。
void TestSourceAttackFrames() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    const auto start=[&](behavior::Entry entry,float health) {
        actor.Reset({0,0,5.71428585F},health,7);stage2::ConfigureBehavior(actor,false);actor.Start(entry(actor));bullets->Reset();
    };
    start(stage2::cirno::FirstNonspell,10000);
    for(int frame=0;frame<=342;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);int expected=0;
        for(int base:{111,181,251})for(int index=0;index<6;++index)if(frame==base+index*2)expected=index+2;
        if(frame==342)expected=18;
        assert(bullets->Pool().ActiveCount()==static_cast<std::size_t>(expected));
        if(frame==111)bullets->Pool().ForEachActive([](const Bullet& bullet) {
            assert(bullet.nativeType==5&&bullet.nativeColor==6);Near(std::hypot(bullet.velocity.x,bullet.velocity.z),14.0625F);
            Near(bullet.spawnVisualSeconds,16.0F/60);
        });
    }
    start(stage2::cirno::IcicleFall,1500);
    unsigned random=7;const float jitter=Random(random,0.0490873866F);
    for(int frame=0;frame<=147;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==120 ? 6U : frame==147 ? 11U : 0U));
        if(frame==120) {
            int index=0;bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                const float angle=index<3 ? -0.196349546F+jitter : -2.94524312F-jitter;
                const float speed=17.4107151F-(17.4107151F-1.33928573F)*(index%3)/3;
                Near(bullet.velocity.x,std::cos(angle)*speed);Near(bullet.velocity.z,-std::sin(angle)*speed);
                Near(bullet.turnDelaySeconds,1);Near(bullet.turnAngle,index<3 ? pi/2 : -pi/2);
                ++index;
            });
        }
    }
    start(stage2::cirno::PerfectFreeze,3200);Bullet* friendly=nullptr;Bullet* beam=nullptr;
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
                    ++frozen;Near(BulletSystem::Length(bullet.velocity),0);assert(bullet.nativeColor==15);
                }
            });
            assert(frozen>0&&friendly->velocity.x==1&&beam->active);
        }
        if(frame==570) {
            int released=0;bullets->Pool().ForEachActive([&](const Bullet& bullet) {
                if(bullet.owner==BulletOwner::Enemy&&bullet.beamLength==0) {
                    ++released;Near(BulletSystem::Length(bullet.acceleration),1.60714281F);
                    Near(bullet.accelerationSeconds,220.0F/60);assert(bullet.nativeColor==15);
                }
            });
            assert(released>0&&friendly->velocity.x==1&&beam->active);
        }
        bullets->Update(1.0F/60,{}, {2,3,-6});
    }
    start(stage2::cirno::DiamondBlizzard,1400);
    for(int frame=0;frame<=190;++frame) {
        bullets->Reset();actor.Update(1.0F/60,{2,3,-6},*bullets);
        assert(bullets->Pool().ActiveCount()==(frame==190 ? 12U : 0U));
    }
    start(stage2::daiyousei::Midboss,10000);
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
                    Near(shots[i].velocity.x,expectedShots[i].velocity.x,1e-5F);Near(shots[i].velocity.y,expectedShots[i].velocity.y,1e-5F);
                    Near(shots[i].velocity.z,expectedShots[i].velocity.z,1e-5F);
                }
            }
        }
    }
    assert(covered==12);
}
}
/// @brief 核对本关独立静态事实及真实公共组件执行，不依赖原游戏安装目录。
void RunStage2Tests() {
    TestBirthsAndSourceMovement();TestActorDefeatAndRandomSkills();TestBossHealthAndTimeouts();TestSourceAttackFrames();
    TestDialogueAndItems();Test120HzAcrossDisplayRates();TestRandomShotInterval();
}

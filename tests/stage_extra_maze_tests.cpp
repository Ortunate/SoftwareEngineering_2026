// 本文件以独立源指令对照Maze of Love的条件扇、移动/循环时钟、三维几何及角色交接，不生成生产配方。
#include "stage_extra_flandre_reference.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "gameplay/spatial.hpp"
#include "stages/common/ids.hpp"
#include <cassert>
#include <iostream>
#include <memory>

namespace {
using namespace koumakyou;using F=bosses::FlandreBoss;
int checkedFrame=0,checkedRank=0;unsigned checkedSeed=0;
void Near(float actual,float expected,const char* field,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {
        std::cerr<<"Maze source62 frame "<<checkedFrame<<" rank "<<checkedRank<<" seed "<<checkedSeed<<' '<<field<<": "<<actual<<" vs "<<expected<<'\n';std::abort();
    }
}
void TestSource() {
    constexpr float scale=20.0F/448,speedScale=scale*60;
    for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,18,31,32}) {
        checkedSeed=seed;checkedRank=rank;auto bullets=std::make_unique<BulletSystem>();
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5.71428585F},1400,seed);actor.SetRank(rank);
        F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::MazeOfLove(actor));
        stage_extra::reference::FlandreReference source(62,{192,96,0},seed,rank,1400);source.death=65;
        int narrowRounds=0,wallRounds=0;std::vector<int> walls;
        for(int frame=0;frame<3600;++frame) {
            checkedFrame=frame;bullets->Reset();const Vector3 player{240+30*std::sin(frame*.01F),350,0};
            source.Tick(player);actor.Update(1.0F/60,{(player.x-192)*scale,0,10-player.y*scale},*bullets);
            Near(actor.position.x,(source.position.x-192)*scale,"x");Near(actor.position.z,10-source.position.y*scale,"z");
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds&&actor.dropEvents==source.drops);
            assert(actor.damageable==source.damageable&&actor.contactEnabled==source.contact&&actor.interactable==source.interactable);
            assert(actor.visible==source.visible&&actor.defeatActors==source.defeatActors&&actor.spawnEvents.empty()&&source.spawns.empty());
            assert(source.card==126&&source.health==1400&&source.timeout==3600&&source.timeoutCallback==65&&source.nativeCallback==(frame>=120?10:-1));
            std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
            if(actual.size()!=source.emissions.size())std::cerr<<"Maze source62 frame "<<frame<<" count "<<actual.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(actual.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<actual.size();++n) {
                const auto& b=*actual[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart&&e.flags==512);
                Near(b.position.x,(e.position.x-192)*scale,"bullet.x");Near(b.position.z,10-e.position.y*scale,"bullet.z");
                Near(b.Velocity().x,e.velocity.x*speedScale,"velocity.x",1e-4F);Near(b.Velocity().z,-e.velocity.y*speedScale,"velocity.z",1e-4F);
            }
            if(!actual.empty()) {++narrowRounds;if(actual.size()==76) {++wallRounds;walls.push_back(frame);}}
            // 首发与反转均取源时钟的明确边界；移动仍在进行时就必须发出第一轮。
            if(frame==120||frame==780||frame==1147||frame==1807)assert(actual.size()>=4);
            if(frame==120)assert(actor.position.z>6&&actual.front()->nativeColor==8);
            if(frame==780)assert(actual.front()->nativeColor==14);
        }
        assert(narrowRounds==1640&&wallRounds==82&&walls.front()==164&&walls[1]==202&&actor.Random(1)==source.Random(1));
        if(seed==7&&rank==16)std::cout<<"Maze source: "<<narrowRounds<<" narrow rounds / "<<wallRounds<<" conditional walls\n";
    }
}
void TestClocksAndDisplayRates() {
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120}) {
        const int factor=rate==behavior::UpdateRate::Hz120?2:1;auto bullets=std::make_unique<BulletSystem>();
        behavior::Actor actor(rate);actor.Reset({0,0,5.71428585F},1400,7);F::Behaviors::ConfigurePractice(actor,false);actor.Start(F::Behaviors::MazeOfLove(actor));
        for(int tick=0;tick<2200*factor;++tick) {
            bullets->Reset();actor.Update(1.0F/(60*factor),{},*bullets);const int frame=tick/factor;
            assert(actor.CanTakeDamage()==(tick>=120*factor));
            const int cycle=(frame-120)/1027,local=frame>=120?frame-120-cycle*1027:-1;
            const bool firing=local>=0&&(local<600&&local%2==0||local>=660&&local<1020&&(local-660)%2==0)&&tick%factor==0;
            assert((bullets->Pool().ActiveCount()>0)==firing);
            if(tick==149*factor+factor-1)Near(actor.position.z,0,"central.move.end");
        }
    }
    for(bool spatial:{false,true}) {
        Vector3 position{};std::vector<Bullet> snapshot;float random=0;
        auto bullets=std::make_unique<BulletSystem>();
        for(int fps:{120,30,60,144,240}) {
            bullets->Reset();behavior::Actor actor;actor.Reset({0,0,5.71428585F},1400,7);F::Behaviors::ConfigurePractice(actor,spatial);actor.Start(F::Behaviors::MazeOfLove(actor));
            for(int frame=0;frame<20*fps;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});const auto next=actor.Random(1);
            assert(!bullets->Pool().DroppedSpawns()&&actor.phaseFrames==1200);
            if(fps==120) {snapshot=actual;position=actor.position;random=next;}
            else {
                assert(actual.size()==snapshot.size()&&next==random);
                Near(actor.position.x,position.x,"display.x");Near(actor.position.y,position.y,"display.y");Near(actor.position.z,position.z,"display.z");
                for(std::size_t n=0;n<actual.size();++n) {
                    assert(actual[n].nativeType==snapshot[n].nativeType&&actual[n].nativeColor==snapshot[n].nativeColor);
                    Near(actual[n].position.x,snapshot[n].position.x,"display.bullet.x");Near(actual[n].position.y,snapshot[n].position.y,"display.bullet.y");Near(actual[n].position.z,snapshot[n].position.z,"display.bullet.z");
                    Near(actual[n].Velocity().x,snapshot[n].Velocity().x,"display.velocity.x",1e-4F);Near(actual[n].Velocity().y,snapshot[n].Velocity().y,"display.velocity.y",1e-4F);Near(actual[n].Velocity().z,snapshot[n].Velocity().z,"display.velocity.z",1e-4F);
                }
            }
        }
    }
}
/// @brief 三维扇保留源扇的最大半张角，并在球冠内按等立体角中点采样；每速度层方向一致。
void TestSpatialCones() {
    auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor(behavior::UpdateRate::Hz60);
    actor.Reset({0,0,5.71428585F},1400,7);actor.SetRank(32);F::Behaviors::ConfigurePractice(actor,true);actor.Start(F::Behaviors::MazeOfLove(actor));
    stage_extra::reference::FlandreReference source(62,{192,96,0},7,32,1400);int checkedWalls=0;
    for(int frame=0;frame<1100;++frame) {
        bullets->Reset();source.Tick({240,350,0});actor.Update(1.0F/60,{2,3,-6},*bullets);
        std::vector<const Bullet*> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(&b);});
        assert(actual.size()==source.emissions.size());
        if(actual.empty())continue;
        for(int group=0;group<(actual.size()>4?2:1);++group) {
            const int begin=group?4:0,count=group?24:4,layers=group?3:1;
            // 由源平面逐发速度的向量和独立还原中轴，不读取生产Shot的angle/coneHalfAngle。
            float x=0,z=0;for(int n=0;n<count;++n) {x+=source.emissions[begin+n].velocity.x;z-=source.emissions[begin+n].velocity.y;}
            const float length=std::hypot(x,z);x/=length;z/=length;
            const float halfAngle=(count-1)*0.5F*(group?0.19634954631328583F:0.06544984877109528F);
            bool above=false,below=false;
            for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
                const auto& b=*actual[begin+layer*count+n];const auto v=b.Velocity();const float speed=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
                const float dot=(v.x*x+v.z*z)/speed;
                Near(dot,1-(1-std::cos(halfAngle))*(n+0.5F)/count,"spatial.solidAngle",1e-5F);
                above|=v.y>1e-5F;below|=v.y<-1e-5F;
                if(layer>0) {
                    const auto first=actual[begin+n]->Velocity();const float firstSpeed=std::sqrt(first.x*first.x+first.y*first.y+first.z*first.z);
                    Near(v.x/speed,first.x/firstSpeed,"spatial.layer.x",1e-5F);Near(v.y/speed,first.y/firstSpeed,"spatial.layer.y",1e-5F);Near(v.z/speed,first.z/firstSpeed,"spatial.layer.z",1e-5F);
                }
            }
            assert(above&&below);if(group)++checkedWalls;
        }
    }
    assert(checkedWalls>20&&!bullets->Pool().DroppedSpawns());
}
void TestHandoffAndBomb() {
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        F boss;const auto setting=boss.DescribeSkill(F::Behavior::MazeOfLove,static_cast<ShotType>(shot));
        assert(setting.health==1400&&setting.timeoutFrames==3600&&setting.spellCard==126&&setting.reactToBomb);
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::MazeOfLove,F::Behavior::SixthNonspell},{{{0,0.5F,PINK},{1,0.5F,WHITE}}}}}};
        for(auto difficulty:{native_combat::Difficulty::Easy,native_combat::Difficulty::Hard,native_combat::Difficulty::Lunatic}) {
            bool rejected=false;try {boss.DescribeSkill(F::Behavior::MazeOfLove,static_cast<ShotType>(shot),difficulty);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
        }
        boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false,bat=false,recovered=false;
        for(int tick=0;tick<7300;++tick) {
            bullets->Reset();boss.SetPlayerBombActive(tick<30||tick>=250&&tick<280);const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(tick<240)assert(boss.animationScript!=165&&!boss.CanTakeDamage());
            if(tick==260) {assert(boss.animationScript==165&&!boss.CanBeTargeted());bat=true;}
            if(tick==400) {assert(boss.CanTakeDamage()&&boss.animationScript!=165);recovered=true;}
            if(damage&&tick>2400&&boss.CanTakeDamage())boss.health=0;
            if(ready||boss.ShouldAdvanceFromDamage()) {if(!damage)assert(tick==7200);assert(boss.AdvanceSkill());advanced=true;break;}
        }
        assert(advanced&&bat&&recovered&&boss.attack==2&&boss.health==10000&&!boss.CanTakeDamage());
        boss.Update(1.0F/120,{},*bullets);assert(!boss.CanTakeDamage()&&boss.DefeatActorsRequested());
        boss.SetPlayerBombActive(true);boss.StartBoss(config);boss.Update(1.0F/120,{},*bullets);assert(boss.animationScript!=165);
    }
    for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
        const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.configure=F::Behaviors::ConfigurePractice,
            .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
            .phases={{{F::Behavior::FifthNonspell,F::Behavior::MazeOfLove},{{{0,0.5F,WHITE},{1,0.5F,PINK}}}}}};
        F boss;boss.StartBoss(config);auto bullets=std::make_unique<BulletSystem>();bool advanced=false;
        for(int tick=0;tick<3700;++tick) {
            bullets->Reset();const bool ready=boss.Update(1.0F/120,{2,3,-6},*bullets);
            if(damage&&tick>400&&boss.CanTakeDamage())boss.health=1399;
            if(ready||boss.ShouldAdvanceFromDamage()) {assert(boss.AdvanceSkill());advanced=true;break;}
        }
        assert(advanced&&boss.health==1400&&boss.attack==2&&!boss.CanTakeDamage());
        for(int tick=0;tick<=240;++tick) {bullets->Reset();boss.Update(1.0F/120,{},*bullets);}
        // 84(-1)只关闭旧配方的播放位；新512配方须保留第五非符已经设置的音效身份7。
        assert(boss.CanTakeDamage()&&boss.SpellCardNumber()==126&&boss.SoundEvents().size()==1&&boss.SoundEvents()[0]==7);
        assert(bullets->Pool().ActiveCount()==4);
    }
}
}
/// @brief 第五符独立差分与角色专项；不代表完整Extra关卡或原生全局RNG/弹池生命周期验收。
void RunStageExtraMazeTests() {
    TestSource();TestClocksAndDisplayRates();TestSpatialCones();TestHandoffAndBomb();
    std::cout<<"Validated Extra Maze of Love source diffs, clocks, display rates, role handoffs and Bomb recovery\n";
}

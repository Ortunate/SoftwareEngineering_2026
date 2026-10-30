// 本文件检查五面出生、世界轨迹、Boss 源时钟、时停与随机顺序；完整渲染和资源生命周期另由图形目标验证。
#include "stages/stage5/data.hpp"
#include "stages/stage5/behaviors.hpp"
#include "stages/stage5/ids.hpp"
#include "stages/stage5/boss_config.hpp"
#include "gameplay/bosses/sakuya.hpp"
#include "stage5_reference_program.hpp"
#include "stage5_combat_reference.hpp"
#include "stages/stage5/content.hpp"
#include "app/spell_practice.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>
namespace {
using namespace koumakyou;
constexpr float pi=std::numbers::pi_v<float>;
void Near(float actual,float expected,float tolerance=5e-4F) {
    if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance) {std::cerr<<"Stage 5 mismatch "<<actual<<" vs "<<expected<<'\n';std::abort();}
}
unsigned Next(unsigned& random) {random^=random<<13;random^=random>>17;random^=random<<5;return random;}
/// @brief 同一步源请求精确核对种类/数量/ARGB；只在参考侧换算源位置、绕轴方向与半径。
void CompareStage5Effects(const behavior::Actor& actor,const std::vector<stage5::reference::ExternalEffect>& expected) {
    assert(actor.effectEvents.size()==expected.size());constexpr float scale=20.0F/448;
    for(std::size_t n=0;n<expected.size();++n) {
        const auto& a=actor.effectEvents[n];const auto& e=expected[n];assert(e.type==3||e.type==13||e.type==17);
        assert(a.kind==(e.type==3?effects::Kind::Burst:e.type==13?effects::Kind::Attached:effects::Kind::Charge)&&a.count==e.count);
        assert(a.color.r==((e.color>>16)&255)&&a.color.g==((e.color>>8)&255)&&a.color.b==(e.color&255)&&a.color.a==(e.color>>24));
        Near(a.position.x,(e.position.x-192)*scale);Near(a.position.y,-e.position.z*scale);Near(a.position.z,10-e.position.y*scale);
        if(e.type==13) {
            assert(a.axis.x==e.axis.x&&a.axis.y==-e.axis.z&&a.axis.z==-e.axis.y);
            const auto anchor=a.attachment.lock();assert(anchor);Near(anchor->Radius(),e.radius*scale,1e-5F);
        }
    }
}
/// @brief 六轮源退场逐帧比较五色请求、声音、运动及第60帧清弹，不能只比较最终粒子总数。
void TestSakuyaRetirementEffects() {
    using S=bosses::SakuyaBoss;auto bullets=std::make_unique<BulletSystem>();constexpr float scale=20.0F/448;
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,5},1,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);S::Behaviors::ConfigureFinalPractice(actor,false);
        actor.Start(S::Behaviors::Defeated(actor));stage5::reference::CombatReference source(d,63,{192,112,0},seed,32);
        int requests=0,particles=0;
        for(int frame=0;frame<=60;++frame) {
            bullets->Reset();auto* retained=bullets->Spawn(BulletOwner::Enemy,{}, {},.1F,WHITE,stage5::kBlueBullet);assert(retained);
            source.Tick({245,360,0});actor.Update(1.0F/60,{},*bullets);CompareStage5Effects(actor,source.effects);
            assert(source.clearBullets==(frame==60)&&retained->active==!source.clearBullets&&actor.soundEvents==source.sounds);
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            for(const auto& e:actor.effectEvents) {++requests;particles+=e.count;}
        }
        assert(requests==30&&particles==60&&actor.finished&&actor.Random(1)==source.Random(1));
    }
}
/// @brief 两更新频率分别覆盖五显示FPS；时停期间照常消费行为蓄力，不能从冻结的阶段时钟反推等待。
void TestSakuyaEffectDisplayRates() {
    using B=bosses::SakuyaBoss::Behaviors;
    const std::array<behavior::Entry,8> entries{B::FirstNonspell,B::ClockCorpse,B::JackTheLudoBile,B::LunaClock,
        B::TheWorld,B::Marionette,B::KillingDoll,B::Defeated};
    for(auto rate:{behavior::UpdateRate::Hz60,behavior::UpdateRate::Hz120})for(std::size_t route=0;route<entries.size();++route) {
        std::vector<effects::Request> expected;
        for(int fps:{120,30,60,144,240}) {
            behavior::Actor actor(rate);actor.Reset({0,0,5},19000,7);
            actor.difficulty=route==2||route==4||route==6?native_combat::Difficulty::Hard:native_combat::Difficulty::Normal;
            B::ConfigureFinalPractice(actor,false);actor.Start(entries[route](actor));
            auto bullets=std::make_unique<BulletSystem>();std::vector<effects::Request> actual;
            for(int frame=0;frame<fps*12;++frame) {
                bullets->Reset();actor.Update(1.0F/fps,{2,0,-6},*bullets);
                actual.insert(actual.end(),actor.effectEvents.begin(),actor.effectEvents.end());
            }
            assert(!actual.empty());
            if(route==0) {assert(actual.size()==5);for(int n=0;n<5;++n)assert(actual[n].contentFrame==n*10);}
            if(route==1||route==2||route==3||route==4)assert(actual.front().contentFrame==120);
            if(route==7) {assert(actual.size()==30);for(int n=0;n<30;++n)assert(actual[n].contentFrame==n/5*10+(n%5+1)*2);}
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
int DialogueFrames(std::span<const DialogueLine> lines) {
    int frames=0;for(const auto& line:lines)frames+=static_cast<int>(std::lround(line.duration*60));return frames;
}
std::vector<const native_combat::HealthRecord*> Births() {
    std::vector<const native_combat::HealthRecord*> births;
    for(const auto& record:native_combat::Stage(5)->health)
        if(record.scope==native_combat::HealthScope::TimelineSpawn&&record.targetSub!=12&&record.targetSub!=23)births.push_back(&record);
    return births;
}
/// @brief 只执行五面普通演员可达的源指令；位置为像素，速率为像素/帧，帧为 60 Hz 局部时钟。
/// 独立维护发射、rank 快照和项目 xorshift；不调用生产移动/弹幕配方，也不代表原生全局 RNG。
class OrdinaryReference {
public:
    struct Emission {Vector3 position,velocity;int type,color;};
    OrdinaryReference(int difficulty,int sub,Vector3 point,unsigned seed,int rank,bool mirror)
        :position(point),difficulty_(difficulty),sub_(sub),rank_(rank),random_(seed),mirror_(mirror) {}
    float Random(float range) {return static_cast<float>(Next(random_)>>8)/16777216.0F*range;}
    void Defeat() {assert(death>=0);sub_=death;pc_=frame_=period_=0;}
    void Tick(Vector3 player) {
        emissions.clear();sounds.clear();drops.clear();position.x+=vx_*(mirror_?-1:1);position.y+=vy_;
        const auto program=stage5::reference::ProgramsForDifficulty(difficulty_)[sub_];int budget=4096;
        while(active&&pc_<int(program.size())&&program[pc_].frame<=frame_&&--budget) {
            const auto& ins=program[pc_++];const auto& a=ins.args;
            const auto v=[&](int n){return Value(a[n]);};
            const auto jump=[&]{frame_=int(a[0]);pc_=int(a[1]);};
            switch(ins.opcode) {
                case 1:active=false;break;
                case 2:jump();break;
                case 3:if(--Variable(a[2])>0)jump();break;
                case 4:case 5:Variable(a[0])=v(1);break;
                case 9:Variable(a[0])=Random(v(1))+v(2);break;
                case 20:Variable(a[0])=v(1)+v(2);break;
                case 27:equal_=v(0)==v(1);break;
                case 34:if(!equal_)jump();break;
                case 45:angle_=v(0);speed_=v(1);break;
                case 48:acceleration_=v(0);break;
                case 67:case 68:case 69:case 75: {
                    shot_=ins;for(int n=0;n<9;++n)shot_.args[n]=v(n);
                    const float influence=-0.5F+rank_/32.0F;
                    shot_.args[4]=std::max(0.3F,v(4)+influence);shot_.args[5]=std::max(0.3F,v(5)+influence/2);
                    if(!suppressed_)Emit(player);break;
                }
                case 77:
                    period_=int(a[0]);period_+=period_/5-(period_/5*2*rank_)/32;
                    elapsed_=int(Next(random_)%static_cast<unsigned>(period_));break;
                case 78:suppressed_=true;break;
                case 79:suppressed_=false;break;
                case 84:
                    sound_=int(a[0]);shot_.args[8]=float(int(shot_.args[8])|512);break;
                case 97:animation=int(a[0]);break;
                case 103:radius=std::min(a[0],a[1])/2;break;
                case 107:break; // 回调槽选择不影响普通演员仅有的一条死亡后继。
                case 108:death=int(a[0]);break;
                case 119:drops.push_back(int(a[0]));break;
                default:throw std::logic_error("Unsupported Stage 5 ordinary opcode "+std::to_string(ins.opcode));
            }
        }
        assert(budget>0);if(active&&period_>0&&++elapsed_>=period_) {elapsed_-=period_;Emit(player);}
        speed_+=acceleration_;vx_=std::cos(angle_)*speed_;vy_=std::sin(angle_)*speed_;++frame_;
    }
    Vector3 position{};bool active=true;int animation=-1,death=-1;float radius=0;
    std::vector<Emission> emissions;std::vector<int> sounds,drops;
private:
    float& Variable(float id) {return variables_.at(int(-id)-10001);}
    float Value(float id) const {
        if(id<=-10001&&id>=-10012)return variables_.at(int(-id)-10001);
        if(id==-10013)return float(difficulty_);if(id==-10015)return position.x;if(id==-10016)return position.y;
        return id;
    }
    void Emit(Vector3 player) {
        const auto& a=shot_.args;const int count=int(a[2]),layers=int(a[3]);
        for(int layer=0;layer<layers;++layer)for(int n=0;n<count;++n) {
            float angle=a[6],speed=a[4]-(a[4]-a[5])*layer/layers;
            if(shot_.opcode==69)angle+=n*2*pi/count+layer*a[7];
            else if(shot_.opcode==67||shot_.opcode==68) {
                float spread=count%2?(n+1)/2*a[7]:n/2*a[7]+a[7]/2;if(n%2)spread=-spread;angle+=spread;
            } else if(shot_.opcode==75) {angle=Random(a[6]-a[7])+a[7];speed=Random(a[4]-a[5])+a[5];}
            else throw std::logic_error("Unsupported Stage 5 ordinary formation");
            if(shot_.opcode==67||shot_.opcode==69)angle+=std::atan2(player.y-position.y,player.x-position.x);
            emissions.push_back({position,{std::cos(angle)*speed,std::sin(angle)*speed,0},int(a[0]),int(a[1])});
        }
        if((int(a[8])&512)&&sound_>=0)sounds.push_back(sound_);
    }
    int difficulty_,sub_,rank_,pc_=0,frame_=0,period_=0,elapsed_=0,sound_=-1;unsigned random_;
    bool mirror_,suppressed_=false,equal_=false;float speed_=0,angle_=0,acceleration_=0,vx_=0,vy_=0;
    std::array<float,12> variables_{};stage5::reference::Instruction shot_{};
};
/// @brief 四难度十种普通配方直接对照独立夹具，覆盖两种子、四 rank、镜像、逐发参数和死亡掉落。
void TestDifficultyOrdinaryReference() {
    struct Case {int sub;behavior::Pattern pattern;};
    const std::array<Case,10> cases{{
        {0,stage5::kAcceleratingBlueRings},{1,stage5::kPassingRandomCrystals},{2,stage5::kShortBlueRings},
        {3,stage5::kBlueLayeredRings},{4,stage5::kRedLayeredRings},{5,stage5::kGreenLayeredRings},
        {6,stage5::kRandomBlueFans},{9,stage5::kPassingBlueFans},{10,stage5::kPassingRedFans},{11,stage5::kPassingRandomRedRings},
    }};
    constexpr float scale=20.0F/448,speedScale=scale*60;auto bullets=std::make_unique<BulletSystem>();
    for(int d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(bool mirror:{false,true})for(const auto& c:cases) {
        auto pattern=c.pattern;pattern.spatialShots=false;
        behavior::Spawn spawn{.position={0,0,10-32*scale},.pattern=pattern,.difficulty=*native_combat::DifficultyFromIndex(d),
            .mirrored=mirror,.sourceHealth=100,.seed=seed,.visual=stage5::kOrb,.hitRadius=14*scale,.retireOutside=false};
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);stage5::ConfigureBehavior(actor,false);
        OrdinaryReference source(d,c.sub,{192,32,0},seed,rank,mirror);
        const auto near=[&](float actual,float expected,float tolerance) {
            if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance) {
                std::cerr<<"Stage5 ordinary d "<<d<<" sub "<<c.sub<<" seed "<<seed<<" rank "<<rank<<" mismatch "<<actual<<" vs "<<expected<<'\n';std::abort();
            }
        };
        for(int frame=0;frame<400;++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            near(actor.position.x,(source.position.x-192)*scale,5e-4F);near(actor.position.z,10-source.position.y*scale,5e-4F);
            assert(actor.animationScript==source.animation&&actor.soundEvents==source.sounds);near(actor.hitRadius,source.radius*scale,1e-5F);
            std::vector<const Bullet*> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(&b);});
            if(shots.size()!=source.emissions.size())std::cerr<<"Stage5 ordinary d "<<d<<" sub "<<c.sub<<" frame "<<frame<<" shots "<<shots.size()<<" vs "<<source.emissions.size()<<'\n';
            assert(shots.size()==source.emissions.size()&&!bullets->Pool().DroppedSpawns());
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=source.emissions[n];
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.sampleMotionAtStart);
                near(b.position.x,(e.position.x-192)*scale,5e-4F);near(b.position.z,10-e.position.y*scale,5e-4F);
                near(b.velocity.x,e.velocity.x*speedScale,1e-4F);near(b.velocity.y,0,1e-5F);near(b.velocity.z,-e.velocity.y*speedScale,1e-4F);
            }
        }
        assert(actor.Random(1)==source.Random(1));
        if(source.death>=0) {
            assert(actor.Defeat());source.Defeat();source.Tick({245,360,0});bullets->Reset();actor.Update(1.0F/60,{},*bullets);
            assert(actor.finished&&!source.active&&actor.dropEvents==source.drops);
        }
    }
}
/// @brief 四难度普通配方默认 120 Hz 在五种显示帧率下交付一致；冻结弹体积分，只检查行为、空间发射及随机流。
void TestOrdinaryDisplayRates() {
    const std::array patterns{stage5::kAcceleratingBlueRings,stage5::kPassingRandomCrystals,stage5::kShortBlueRings,
        stage5::kBlueLayeredRings,stage5::kRedLayeredRings,stage5::kGreenLayeredRings,stage5::kRandomBlueFans,
        stage5::kPassingBlueFans,stage5::kPassingRedFans,stage5::kPassingRandomRedRings};
    for(int d=0;d<4;++d)for(const auto& pattern:patterns) {
        std::vector<Bullet> expected;Vector3 position{};float random=0;int animation=-1;double phase=0;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;
            behavior::Spawn spawn{.position={0,0,8},.pattern=pattern,.difficulty=*native_combat::DifficultyFromIndex(d),
                .sourceHealth=100,.seed=7,.visual=stage5::kOrb,.retireOutside=false};
            actor.Activate(spawn);actor.SetRank(32);stage5::ConfigureBehavior(actor,true);
            for(int frame=0;frame<fps*6;++frame)actor.Update(1.0F/fps,{2,3,-6},*bullets);
            std::vector<Bullet> actual;bullets->Pool().ForEachActive([&](const Bullet& b){actual.push_back(b);});
            const float nextRandom=actor.Random(1);
            if(fps==120) {expected=actual;position=actor.position;random=nextRandom;animation=actor.animationScript;phase=actor.phaseFrames;}
            else {
                assert(actual.size()==expected.size()&&random==nextRandom&&animation==actor.animationScript&&phase==actor.phaseFrames);
                Near(actor.position.x,position.x);Near(actor.position.y,position.y);Near(actor.position.z,position.z);
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto& a=actual[n];const auto& b=expected[n];
                    assert(a.nativeType==b.nativeType&&a.nativeColor==b.nativeColor&&a.sampleMotionAtStart==b.sampleMotionAtStart);
                    Near(a.position.x,b.position.x);Near(a.position.y,b.position.y);Near(a.position.z,b.position.z);
                    Near(a.velocity.x,b.velocity.x,1e-4F);Near(a.velocity.y,b.velocity.y,1e-4F);Near(a.velocity.z,b.velocity.z,1e-4F);
                    Near(a.spawnVisualSeconds,b.spawnVisualSeconds,1e-5F);
                }
            }
            assert(!bullets->Pool().DroppedSpawns());
        }
    }
}
/// @brief 道中非符/符卡逐帧对照独立源调用栈，检查瞬移、移动、周期、动画、音效、反弹快照和随机读取。
void TestMidbossDifficultyReference() {
    constexpr float scale=20.0F/448,speedScale=scale*60;auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(int sub:{13,18}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},15000,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);actor.SetRank(rank);stage5::ConfigureBehavior(actor,false);
        actor.SetPoses({128,131,132,129,130}); // 恢复Sub12入场已建立的动画表。
        if(sub==18) {actor.motion.bounds={{-7.14285714F,2.14285714F},{7.14285714F,7.85714286F}};actor.motion.clamped=true;}
        actor.Start(sub==13?bosses::SakuyaBoss::Behaviors::MisdirectionNonspell(actor):bosses::SakuyaBoss::Behaviors::Misdirection(actor));
        stage5::reference::CombatReference source(d,sub,{192,128,0},seed,rank);
        for(int frame=0;frame<1600;++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            CompareStage5Effects(actor,source.effects);
            std::vector<const Bullet*> shots;bullets->Pool().ForEachActive([&](const Bullet& b){shots.push_back(&b);});
            if(shots.size()!=source.emissions.size()||actor.animationScript!=source.animation||actor.soundEvents!=source.sounds)
                std::cerr<<"Stage5 mid d "<<d<<" sub "<<sub<<" frame "<<frame<<" count "<<shots.size()<<'/'<<source.emissions.size()<<" animation "<<actor.animationScript<<'/'<<source.animation<<'\n';
            assert(shots.size()==source.emissions.size()&&actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
            if(std::abs(actor.position.x-(source.position.x-192)*scale)>5e-4F||std::abs(actor.position.z-(10-source.position.y*scale))>5e-4F)
                std::cerr<<"Stage5 mid position d "<<d<<" sub "<<sub<<" frame "<<frame<<'\n';
            Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.z,10-source.position.y*scale);
            assert(actor.CanTakeDamage()==(source.damageable&&source.interactable));
            for(std::size_t n=0;n<shots.size();++n) {
                const auto& b=*shots[n];const auto& e=source.emissions[n];
                Near(b.position.x,(e.position.x-192)*scale);Near(b.position.z,10-e.position.y*scale);
                if(std::abs(b.velocity.x-e.velocity.x*speedScale)>1e-4F||std::abs(b.velocity.z+e.velocity.y*speedScale)>1e-4F)
                    std::cerr<<"Stage5 mid velocity d "<<d<<" sub "<<sub<<" seed "<<seed<<" rank "<<rank<<" frame "<<frame<<" shot "<<n<<'\n';
                Near(b.velocity.x,e.velocity.x*speedScale,1e-4F);Near(b.velocity.y,0,1e-5F);Near(b.velocity.z,-e.velocity.y*speedScale,1e-4F);
                assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.bouncesRemaining==e.bounces);
                if(e.bounces) {assert(b.reflectLowerZ==e.reflectLower);Near(b.bounceSpeed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,1e-4F);}
            }
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief 按源单位核对同一更新步的位姿、逐发、音效与掉落；不积分弹体，防止后续运动掩盖出生差异。
void CompareCombatFrame(const behavior::Actor& actor,const stage5::reference::CombatReference& source,
                        BulletSystem& bullets,int difficulty,int sub,int frame) {
    CompareStage5Effects(actor,source.effects);
    constexpr float scale=20.0F/448,speedScale=scale*60;
    std::vector<const Bullet*> shots;bullets.Pool().ForEachActive([&](const Bullet& b){shots.push_back(&b);});
    if(shots.size()!=source.emissions.size()||actor.animationScript!=source.animation||actor.soundEvents!=source.sounds)
        std::cerr<<"Stage5 final d "<<difficulty<<" sub "<<sub<<" frame "<<frame<<" count "<<shots.size()<<'/'<<source.emissions.size()
            <<" animation "<<actor.animationScript<<'/'<<source.animation<<" sounds "<<actor.soundEvents.size()<<'/'<<source.sounds.size()<<'\n';
    assert(shots.size()==source.emissions.size()&&actor.animationScript==source.animation&&actor.soundEvents==source.sounds);
    Near(actor.position.x,(source.position.x-192)*scale);Near(actor.position.y,-source.position.z*scale);Near(actor.position.z,10-source.position.y*scale);
    assert(actor.CanBeTargeted()==source.interactable&&actor.dropEvents==source.drops);
    assert(actor.CanTakeDamage()==(source.damageable&&source.interactable));
    for(std::size_t n=0;n<shots.size();++n) {
        const auto& b=*shots[n];const auto& e=source.emissions[n];
        Near(b.position.x,(e.position.x-192)*scale);Near(b.position.y,-e.position.z*scale);Near(b.position.z,10-e.position.y*scale);
        Near(b.velocity.x,e.velocity.x*speedScale,1e-4F);Near(b.velocity.y,-e.velocity.z*speedScale,1e-5F);Near(b.velocity.z,-e.velocity.y*speedScale,1e-4F);
        assert(b.nativeType==e.type&&b.nativeColor==e.color&&b.bouncesRemaining==e.bounces);
        if(e.bounces) {assert(b.reflectLowerZ==e.reflectLower);Near(b.bounceSpeed,std::hypot(e.velocity.x,e.velocity.y)*speedScale,1e-4F);}
    }
    if(source.interrupt)assert(actor.animationInterrupt&&actor.animationInterrupt->label==source.interrupt);
}
/// @brief 从源95出生请求检查实例参数，再独立执行使魔至自然退场；方向、难度和种子必须按值随请求保存。
void TestSummonedDifficultyReference(behavior::Spawn spawn,const stage5::reference::CombatSpawn& birth,int rank,BulletSystem& bullets) {
    constexpr float scale=20.0F/448;
    assert(spawn.sourceHealth==birth.health&&spawn.sourceScore==birth.score&&spawn.sourceItem==birth.item);
    Near(spawn.position.x,(birth.position.x-192)*scale);Near(spawn.position.z,10-birth.position.y*scale);
    spawn.pattern.spatialShots=false;
    const int d=native_combat::DifficultyIndex(spawn.difficulty);
    for(bool defeated:{false,true}) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Activate(spawn);actor.SetRank(rank);
        stage5::reference::CombatReference source(d,birth.sub,birth.position,spawn.seed,rank);
        int frames=0;
        for(;frames<400&&source.active;++frames) {
            if(defeated&&frames==123) {assert(actor.Defeat());source.DefeatFamiliar();}
            bullets.Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},bullets);
            CompareCombatFrame(actor,source,bullets,d,birth.sub,frames);assert(actor.active==source.active);
            Near(actor.hitRadius,source.radius*scale);
        }
        assert(!actor.active&&!source.active&&frames==(defeated?184:birth.sub>=38?349:351));
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief 三段关底非符逐帧对照四难度、两种子和四rank；使魔按源出生次序单独执行，不借用生产配方生成期望值。
void TestFinalNonspellDifficultyReference() {
    const std::array<behavior::Entry,3> entries{bosses::SakuyaBoss::Behaviors::FirstNonspell,
        bosses::SakuyaBoss::Behaviors::SecondNonspell,bosses::SakuyaBoss::Behaviors::ThirdNonspell};
    constexpr std::array subs{24,36,43};constexpr float scale=20.0F/448;auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(int phase=0;phase<3;++phase) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},19000,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);actor.SetRank(rank);stage5::ConfigureBehavior(actor,false);
        actor.SetPoses({128,131,132,129,130});actor.Start(entries[phase](actor));
        stage5::reference::CombatReference source(d,subs[phase],{192,128,0},seed,rank);
        unsigned spawnSeed=seed;
        for(int frame=0;frame<1600;++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            CompareCombatFrame(actor,source,*bullets,d,subs[phase],frame);
            if(actor.spawnEvents.size()!=source.spawns.size())std::cerr<<"Stage5 spawn d "<<d<<" sub "<<subs[phase]<<" frame "<<frame<<" count "<<actor.spawnEvents.size()<<'/'<<source.spawns.size()<<'\n';
            assert(actor.spawnEvents.size()==source.spawns.size());
            for(std::size_t i=0;i<source.spawns.size();++i) {
                assert(actor.spawnEvents[i].difficulty==actor.difficulty&&actor.spawnEvents[i].seed==++spawnSeed);
                TestSummonedDifficultyReference(actor.spawnEvents[i],source.spawns[i],rank,*bullets);
            }
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief 独立单非符配置只用于验证四难度技能绑定和默认120 Hz阈值/超时交接，不能替代尚未完成的关底符卡组合。
void TestFinalNonspellDifficultyLifecycle() {
    using S=bosses::SakuyaBoss;using B=S::Behavior;
    constexpr std::array entries{B::FirstNonspell,B::SecondNonspell,B::ThirdNonspell};constexpr std::array subs{24,36,43};
    auto bullets=std::make_unique<BulletSystem>();S inspector;
    for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot)for(int phase=0;phase<3;++phase) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        stage5::reference::CombatReference source(d,subs[phase],{192,128,0},7,16);source.Tick({245,360,0});
        const auto definition=inspector.DescribeSkill(entries[phase],type,difficulty);
        Near(definition.NativeHealth(),source.health);Near(definition.reserveHealth,source.threshold);assert(definition.timeoutFrames==source.timeout);
        for(bool timeout:{false,true}) {
            auto config=stage5::SakuyaFinalConfig(type);config.difficulty=difficulty;
            config.phases={{{entries[phase]},{{{0,1,WHITE}}}}};S boss;boss.StartBoss(config);boss.SetChallengeRank(32);
            for(int tick=0;tick<140;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            boss.ResumeCombat();assert(boss.attack==1&&boss.HealthBar().count==1);Near(boss.health,source.health);
            bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));assert(boss.CanTakeDamage());
            if(timeout) {
                bool ended=false;for(int tick=0;tick<source.timeout*2+2&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                assert(ended&&!boss.SkillDefeated());Near(boss.health,source.threshold);
            } else {boss.health=source.threshold-1;assert(boss.ShouldAdvanceFromDamage()&&!boss.SkillDefeated());}
            assert(boss.AdvanceSkill()&&!boss.CanTakeDamage());bool ended=false;
            for(int tick=0;tick<150&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
            assert(ended&&!boss.AdvanceSkill()&&!boss.active&&!boss.WorldTimeStopped());
        }
    }
}
/// @brief 三组符卡按四难度执行独立ECL控制流，覆盖准备、循环、原生弧线、时停时钟和后续随机流。
/// 每步只比较新生弹，改向时弹池为空；既有候选的改向由TestRedirectRandomOrder及图形路径互补。
void TestFinalSpellDifficultyReference() {
    using S=bosses::SakuyaBoss;constexpr float scale=20.0F/448;auto bullets=std::make_unique<BulletSystem>();
    for(unsigned d=0;d<4;++d)for(unsigned seed:{7U,0x9e3779b9U})for(int rank:{10,16,31,32})for(int group=0;group<3;++group) {
        const std::array<behavior::Entry,3> entries{d>=2?S::Behaviors::JackTheLudoBile:S::Behaviors::ClockCorpse,
            d>=2?S::Behaviors::TheWorld:S::Behaviors::LunaClock,d>=2?S::Behaviors::KillingDoll:S::Behaviors::Marionette};
        const int sub=group==0?(d>=2?49:46):group==1?(d>=2?55:52):60;
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},group==2?1700:1400,seed);
        actor.difficulty=*native_combat::DifficultyFromIndex(d);actor.SetRank(rank);stage5::ConfigureBehavior(actor,false);
        actor.motion.bounds={{-7.14285714F,4.10714286F},{7.14285714F,7.85714286F}};actor.motion.clamped=true;
        actor.Start(entries[group](actor));stage5::reference::CombatReference source(d,sub,{192,128,0},seed,rank);
        for(int frame=0;frame<1600;++frame) {
            bullets->Reset();source.Tick({245,360,0});actor.Update(1.0F/60,{(245-192)*scale,0,10-360*scale},*bullets);
            CompareCombatFrame(actor,source,*bullets,d,sub,frame);
            if(actor.phaseClockPaused!=source.timeStopped||actor.phaseFrames!=source.phaseFrames)
                std::cerr<<"Stage5 spell clock d "<<d<<" sub "<<sub<<" frame "<<frame<<" phase "<<actor.phaseFrames<<'/'<<source.phaseFrames<<'\n';
            assert(actor.phaseClockPaused==source.timeStopped&&actor.phaseFrames==source.phaseFrames);
        }
        assert(actor.Random(1)==source.Random(1));
    }
}
/// @brief 四机体在各难度沿完整关底六技能走击破/超时，元数据独立读取源准备表，跨阶段立即解除时停并保护。
void TestFinalDifficultyLifecycle() {
    using S=bosses::SakuyaBoss;using B=S::Behavior;auto bullets=std::make_unique<BulletSystem>();S inspector;
    for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        const auto config=stage5::SakuyaFinalConfig(type,difficulty);
        for(int group=0;group<3;++group) {
            stage5::reference::CombatReference source(d,group==0?(d>=2?49:46):group==1?(d>=2?55:52):60,{192,128,0},7,16);source.Tick({245,360,0});
            const auto spell=inspector.DescribeSkill(config.phases[group].skills[1].id,type,difficulty);
            assert(spell.spellCard==source.card+1&&spell.timeoutFrames==source.timeout);Near(spell.health,group==2?1700:1400);
            const auto invalid=group==0?(d>=2?B::ClockCorpse:B::JackTheLudoBile):group==1?(d>=2?B::LunaClock:B::TheWorld):d>=2?B::Marionette:B::KillingDoll;
            bool rejected=false;try{(void)inspector.DescribeSkill(invalid,type,difficulty);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
        }
        for(bool timeout:{false,true}) {
            S boss;boss.StartBoss(config);boss.SetChallengeRank(32);
            for(int tick=0;tick<140;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            boss.ResumeCombat();
            for(int attack=1;attack<=6;++attack) {
                assert(boss.attack==attack&&boss.phase==(attack+1)/2&&boss.skill==(attack%2?1:2)&&boss.HealthBar().count==2);
                int ticks=0;bool ended=false,stopped=false,announced=false;
                for(;ticks<65*120&&!ended;++ticks) {
                    bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);stopped|=boss.WorldTimeStopped();
                    if(attack%2==0&&boss.SpellName()) {announced=true;assert(boss.SpellCardNumber()==89+((attack-1)/2)*4+d);}
                    if(!timeout&&boss.CanTakeDamage()&&boss.skillTimer>10) {boss.health=attack%2?(attack==5?1699:1399):0;ended=boss.ShouldAdvanceFromDamage();}
                }
                assert(ended&&ticks<65*120&&(attack%2||announced));if(attack%2==0)assert(stopped);
                assert(boss.SkillDefeated()==(!timeout&&attack%2==0));assert(boss.AdvanceSkill()&&!boss.CanTakeDamage()&&!boss.WorldTimeStopped());
            }
            bool ended=false;for(int tick=0;tick<150&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
            assert(ended&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==!timeout);
        }
    }
}
/// @brief 十六张单技能符卡分别核对四机体的宣告、实际发射和完整退场，目录开放前也能拒绝错配的难度/卡号。
void TestSakuyaPracticeSkills() {
    auto bullets=std::make_unique<BulletSystem>();int count=0;
    for(const auto& card:spell::Cards())if(card.stage==4) {
        ++count;const auto difficulty=*native_combat::DifficultyFromIndex(card.difficulty);stage5::Stage5Content content(difficulty);
        for(int shot=0;shot<4;++shot) {
            auto boss=content.CreateBoss(content.PracticeBoss(card.number),static_cast<ShotType>(shot));
            assert(boss->attack==1&&!boss->CanTakeDamage()&&boss->HealthBar().count==1);std::size_t peak=0;int ticks=0;
            // Easy Luna Clock没有初始环弹，首批弧线处于时停保护；等待同轮恢复交互后再击破。
            for(;ticks<1440&&(!boss->CanTakeDamage()||peak==0);++ticks) {
                bullets->Reset();assert(!boss->Update(1.0F/120,{2,3,-6},*bullets));peak=std::max(peak,bullets->Pool().ActiveCount());
            }
            if(ticks>=1440||!peak||boss->SpellCardNumber()!=card.number||std::string_view(boss->SpellName())!=card.name)
                std::cerr<<"Stage5 practice card "<<card.number<<" shot "<<shot<<" ticks "<<ticks<<" peak "<<peak<<" declared "<<boss->SpellCardNumber()<<" name "<<(boss->SpellName()?boss->SpellName():"null")<<'\n';
            assert(ticks<1440&&peak>0&&boss->SpellCardNumber()==card.number&&std::string_view(boss->SpellName())==card.name);
            boss->health=0;assert(boss->ShouldAdvanceFromDamage()&&boss->SkillDefeated());assert(boss->AdvanceSkill());bool ended=false;
            for(int tick=0;tick<500&&!ended;++tick) {bullets->Reset();ended=boss->Update(1.0F/120,{},*bullets);}
            assert(ended&&!boss->AdvanceSkill()&&!boss->active&&boss->Defeated());
        }
        for(int number:{0,84,101,131}) {bool rejected=false;try{(void)content.PracticeBoss(number);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
        stage5::Stage5Content wrong(*native_combat::DifficultyFromIndex((card.difficulty+1)%4));
        bool rejected=false;try{(void)wrong.PracticeBoss(card.number);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    }
    assert(count==16);
}
/// @brief 四难度道中工厂分别验证非符超时、整场击破和符卡超时，完整退场后才结束并按路径交付奖励。
void TestMidbossDifficultyLifecycle() {
    using S=bosses::SakuyaBoss;using B=S::Behavior;auto bullets=std::make_unique<BulletSystem>();S inspector;
    for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot) {
        const auto difficulty=*native_combat::DifficultyFromIndex(d);const auto type=static_cast<ShotType>(shot);
        stage5::reference::CombatReference nonspell(d,13,{192,128,0},7,16),spell(d,18,{192,128,0},7,16);
        nonspell.Tick({245,360,0});spell.Tick({245,360,0});
        const auto n=inspector.DescribeSkill(B::MisdirectionNonspell,type,difficulty),s=inspector.DescribeSkill(B::Misdirection,type,difficulty);
        Near(n.NativeHealth(),15000);Near(n.reserveHealth,nonspell.threshold);Near(s.health,nonspell.threshold);
        assert(n.timeoutFrames==nonspell.timeout&&s.timeoutFrames==spell.timeout&&s.spellCard==spell.card+1);
        for(int route=0;route<3;++route) {
            S boss;boss.StartBoss(stage5::SakuyaMidbossConfig(type,difficulty));boss.SetChallengeRank(32);
            for(int tick=0;tick<140;++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            boss.ResumeCombat();assert(boss.attack==1&&boss.HealthBar().count==2&&!boss.CanTakeDamage());
            bool ended=false;
            for(int tick=0;tick<500&&!boss.CanTakeDamage();++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
            assert(boss.CanTakeDamage());
            if(route==0) {
                for(int tick=0;tick<5000&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                assert(ended&&!boss.SkillDefeated());assert(boss.AdvanceSkill()&&boss.attack==1);
            } else {
                boss.health=nonspell.threshold-1;assert(boss.ShouldAdvanceFromDamage()&&!boss.SkillDefeated());
                assert(boss.AdvanceSkill()&&boss.attack==2&&!boss.CanTakeDamage());Near(boss.health,nonspell.threshold);
                for(int tick=0;tick<500&&!boss.CanTakeDamage();++tick) {bullets->Reset();assert(!boss.Update(1.0F/120,{2,3,-6},*bullets));}
                assert(boss.CanTakeDamage()&&boss.SpellCardNumber()==spell.card+1);
                if(route==2) {
                    for(int tick=0;tick<4000&&!ended;++tick) {bullets->Reset();ended=boss.Update(1.0F/120,{2,3,-6},*bullets);}
                    assert(ended&&!boss.SkillDefeated());
                } else {boss.health=0;assert(boss.ShouldAdvanceFromDamage()&&boss.SkillDefeated());}
                assert(boss.AdvanceSkill());
            }
            ended=false;int drops=0,lives=0;
            for(int tick=0;tick<450&&!ended;++tick) {
                bullets->Reset();ended=boss.Update(1.0F/120,{},*bullets);assert(!boss.CanTakeDamage()&&!boss.WorldTimeStopped());
                for(int count:boss.DropEvents())drops+=count;for(int item:boss.ItemEvents())lives+=item==5;
            }
            assert(ended&&!boss.AdvanceSkill()&&!boss.active&&boss.Defeated()==(route==1));
            assert(drops==(route==1?10:0)&&lives==(route==1?1:0));
        }
    }
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

    struct Case {behavior::Entry entry;int first,cycle,stop,unstop,initialCount;};
    const std::array<Case,3> cases{{{bosses::SakuyaBoss::Behaviors::ClockCorpse,288,400,338,490,128},{bosses::SakuyaBoss::Behaviors::LunaClock,268,418,318,438,128},
        {bosses::SakuyaBoss::Behaviors::Marionette,120,0,195,273,8}}};
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
            if(c.entry!=bosses::SakuyaBoss::Behaviors::Marionette&&frame>=c.stop&&frame<c.stop+(c.entry==bosses::SakuyaBoss::Behaviors::ClockCorpse ? 90 : 60))
                assert(bullets->Pool().ActiveCount()==((frame-c.stop)%9==0 ? 9U : 0U));
            if(frame==c.unstop) {
                assert(!actor.phaseClockPaused&&actor.interactable);assert(actor.animationScript==(actor.Position().x<0 ? 140 : 139));Near(static_cast<float>(actor.phaseFrames),frame+1-paused);
            }
        }
    }
    behavior::Actor mid(behavior::UpdateRate::Hz60);mid.Reset({0,0,4.28571415F},15000,49);
    stage5::ConfigureBehavior(mid,false);mid.Start(bosses::SakuyaBoss::Behaviors::MisdirectionNonspell(mid));
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
    stage5::ConfigureBehavior(misdirection,false);misdirection.Start(bosses::SakuyaBoss::Behaviors::Misdirection(misdirection));
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
    auto bullets=std::make_unique<BulletSystem>();
    for(int phase=0;phase<3;++phase) {
        behavior::Actor actor(behavior::UpdateRate::Hz60);actor.Reset({0,0,4.28571415F},19000,49);stage5::ConfigureBehavior(actor,false);
        const std::array<behavior::Entry,3> entries{bosses::SakuyaBoss::Behaviors::FirstNonspell,bosses::SakuyaBoss::Behaviors::SecondNonspell,bosses::SakuyaBoss::Behaviors::ThirdNonspell};actor.Start(entries[phase](actor));
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
behavior::Routine ArcProbe(behavior::Actor& actor) {for(int pulse=0;pulse<10;++pulse)bosses::SakuyaBoss::Behaviors::EmitClockArc(actor,pulse);co_await actor.At(100);}
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
        bosses::SakuyaBoss boss(behavior::UpdateRate::Hz60,false);boss.StartBoss(final ? stage5::SakuyaFinalConfig() : stage5::SakuyaMidbossConfig());
        for(int frame=0;frame<65;++frame) {assert(!boss.Update(1.0F/60,{},*bullets));assert(!boss.CanTakeDamage()&&!boss.CanContactPlayer()&&boss.CanBeTargeted());}
        boss.ResumeCombat();const int phases=final ? 6 : timeout ? 1 : 2;
        for(int phase=1;phase<=phases;++phase) {
            assert(boss.attack==phase);bool ready=false;
            for(int frame=0;frame<10000&&!ready;++frame) {
                bullets->Reset();ready=boss.Update(1.0F/60,{},*bullets);
                if(!timeout&&boss.CanTakeDamage()) {boss.health=0;ready=boss.ShouldAdvanceFromDamage();}
            }
            assert(ready);if(!timeout&&phase%2==0)assert(boss.SkillDefeated());
            assert(boss.AdvanceSkill());assert(!boss.WorldTimeStopped()&&!boss.CanTakeDamage());
        }
        bool ready=false;for(int frame=0;frame<200&&!ready;++frame)ready=boss.Update(1.0F/60,{},*bullets);
        assert(ready&&!boss.AdvanceSkill()&&!boss.active);assert(boss.Defeated()==!timeout);
    }
    for(bool ricochet:{false,true}) {
        behavior::Actor boss(behavior::UpdateRate::Hz60);boss.Reset({0,0,4.28571415F},19000,49);stage5::ConfigureBehavior(boss,false);
        boss.Start(ricochet ? bosses::SakuyaBoss::Behaviors::SecondNonspell(boss) : bosses::SakuyaBoss::Behaviors::FirstNonspell(boss));
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

behavior::Routine RedirectProbe(behavior::Actor& actor) {bosses::SakuyaBoss::Behaviors::RedirectStoppedBullets(actor);co_await actor.At(100);}
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

    using S=bosses::SakuyaBoss;
    const std::array<behavior::Entry,11> entries{S::Behaviors::MisdirectionNonspell,S::Behaviors::Misdirection,S::Behaviors::FirstNonspell,S::Behaviors::ClockCorpse,S::Behaviors::SecondNonspell,S::Behaviors::LunaClock,S::Behaviors::ThirdNonspell,S::Behaviors::Marionette,S::Behaviors::JackTheLudoBile,S::Behaviors::TheWorld,S::Behaviors::KillingDoll};
    struct Snapshot {Vector3 position,velocity;int type,color,bounces;};
    for(int d=0;d<4;++d)for(auto entry:entries) {
        const bool low=entry==S::Behaviors::ClockCorpse||entry==S::Behaviors::LunaClock||entry==S::Behaviors::Marionette;
        const bool high=entry==S::Behaviors::JackTheLudoBile||entry==S::Behaviors::TheWorld||entry==S::Behaviors::KillingDoll;
        if((low&&d>=2)||(high&&d<2))continue;
        std::vector<Snapshot> expected;Vector3 position{};float random=0;double phase=0;bool stopped=false;
        for(int fps:{120,30,60,144,240}) {
            auto bullets=std::make_unique<BulletSystem>();behavior::Actor actor;actor.Reset({0,0,4},19000,7);
            actor.difficulty=*native_combat::DifficultyFromIndex(d);
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
/// @brief 单独运行新增四难度普通演员源对照，便于定位首次分歧。
void RunStage5DifficultyTests() {TestDifficultyOrdinaryReference();TestOrdinaryDisplayRates();TestMidbossDifficultyReference();TestMidbossDifficultyLifecycle();TestFinalNonspellDifficultyReference();TestFinalNonspellDifficultyLifecycle();TestFinalSpellDifficultyReference();TestFinalDifficultyLifecycle();TestSakuyaPracticeSkills();TestSakuyaRetirementEffects();TestSakuyaEffectDisplayRates();}
/// @brief 运行五面出生、攻击时钟、改向随机顺序、Boss 生命周期与显示帧率一致性；图形由集成目标验证。
void RunStage5Tests() {RunStage5DifficultyTests();TestBirthsMovementAndAttacks();TestDialogueWaitAndRelativeClock();TestWorldTimeClockPrimitives();TestBounceAndLockedSpatialAxis();TestBossSourceClocks();TestMainNonspellSourceClocks();TestClockArcCoordinates();TestBossLifecycleAndSummons();TestRedirectRandomOrder();TestSpatialDisplayFrameRates();}

/// @brief 单独运行道中四难度源时钟差分，避免每次定位重复普通演员用例。
void RunStage5MidbossTests() {TestMidbossDifficultyReference();TestMidbossDifficultyLifecycle();}
/// @brief 仅运行五面关底非符与使魔源差分，供增量移植定位首个分歧。
void RunStage5FinalNonspellTests() {TestFinalNonspellDifficultyReference();TestFinalNonspellDifficultyLifecycle();}
/// @brief 单独运行关底三组符卡四难度源对照，便于定位高难替换卡差异。
void RunStage5FinalSpellTests() {TestFinalSpellDifficultyReference();TestFinalDifficultyLifecycle();TestSakuyaPracticeSkills();}
/// @brief 单独运行五面单技能工厂契约，避免入口定位重复全部源时钟对照。
void RunStage5PracticeTests() {TestSakuyaPracticeSkills();}
/// @brief 单独核对五面102/118全部实际战斗请求及退场、默认频率时钟；复用独立源执行器而非生产期望表。
void RunStage5EffectTests() {TestFinalNonspellDifficultyReference();TestFinalSpellDifficultyReference();TestSakuyaRetirementEffects();TestSakuyaEffectDisplayRates();}

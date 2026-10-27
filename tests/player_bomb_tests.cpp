// 本文件验证四 Bomb 的真实 ANM、固定帧序、三维视觉差异、资源借用边界与玩家消耗保护期。
#include "gameplay/effects/player_bomb.hpp"
#include "gameplay/actors/player.hpp"
#include "stages/stage1/content.hpp"
#include "app/math.hpp"
#include "gameplay/combat/damage_rules.hpp"
#include "gameplay/arena.hpp"
#include "render/bomb_geometry.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace koumakyou;
const Camera3D kCamera{{0,0,-6.5F},{0,0,3.5F},{0,1,0},50,CAMERA_PERSPECTIVE};
void TestNativeBombs() {
    stage1::Stage1Content stage;
    const std::array<int,4> scripts{133,137,5,8},durations{300,140,250,300},invulnerability{360,200,310,360};
    for(int type=0;type<4;++type) {
        const auto shot=static_cast<ShotType>(type);
        const auto& definition=BombDefinition(shot);
        assert(definition.firstScript==scripts[type] && definition.durationFrames==durations[type]);
        assert(definition.invulnerabilityFrames==invulnerability[type] && !definition.name.empty());
        const auto archive=anm::LoadArchive(std::filesystem::path(KOUMAKYOU_ASSET_SOURCE_ROOT)/battle::Describe(definition.atlas).anm);
        const auto visuals=stage.PlayerBombVisuals(shot);
        for(int layer=0;layer<definition.layers;++layer) {
            assert(visuals[layer]!=kInvalidVisualId && archive.FindScript(scripts[type]+layer));
            assert(stage.Assets().visuals[visuals[layer]].animation==battle::Selection(definition.atlas,scripts[type]+layer));
        }
        std::vector<PlayerBombSprite> expected;
        for(int fps:{30,60,144}) {
            PlayerBombEffect bomb;
            bomb.Start(shot,kCamera,archive,42);
            assert(bomb.Active() && bomb.Frame()==0 && bomb.BackdropAlpha()==0);
            assert(bomb.Sprites().size()==(type==0 ? 0 : type==1 ? 6 : type==2 ? kBombStarCount*2 : 4));
            for(int frame=0;frame<fps*2;++frame)bomb.Update(1.0F/fps,kCamera);
            assert(bomb.Frame()==120 && bomb.BackdropAlpha()==(type==1 ? 58 : type==3 ? 96 : 176));
            if(fps==30)expected.assign(bomb.Sprites().begin(),bomb.Sprites().end());
            else {
                assert(expected.size()==bomb.Sprites().size());
                for(std::size_t i=0;i<expected.size();++i) {
                    const auto& actual=bomb.Sprites()[i];const auto& before=expected[i];
                    assert(actual.layer==before.layer && actual.shape==before.shape);
                    assert(game_math::DistanceSquared(actual.position,before.position)<1e-8F);
                    assert(game_math::DistanceSquared(actual.end,before.end)<1e-8F);
                    assert(actual.frame.color.a==before.frame.color.a && actual.frame.scale.x==before.frame.scale.x);
                }
            }
            bomb.Update((durations[type]-120.0F)/60,kCamera);
            assert(!bomb.Active() && bomb.Sprites().empty() && bomb.BackdropAlpha()==0);
        }
        assert(expected.size()==(type==0 ? 12 : type==1 ? 6 : type==2 ? kBombStarCount*2 : 4));
        if(type==0) {
            // 第三颗光球在 112 帧出生；第 120 帧仍处于原四色层的不同增长相位。
            assert(expected[8].frame.sprite==131 && expected[8].frame.scale.x>0);
            assert(expected[8].frame.color.g!=expected[9].frame.color.g);
        } else if(type==1) {
            assert(expected[0].frame.sprite==132 && expected[0].shape==PlayerBombSprite::Shape::Slab);
            assert(expected[0].position.x!=expected[1].position.x && expected[2].position.y!=expected[3].position.y);
            assert(expected[4].position.z!=expected[5].position.z);
        } else if(type==2) {
            assert(expected[0].frame.sprite==18 && expected[1].trail && expected[1].height<expected[0].height);
            assert(std::abs(expected[0].position.x+5)<0.0001F && std::abs(expected[0].position.y+5)<0.0001F);
            assert(expected[0].position.y<0 && expected[0].position.z<kCamera.position.z);
            assert(expected[126].position.y>0 && expected[126].position.z>kCamera.position.z);
            assert(game_math::DistanceSquared(expected[0].position,expected[2].position)>1);
            PlayerBombEffect held;held.Start(shot,kCamera,archive);
            for(int frame=1;frame<250;++frame) {
                held.Update(1.0F/60,kCamera);
                if(frame>=180) {
                    assert(held.Sprites().size()==128); // 原脚本 220 帧结束后，扩展覆盖阶段不能出现隐形星体。
                    for(const auto& sprite:held.Sprites())assert(sprite.frame.visible && sprite.frame.color.a>0);
                }
            }
        } else {
            assert(expected[0].shape==PlayerBombSprite::Shape::Beam && expected[0].width>28);
            for(const auto& layer:expected) {
                assert(game_math::DistanceSquared(layer.position,kCamera.position)==0);
                assert(game_math::DistanceSquared(layer.end,expected[0].end)==0);
            }
            PlayerBombEffect bomb;bomb.Start(shot,kCamera,archive);bomb.Update(1,kCamera);
            auto turned=kCamera;turned.target={10,2,-6.5F};bomb.Update(0,turned);
            assert(bomb.Sprites()[0].end.x>10 && bomb.Frame()==60); // 转向跟随不重启原生动画。
            const auto position=bomb.Sprites()[0].position;
            bomb.Update(0,turned);assert(game_math::DistanceSquared(position,bomb.Sprites()[0].position)==0);
        }
    }
}
void TestSealScheduleAndClear() {
    const auto archive=anm::LoadArchive(std::filesystem::path(KOUMAKYOU_ASSET_SOURCE_ROOT)/"battle/native/player00.anm");
    PlayerBombEffect bomb;bomb.Start(ShotType::HomingAmulet,kCamera,archive,7);
    for(int frame=1;frame<=176;++frame) {
        bomb.Update(1.0F/60,kCamera);
        const int orbs=frame<80 ? 0 : (frame-80)/16+1;
        assert(bomb.Sprites().size()==static_cast<std::size_t>(orbs*4));
        const bool spawned=frame>=80 && frame%16==0;
        assert(bomb.SoundEvents().size()==(spawned ? 1U : 0U));
        if(spawned)assert(bomb.SoundEvents()[0]==13);
    }
    bomb.Update(94.0F/60,kCamera);assert(bomb.Frame()==270 && bomb.Sprites().size()==28);
    assert(bomb.SoundEvents().size()==7);
    for(int sound:bomb.SoundEvents())assert(sound==15);
    const auto alpha=bomb.Sprites()[0].frame.color.a;
    bomb.Update(10.0F/60,kCamera);assert(bomb.Sprites()[0].frame.color.a<alpha);
    bomb.Clear();assert(!bomb.Active() && bomb.Sprites().empty());
    bool invalid=false;
    try { bomb.Update(-1,kCamera); } catch(const std::invalid_argument&) { invalid=true; }
    assert(invalid);
    invalid=false;
    try { (void)BombDefinition(static_cast<ShotType>(99)); } catch(const std::invalid_argument&) { invalid=true; }
    assert(invalid);
    invalid=false;
    try { bomb.Start(ShotType::HomingAmulet,kCamera,anm::Archive{}); } catch(const std::invalid_argument&) { invalid=true; }
    assert(invalid && !bomb.Active());
}
void TestBombConsumption() {
    auto bullets=std::make_unique<BulletSystem>();
    for(int type=0;type<4;++type) {
        Player player;player.ConfigureRun(3,3,false,static_cast<ShotType>(type));
        const auto& bomb=BombDefinition(player.Shot());
        assert(player.Update(0,{.bomb=true},*bullets,0,1,true) && player.Bombs()==2);
        assert(!player.Update(bomb.durationFrames/60.0F-0.01F,{.bomb=true},*bullets,0,1,true));
        assert(player.Bombs()==2);
        assert(player.Update(0.011F,{.bomb=true},*bullets,0,1,true) && player.Bombs()==1);
        player.Update(bomb.invulnerabilityFrames/60.0F-0.01F,{},*bullets,0,1,true);
        Bullet hit{};hit.active=true;hit.position=player.Position();
        assert(player.ContactEnemyBullet(hit)==PlayerContact::None);
        player.Update(0.011F,{},*bullets,0,1,true);
        hit.active=true;assert(player.ContactEnemyBullet(hit)==PlayerContact::Hit);
    }
    Player slow,normal;
    slow.ConfigureRun(3,3,false,ShotType::IllusionLaser);normal.ConfigureRun(3,3,false,ShotType::IllusionLaser);
    assert(slow.Update(0,{.bomb=true},*bullets,0,1,true));
    const auto origin=slow.Position();
    slow.Update(0.5F,{.ascend=true},*bullets,0,1,true);normal.Update(0.5F,{.ascend=true},*bullets,0,1,true);
    assert(std::abs((slow.Position().y-origin.y)/(normal.Position().y-origin.y)-0.3F)<0.0001F);
    slow.ResetPose();normal.ResetPose();
    slow.Update(0.25F,{.ascend=true},*bullets,0,1,true);normal.Update(0.25F,{.ascend=true},*bullets,0,1,true);
    assert(slow.Position().y==normal.Position().y); // 取消或复活不能残留恋札移速。
}

void TestSpatialVolumes() {
    BombVolume cone;cone.shape=BombVolume::Shape::Frustum;cone.end={0,0,10};cone.radius=1;cone.endRadius=3;
    assert(cone.IntersectsSphere({0,0,5},0) && cone.IntersectsSphere({2,0,5},0));
    assert(cone.IntersectsSphere({2.2F,0,5},0.2F) && !cone.IntersectsSphere({2.3F,0,5},0.2F));
    assert(!cone.IntersectsSphere({0,0,-2},0.5F) && !cone.IntersectsSphere({0,0,11},0.5F));
    assert(cone.IntersectsSphere({0,0,10.4F},0.5F));
    assert(cone.IntersectsCapsule({-5,0,5},{5,0,5},0.1F)); // 两端均在外的横穿激光仍须清除。
    assert(!cone.IntersectsCapsule({-5,5,5},{5,5,5},0.1F));
    assert(!cone.IntersectsSphere({0,4,5},0.1F)); // Y 方向不能继承原作二维的无限高度。
    for(float t:{0.0F,0.25F,0.75F,1.0F})for(int i=0;i<16;++i) {
        const auto point=BombBeamSurface(cone,t,i*2*PI/16);
        assert(cone.DistanceSquared(point)<1e-7F);
    }
    assert(std::abs(BombBeamSurface(cone,0.5F,0).x-BombBeamSurface(cone,0.5F,PI).x)>3.9F);
    assert(std::abs(BombBeamSurface(cone,0.5F,PI/2).y-BombBeamSurface(cone,0.5F,-PI/2).y)>3.9F);
    BombVolume slab;slab.shape=BombVolume::Shape::Slab;slab.halfExtents={1,10,11.5F};
    assert(slab.IntersectsSphere({1.4F,9,0},0.5F));
    assert(!slab.IntersectsSphere({1.4F,10.4F,0},0.5F));
    assert(slab.IntersectsCapsule({-5,3,4},{5,3,4},0.01F));
}

// 对一个足以覆盖整场的目标结算，隔离帧序与通道合并；同时保留真实光球累计反馈，不能用 DPS 常量替代。
int TotalDamage(ShotType shot,int fps,std::vector<int>& bursts) {
    PlayerBombEffect bomb;bomb.Start(shot,kCamera);
    assert(bomb.Attacks().empty());
    int total=0,callbacks=0;
    bomb.Update(0,kCamera,{},[&](PlayerBombEffect&) { assert(false); });
    for(int step=0;step<fps*6;++step)bomb.Update(1.0F/fps,kCamera,{},[&](PlayerBombEffect& current) {
        ++callbacks;
        const auto channels=current.ContactDamage({},1000);
        for(const auto& attack:current.Attacks()) {
            assert(attack.volume.IntersectsSphere({},1000));
            if(attack.damage==200)bursts.push_back(current.Frame());
        }
        int raw=0;
        for(int region=0;region<8;++region) { raw+=channels[region];current.ReportHit(region,channels[region]); }
        total+=static_cast<int>(ResolveDamage({DamageSource::Bomb,DamageTarget::Enemy,shot,static_cast<float>(raw),
                                             true,false,nullptr,0,0,1.0F/60}).amount);
        if(shot==ShotType::Needle)assert((raw>0)==(current.Frame()>60 && current.Frame()%2!=0));
        if(shot==ShotType::MagicMissile)assert((raw>0)==(current.Frame()%3!=0));
        if(shot==ShotType::IllusionLaser) { assert(raw==0 || raw==12);assert((raw>0)==(current.Frame()%4!=0)); }
    });
    assert(callbacks==BombDefinition(shot).durationFrames-1 && !bomb.Active());
    return total;
}
void TestAttackCadenceAndCoverage() {
    // 本机新典的分支是「不是 N 的倍数」，魔札和恋札并非每 3/4 帧各打一发。
    constexpr std::array<int,4> totals{1218,1280,10624,2700};
    for(int type=0;type<4;++type)for(int fps:{30,60,144}) {
        std::vector<int> bursts;
        assert(TotalDamage(static_cast<ShotType>(type),fps,bursts)==totals[type]);
        if(type==0)assert((bursts==std::vector<int>{93,109,125,141,157,173,189}));
    }
    PlayerBombEffect seal;seal.Start(ShotType::Needle,kCamera);
    std::array<bool,125> reached{};
    for(int frame=1;frame<140;++frame)seal.Update(1.0F/60,kCamera,{},[&](PlayerBombEffect& current) {
        int index=0;
        for(float x:{-9.9F,-5.0F,0.0F,5.0F,9.9F})for(float y:{-9.9F,-5.0F,0.0F,5.0F,9.9F})
            for(float z:{-11.4F,-5.0F,0.0F,5.0F,11.4F}) {
                for(const auto& attack:current.Attacks())if(attack.volume.IntersectsSphere({x,y,z},0.1F))reached[index]=true;
                ++index;
            }
        if(frame<=60)assert(current.Attacks().empty() && current.CancelVolumes().empty());
        if(frame==62)assert(current.Attacks().empty() && current.CancelVolumes().size()==6);
    });
    assert(std::all_of(reached.begin(),reached.end(),[](bool hit) {return hit;}));
    PlayerBombEffect spark;spark.Start(ShotType::IllusionLaser,kCamera);spark.Update(1.0F/60,kCamera);
    bool front=false,behind=false,above=false;
    for(const auto& attack:spark.Attacks()) {
        front|=attack.volume.IntersectsSphere({0,0,8},0.5F);
        behind|=attack.volume.IntersectsSphere({0,0,-10},0.5F);
        above|=attack.volume.IntersectsSphere({0,9,-4},0.5F);
    }
    assert(front && !behind && above);
    auto turned=kCamera;turned.target={10,0,-6.5F};int extra=0;
    spark.Update(0,turned,{},[&](PlayerBombEffect&) {++extra;});assert(extra==0);
    assert(spark.Attacks().size()==1 && spark.Attacks()[0].volume.IntersectsSphere({10,0,-6.5F},0.5F));
}

void TestContactBoundaries() {
    const auto noDamage=[](const auto& channels) {return std::all_of(channels.begin(),channels.end(),[](int hp){return hp==0;});};
    PlayerBombEffect bomb;bomb.Start(ShotType::HomingAmulet,kCamera);
    assert(noDamage(bomb.ContactDamage(kCamera.position,1000)) && bomb.CancelVolumes().size()==1);
    bomb.Update(80.0F/60,kCamera);
    const auto sphere=bomb.Attacks()[0].volume;
    assert(bomb.ContactDamage(sphere.origin,0)[1]==8);
    // 进入清弹容差仍没有敌机接触；光晕距离与追踪目标距离均不能反馈累计爆破。
    const auto marginPoint=game_math::Add(sphere.origin,{sphere.radius+kBombCancelMargin/2,0,0});
    assert(noDamage(bomb.ContactDamage(marginPoint,0.01F)));
    assert(bomb.CancelVolumes()[0].IntersectsSphere(marginPoint,0.01F+kBombCancelMargin));
    bomb.Update(13.0F/60,kCamera,std::array<Vector3,1>{sphere.origin});
    assert(bomb.Frame()==93 && bomb.Attacks().size()==1 && bomb.Attacks()[0].damage==8);
    bomb.Update(177.0F/60,kCamera);
    for(const auto& attack:bomb.Attacks())assert(attack.damage==200);
    bomb.Update(1.0F/60,kCamera);
    assert(bomb.Attacks().empty() && bomb.CancelVolumes().empty()); // 第 271 帧仍有演出，不能继续扣血或清弹。
    bomb.Clear();assert(noDamage(bomb.ContactDamage({},1000)));
}

void TestSealOutwardAndSparkField() {
    PlayerBombEffect seal;seal.Start(ShotType::Needle,kCamera);
    seal.Update(61.0F/60,kCamera);
    assert(seal.Attacks().size()==6);
    for(const auto& attack:seal.Attacks()) {
        assert(game_math::DistanceSquared(attack.volume.origin,kCamera.position)<0.1F);
        assert(!attack.volume.IntersectsSphere({9,0,kCamera.position.z},0)); // 同轴远端也不得被贯穿截面提前伤害。
        assert(!attack.volume.IntersectsSphere({0,9,kCamera.position.z},0));
    }
    const auto early=seal.Attacks()[0].volume;
    seal.Update(60.0F/60,kCamera);
    const auto later=seal.Attacks()[0].volume;
    assert(later.origin.x>early.origin.x && later.halfExtents.y>early.halfExtents.y);
    auto moved=kCamera;moved.position={8,7,6};moved.target={0,0,0};
    seal.Update(0,moved);assert(game_math::DistanceSquared(seal.Attacks()[0].volume.origin,later.origin)==0);

    // 从中心和八个角落，向水平、竖直和斜向开火；零半径样本验证真实前半场覆盖，不借助目标尺寸。
    std::vector<Vector3> origins{{0,0,0}};
    for(float x:{-9.65F,9.65F})for(float y:{-9.65F,9.65F})for(float z:{-11.15F,11.15F})origins.push_back({x,y,z});
    for(const auto position:origins)for(const auto direction:std::array<Vector3,8>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},
                                                                               {0,0,1},{0,0,-1},{1,1,1},{1,2,-1}}}) {
        const auto axis=game_math::Normalize(direction);
        Camera3D camera{position,game_math::Add(position,axis),std::abs(axis.y)>0.99F ? Vector3{0,0,1} : Vector3{0,1,0},50,CAMERA_PERSPECTIVE};
        PlayerBombEffect spark;spark.Start(ShotType::IllusionLaser,camera);spark.Update(1.0F/60,camera);
        assert(spark.Attacks().size()==1);
        const auto& volume=spark.Attacks()[0].volume;
        assert(game_math::DistanceSquared(volume.origin,position)==0);
        assert(!volume.IntersectsSphere(game_math::Subtract(position,game_math::Scale(axis,2)),0));
        for(float x:{-10.0F,-5.0F,0.0F,5.0F,10.0F})for(float y:{-10.0F,-5.0F,0.0F,5.0F,10.0F})
            for(float z:{-11.5F,-5.0F,0.0F,5.0F,11.5F}) {
                const Vector3 point{x,y,z};const auto delta=game_math::Subtract(point,position);
                if(delta.x*axis.x+delta.y*axis.y+delta.z*axis.z>0.001F)assert(volume.IntersectsSphere(point,0));
            }
    }
}

void TestStarVolumeCoverage() {
    PlayerBombEffect stars;stars.Start(ShotType::MagicMissile,kCamera);stars.Update(181.0F/60,kCamera);
    assert(stars.Frame()==181 && stars.Attacks().size()==64);
    std::array<BombVolume,64> volumes{};
    for(int i=0;i<64;++i)volumes[i]=stars.Attacks()[i].volume;
    // 每轴 128 格，总计 2,097,152 个等体积小盒。仅在整个小盒都被某颗真实攻击球包含时计入覆盖。
    // 取离球心最远的顶点验证，故这是体积覆盖率下界，不能靠增大测试目标半径或只测中心点达标。
    constexpr int n=128;
    const Vector3 step{20.0F/n,20.0F/n,23.0F/n};
    std::size_t covered=0;
    for(int x=0;x<n;++x)for(int y=0;y<n;++y)for(int z=0;z<n;++z) {
        const auto& v=volumes[x/32+4*(y/32)+16*(z/32)];
        const Vector3 center{-10+(x+0.5F)*step.x,-10+(y+0.5F)*step.y,-11.5F+(z+0.5F)*step.z};
        const Vector3 far{v.origin.x+std::abs(center.x-v.origin.x)+step.x/2,
                          v.origin.y+std::abs(center.y-v.origin.y)+step.y/2,
                          v.origin.z+std::abs(center.z-v.origin.z)+step.z/2};
        if(v.IntersectsSphere(far,0))++covered;
    }
    // 统一为 20/448 后，原生 64 像素半径为 20/7；独立网格计算的完整小盒数为 1,216,000。
    // 不为维持旧 20/384 尺度的 80% 指标而放大原作攻击半径。
    assert(covered==1216000);
    // 从所有八个角落释放并转动视角，仍抵达相同攻击位置，因此覆盖保证不依赖初始视野/自机深度。
    for(float x:{-9.65F,9.65F})for(float y:{-9.65F,9.65F})for(float z:{-11.15F,11.15F}) {
        auto camera=kCamera;camera.position={x,y,z};camera.target=game_math::Add(camera.position,{1,2,3});
        PlayerBombEffect corner;corner.Start(ShotType::MagicMissile,camera);corner.Update(181.0F/60,camera);
        assert(corner.Attacks().size()==64);
        for(int i=0;i<64;++i)assert(game_math::DistanceSquared(corner.Attacks()[i].volume.origin,volumes[i].origin)<1e-8F);
    }
}
} // namespace
/// @brief 无图形上下文运行四 Bomb 的素材、时序和生命周期回归，断言失败即终止。
void RunPlayerBombTests() {
    TestNativeBombs();TestSealScheduleAndClear();TestBombConsumption();
    TestSpatialVolumes();TestAttackCadenceAndCoverage();TestContactBoundaries();TestSealOutwardAndSparkField();TestStarVolumeCoverage();
}

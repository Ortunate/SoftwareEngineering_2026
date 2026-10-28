// 本文件负责验证弹幕方向、敌人行动和 Boss 阶段行为。

#include "actor_fixture.hpp"
#include "gameplay/bullets/system.hpp"
#include "gameplay/bullets/patterns.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/actors/behavior.hpp"
#include "gameplay/actors/player.hpp"
#include "gameplay/arena.hpp"
#include "config/settings.hpp"
#include "stage/system.hpp"
#include "stage/catalog.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/boss_config.hpp"
#include "gameplay/bosses/rumia.hpp"
#include "reference_script.hpp"

#include <cassert>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <limits>
#include <string_view>
#include <numbers>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {
/// @brief 非符与符卡共享固定血量区间；击破、超时、下一组和退场均不能把符卡段错误铺满。
void TestBossHealthGroups() {
    auto bullets=std::make_unique<koumakyou::BulletSystem>();
    koumakyou::bosses::RumiaBoss boss;
    assert(boss.HealthBar().count==0);
    boss.StartBoss(koumakyou::stage1::RumiaMidbossConfig());
    auto bar=boss.HealthBar();
    assert(bar.count==1 && bar.remainingBars==0 && ColorToInt(bar.segments[0].color)==ColorToInt(WHITE));
    boss.health=boss.maxHealth/2;
    assert(std::abs(boss.HealthBar().segments[0].remaining-0.5F)<0.00001F);
    boss.StartBoss(koumakyou::stage1::RumiaFinalConfig());
    bar=boss.HealthBar();
    assert(bar.count==2 && bar.remainingBars==1);
    assert(ColorToInt(bar.segments[0].color)==ColorToInt(Color{239,138,152,255}) && ColorToInt(bar.segments[1].color)==ColorToInt(WHITE));
    assert(std::abs(bar.segments[0].capacity-900.0F/7000)<0.00001F);
    assert(std::abs(bar.segments[1].capacity-6100.0F/7000)<0.00001F);
    assert(bar.segments[0].remaining==bar.segments[0].capacity && bar.segments[1].remaining==bar.segments[1].capacity);
    boss.health=(900+6100.0F/2);
    bar=boss.HealthBar();
    assert(bar.segments[0].remaining==bar.segments[0].capacity);
    assert(std::abs(bar.segments[1].remaining-3050.0F/7000)<0.00001F);
    boss.health=900;
    const float spellWidth=boss.HealthBar().segments[0].remaining;
    assert(boss.AdvanceSkill() && boss.attack==2);
    bar=boss.HealthBar();
    assert(bar.segments[0].remaining==spellWidth && bar.segments[1].remaining==0);
    boss.health=450;
    assert(std::abs(boss.HealthBar().segments[0].remaining-450.0F/7000)<0.00001F);
    boss.health=0;
    assert(boss.AdvanceSkill() && boss.attack==3);
    // 真实主循环在 UpdateBoss 后还会执行碰撞；下一 120 Hz 更新步前必须已交接新生命，且准备期不能再次推进。
    assert(boss.health==7500 && boss.maxHealth==7500);
    assert(!boss.ShouldAdvanceFromDamage());
    boss.Update(1.0F/60,{},*bullets); // Sub19 首帧与交接时保持同一组总生命。
    bar=boss.HealthBar();
    assert(bar.remainingBars==0);
    assert(std::abs(bar.segments[0].capacity-800.0F/7500)<0.00001F);
    assert(std::abs(bar.segments[1].remaining-6700.0F/7500)<0.00001F);
    boss.health=800;
    assert(boss.AdvanceSkill() && boss.attack==4);
    bar=boss.HealthBar();
    assert(std::abs(bar.segments[0].remaining-800.0F/7500)<0.00001F && bar.segments[1].remaining==0);
    boss.health=0;
    assert(boss.AdvanceSkill() && boss.HealthBar().count==0);
    boss.StartBoss(koumakyou::stage1::RumiaFinalConfig());
    assert(boss.Update(35.1F,{},*bullets));
    assert(boss.AdvanceSkill() && boss.attack==2);
    assert(std::abs(boss.HealthBar().segments[0].remaining-900.0F/7000)<0.00001F);
}

/// @brief 验证预警不伤人、光束中段与端点胶囊碰撞、持续擦弹及清除，不依赖图形窗口。
void TestBeamContacts() {
    using namespace koumakyou;
    auto pool = std::make_unique<BulletSystem>();
    Player player;
    player.SetInvulnerability(0);
    const Vector3 target = player.Position();
    Bullet* beam = pool->SpawnBeam({target.x - 4, target.y, target.z}, {2, 0, 0}, 8, 0.2F, 1, 0.6F, SKYBLUE);
    assert(beam && !beam->Dangerous());
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None);
    pool->Update(0.99F);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None);
    pool->Update(0.02F);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::Hit && beam->active);
    player.SetInvulnerability(2);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None && beam->active);
    beam->position.y += 0.6F;
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::Graze);
    assert(player.ContactEnemyBullet(*beam) == PlayerContact::None);
    assert(std::abs(beam->DistanceSquaredTo({target.x + 4.3F, target.y + 0.6F, target.z}) - 0.09F) < 0.0001F);
    pool->Update(0.6F);
    assert(!beam->active);
    assert(!pool->SpawnBeam({}, {}, -1, 0.2F, 1, 1, WHITE));
    beam = pool->SpawnBeam({}, {0, 0, 1}, 5, 0.2F, 1, 1, WHITE);
    assert(beam);
    auto* own = pool->Spawn(BulletOwner::Player, {}, {}, 0.1F, WHITE, 0);
    pool->ClearEnemyBullets();
    assert(!beam->active && own->active);

    // 各层同向开口中没有采样点；高仰俯角也保持单位方向与正确张角。
    for (Vector3 axis : {Vector3{0, 0, 1}, Vector3{0, 1, 0}, Vector3{1, 2, 3}}) {
        const auto forward = pattern::Normalize(axis);
        int count = 0;
        pattern::ForEachOpenRingDirection(axis, 24, 0.6F, 0, 0.42F, [&](Vector3 direction) {
            assert(std::abs(BulletSystem::Length(direction) - 1) < 0.0001F);
            const float dot = forward.x * direction.x + forward.y * direction.y + forward.z * direction.z;
            assert(std::abs(dot - std::cos(0.6F)) < 0.0001F);
            ++count;
        });
        assert(count == 21);
    }
}

/// @brief 本体接触使用三维命中球和统一保护期；不依赖敌方是否免伤，也不触发擦弹或自行扣残机。
void TestEnemyBodyContacts() {
    using namespace koumakyou;
    Player player;player.Reset();const auto origin=player.Position();
    assert(!player.ContactEnemyBody(origin,0.5F)); // 开局保护覆盖本体。
    player.SetInvulnerability(0);
    for(const Vector3 offset:{Vector3{0.554F,0,0},{0,-0.554F,0},{0,0,0.554F},{0,0,-0.554F}})
        assert(player.ContactEnemyBody({origin.x+offset.x,origin.y+offset.y,origin.z+offset.z},0.5F));
    assert(!player.ContactEnemyBody({origin.x+0.557F,origin.y,origin.z},0.5F));
    assert(!player.ContactEnemyBody({origin.x+0.45F,origin.y+0.45F,origin.z+0.45F},0.5F));
    for(float invalid:{0.0F,-1.0F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        assert(!player.ContactEnemyBody(origin,invalid));
    assert(player.ContactEnemyBody(origin,0.5F) && player.RemainingLives()==4 && player.Bombs()==3);
    auto bullets=std::make_unique<BulletSystem>();
    for(const auto shot:{ShotType::HomingAmulet,ShotType::Needle,ShotType::MagicMissile,ShotType::IllusionLaser}) {
        player.Reset();player.ConfigureRun(3,3,false,shot);player.SetInvulnerability(0);
        assert(player.Update(0,{.bomb=true},*bullets,PlayerShotVisuals{},false));
        assert(!player.ContactEnemyBody(player.Position(),0.5F));
    }
    player.SetInvulnerability(0);assert(player.TakeHit());
    assert(!player.ContactEnemyBody(player.Position(),0.5F));
    player.Update(2.81F,{},*bullets,PlayerShotVisuals{},false);
    assert(player.ContactEnemyBody(player.Position(),0.5F));

    behavior::Actor enemy;enemy.Activate(testing::SpawnAt());enemy.damageable=false;
    assert(!enemy.CanTakeDamage() && enemy.CanContactPlayer());
    enemy.presentation.PrepareExit(0);assert(enemy.CanContactPlayer()); // 视觉淡出不能自行清除战斗本体。
    enemy.health=0;assert(!enemy.CanContactPlayer());
    enemy.health=10;enemy.active=false;assert(!enemy.CanContactPlayer());

    bosses::RumiaBoss boss;assert(!boss.CanContactPlayer());
    boss.StartBoss(koumakyou::stage1::RumiaMidbossConfig());assert(!boss.CanTakeDamage() && boss.CanContactPlayer());
    assert(boss.AdvanceSkill() && boss.active && !boss.CanContactPlayer()); // 超时退场仍有演员与占位生命。
    boss.StartBoss(koumakyou::stage1::RumiaFinalConfig());assert(boss.CanContactPlayer());
    boss.health=0;assert(!boss.CanContactPlayer());
}

}  // namespace

/// @brief 验证弹幕方向、生成及敌人和 Boss 行为；断言失败时终止进程。
void RunGameplayTests() {
    TestBossHealthGroups();
    TestBeamContacts();
    TestEnemyBodyContacts();
    using koumakyou::BulletOwner;
    using koumakyou::BulletSystem;
    const Vector3 forward = BulletSystem::DirectionFromYawPitch(0.0F, 0.0F);
    assert(std::abs(forward.x) < 0.0001F);
    assert(std::abs(forward.y) < 0.0001F);
    assert(std::abs(forward.z - 1.0F) < 0.0001F);

    const Vector3 right = BulletSystem::DirectionFromYawPitch(
        std::numbers::pi_v<float> * 0.5F, 0.0F);
    assert(std::abs(right.x - 1.0F) < 0.0001F);
    assert(std::abs(right.z) < 0.0001F);

    const Vector3 aimed = BulletSystem::DirectionTo(
        {0.0F, 0.0F, 0.0F}, {0.0F, 3.0F, 4.0F});
    assert(std::abs(aimed.y - 0.6F) < 0.0001F);
    assert(std::abs(aimed.z - 0.8F) < 0.0001F);

    BulletSystem bullets;
    koumakyou::pattern::ForEachSphereDirection(64, 0.0F,
        [&bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, {},
                          koumakyou::pattern::Scale(direction, 2.5F), 0.16F, RED,
                          koumakyou::stage1::kRedBullet);
        });
    assert(bullets.Pool().ActiveCount() == 64);

    bool hasPositiveX = false;
    bool hasNegativeX = false;
    bool hasPositiveY = false;
    bool hasNegativeY = false;
    bool hasPositiveZ = false;
    bool hasNegativeZ = false;
    bullets.Pool().ForEachActive([&](const koumakyou::Bullet& bullet) {
        assert(bullet.visual == koumakyou::stage1::kRedBullet);
        assert(std::abs(BulletSystem::Length(bullet.velocity) - 2.5F) < 0.0002F);
        hasPositiveX = hasPositiveX || bullet.velocity.x > 0.2F;
        hasNegativeX = hasNegativeX || bullet.velocity.x < -0.2F;
        hasPositiveY = hasPositiveY || bullet.velocity.y > 0.2F;
        hasNegativeY = hasNegativeY || bullet.velocity.y < -0.2F;
        hasPositiveZ = hasPositiveZ || bullet.velocity.z > 0.2F;
        hasNegativeZ = hasNegativeZ || bullet.velocity.z < -0.2F;
    });
    assert(hasPositiveX && hasNegativeX && hasPositiveY && hasNegativeY &&
           hasPositiveZ && hasNegativeZ);

    bullets.Reset();
    koumakyou::pattern::ForEachConeDirection({0.0F, 0.0F, 1.0F},
        2, 6, 0.45F, 0.0F, [&bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, {},
                          koumakyou::pattern::Scale(direction, 3.0F), 0.16F, BLUE,
                          koumakyou::stage1::kBlueBullet);
        });
    assert(bullets.Pool().ActiveCount() == 13);
    bullets.Pool().ForEachActive([](const koumakyou::Bullet& bullet) {
        assert(bullet.visual == koumakyou::stage1::kBlueBullet);
    });

    bullets.ClearEnemyBullets();
    assert(bullets.Pool().ActiveCount() == 0);

    // 回收仍保留原来的世界单位余量；Z 新增区间内的对象不能按旧立方体过早失活。
    for (int axis = 0; axis < 3; ++axis) {
        for (float sign : {-1.0F, 1.0F}) {
            const auto axisPosition = [axis, sign](float distance) -> Vector3 {
                if (axis == 0) return {sign * distance, 0.0F, 0.0F};
                if (axis == 1) return {0.0F, sign * distance, 0.0F};
                return {0.0F, 0.0F, sign * distance};
            };
            const float bulletLimit = axis == 2 ? 14.5F : 13.0F;
            auto* edgeBullet = bullets.Spawn(BulletOwner::Enemy,
                axisPosition(bulletLimit), {}, 0.1F, RED,
                koumakyou::stage1::kRedBullet);
            assert(edgeBullet);
            bullets.Update(0.0F);
            assert(edgeBullet->active);
            edgeBullet->position = axisPosition(bulletLimit + 0.01F);
            bullets.Update(0.0F);
            assert(!edgeBullet->active);

            bullets.Reset();
        }
    }
    // 使用原始帧节点核对道中首发（48 个等面积方向×3 速度层）与固定 24 秒上限。
    // 入场从 +Z 场外缓冲带开始；普通演员保留原 X/Z 路线与固定编队高度。
    const Vector3 entry=Vector3{0,0,11.4285717F};
    assert(entry.x==0 && entry.y==0 && entry.z>11 && entry.z<11.5F);
    const Vector3 exit=Vector3{0,0,-10};
    assert(exit.y==0 && std::abs(exit.z+10)<0.0001F);
    koumakyou::behavior::Actor entryEnemy;
    auto entrySpec=koumakyou::testing::SpawnAt(entry);entrySpec.pattern=koumakyou::stage1::kTurningFairy;entryEnemy.Activate(entrySpec);
    entryEnemy.Update(2.0F/60,{},bullets);
    assert(entryEnemy.active && entryEnemy.position.z<entry.z && std::abs(entryEnemy.position.y)<0.0001F);
    // 出生点直接保存在世界坐标中，高度不经过二维投影或逆映射。
    koumakyou::behavior::Actor elevatedEnemy;entrySpec.position={entry.x,2,entry.z};elevatedEnemy.Activate(entrySpec);
    assert(std::abs(elevatedEnemy.position.y-2)<0.0001F);
    // Normal 妖精镜像只反转 X 位移；直行和转弯后左右轨迹保持对称，Z 与脚本时钟一致。
    koumakyou::stage1::ReferenceScript left,mirrored,dark;
    left.Reset(0,{60,-32,0},32,1); mirrored.Reset(0,{324,-32,0},32,1,true);
    bullets.Reset();
    for(int frame=0;frame<180;++frame) {
        left.Update(1.0F/60,{},bullets); mirrored.Update(1.0F/60,{},bullets);
        assert(std::abs(left.sourcePosition.x+mirrored.sourcePosition.x-384)<0.001F);
        assert(std::abs(left.sourcePosition.y-mirrored.sourcePosition.y)<0.001F);
        assert(std::abs(left.Position().x+mirrored.Position().x)<0.001F);
        assert(std::abs(left.Position().z-mirrored.Position().z)<0.001F);
        assert(left.Position().y==0 && mirrored.Position().y==0);
    }
    assert(bullets.Pool().ActiveCount()==0);
    dark.Reset(3,{192,-32,0},256,1);
    constexpr Vector3 raisedPlayer{2,3,-6.5F};
    for(int frame=0;frame<70;++frame)dark.Update(1.0F/60,raisedPlayer,bullets);
    assert(bullets.Pool().ActiveCount()==0);
    dark.Update(1.0F/60,raisedPlayer,bullets);
    assert(bullets.Pool().ActiveCount()==7);
    int aimedCenters=0;
    bullets.Pool().ForEachActive([&](const koumakyou::Bullet& b) {
        assert(!b.Dangerous() && std::abs(b.spawnVisualSeconds-10.0F/60)<0.0001F);
        assert(b.position.y==0 && b.velocity.y>0);
        assert(b.rotationAxis.x==0 && b.rotationAxis.y==1 && b.rotationAxis.z==0);
        const Vector3 toPlayer=koumakyou::BulletSystem::DirectionTo(b.position,raisedPlayer);
        const Vector3 direction=koumakyou::pattern::Normalize(b.velocity);
        const float alignment=direction.x*toPlayer.x+direction.y*toPlayer.y+direction.z*toPlayer.z;
        if(alignment>0.99999F)++aimedCenters;
    });
    assert(aimedCenters==1); // 七向扇形的中心弹必须穿过玩家的 X/Z 位置与 Y 高度。
    bullets.Reset(); dark.Reset(4,{192,-32,0},256,1);
    for(int frame=0;frame<220;++frame)dark.Update(1.0F/60,{},bullets);
    assert(bullets.Pool().ActiveCount()==0); // Sub4 在 Normal 是无攻击的后段妖精。

    koumakyou::bosses::RumiaBoss boss;
    boss.StartBoss(koumakyou::stage1::RumiaMidbossConfig());
    for (int frame=0;frame<160;++frame) assert(!boss.Update(1.0F/60,{},bullets));
    assert(bullets.Pool().ActiveCount()==0);
    boss.Update(1.0F/60,{},bullets);
    assert(bullets.Pool().ActiveCount()==144); // 48 个球面方向、3 速度层。
    assert(std::abs(boss.TimeLimit()-24)<0.001F && boss.SpellName()==nullptr);
    bullets.Reset();
    for (int frame=161;frame<1441;++frame) boss.Update(1.0F/60,{},bullets);
    assert(boss.AdvanceSkill()); // 超时后保留原作 60 帧退场。
    for (int frame=0;frame<61;++frame) boss.Update(1.0F/60,{},bullets);
    assert(!boss.AdvanceSkill() && !boss.active);
    boss.StartBoss(koumakyou::stage1::RumiaFinalConfig());
    bullets.Reset();
    for (int frame=0;frame<112;++frame) boss.Update(1.0F/60,{0,0,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==0);
    boss.Update(1.0F/60,{0,0,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==10); // 首次红弹保持十个速度层和真实玩家中心瞄准。
    bullets.Pool().ForEachActive([](const koumakyou::Bullet& bullet) {
        const Vector3 expected=koumakyou::BulletSystem::DirectionTo(bullet.position,{0,0,-6.5F});
        const Vector3 actual=koumakyou::pattern::Normalize(bullet.velocity);
        assert(std::abs(expected.x-actual.x)<0.0001F&&std::abs(expected.y-actual.y)<0.0001F&&std::abs(expected.z-actual.z)<0.0001F);
    });
    assert(std::abs(boss.TimeLimit()-35)<0.001F);
    assert(boss.AdvanceSkill() && std::string_view(boss.SpellName())=="夜符「Night Bird」");
    bullets.Reset();
    for (int frame=0;frame<122;++frame) boss.Update(1.0F/60,{0,3,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==0);
    boss.Update(1.0F/60,{0,3,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==1);
    assert(std::abs(boss.TimeLimit()-25)<0.001F);
    assert(boss.AdvanceSkill() && boss.SpellName()==nullptr);
    bullets.Reset();
    for (int frame=0;frame<261;++frame) boss.Update(1.0F/60,raisedPlayer,bullets);
    int beams=0;
    bullets.Pool().ForEachActive([&](const koumakyou::Bullet& b){
        if(b.beamLength<=0)return;
        ++beams;
        const Vector3 expected=koumakyou::BulletSystem::DirectionTo(b.position,raisedPlayer);
        assert(b.beamDirection.y>0 && b.beamDirection.z<0);
        assert(std::abs(b.beamDirection.x-expected.x)<0.0001F);
        assert(std::abs(b.beamDirection.y-expected.y)<0.0001F);
        assert(std::abs(b.beamDirection.z-expected.z)<0.0001F);
    });
    assert(beams==6 && std::abs(boss.TimeLimit()-30)<0.001F);
    assert(boss.AdvanceSkill() && std::string_view(boss.SpellName())=="暗符「Demarcation」");
    bullets.Reset();
    for (int frame=0;frame<121;++frame) boss.Update(1.0F/60,raisedPlayer,bullets);
    assert(bullets.Pool().ActiveCount()==98); // rank=16 时每层基础 48 方向加 1，双球壳共 98 弹。
    bullets.Pool().ForEachActive([](const koumakyou::Bullet& b){
        assert(std::abs(b.turnDelaySeconds-40.0F/60)<0.0001F);
        assert(std::abs(std::abs(b.turnAngle)-std::numbers::pi_v<float>/2)<0.0001F);
        assert(std::abs(koumakyou::BulletSystem::Length(b.rotationAxis)-1)<0.0001F);
    });
    // 实际延迟转向绕各方向自己的切向轴完成 90°，保持球面切向运动。
    bullets.Update(1.1F,{},raisedPlayer);
    assert(bullets.Pool().ActiveCount()==98);
    bullets.Pool().ForEachActive([](const koumakyou::Bullet& b){
        assert(b.turnApplied && !b.turnAimed);
        const Vector3 before=koumakyou::pattern::Normalize(b.baseVelocity);
        const Vector3 after=koumakyou::pattern::Normalize(b.velocity);
        assert(std::abs(before.x*after.x+before.y*after.y+before.z*after.z)<0.0001F);
        const Vector3 cross{before.y*after.z-before.z*after.y,before.z*after.x-before.x*after.z,
                            before.x*after.y-before.y*after.x};
        assert((cross.x*b.rotationAxis.x+cross.y*b.rotationAxis.y+cross.z*b.rotationAxis.z)*b.turnAngle>0);
    });
}

// 本文件负责验证弹幕方向、敌人行动和 Boss 阶段行为。

#include "gameplay/bullets/system.hpp"
#include "gameplay/bullets/patterns.hpp"
#include "gameplay/actors/boss.hpp"
#include "gameplay/actors/enemy.hpp"
#include "gameplay/actors/player.hpp"
#include "gameplay/arena.hpp"
#include "config/settings.hpp"
#include "stage/system.hpp"
#include "stage/catalog.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage1/enemies.hpp"
#include "stages/stage1/boss.hpp"

#include <cassert>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string_view>
#include <numbers>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {
/// @brief 非符与符卡共享固定血量区间；击破、超时、下一组和退场均不能把符卡段错误铺满。
void TestBossHealthGroups() {
    using koumakyou::stage1::kReferenceHealthUnit;
    using koumakyou::BossHealthKind;
    auto bullets=std::make_unique<koumakyou::BulletSystem>();
    koumakyou::stage1::RumiaBoss boss;
    assert(boss.HealthBar().count==0);
    boss.StartMidboss();
    auto bar=boss.HealthBar();
    assert(bar.count==1 && bar.remainingBars==0 && bar.segments[0].kind==BossHealthKind::NonSpell);
    boss.health=boss.maxHealth/2;
    assert(std::abs(boss.HealthBar().segments[0].remaining-0.5F)<0.00001F);
    boss.StartFinalBoss();
    bar=boss.HealthBar();
    assert(bar.count==2 && bar.remainingBars==1);
    assert(bar.segments[0].kind==BossHealthKind::Spell && bar.segments[1].kind==BossHealthKind::NonSpell);
    assert(std::abs(bar.segments[0].capacity-900.0F/7000)<0.00001F);
    assert(std::abs(bar.segments[1].capacity-6100.0F/7000)<0.00001F);
    assert(bar.segments[0].remaining==bar.segments[0].capacity && bar.segments[1].remaining==bar.segments[1].capacity);
    boss.health=(900+6100.0F/2)/kReferenceHealthUnit;
    bar=boss.HealthBar();
    assert(bar.segments[0].remaining==bar.segments[0].capacity);
    assert(std::abs(bar.segments[1].remaining-3050.0F/7000)<0.00001F);
    boss.health=900/kReferenceHealthUnit;
    const float spellWidth=boss.HealthBar().segments[0].remaining;
    assert(boss.AdvancePhase() && boss.phase==2);
    bar=boss.HealthBar();
    assert(bar.segments[0].remaining==spellWidth && bar.segments[1].remaining==0);
    boss.health=450/kReferenceHealthUnit;
    assert(std::abs(boss.HealthBar().segments[0].remaining-450.0F/7000)<0.00001F);
    boss.health=0;
    assert(boss.AdvancePhase() && boss.phase==3);
    // 真实主循环在 UpdateBoss 后还会执行碰撞；下一 60 Hz 帧前必须已交接新生命，且准备期不能再次推进。
    assert(boss.health==7500/kReferenceHealthUnit && boss.maxHealth==7500/kReferenceHealthUnit);
    assert(!boss.ShouldAdvanceFromDamage());
    boss.Update(1.0F/60,{},*bullets); // Sub19 首帧与交接时保持同一组总生命。
    bar=boss.HealthBar();
    assert(bar.remainingBars==0);
    assert(std::abs(bar.segments[0].capacity-800.0F/7500)<0.00001F);
    assert(std::abs(bar.segments[1].remaining-6700.0F/7500)<0.00001F);
    boss.health=800/kReferenceHealthUnit;
    assert(boss.AdvancePhase() && boss.phase==4);
    bar=boss.HealthBar();
    assert(std::abs(bar.segments[0].remaining-800.0F/7500)<0.00001F && bar.segments[1].remaining==0);
    boss.health=0;
    assert(boss.AdvancePhase() && boss.HealthBar().count==0);
    boss.StartFinalBoss();
    assert(boss.Update(35.1F,{},*bullets));
    assert(boss.AdvancePhase() && boss.phase==2);
    assert(std::abs(boss.HealthBar().segments[0].remaining-900.0F/7000)<0.00001F);
}

/// @brief 验证暗色妖精的 Z 轴往返、单次稀疏球面散射以及跨帧阶段切换。
void TestDarkFairyBehavior() {
    using koumakyou::Bullet;
    using koumakyou::BulletOwner;
    using koumakyou::BulletSystem;
    using koumakyou::stage1::DarkFairyEnemy;
    const Vector3 start{2.0F, -1.0F, 8.0F};
    const float travelDistance = koumakyou::kArenaHalfExtents.z * 2.0F / 5.0F;
    const float approachTime = travelDistance / 1.7F;
    const Vector3 turn{start.x, start.y, start.z - travelDistance};
    auto storage = std::make_unique<BulletSystem>();
    BulletSystem& bullets = *storage;
    DarkFairyEnemy fairy({start});

    fairy.Update(approachTime - 0.01F, {}, bullets);
    assert(fairy.active && fairy.position.x == start.x && fairy.position.y == start.y);
    assert(fairy.position.z > turn.z && fairy.position.z < start.z);
    assert(bullets.Pool().ActiveCount() == 0);
    fairy.Update(0.02F, {}, bullets);
    assert(fairy.active && std::abs(fairy.position.z - turn.z) < 0.0001F);
    assert(bullets.Pool().ActiveCount() == 18);

    std::vector<Vector3> directions;
    Vector3 velocitySum{};
    std::array<bool, 6> axisSigns{};
    int planarDirections = 0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        assert(bullet.owner == BulletOwner::Enemy && bullet.visual == koumakyou::stage1::kRedBullet);
        assert(bullet.position.x == turn.x && bullet.position.y == turn.y);
        assert(std::abs(bullet.position.z - turn.z) < 0.0001F);
        assert(std::abs(BulletSystem::Length(bullet.velocity) - 3.5F) < 0.0001F);
        const Vector3 direction = koumakyou::pattern::Normalize(bullet.velocity);
        // 图中疏散环的三维扩展须无重叠方向，不能退化为平面环或密集同向弹束。
        for (Vector3 previous : directions) {
            const float dot = direction.x * previous.x + direction.y * previous.y +
                              direction.z * previous.z;
            assert(dot <= std::cos(std::numbers::pi_v<float> / 4.0F) + 0.0001F);
        }
        directions.push_back(direction);
        if (std::abs(direction.z) < 0.0001F) ++planarDirections;
        velocitySum = koumakyou::pattern::Add(velocitySum, bullet.velocity);
        if (direction.x > 0.1F) axisSigns[0] = true;
        if (direction.x < -0.1F) axisSigns[1] = true;
        if (direction.y > 0.1F) axisSigns[2] = true;
        if (direction.y < -0.1F) axisSigns[3] = true;
        if (direction.z > 0.1F) axisSigns[4] = true;
        if (direction.z < -0.1F) axisSigns[5] = true;
    });
    for (bool populated : axisSigns) assert(populated);
    assert(planarDirections == 8);
    assert(BulletSystem::Length(velocitySum) < 0.0001F);

    fairy.Update(0.5F, {}, bullets);
    assert(fairy.position.z == turn.z && fairy.active);
    fairy.Update(0.5F, {}, bullets);
    assert(fairy.position.z > turn.z && fairy.position.z < start.z);
    assert(fairy.position.x == start.x && fairy.position.y == start.y);
    assert(bullets.Pool().ActiveCount() == 18);
    fairy.Update(approachTime, {}, bullets);
    assert(!fairy.active && fairy.position.x == start.x && fairy.position.y == start.y);
    assert(fairy.position.z == start.z && bullets.Pool().ActiveCount() == 18);

    // 单帧跨过前进、停留和返回时仍只发射一次，发弹点不能错误地落在已返回的位置。
    bullets.Reset();
    DarkFairyEnemy coarse({start});
    coarse.Update(approachTime * 2.0F + 2.0F, {9.0F, 9.0F, 9.0F}, bullets);
    assert(!coarse.active && coarse.position.z == start.z);
    assert(bullets.Pool().ActiveCount() == 18);
    std::size_t directionIndex = 0;
    bullets.Pool().ForEachActive([&](const Bullet& bullet) {
        assert(std::abs(bullet.position.z - turn.z) < 0.0001F);
        const Vector3 direction = koumakyou::pattern::Normalize(bullet.velocity);
        const Vector3 expected = directions[directionIndex++];
        assert(std::abs(direction.x - expected.x) < 0.0001F);
        assert(std::abs(direction.y - expected.y) < 0.0001F);
        assert(std::abs(direction.z - expected.z) < 0.0001F);
    });

    // 等待时间属于实例配置，零等待仍在转折点发射一次，然后立即开始返回。
    bullets.Reset();
    DarkFairyEnemy immediate({start, 0.0F});
    immediate.Update(approachTime + 0.5F, {}, bullets);
    assert(immediate.active && immediate.position.z > turn.z);
    assert(bullets.Pool().ActiveCount() == 18);
    immediate.Update(approachTime, {}, bullets);
    assert(!immediate.active && bullets.Pool().ActiveCount() == 18);
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

}  // namespace

/// @brief 验证弹幕方向、生成及敌人和 Boss 行为；断言失败时终止进程。
void RunGameplayTests() {
    TestBossHealthGroups();
    TestBeamContacts();
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

            const float enemyLimit = axis == 2 ? 13.5F : 12.0F;
            koumakyou::stage1::FairyEnemy edgeEnemy(
                {axisPosition(enemyLimit)});
            edgeEnemy.Update(0.0F, {}, bullets);
            assert(edgeEnemy.active);
            edgeEnemy.origin = axisPosition(enemyLimit + 0.01F);
            edgeEnemy.Update(0.0F, {}, bullets);
            assert(!edgeEnemy.active);
            bullets.Reset();
        }
    }
    koumakyou::stage1::FairyEnemy fairy({{0.0F, 0.0F, 5.0F}});
    const Vector3 fairyMuzzle = fairy.MuzzlePosition({0.0F, 0.0F, 0.0F});
    assert(std::abs(fairyMuzzle.z - (5.0F - fairy.hitRadius * 0.55F)) <
           0.0001F);
    fairy.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 0);
    // 覆盖旧首次攻击和后续攻击时刻，避免仅延后首发而未移除攻击。
    for (int frame = 0; frame < 8; ++frame) fairy.Update(0.5F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 0);
    assert(fairy.health > 0.0F && fairy.health == fairy.maxHealth);
    // 使用真实玩家射击验证低火力直射弹与满火力追踪弹均能一发耗尽妖精血量。
    for (int power : {0, koumakyou::Player::kMaxPower}) {
        koumakyou::Player player;
        player.AdjustPower(power);
        player.Update(0.0F, {.fire = true}, bullets,
                      koumakyou::stage1::kPlayerGoldCard,
                      koumakyou::stage1::kPlayerCyanCard, true);
        assert(bullets.Pool().ActiveCount() > 0);
        bullets.Pool().ForEachActive([&fairy](const koumakyou::Bullet& bullet) {
            assert(bullet.damage >= fairy.health);
        });
        bullets.Reset();
    }
    bullets.Reset();

    TestDarkFairyBehavior();

    koumakyou::stage1::FairyEnemy helixFairy(
        {{0.0F, 0.0F, 5.0F}, koumakyou::stage1::FairyParams::Movement::Helix});
    helixFairy.Update(0.9F, {}, bullets);
    assert(helixFairy.position.x != fairy.position.x);
    assert(bullets.Pool().ActiveCount() == 0);
    bullets.Reset();
    koumakyou::stage1::FairyEnemy crossingFairy(
        {{-9.0F, 0.0F, 5.0F}, koumakyou::stage1::FairyParams::Movement::CrossFromLeft});
    crossingFairy.Update(0.9F, {}, bullets);
    assert(crossingFairy.position.x > -9.0F);
    assert(crossingFairy.visual == fairy.visual);
    assert(bullets.Pool().ActiveCount() == 0);
    bullets.Reset();

    koumakyou::stage1::OrbEnemy orb({{0.0F, 0.0F, 5.0F}});
    orb.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 6);
    bullets.Reset();

    // 使用新典原始帧节点核对道中首发（16 向×3 速度层）与固定 24 秒上限。
    // 入场从 +Z 场外缓冲带开始，源纵向贯穿战场深度，高度不参与二维出怪轨迹。
    const Vector3 entry=koumakyou::stage1::ReferencePosition({192,-32,0});
    assert(entry.x==0 && entry.y==0 && entry.z>11 && entry.z<11.5F);
    const Vector3 exit=koumakyou::stage1::ReferencePosition({192,448,0});
    assert(exit.y==0 && std::abs(exit.z+10)<0.0001F);
    koumakyou::stage1::ReferenceEnemy entryEnemy({entry});
    entryEnemy.Update(2.0F/60,{},bullets);
    assert(entryEnemy.active && entryEnemy.position.z<entry.z && entryEnemy.position.y==0);
    // 带高度的世界出生点经过演员的逆映射后仍保留该高度，避免构造路径混用旧坐标系。
    koumakyou::stage1::ReferenceEnemy elevatedEnemy({{entry.x,2,entry.z}});
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

    koumakyou::stage1::RumiaBoss boss;
    boss.StartMidboss();
    for (int frame=0;frame<160;++frame) assert(!boss.Update(1.0F/60,{},bullets));
    assert(bullets.Pool().ActiveCount()==0);
    boss.Update(1.0F/60,{},bullets);
    assert(bullets.Pool().ActiveCount()==48);
    assert(std::abs(boss.TimeLimit()-24)<0.001F && boss.SpellName()==nullptr);
    bullets.Reset();
    for (int frame=161;frame<1441;++frame) boss.Update(1.0F/60,{},bullets);
    assert(boss.AdvancePhase()); // 超时后保留原作 60 帧退场。
    for (int frame=0;frame<61;++frame) boss.Update(1.0F/60,{},bullets);
    assert(!boss.AdvancePhase() && !boss.active);
    boss.StartFinalBoss();
    bullets.Reset();
    for (int frame=0;frame<112;++frame) boss.Update(1.0F/60,{0,0,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==0);
    boss.Update(1.0F/60,{0,0,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==10); // 首次红弹为十个速度层，而非环/锥。
    assert(std::abs(boss.TimeLimit()-35)<0.001F);
    assert(boss.AdvancePhase() && std::string_view(boss.SpellName())=="夜符「Night Bird」");
    bullets.Reset();
    for (int frame=0;frame<122;++frame) boss.Update(1.0F/60,{0,3,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==0);
    boss.Update(1.0F/60,{0,3,-6.5F},bullets);
    assert(bullets.Pool().ActiveCount()==1);
    assert(std::abs(boss.TimeLimit()-25)<0.001F);
    assert(boss.AdvancePhase() && boss.SpellName()==nullptr);
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
    assert(boss.AdvancePhase() && std::string_view(boss.SpellName())=="暗符「Demarcation」");
    bullets.Reset();
    for (int frame=0;frame<121;++frame) boss.Update(1.0F/60,raisedPlayer,bullets);
    assert(bullets.Pool().ActiveCount()==34); // rank=16 时原作 -3…6 的弹数修正增加 1 向。
    bullets.Pool().ForEachActive([](const koumakyou::Bullet& b){
        assert(std::abs(b.turnDelaySeconds-40.0F/60)<0.0001F);
        assert(std::abs(std::abs(b.turnAngle)-std::numbers::pi_v<float>/2)<0.0001F);
        assert(b.rotationAxis.x==0 && b.rotationAxis.y==1 && b.rotationAxis.z==0);
    });
    // 实际延迟转向必须在 X/Z 内完成 90°，Y 坡度保持不变，避免只修正出生位置。
    bullets.Update(1.1F,{},raisedPlayer);
    assert(bullets.Pool().ActiveCount()==34);
    bullets.Pool().ForEachActive([](const koumakyou::Bullet& b){
        assert(b.turnApplied && !b.turnAimed);
        const Vector3 before=koumakyou::pattern::Normalize(b.baseVelocity);
        const Vector3 after=koumakyou::pattern::Normalize(b.velocity);
        assert(std::abs(before.y-after.y)<0.0001F);
        assert(std::abs(before.x*after.x+before.z*after.z)<0.0001F);
        assert((before.z*after.x-before.x*after.z)*b.turnAngle>0);
    });
}

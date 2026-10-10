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
/// @brief 对比原脚本与扩展模式的逐帧轨迹、出生弹数和阶段回调；扩展不得污染 Z、ECL 随机数或时钟。
void TestSpatialStageTimeline() {
    using namespace redmist::stage1;
    auto baselineBullets=std::make_unique<redmist::BulletSystem>();
    auto spatialBullets=std::make_unique<redmist::BulletSystem>();
    for(int sub: {0,1,2,3,4,9,14}) {
        ReferenceScript baseline,spatial;
        const Vector3 start{192,sub==14 ? 96.0F : -32.0F,0};
        const float hp=sub==9 ? 6000 : sub==14 ? 7000 : 256;
        baseline.Reset(sub,start,hp,31);
        spatial.Reset(sub,start,hp,31,false,ReferenceGeometry::Spatial);
        bool movedX=false,movedY=false,volumetric=false;
        int callbacks=0;
        for(int frame=0;frame<(sub>=5 ? 9000 : 400);++frame) {
            baselineBullets->Reset(); spatialBullets->Reset();
            baseline.Update(1.0F/60,{2,3,-6.5F},*baselineBullets);
            spatial.Update(1.0F/60,{2,3,-6.5F},*spatialBullets);
            assert(baseline.sourcePosition.x==spatial.sourcePosition.x);
            assert(baseline.sourcePosition.y==spatial.sourcePosition.y);
            assert(baseline.sourcePosition.z==spatial.sourcePosition.z);
            assert(baseline.Position().z==spatial.Position().z);
            assert(baseline.phaseFrames==spatial.phaseFrames && baseline.pendingCallback==spatial.pendingCallback);
            assert(baseline.soundEvents==spatial.soundEvents && baseline.dropEvents==spatial.dropEvents);
            assert(baseline.health==spatial.health && baseline.finished==spatial.finished);
            assert(baselineBullets->Pool().ActiveCount()==spatialBullets->Pool().ActiveCount());
            movedX|=std::abs(baseline.Position().x-spatial.Position().x)>0.1F;
            movedY|=std::abs(baseline.Position().y-spatial.Position().y)>1;
            assert(std::abs(spatial.Position().x)<=9 && std::abs(spatial.Position().y)<=8.5F);
            std::vector<Vector3> directions;
            spatialBullets->Pool().ForEachActive([&](const redmist::Bullet& bullet) {
                if(bullet.beamLength==0)directions.push_back(redmist::pattern::Normalize(bullet.velocity));
            });
            // 三个方向的混合积非零，才能证明弹幕不只是把一个平面整体倾斜。
            if(directions.size()>=3)for(std::size_t n=2;n<directions.size();++n) {
                const Vector3 normal=redmist::pattern::Cross(directions[0],directions[1]);
                if(std::abs(normal.x*directions[n].x+normal.y*directions[n].y+normal.z*directions[n].z)>0.01F)
                    volumetric=true;
            }
            if(baseline.pendingCallback>=0) {
                ++callbacks;
                const Vector3 before=spatial.Position();
                baseline.Transition(); spatial.Transition();
                const Vector3 after=spatial.Position();
                assert(before.x==after.x && before.y==after.y && before.z==after.z);
            }
            if(baseline.finished)break;
        }
        assert(movedX && movedY);
        if(sub==3 || sub>=5)assert(volumetric);
        if(sub==9)assert(callbacks==1 && spatial.finished);
        if(sub==14)assert(callbacks==5 && spatial.finished);
    }
    // 几何相位不受渲染帧率影响；30/60/240 Hz 推进相同时间应得到同一扩展位置。
    std::array<Vector3,3> results{};
    int index=0;
    for(int fps: {30,60,240}) {
        ReferenceScript script;
        script.Reset(3,{192,-32,0},256,72,false,ReferenceGeometry::Spatial);
        spatialBullets->Reset();
        for(int frame=0;frame<fps*2;++frame)script.Update(1.0F/fps,{},*spatialBullets);
        results[index++]=script.Position();
    }
    for(Vector3 result:results) {
        assert(std::abs(result.x-results[0].x)<0.0001F);
        assert(std::abs(result.y-results[0].y)<0.0001F);
        assert(std::abs(result.z-results[0].z)<0.0001F);
    }
    ReferenceScript left,right;
    left.Reset(0,{60,-32,0},32,72,false,ReferenceGeometry::Spatial);
    right.Reset(0,{324,-32,0},32,72,true,ReferenceGeometry::Spatial);
    for(int frame=0;frame<180;++frame) {
        left.Update(1.0F/60,{},*spatialBullets); right.Update(1.0F/60,{},*spatialBullets);
        assert(std::abs(left.Position().x+right.Position().x)<0.001F);
        assert(left.Position().y==right.Position().y && left.Position().z==right.Position().z);
    }
}
/// @brief 验证暗色妖精的 Z 轴往返、单次稀疏球面散射以及跨帧阶段切换。
void TestDarkFairyBehavior() {
    using redmist::Bullet;
    using redmist::BulletOwner;
    using redmist::BulletSystem;
    using redmist::stage1::DarkFairyEnemy;
    const Vector3 start{2.0F, -1.0F, 8.0F};
    const float travelDistance = redmist::kArenaHalfExtents.z * 2.0F / 5.0F;
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
        assert(bullet.owner == BulletOwner::Enemy && bullet.visual == redmist::stage1::kRedBullet);
        assert(bullet.position.x == turn.x && bullet.position.y == turn.y);
        assert(std::abs(bullet.position.z - turn.z) < 0.0001F);
        assert(std::abs(BulletSystem::Length(bullet.velocity) - 3.5F) < 0.0001F);
        const Vector3 direction = redmist::pattern::Normalize(bullet.velocity);
        // 图中疏散环的三维扩展须无重叠方向，不能退化为平面环或密集同向弹束。
        for (Vector3 previous : directions) {
            const float dot = direction.x * previous.x + direction.y * previous.y +
                              direction.z * previous.z;
            assert(dot <= std::cos(std::numbers::pi_v<float> / 4.0F) + 0.0001F);
        }
        directions.push_back(direction);
        if (std::abs(direction.z) < 0.0001F) ++planarDirections;
        velocitySum = redmist::pattern::Add(velocitySum, bullet.velocity);
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
        const Vector3 direction = redmist::pattern::Normalize(bullet.velocity);
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
    using namespace redmist;
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
    TestBeamContacts();
    using redmist::BulletOwner;
    using redmist::BulletSystem;
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
    redmist::pattern::ForEachSphereDirection(64, 0.0F,
        [&bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, {},
                          redmist::pattern::Scale(direction, 2.5F), 0.16F, RED,
                          redmist::stage1::kRedBullet);
        });
    assert(bullets.Pool().ActiveCount() == 64);

    bool hasPositiveX = false;
    bool hasNegativeX = false;
    bool hasPositiveY = false;
    bool hasNegativeY = false;
    bool hasPositiveZ = false;
    bool hasNegativeZ = false;
    bullets.Pool().ForEachActive([&](const redmist::Bullet& bullet) {
        assert(bullet.visual == redmist::stage1::kRedBullet);
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
    redmist::pattern::ForEachConeDirection({0.0F, 0.0F, 1.0F},
        2, 6, 0.45F, 0.0F, [&bullets](Vector3 direction) {
            bullets.Spawn(BulletOwner::Enemy, {},
                          redmist::pattern::Scale(direction, 3.0F), 0.16F, BLUE,
                          redmist::stage1::kBlueBullet);
        });
    assert(bullets.Pool().ActiveCount() == 13);
    bullets.Pool().ForEachActive([](const redmist::Bullet& bullet) {
        assert(bullet.visual == redmist::stage1::kBlueBullet);
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
                redmist::stage1::kRedBullet);
            assert(edgeBullet);
            bullets.Update(0.0F);
            assert(edgeBullet->active);
            edgeBullet->position = axisPosition(bulletLimit + 0.01F);
            bullets.Update(0.0F);
            assert(!edgeBullet->active);

            const float enemyLimit = axis == 2 ? 13.5F : 12.0F;
            redmist::stage1::FairyEnemy edgeEnemy(
                {axisPosition(enemyLimit)});
            edgeEnemy.Update(0.0F, {}, bullets);
            assert(edgeEnemy.active);
            edgeEnemy.origin = axisPosition(enemyLimit + 0.01F);
            edgeEnemy.Update(0.0F, {}, bullets);
            assert(!edgeEnemy.active);
            bullets.Reset();
        }
    }
    redmist::stage1::FairyEnemy fairy({{0.0F, 0.0F, 5.0F}});
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
    for (int power : {0, redmist::Player::kMaxPower}) {
        redmist::Player player;
        player.AdjustPower(power);
        player.Update(0.0F, {.fire = true}, bullets,
                      redmist::stage1::kPlayerGoldCard,
                      redmist::stage1::kPlayerCyanCard, true);
        assert(bullets.Pool().ActiveCount() > 0);
        bullets.Pool().ForEachActive([&fairy](const redmist::Bullet& bullet) {
            assert(bullet.damage >= fairy.health);
        });
        bullets.Reset();
    }
    bullets.Reset();

    TestDarkFairyBehavior();
    TestSpatialStageTimeline();

    redmist::stage1::FairyEnemy helixFairy(
        {{0.0F, 0.0F, 5.0F}, redmist::stage1::FairyParams::Movement::Helix});
    helixFairy.Update(0.9F, {}, bullets);
    assert(helixFairy.position.x != fairy.position.x);
    assert(bullets.Pool().ActiveCount() == 0);
    bullets.Reset();
    redmist::stage1::FairyEnemy crossingFairy(
        {{-9.0F, 0.0F, 5.0F}, redmist::stage1::FairyParams::Movement::CrossFromLeft});
    crossingFairy.Update(0.9F, {}, bullets);
    assert(crossingFairy.position.x > -9.0F);
    assert(crossingFairy.visual == fairy.visual);
    assert(bullets.Pool().ActiveCount() == 0);
    bullets.Reset();

    redmist::stage1::OrbEnemy orb({{0.0F, 0.0F, 5.0F}});
    orb.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 6);
    bullets.Reset();

    // 使用新典原始帧节点核对道中首发（16 向×3 速度层）与固定 24 秒上限。
    // 入场从 +Z 场外缓冲带开始，源纵向贯穿战场深度，高度不参与二维出怪轨迹。
    const Vector3 entry=redmist::stage1::ReferencePosition({192,-32,0});
    assert(entry.x==0 && entry.y==0 && entry.z>11 && entry.z<11.5F);
    const Vector3 exit=redmist::stage1::ReferencePosition({192,448,0});
    assert(exit.y==0 && std::abs(exit.z+10)<0.0001F);
    redmist::stage1::ReferenceEnemy entryEnemy({entry});
    entryEnemy.Update(2.0F/60,{},bullets);
    assert(entryEnemy.active && entryEnemy.position.z<entry.z && std::abs(entryEnemy.position.y)<0.03F);
    // 带高度的世界出生点经过演员的逆映射后仍保留该高度，避免构造路径混用旧坐标系。
    redmist::stage1::ReferenceEnemy elevatedEnemy({{entry.x,2,entry.z}});
    assert(std::abs(elevatedEnemy.position.y-2)<0.0001F);
    // Normal 妖精镜像只反转 X 位移；直行和转弯后左右轨迹保持对称，Z 与脚本时钟一致。
    redmist::stage1::ReferenceScript left,mirrored,dark;
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
    bullets.Pool().ForEachActive([&](const redmist::Bullet& b) {
        assert(!b.Dangerous() && std::abs(b.spawnVisualSeconds-10.0F/60)<0.0001F);
        assert(b.position.y==0 && b.velocity.y>0);
        assert(b.rotationAxis.x==0 && b.rotationAxis.y==1 && b.rotationAxis.z==0);
        const Vector3 toPlayer=redmist::BulletSystem::DirectionTo(b.position,raisedPlayer);
        const Vector3 direction=redmist::pattern::Normalize(b.velocity);
        const float alignment=direction.x*toPlayer.x+direction.y*toPlayer.y+direction.z*toPlayer.z;
        if(alignment>0.99999F)++aimedCenters;
    });
    assert(aimedCenters==1); // 七向扇形的中心弹必须穿过玩家的 X/Z 位置与 Y 高度。
    bullets.Reset(); dark.Reset(4,{192,-32,0},256,1);
    for(int frame=0;frame<220;++frame)dark.Update(1.0F/60,{},bullets);
    assert(bullets.Pool().ActiveCount()==0); // Sub4 在 Normal 是无攻击的后段妖精。

    redmist::stage1::RumiaBoss boss;
    boss.StartMidboss();
    for (int frame=0;frame<160;++frame) assert(!boss.Update(1.0F/60,{},bullets));
    assert(bullets.Pool().ActiveCount()==0);
    boss.Update(1.0F/60,{},bullets);
    assert(bullets.Pool().ActiveCount()==48);
    bullets.Pool().ForEachActive([&](const redmist::Bullet& b){
        assert(std::abs(b.position.y-boss.position.y)<0.0001F); // 发弹高度必须跟随实际三维判定中心。
    });
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
    int beams=0,centerBeams=0;
    std::vector<Vector3> beamDirections;
    bullets.Pool().ForEachActive([&](const redmist::Bullet& b){
        if(b.beamLength<=0)return;
        ++beams;
        const Vector3 expected=redmist::BulletSystem::DirectionTo(b.position,raisedPlayer);
        assert(b.beamDirection.z<0);
        const float alignment=b.beamDirection.x*expected.x+b.beamDirection.y*expected.y+b.beamDirection.z*expected.z;
        if(alignment>0.99999F)++centerBeams;
        else assert(std::abs(alignment-std::cos(0.18F))<0.0001F);
        for(Vector3 direction:beamDirections)
            assert(redmist::BulletSystem::Length({direction.x-b.beamDirection.x,direction.y-b.beamDirection.y,
                                                 direction.z-b.beamDirection.z})>0.01F);
        beamDirections.push_back(b.beamDirection);
    });
    assert(beams==6 && centerBeams==1 && std::abs(boss.TimeLimit()-30)<0.001F);
    assert(boss.AdvancePhase() && std::string_view(boss.SpellName())=="暗符「Demarcation」");
    bullets.Reset();
    for (int frame=0;frame<121;++frame) boss.Update(1.0F/60,raisedPlayer,bullets);
    assert(bullets.Pool().ActiveCount()==34); // rank=16 时原作 -3…6 的弹数修正增加 1 向。
    bullets.Pool().ForEachActive([](const redmist::Bullet& b){
        assert(std::abs(b.turnDelaySeconds-40.0F/60)<0.0001F);
        assert(std::abs(std::abs(b.turnAngle)-std::numbers::pi_v<float>/2)<0.0001F);
        assert(std::abs(redmist::BulletSystem::Length(b.rotationAxis)-1)<0.0001F);
    });
    // 倾斜环仍绕其自身法向完成 90°，轴向分量保持不变；不能仅旋转画面而保留旧 Y 转向轴。
    bullets.Update(1.1F,{},raisedPlayer);
    assert(bullets.Pool().ActiveCount()==34);
    bullets.Pool().ForEachActive([](const redmist::Bullet& b){
        assert(b.turnApplied && !b.turnAimed);
        const Vector3 before=redmist::pattern::Normalize(b.baseVelocity);
        const Vector3 after=redmist::pattern::Normalize(b.velocity);
        const Vector3 expected=redmist::BulletSystem::RotateAroundAxis(before,b.rotationAxis,b.turnAngle);
        assert(std::abs(after.x-expected.x)<0.0001F);
        assert(std::abs(after.y-expected.y)<0.0001F);
        assert(std::abs(after.z-expected.z)<0.0001F);
    });
}

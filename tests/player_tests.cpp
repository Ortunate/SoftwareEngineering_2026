// 本文件负责验证玩家封装后的移动、射击、Bomb 和敌弹接触规则。

#include "gameplay/actors/player.hpp"
#include "stages/stage1/ids.hpp"

#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

namespace {
// 验证发射点保持在画面下方、gold 单模块对称散射，以及 cyan 双模块散射和追踪。
void TestShotSpread() {
    using redmist::Bullet;
    using redmist::BulletSystem;
    using redmist::Player;
    using redmist::PlayerInput;
    const auto dot = [](Vector3 a, Vector3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    auto storage = std::make_unique<BulletSystem>();
    BulletSystem& bullets = *storage;
    for (float yaw : {0.0F, 1.1F, 3.1F}) {
        for (float pitch : {-1.45F, 0.0F, 1.45F}) {
            for (bool focus : {false, true}) {
                Player player;
                player.AdjustPower(128);
                player.SetView(yaw, pitch);
                bullets.Reset();
                player.Update(0.0F, {.focus = focus, .fire = true}, bullets,
                              redmist::stage1::kPlayerGoldCard,
                              redmist::stage1::kPlayerCyanCard, true);
                const Vector3 forward = BulletSystem::DirectionFromYawPitch(yaw, pitch);
                const Vector3 right{-std::cos(yaw), 0.0F, std::sin(yaw)};
                const Vector3 up{-std::sin(yaw) * std::sin(pitch), std::cos(pitch),
                                 -std::cos(yaw) * std::sin(pitch)};
                float goldHalfAngle = 0.0F;
                float cyanHalfAngle = 0.0F;
                float goldLateralSum = 0.0F;
                float cyanLateralSum = 0.0F;
                bullets.Pool().ForEachActive([&](const Bullet& bullet) {
                    const bool cyan = bullet.visual == redmist::stage1::kPlayerCyanCard;
                    const float speed = cyan ? 14.0F : 20.0F;
                    assert(std::abs(BulletSystem::Length(bullet.velocity) - speed) < 0.0001F);
                    const float lateral = dot(bullet.velocity, right);
                    const float forwardSpeed = dot(bullet.velocity, forward);
                    assert(forwardSpeed > 0.0F);
                    assert(std::abs(dot(bullet.velocity, up)) < 0.0001F);
                    const Vector3 relative{bullet.position.x - player.Position().x,
                                           bullet.position.y - player.Position().y,
                                           bullet.position.z - player.Position().z};
                    assert(std::abs(dot(relative, forward) - 1.2F) < 0.0001F);
                    assert(std::abs(dot(relative, up) + 0.6F) < 0.0001F);
                    const float originSide = dot(relative, right);
                    if (cyan) {
                        assert(std::abs(std::abs(originSide) - 0.78F) < 0.0001F);
                        assert(originSide * lateral > 0.0F);
                    } else {
                        assert(std::abs(originSide) < 0.0001F);
                        assert(std::abs(relative.x - (forward.x * 1.2F - up.x * 0.6F)) < 0.0001F);
                        assert(std::abs(relative.y - (forward.y * 1.2F - up.y * 0.6F)) < 0.0001F);
                        assert(std::abs(relative.z - (forward.z * 1.2F - up.z * 0.6F)) < 0.0001F);
                    }
                    const float angle = std::abs(std::atan2(lateral, forwardSpeed));
                    if (cyan) {
                        cyanHalfAngle = std::max(cyanHalfAngle, angle);
                        cyanLateralSum += lateral;
                    } else {
                        goldHalfAngle = std::max(goldHalfAngle, angle);
                        goldLateralSum += lateral;
                    }
                });
                assert(std::abs(goldHalfAngle - 8.0F * std::numbers::pi_v<float> / 180.0F) < 0.0001F);
                assert(std::abs(cyanHalfAngle - 10.0F * std::numbers::pi_v<float> / 180.0F) < 0.0001F);
                assert(std::abs(goldLateralSum) < 0.0001F && std::abs(cyanLateralSum) < 0.0001F);
            }
        }
    }

    // gold 每轮都对称，cyan 两轮奇数发散射的横向速度互相抵消。
    for (int power : {16, 32, 80, 96}) {
        Player player;
        player.AdjustPower(power);
        bullets.Reset();
        for (float deltaTime : {0.0F, 0.09F}) {
            player.Update(deltaTime, {.fire = true}, bullets,
                          redmist::stage1::kPlayerGoldCard,
                          redmist::stage1::kPlayerCyanCard, true);
            float goldRoundSum = 0.0F;
            bullets.Pool().ForEachActive([&](const Bullet& bullet) {
                if (bullet.visual == redmist::stage1::kPlayerGoldCard)
                    goldRoundSum += bullet.velocity.x;
            });
            assert(std::abs(goldRoundSum) < 0.0001F);
        }
        float goldSum = 0.0F;
        float cyanSum = 0.0F;
        bullets.Pool().ForEachActive([&](const Bullet& bullet) {
            if (bullet.visual == redmist::stage1::kPlayerGoldCard)
                goldSum += bullet.velocity.x;
            else
                cyanSum += bullet.velocity.x;
        });
        assert(std::abs(goldSum) < 0.0001F && std::abs(cyanSum) < 0.0001F);
    }

    Player player;
    player.AdjustPower(128);
    bullets.Reset();
    player.Update(0.0F, {.fire = true}, bullets,
                  redmist::stage1::kPlayerGoldCard, redmist::stage1::kPlayerCyanCard, true);
    const std::array<Vector3, 1> target{{{0.0F, 0.0F, 6.0F}}};
    bullets.Update(0.1F, target);
    bullets.Pool().ForEachActive([](const Bullet& bullet) {
        if (bullet.visual == redmist::stage1::kPlayerCyanCard) {
            assert(bullet.position.x * bullet.velocity.x < 0.0F);
            assert(std::abs(BulletSystem::Length(bullet.velocity) - 14.0F) < 0.0001F);
        } else {
            assert(bullet.position.x * bullet.velocity.x > 0.0F);
        }
    });
}
}  // namespace

/// @brief 运行玩家行为回归用例；断言失败时终止进程。
void RunPlayerTests() {
    TestShotSpread();
    // 挑战模式反复受击仍可继续；标准模式按额外残机数耗尽后失败。
    redmist::Player challenge;
    challenge.ConfigureRun(2, 0, true, redmist::ShotType::Needle);
    for (int miss = 0; miss < 12; ++miss) assert(challenge.TakeHit());
    assert(challenge.RemainingLives() == 3);
    redmist::Player standard;
    standard.ConfigureRun(1, 0, false, redmist::ShotType::Needle);
    assert(standard.Bombs() == 0 && standard.TakeHit());
    assert(!standard.TakeHit());
    auto needlePool = std::make_unique<redmist::BulletSystem>();
    challenge.AdjustPower(128);
    challenge.Update(0, {.fire = true}, *needlePool, 0, 1, true);
    assert(needlePool->Pool().ActiveCount() == 6);
    needlePool->Pool().ForEachActive([](const redmist::Bullet& shot) {
        assert(shot.homingTurnRate == 0 && shot.damage > 3);
    });
    using redmist::Bullet;
    using redmist::BulletOwner;
    using redmist::BulletSystem;
    using redmist::Player;
    using redmist::PlayerContact;
    using redmist::PlayerInput;

    Player player;
    // 弹幕池含 8192 个槽，测试同时使用多个池时改在堆上分配，避免 Windows 栈溢出。
    auto bulletsStorage = std::make_unique<BulletSystem>();
    BulletSystem& bullets = *bulletsStorage;
    assert(player.RemainingLives() == 4);
    assert(player.Bombs() == 3);
    assert(player.Power() == 0);

    Camera3D camera{};
    player.SyncCamera(camera);
    assert(std::abs(camera.position.z + 6.5F) < 0.0001F);
    assert(std::abs(camera.target.z + 5.5F) < 0.0001F);

    // 相同鼠标位移按倍率改变视角；重置玩家后继续使用输入倍率，不影响火力系统。
    for (float scale : {0.1F, 1.0F, 3.0F}) {
        player.Reset();
        PlayerInput look{};
        look.mouseDelta = {10.0F, 0.0F};
        look.mouseSensitivityScale = scale;
        player.Update(1.0F / 60.0F, look, bullets, redmist::stage1::kPlayerGoldCard,
                      redmist::stage1::kPlayerCyanCard, false);
        player.SyncCamera(camera);
        const float yaw = std::atan2(camera.target.x - camera.position.x,
                                     camera.target.z - camera.position.z);
        assert(std::abs(yaw + 10.0F * 0.0024F * scale) < 0.0001F);
    }
    player.Reset();

    PlayerInput movement{};
    movement.forward = true;
    movement.moveRight = true;
    player.Update(1.0F, movement, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, true);
    const Vector3 moved = player.Position();
    const float displacement = std::sqrt(moved.x * moved.x +
                                         (moved.z + 6.5F) * (moved.z + 6.5F));
    assert(std::abs(displacement - 6.5F) < 0.0001F);

    // 六个方向分别越界，验证 Z 新增的可移动空间及所有表面的内侧留白。
    const std::array<PlayerInput, 6> boundaryInputs{{
        {.forward = true}, {.backward = true}, {.moveLeft = true},
        {.moveRight = true}, {.ascend = true}, {.descend = true},
    }};
    const std::array<Vector3, 6> expectedPositions{{
        {0.0F, 0.0F, 11.15F}, {0.0F, 0.0F, -11.15F},
        {9.65F, 0.0F, -6.5F}, {-9.65F, 0.0F, -6.5F},
        {0.0F, 9.65F, -6.5F}, {0.0F, -9.65F, -6.5F},
    }};
    for (std::size_t index = 0; index < boundaryInputs.size(); ++index) {
        player.Reset();
        player.Update(10.0F, boundaryInputs[index], bullets,
                      redmist::stage1::kPlayerGoldCard,
                      redmist::stage1::kPlayerCyanCard, false);
        const Vector3 actual = player.Position();
        const Vector3 expected = expectedPositions[index];
        assert(std::abs(actual.x - expected.x) < 0.0001F);
        assert(std::abs(actual.y - expected.y) < 0.0001F);
        assert(std::abs(actual.z - expected.z) < 0.0001F);
        player.SyncCamera(camera);
        assert(camera.position.z == actual.z);
    }

    player.Reset();
    PlayerInput firing{};
    firing.fire = true;
    player.Update(0.1F, firing, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, false);
    assert(bullets.Pool().ActiveCount() == 0);
    player.Update(0.0F, firing, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 2);
    bullets.Pool().ForEachActive([](const Bullet& bullet) {
        assert(bullet.owner == BulletOwner::Player);
        assert(bullet.visual == redmist::stage1::kPlayerGoldCard);
        assert(bullet.color.a == 51);
        assert(bullet.position.x == 0.0F && bullet.position.y == -0.6F);
        assert(std::abs(bullet.position.z + 5.3F) < 0.0001F);
        assert(bullet.velocity.x != 0.0F);
        const float angle = std::abs(std::atan2(bullet.velocity.x, bullet.velocity.z));
        assert(std::abs(angle - 2.0F * std::numbers::pi_v<float> / 180.0F) < 0.0001F);
    });
    player.Update(0.01F, firing, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 2);
    player.Update(0.09F, firing, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 4);

    // 每个火力档位应增加弹数并扩大外侧散射角，保持单发伤害和速度。
    constexpr std::array<int, 8> thresholds{8, 16, 32, 48, 64, 80, 96, 128};
    constexpr std::array<std::size_t, 8> shotCounts{4, 5, 6, 7, 8, 9, 10, 12};
    float previousGoldAngle = 2.0F * std::numbers::pi_v<float> / 180.0F;
    float previousCyanAngle = 0.0F;
    for (std::size_t index = 0; index < thresholds.size(); ++index) {
        Player upgraded;
        // 弹幕池含 8192 个槽，测试同时使用多个池时改在堆上分配，避免 Windows 栈溢出。
        auto volleyStorage = std::make_unique<BulletSystem>();
        BulletSystem& volley = *volleyStorage;
        upgraded.AdjustPower(thresholds[index]);
        assert(upgraded.Power() == thresholds[index]);
        upgraded.Update(0.0F, firing, volley,
                        redmist::stage1::kPlayerGoldCard,
                        redmist::stage1::kPlayerCyanCard, true);
        assert(volley.Pool().ActiveCount() == shotCounts[index]);
        int homingCount = 0;
        float maxGoldAngle = 0.0F;
        float maxCyanAngle = 0.0F;
        volley.Pool().ForEachActive([&](const Bullet& bullet) {
            const float angle = std::abs(std::atan2(bullet.velocity.x, bullet.velocity.z));
            if (bullet.homingTurnRate > 0.0F) {
                assert(bullet.visual == redmist::stage1::kPlayerCyanCard);
                assert(bullet.damage == 1.0F && bullet.homingTurnRate == 5.5F);
                assert(std::abs(BulletSystem::Length(bullet.velocity) - 14.0F) < 0.0001F);
                maxCyanAngle = std::max(maxCyanAngle, angle);
                ++homingCount;
            } else {
                assert(bullet.visual == redmist::stage1::kPlayerGoldCard);
                assert(bullet.damage == 2.0F);
                assert(std::abs(BulletSystem::Length(bullet.velocity) - 20.0F) < 0.0001F);
                maxGoldAngle = std::max(maxGoldAngle, angle);
            }
            if (bullet.visual == redmist::stage1::kPlayerCyanCard) {
                assert(std::abs(std::abs(bullet.position.x) - 0.78F) < 0.0001F);
                assert(bullet.position.x * bullet.velocity.x > 0.0F);
            } else {
                assert(bullet.position.x == 0.0F);
            }
            assert(bullet.position.y == -0.6F);
            assert(std::abs(bullet.position.z + 5.3F) < 0.0001F);
            assert(bullet.velocity.y == 0.0F);
            assert(bullet.color.a == 51);
        });
        assert(homingCount > 0);
        assert(maxGoldAngle > previousGoldAngle && maxCyanAngle > previousCyanAngle);
        assert(maxGoldAngle <= 8.0F * std::numbers::pi_v<float> / 180.0F + 0.0001F);
        assert(maxCyanAngle <= 10.0F * std::numbers::pi_v<float> / 180.0F + 0.0001F);
        previousGoldAngle = maxGoldAngle;
        previousCyanAngle = maxCyanAngle;
    }
    Player alternating;
    // 弹幕池含 8192 个槽，测试同时使用多个池时改在堆上分配，避免 Windows 栈溢出。
    auto alternatingBulletsStorage = std::make_unique<BulletSystem>();
    BulletSystem& alternatingBullets = *alternatingBulletsStorage;
    alternating.AdjustPower(16);
    alternating.Update(0.0F, firing, alternatingBullets,
                       redmist::stage1::kPlayerGoldCard,
                       redmist::stage1::kPlayerCyanCard, true);
    alternating.Update(0.09F, firing, alternatingBullets,
                       redmist::stage1::kPlayerGoldCard,
                       redmist::stage1::kPlayerCyanCard, true);
    int leftShots = 0;
    int rightShots = 0;
    int centerShots = 0;
    alternatingBullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (bullet.visual != redmist::stage1::kPlayerGoldCard) return;
        assert(bullet.position.x == 0.0F);
        if (bullet.velocity.x < 0.0F) ++leftShots;
        else if (bullet.velocity.x > 0.0F) ++rightShots;
        else ++centerShots;
    });
    assert(leftShots == 2 && rightShots == 2 && centerShots == 2);
    // 升满只报告一次；继续收集不重复报告，降级后再次达到上限可重新报告。
    assert(player.AdjustPower(999));
    assert(player.Power() == Player::kMaxPower);
    assert(!player.AdjustPower(1));
    assert(!player.AdjustPower(0));
    assert(!player.AdjustPower(-999));
    assert(player.Power() == 0);
    assert(!player.AdjustPower(Player::kMaxPower - 1));
    assert(player.AdjustPower(1));
    assert(!player.AdjustPower(8));
    assert(!player.AdjustPower(-Player::kMaxPower));

    PlayerInput bombInput{};
    bombInput.bomb = true;
    assert(player.Update(0.0F, bombInput, bullets,
                         redmist::stage1::kPlayerGoldCard,
                         redmist::stage1::kPlayerCyanCard, true));
    assert(player.Bombs() == 2);
    assert(!player.Update(0.5F, bombInput, bullets,
                          redmist::stage1::kPlayerGoldCard,
                          redmist::stage1::kPlayerCyanCard, true));
    assert(player.Bombs() == 2);
    assert(player.Update(0.5F, bombInput, bullets,
                         redmist::stage1::kPlayerGoldCard,
                         redmist::stage1::kPlayerCyanCard, true));
    assert(player.Bombs() == 1);

    player.Update(2.6F, {}, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, true);
    Bullet graze{};
    graze.active = true;
    graze.position = {0.5F, 0.0F, -6.5F};
    assert(player.ContactEnemyBullet(graze) == PlayerContact::Graze);
    assert(graze.grazed);
    assert(player.ContactEnemyBullet(graze) == PlayerContact::None);

    Bullet hit{};
    hit.active = true;
    hit.position = player.Position();
    assert(player.ContactEnemyBullet(hit) == PlayerContact::Hit);
    assert(!hit.active);
    assert(player.TakeHit());
    assert(player.RemainingLives() == 3);
    assert(player.Bombs() == 2);
    Bullet protectedHit{};
    protectedHit.active = true;
    protectedHit.position = player.Position();
    assert(player.ContactEnemyBullet(protectedHit) == PlayerContact::None);
    assert(!protectedHit.active);

    player.AdjustPower(128);
    assert(player.TakeHit());
    assert(player.Power() == 112);
    assert(player.AdjustPower(16));

    // 弹幕池含 8192 个槽，测试同时使用多个池时改在堆上分配，避免 Windows 栈溢出。
    auto trackingStorage = std::make_unique<BulletSystem>();
    BulletSystem& tracking = *trackingStorage;
    Bullet* guided = tracking.Spawn(BulletOwner::Player, {},
        {0.0F, 0.0F, 10.0F}, 0.1F, WHITE,
        redmist::stage1::kPlayerCyanCard);
    assert(guided != nullptr);
    guided->homingTurnRate = 4.0F;
    const std::array<Vector3, 1> target{{{8.0F, 0.0F, 8.0F}}};
    tracking.Update(0.2F, target);
    assert(guided->velocity.x > 0.0F);
    assert(std::abs(BulletSystem::Length(guided->velocity) - 10.0F) < 0.0001F);

    player.Reset();
    assert(player.RemainingLives() == 4);
    assert(player.Bombs() == 3);
    assert(player.TakeHit());
    assert(player.TakeHit());
    assert(player.TakeHit());
    assert(!player.TakeHit());
    assert(player.RemainingLives() == 0);
}

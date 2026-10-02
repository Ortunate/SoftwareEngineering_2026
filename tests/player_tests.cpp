// 本文件负责验证玩家封装后的移动、射击、Bomb 和敌弹接触规则。

#include "gameplay/actors/player.hpp"
#include "stages/stage1/ids.hpp"

#include <cassert>
#include <array>
#include <cmath>
#include <memory>

/// @brief 运行玩家行为回归用例；断言失败时终止进程。
void RunPlayerTests() {
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
        assert(std::abs(bullet.position.x) >= 0.62F);
        assert(std::abs(bullet.position.z + 5.3F) < 0.0001F);
        assert(bullet.position.x * bullet.velocity.x < 0.0F);
        const float timeToAim = (3.5F - bullet.position.z) / bullet.velocity.z;
        assert(std::abs(bullet.position.x + bullet.velocity.x * timeToAim) < 0.0001F);
    });
    player.Update(0.01F, firing, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 2);
    player.Update(0.09F, firing, bullets, redmist::stage1::kPlayerGoldCard,
                  redmist::stage1::kPlayerCyanCard, true);
    assert(bullets.Pool().ActiveCount() == 4);

    // 每个原作门槛都应改变三维化的直射或追踪弹数量。
    constexpr std::array<int, 8> thresholds{8, 16, 32, 48, 64, 80, 96, 128};
    constexpr std::array<std::size_t, 8> shotCounts{4, 5, 6, 7, 8, 9, 10, 12};
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
        volley.Pool().ForEachActive([&](const Bullet& bullet) {
            if (bullet.homingTurnRate > 0.0F) {
                assert(bullet.visual == redmist::stage1::kPlayerCyanCard);
                assert(bullet.damage < 2.0F);
                ++homingCount;
            } else {
                assert(bullet.visual == redmist::stage1::kPlayerGoldCard);
            }
            assert(std::abs(bullet.position.x) >= 0.62F);
            assert(bullet.color.a < 255);
        });
        assert(homingCount > 0);
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
    alternatingBullets.Pool().ForEachActive([&](const Bullet& bullet) {
        if (bullet.visual != redmist::stage1::kPlayerGoldCard) return;
        if (bullet.position.x < 0.0F) ++leftShots;
        else ++rightShots;
    });
    assert(leftShots == 3 && rightShots == 3);
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

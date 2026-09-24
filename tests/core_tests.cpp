#include "game/bullet_system.hpp"
#include "game/bullet_patterns.hpp"
#include "game/boss.hpp"
#include "game/enemy.hpp"
#include "game/settings.hpp"
#include "game/stage_system.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <system_error>

int main() {
    using redmist::BulletOwner;
    using redmist::BulletSystem;
    using redmist::GameSettings;
    using redmist::KeyBindings;

    const KeyBindings defaultBindings;
    assert(defaultBindings.descend == KEY_LEFT_SHIFT);
    assert(defaultBindings.focus == KEY_C);

    const std::filesystem::path settingsPath =
        std::filesystem::temp_directory_path() /
        "red_mist_settings_core_test.cfg";
    std::error_code settingsError;
    std::filesystem::remove(settingsPath, settingsError);
    const GameSettings createdSettings =
        redmist::LoadOrCreateSettings(settingsPath);
    assert(std::filesystem::exists(settingsPath));
    assert(createdSettings.fpsLimit == 60);
    assert(createdSettings.bindings.descend == KEY_LEFT_SHIFT);

    GameSettings changedSettings = createdSettings;
    changedSettings.fpsLimit = 144;
    changedSettings.bindings.fire = KEY_F1;
    assert(redmist::SaveSettingsFile(settingsPath, changedSettings));
    const GameSettings loadedSettings =
        redmist::LoadOrCreateSettings(settingsPath);
    assert(loadedSettings.fpsLimit == 144);
    assert(loadedSettings.bindings.fire == KEY_F1);
    std::filesystem::remove(settingsPath, settingsError);

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
                          redmist::pattern::Scale(direction, 2.5F), 0.16F, RED);
        });
    assert(bullets.Pool().ActiveCount() == 64);

    bool hasPositiveX = false;
    bool hasNegativeX = false;
    bool hasPositiveY = false;
    bool hasNegativeY = false;
    bool hasPositiveZ = false;
    bool hasNegativeZ = false;
    bullets.Pool().ForEachActive([&](const redmist::Bullet& bullet) {
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
                          redmist::pattern::Scale(direction, 3.0F), 0.16F, BLUE);
        });
    assert(bullets.Pool().ActiveCount() == 13);

    bullets.ClearEnemyBullets();
    assert(bullets.Pool().ActiveCount() == 0);

    redmist::FairyEnemy fairy(redmist::PathKind::Straight, {0.0F, 0.0F, 5.0F});
    fairy.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 1);
    bullets.Reset();

    redmist::OrbEnemy orb(redmist::PathKind::Straight, {0.0F, 0.0F, 5.0F});
    orb.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 6);
    bullets.Reset();

    redmist::EliteEnemy elite(redmist::PathKind::Straight, {0.0F, 0.0F, 5.0F});
    elite.Update(0.9F, {}, bullets);
    assert(bullets.Pool().ActiveCount() == 42);
    bullets.Reset();

    redmist::RumiaBoss boss;
    boss.Start();
    assert(boss.SpellName() == nullptr);
    assert(std::abs(boss.position.x - 3.0F) < 0.0001F);
    assert(!boss.Update(1.0F, {}, bullets));
    assert(bullets.Pool().ActiveCount() == 22);
    bullets.Reset();
    assert(!boss.Update(5.4F, {}, bullets));
    assert(std::abs(boss.position.x + 3.0F) < 0.0001F);
    assert(boss.SpellName() == nullptr);
    assert(boss.Update(10.6F, {}, bullets));
    assert(!boss.AdvancePhase());
    assert(!boss.active);

    redmist::StageDirector stage;
    int bossEvents = 0;
    int enemyEvents = 0;
    stage.Update(42.0F, [&](const redmist::StageEvent& event) {
        if (event.kind == redmist::StageEventKind::StartBoss) ++bossEvents;
        if (event.kind == redmist::StageEventKind::SpawnLine ||
            event.kind == redmist::StageEventKind::SpawnCrossingPair ||
            event.kind == redmist::StageEventKind::SpawnElite) ++enemyEvents;
    });
    assert(bossEvents == 1);
    assert(enemyEvents == 15);
    stage.Update(60.0F, [&](const redmist::StageEvent& event) {
        if (event.kind == redmist::StageEventKind::StartBoss) ++bossEvents;
    });
    assert(bossEvents == 1);
    return 0;
}

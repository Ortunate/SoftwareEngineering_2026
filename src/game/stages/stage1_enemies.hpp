#pragma once

#include "game/enemy.hpp"
#include "game/stages/stage1_ids.hpp"

namespace redmist::stage1 {

/// @brief Stage 1 敌人共用的四种航线，其他 Stage 可完全自行实现移动。
class Stage1Enemy : public Enemy {
public:
    /// @brief 初始化 Stage 1 外观与航线。
    Stage1Enemy(VisualId visual, Path path, Vector3 position, float delay);

private:
    void Move() override;
    Path path_;
};

/// @brief Stage 1 普通妖精。
class FairyEnemy final : public Stage1Enemy {
public:
    FairyEnemy(Path path, Vector3 position, float delay = 0.0F);
private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

/// @brief Stage 1 暗色妖精。
class DarkFairyEnemy final : public Stage1Enemy {
public:
    DarkFairyEnemy(Path path, Vector3 position, float delay = 0.0F);
private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

/// @brief Stage 1 光球敌人。
class OrbEnemy final : public Stage1Enemy {
public:
    OrbEnemy(Path path, Vector3 position, float delay = 0.0F);
private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

/// @brief Stage 1 精英妖精。
class EliteEnemy final : public Stage1Enemy {
public:
    EliteEnemy(Path path, Vector3 position, float delay = 0.0F);
private:
    void UseSkill(BulletSystem& bullets, Vector3 playerPosition) override;
};

}  // namespace redmist::stage1

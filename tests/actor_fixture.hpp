// 本文件提供直接构造 behavior::Actor 的测试配置，图集编号仅借用第一面，不创建旧敌人子类。
#pragma once
#include "gameplay/actors/behavior.hpp"
#include "stages/stage1/ids.hpp"
namespace koumakyou::testing {
/// @brief 静止、可接触的演员配置；世界坐标、原始 HP、世界半径，默认击破 500 分及一个小 P 点。
/// 不自动越界回收，供碰撞、渲染、掉落和生命周期测试独立控制。
constexpr behavior::Spawn SpawnAt(Vector3 position={},VisualId visual=stage1::kFairy,float health=1,float radius=0.55F,int score=500) {
    return {.position=position,.pattern={.movement={.speed=0},.deathEffect=0,.endFrame=2147483647},
        .sourceHealth=health,.sourceScore=score,.sourceItem=0,.visual=visual,.hitRadius=radius,
        .bulletBase=stage1::kNativeBulletBase,.laserBase=stage1::kNativeLaserBase,
        .defeatBase=stage1::kNativeDefeat,.fallback=stage1::kBlueBullet,.retireOutside=false};
}
/// @brief 独占一个按配置启动的测试演员；调用方控制其池外生命周期。
inline std::unique_ptr<behavior::Actor> MakeActor(const behavior::Spawn& spawn) {
    auto actor=std::make_unique<behavior::Actor>();actor->Activate(spawn);return actor;
}
} // namespace koumakyou::testing

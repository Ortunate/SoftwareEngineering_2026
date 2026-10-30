// 本文件定义无图形依赖的玩家瞄准求解接口。
#pragma once
#include "raylib.h"
#include <span>
namespace koumakyou {
/// @brief 借用目标球快照；坐标和正半径使用世界单位，eligible 仅限制辅助选择。
struct PlayerAimTarget { Vector3 position{}; float radius=0; bool eligible=true; };
/// @brief 单帧世界瞄准结果；depth 为沿原视线的正深度，assisted 表示发生额外偏转。
struct PlayerAimResult {
    Vector3 point{};
    Vector3 forward{};
    float depth=20;
    bool assisted=false;
    // 只在 assisted=true 时有效的目标判定球快照；锁定提示围绕敌机，不围绕弹道落点。
    Vector3 targetPosition{};
    float targetRadius=0;
};
/// @brief position 为世界位置，forward 须归一化；无有效目标时回退前方 20 单位，不保存借用数据。
PlayerAimResult ResolvePlayerAim(Vector3 position,Vector3 forward,std::span<const PlayerAimTarget> targets,bool enabled);
}

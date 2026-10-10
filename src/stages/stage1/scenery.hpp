// 本文件声明一面红月森林与湖面的程序化三维环境绘制接口。
#pragma once

namespace redmist::stage1 {
/// @brief 在战场边界外绘制森林和湖面；须在 BeginMode3D 内调用。
/// seconds 是关卡累计秒数，暂停时保持不变；几何不参与碰撞，不拥有 GPU 资源。
void DrawScenery(float seconds);
}

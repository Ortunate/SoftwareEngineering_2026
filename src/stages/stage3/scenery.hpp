// 本文件声明三面红魔馆庭院的三维背景绘制入口，不拥有图像或运行中计时器。
#pragma once
namespace koumakyou::stage3 {
/// @brief 在 3D 通道绘制盒体外背景；seconds 为暂停冻结的关卡秒数，不在绘制时推进逻辑。
void DrawScenery(float seconds);
}

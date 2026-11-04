// 本文件声明二面雾之湖的三维装饰绘制入口，不持有纹理或推进关卡时钟。
#pragma once
namespace koumakyou::stage2 {
/// @brief seconds 为关卡运行秒数，装饰全部位于可玩盒体之外；调用方须已进入 3D 渲染通道。
void DrawScenery(float seconds);
}

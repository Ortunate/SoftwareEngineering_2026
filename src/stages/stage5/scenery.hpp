// 本文件声明五面红魔馆走廊的无状态三维背景，不持有活动关卡资源。
#pragma once
namespace koumakyou::stage5 {
/// @brief 按冻结暂停及世界时停后的秒数绘制盒体外走廊，不推进游戏状态或加载素材。
void DrawScenery(float seconds);
}

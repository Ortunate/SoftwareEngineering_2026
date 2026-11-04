// 本文件声明六面红月与屋顶的背景采样和绘制接口，不持有活动资源。
#pragma once
#include "raylib.h"
namespace koumakyou::stage6 {
/// @brief 六面三维演出快照；偏移和位置均为世界单位，建筑整体沿 -Y 下沉。
/// 红月球体始终位于可玩体积的 +Z 外侧，避免逼近时穿过演员或镜头。
struct SceneryPose {
    float buildingOffsetY=0;
    Vector3 moonPosition{-20,25,105};
    float cloudOpacity=1;
};
/// @brief 采样道中完整退场后的十秒转场；负数表示未开始，>=10 固定为关底背景。
/// 输入为暂停/时停已冻结的经过秒数；无累计状态，重复采样及跨显示帧率结果一致。
SceneryPose SampleScenery(float transitionSeconds);
/// @brief seconds 为背景世界秒数，用于薄云漂移；transitionSeconds 约定同 SampleScenery。
/// 只绘制盒体外的月亮、薄云和屋顶，不推进逻辑；默认保持开场背景。
void DrawScenery(float seconds,float transitionSeconds=-1);
}

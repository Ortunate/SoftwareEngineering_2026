// 本文件定义战斗提示音的无状态时钟规则；不加载音频或推进战斗。
#pragma once
#include <algorithm>
#include <cmath>
#include <string_view>
namespace koumakyou::combat_audio {
/// @brief limit/elapsed 为有限秒数；负上限表示未计时，返回 -1；11 表示尚未进入最后十秒。
/// 采用真实剩余秒边界，1e-4 秒容差吸收 60 Hz 内容帧转浮点后的舍入误差。
inline int CountdownSecond(float limit,float elapsed){
 if(limit<0)return -1;
 return static_cast<int>(std::clamp(std::ceil(limit-elapsed-.0001F),0.F,11.F));
}
/// @brief previous/current 来自 CountdownSecond；新宣告可用 previous=11。
/// 每次更新至多播放当前一声，跨秒不补播积压提示；停表、倒退、归零均静音。
inline std::string_view CountdownCue(int previous,int current){
 if(current<1||current>10||previous<=current)return {};
 return current<=5?"countdown_urgent":"countdown";
}
}

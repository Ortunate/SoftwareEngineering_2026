// 本文件声明Extra青绿地砖、雾层与卷纹的三维适配，纹理所有权留在当前关卡素材管理器。
#pragma once
#include "raylib.h"
namespace koumakyou::stage_extra {
/// @brief seconds为暂停冻结后的非负有限秒数；atlas借用本关背景纹理，空纹理安全跳过绘制。
/// 装饰全部位于可玩盒体外；本函数不读取STD、不推进逻辑、不加载或保留纹理。
void DrawScenery(float seconds,Texture2D atlas);
}

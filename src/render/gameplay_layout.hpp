// 本文件负责新典 1920×1080 侧栏/场地的等比布局；3D 视口、HUD 与对话使用同一坐标转换。
#pragma once
#include "raylib.h"
#include <algorithm>
namespace redmist {
/// @brief 参考界面以 16:9 等比居中；宽窄窗口均留边，避免拉伸纹理或改变游戏视口纵横比。
struct GameplayLayout {
    float scale;
    Vector2 origin;
    GameplayLayout(float width, float height)
        : scale(std::min(width/1920.0F,height/1080.0F)),
          origin{(width-1920*scale)/2,(height-1080*scale)/2} {}
    [[nodiscard]] Vector2 Point(float x,float y) const { return {origin.x+x*scale,origin.y+y*scale}; }
    [[nodiscard]] Rectangle Rect(float x,float y,float width,float height) const {
        return {origin.x+x*scale,origin.y+y*scale,width*scale,height*scale};
    }
    // frame.anm 的第二张 sprite 位于 (1922,1)，其透明内窗为 x=55…911、y=40…1042。
    [[nodiscard]] Rectangle Viewport() const { return Rect(532,40,856,1002); }
};
} // namespace redmist

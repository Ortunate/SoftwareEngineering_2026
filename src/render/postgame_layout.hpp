// 本文件统一结局后成绩选存的绘制与命中布局，横屏分栏、窄屏纵向排列。
#pragma once
#include "render/menu_layout.hpp"

namespace koumakyou::menu {
/// @brief 参数为窗口逻辑像素；所有交互矩形直接返回屏幕坐标，文字布局可读取 canvas 设计坐标。
struct PostgameLayout {
    Canvas canvas;
    bool compact;
    explicit PostgameLayout(int width,int height):canvas(PageCanvas(width,height)),compact(canvas.width<1000){}
    [[nodiscard]] Rectangle Choice(int index) const {
        return canvas.Rect(compact?48:690,(compact?720:350)+index*(compact?100:80),compact?664:340,60);
    }

};
} // namespace koumakyou::menu

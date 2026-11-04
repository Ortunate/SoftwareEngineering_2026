// 本文件统一Replay播放、保存选槽的表格与分页命中布局；宽屏每页15槽，窄屏每页8槽。
#pragma once
#include "render/menu_layout.hpp"

namespace koumakyou::menu {
/// @brief 参数为窗口逻辑像素；槽位编号属于内容列表，Row只接受本页0起算行号。
struct ReplayLayout {
    Canvas canvas;
    bool compact;
    ReplayLayout(int width,int height):canvas(PageCanvas(width,height)),compact(canvas.width<1000){}
    int PageSize() const {return compact?8:15;}
    Rectangle Row(int index) const {return canvas.Rect(64,compact?240+index*82:200+index*30,canvas.width-128,compact?76:28);}
    Rectangle Arrow(int direction) const {return canvas.Rect(direction<0?18:canvas.width-50,canvas.height/2-30,32,60);}
    Rectangle Status() const {return canvas.Rect(64,compact?944:657,canvas.width-128,compact?58:38);}
    Rectangle Page() const {return canvas.Rect(canvas.width/2-90,compact?1030:710,180,32);}
};
} // namespace koumakyou::menu

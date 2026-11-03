// 本文件定义Score查询的难度列表、标准/挑战双栏成绩及四机体分页布局，绘制和键鼠命中共用。
#pragma once
#include "render/menu_layout.hpp"

namespace koumakyou::menu {
/// @brief 构造参数为窗口逻辑像素；横竖屏均按标准在左、挑战在右排列同一机体的成绩。
struct ScoreLayout {
    Canvas canvas;
    bool compact;
    ScoreLayout(int width,int height):canvas(PageCanvas(width,height)),compact(canvas.width<1000){}
    /// @brief index为Easy～Extra的0～4，所有难度均可查询，不受可玩入口或记录解锁限制。
    Rectangle Difficulty(int index) const {return canvas.Rect(compact?130:350,(compact?190:146)+index*(compact?150:100),compact?500:400,compact?128:88);}
    /// @brief mode为标准0/挑战1；返回设计坐标，行高同样为设计像素，供Ranking绘制器使用。
    Rectangle TableDesign(int mode) const {
        const float gap=compact?24:40,width=(canvas.width-128-gap)/2;
        return {64+mode*(width+gap),compact?330.0F:258.0F,width,RowStep()*10};
    }
    float RowStep() const {return compact?62:38;}
    Rectangle Table(int mode) const {const auto r=TableDesign(mode);return canvas.Rect(r.x,r.y,r.width,r.height);}
    /// @brief 两栏均显示完整机体名称及各自模式；标签只读，不保留原选项卡的点击行为。
    Rectangle ShotLabel(int mode) const {const auto r=TableDesign(mode);return canvas.Rect(r.x,compact?206:148,r.width,36);}
    Rectangle ModeLabel(int mode) const {const auto r=TableDesign(mode);return canvas.Rect(r.x,compact?254:194,r.width,30);}
    /// @brief direction为-1/1；位于双栏列表外侧并垂直居中，循环切换四种机体页。
    Rectangle Arrow(int direction) const {const auto r=TableDesign(0);return canvas.Rect(direction<0?18:canvas.width-50,r.y+r.height/2-30,32,60);}
    Rectangle Page() const {return canvas.Rect(canvas.width/2-90,compact?980:654,180,30);}
};
} // namespace koumakyou::menu

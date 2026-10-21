// 本文件定义符卡练习的三列目录和人物区；绘制、箭头、滚动翻页与点击共用屏幕矩形。
#pragma once
#include "render/menu_layout.hpp"
namespace koumakyou::menu {
/// @brief 输入为窗口逻辑像素；窄屏先展示人物摘要再展示目录，正文页保留八组可见符卡行。
struct SpellPracticeLayout {
    Canvas canvas; bool compact;
    static constexpr int kVisibleGroups=8;
    SpellPracticeLayout(int width,int height):canvas(Canvas::Full(width,ui::CommandBar::ContentHeight(width,height),width<height*1.15F?760:1280,width<height*1.15F?1400:720)),compact(canvas.width<1000){}
    [[nodiscard]] Rectangle Title() const{return canvas.Rect(42,30,compact?676:830,58);}
    [[nodiscard]] Rectangle Heading() const{return canvas.Rect(42,compact?540:105,compact?676:830,42);}
    [[nodiscard]] Rectangle Columns() const{return canvas.Rect(42,compact?602:157,compact?676:830,30);}
    [[nodiscard]] Rectangle StageRow(int index) const{return canvas.Rect(42,(compact?648:202)+index*(compact?78:62),compact?676:830,compact?72:58);}
    [[nodiscard]] Rectangle GroupRow(int visibleIndex) const{return canvas.Rect(42,(compact?648:202)+visibleIndex*(compact?69:52),compact?238:282,compact?64:48);}
    [[nodiscard]] Rectangle Difficulty(int index,bool extra=false) const{return canvas.Rect(compact?300:348,(compact?648:202)+index*(compact?142:112),compact?418:524,extra?(compact?260:200):(compact?134:106));}
    [[nodiscard]] Rectangle GroupPage(int direction) const{return canvas.Rect(42+(direction>0?(compact?124:148):0),compact?1210:626,compact?114:134,30);}
    [[nodiscard]] Rectangle Portrait() const{return canvas.Rect(compact?80:928,compact?112:110,compact?296:300,compact?344:300);}
    /// @brief direction 为 -1/1；整个三角周围都可点击，不使用随动画移动的视觉位置作为命中区。
    [[nodiscard]] Rectangle Arrow(int direction) const{return canvas.Rect(compact?(direction<0?34:372):(direction<0?898:1222),compact?250:288,44,60);}
    [[nodiscard]] Rectangle CharacterInfo() const{return canvas.Rect(compact?420:916,compact?125:425,compact?298:320,compact?320:214);}
    [[nodiscard]] Rectangle Footer() const{return canvas.Rect(42,compact?1300:670,compact?676:1194,45);}
};
} // namespace koumakyou::menu

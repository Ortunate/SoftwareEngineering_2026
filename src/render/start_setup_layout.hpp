// 本文件定义新典 Start 页面布局，绘制与鼠标输入共用；窄屏将人物与选项上下排列。
#pragma once
#include "render/menu_layout.hpp"

namespace koumakyou::menu {
/// @brief 构造参数为窗口逻辑像素；所有区域返回屏幕坐标，人物槽只能等比放入立绘。
/// 横屏采用 1280×720 设计，竖屏采用 760×1100；文字依 canvas.scale 保持等比。
struct StartSetupLayout {
    Canvas canvas;
    bool compact;
    StartSetupLayout(int width,int height)
        : canvas(Canvas::Full(width,height,width<height*1.15F?760:1280,width<height*1.15F?1100:720)),
          compact(canvas.width<1000) {}
    [[nodiscard]] Rectangle Difficulty(int index) const {
        return canvas.Rect(compact?130:440,(compact?200:130)+index*(compact?180:120),compact?500:400,compact?150:104);
    }
    [[nodiscard]] Rectangle Portrait() const { return canvas.Rect(compact?240:480,compact?90:40,compact?280:330,compact?390:410); }
    [[nodiscard]] Rectangle PortraitBackdrop() const { return canvas.Rect(compact?80:150,compact?60:-30,compact?480:480,compact?650:750); }
    [[nodiscard]] Rectangle CharacterInfo() const { return canvas.Rect(compact?180:500,compact?490:465,compact?400:320,compact?145:170); }
    [[nodiscard]] Rectangle Badge() const { return canvas.Rect(compact?52:65,compact?900:525,compact?260:345,compact?105:126); }
    [[nodiscard]] Rectangle Heading() const { return canvas.Rect(compact?62:835,compact?647:65,compact?636:380,compact?62:110); }
    /// @brief 御札、模式、符卡的两行共用区域，index 仅为 0/1；动画不改变最终命中范围。
    [[nodiscard]] Rectangle Choice(int index) const { return canvas.Rect(compact?62:835,(compact?730:210)+index*(compact?76:110),compact?636:380,compact?70:100); }
    [[nodiscard]] Rectangle ShotSummary() const { return canvas.Rect(compact?338:835,compact?936:594,compact?360:380,compact?60:66); }
    [[nodiscard]] Rectangle ModeSummary() const { return canvas.Rect(compact?338:835,compact?872:490,compact?360:380,compact?56:90); }
    [[nodiscard]] Rectangle ReadyBand() const { return canvas.Rect(0,compact?345:235,canvas.width,compact?255:205); }
    [[nodiscard]] Rectangle ReadyButton() const { return canvas.Rect(compact?170:440,compact?500:357,compact?420:400,60); }
};
} // namespace koumakyou::menu

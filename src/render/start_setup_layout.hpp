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
    /// @brief 角色阶段的固定箭头命中区；direction 为 -1/1，晃动不改变命中区，点击只切换候选，不能确认到御札页。
    [[nodiscard]] Rectangle CharacterArrow(int direction) const { return canvas.Rect(compact?(direction<0?175:541):(direction<0?425:821),compact?240:215,44,60); }
    [[nodiscard]] Rectangle CharacterInfo() const { return canvas.Rect(compact?180:500,compact?490:465,compact?400:320,compact?145:170); }
    [[nodiscard]] Rectangle Badge() const { return canvas.Rect(compact?52:65,compact?900:525,compact?260:345,compact?105:126); }
    [[nodiscard]] Rectangle Heading() const { return canvas.Rect(compact?62:835,compact?647:65,compact?636:380,compact?62:110); }
    /// @brief 御札和模式的两行共用区域，index 仅为 0/1；动画不改变最终命中范围。
    [[nodiscard]] Rectangle Choice(int index) const { return canvas.Rect(compact?62:835,(compact?730:210)+index*(compact?76:110),compact?636:380,compact?70:100); }
    [[nodiscard]] Rectangle ShotSummary() const { return canvas.Rect(compact?338:835,compact?936:594,compact?360:380,compact?60:66); }
    [[nodiscard]] Rectangle ModeSummary() const { return canvas.Rect(compact?338:835,compact?872:490,compact?360:380,compact?56:90); }
    /// @brief 关卡页右侧标题及分模式最高分列表；index 为 0～5，未开放项也使用同一绘制区域，六行均避开最终确认横幅。
    [[nodiscard]] Rectangle StageHeading() const { return canvas.Rect(compact?338:835,compact?647:35,compact?360:380,compact?56:32); }
    [[nodiscard]] Rectangle StageRow(int index) const { return canvas.Rect(compact?338:835,(compact?714:77)+index*(compact?34:26),compact?360:380,compact?30:24); }
    /// @brief 竖屏关卡列表占据原摘要位置，摘要下移至列表下方，避开左下角难度札。
    [[nodiscard]] Rectangle StageModeSummary() const { return canvas.Rect(compact?338:835,compact?944:490,compact?360:380,compact?56:90); }
    [[nodiscard]] Rectangle StageShotSummary() const { return canvas.Rect(compact?338:835,compact?1006:594,compact?360:380,compact?58:66); }
    [[nodiscard]] Rectangle ReadyBand() const { return canvas.Rect(0,compact?345:235,canvas.width,compact?255:205); }
    [[nodiscard]] Rectangle ReadyButton() const { return canvas.Rect(compact?170:440,compact?500:357,compact?420:400,60); }
};
} // namespace koumakyou::menu

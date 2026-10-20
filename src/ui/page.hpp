// 本文件提供二级菜单共用的全窗口花纹、回纹边框、页眉、页脚、选中细线和按钮，供选项/改键/开局/信息页复用。
#pragma once

#include "ui/layout.hpp"
#include "ui/components.hpp"
#include "ui/effects.hpp"
#include "ui/motion.hpp"
#include <cmath>

namespace koumakyou::ui {
// 页面模板统一采用暗红、象牙白与金色；所有颜色均由绘制函数按动画透明度使用。
inline constexpr Color kPageIvory{252,246,236,255},kPageGold{233,204,139,255};

/// @brief 绘制窗口边框；width/height 为设计像素，四角纹饰等比，不随窗口纵横比拉伸。
inline void DrawPageFrame(const Canvas& canvas,Color color,float width,float height) {
    const auto bounds=canvas.Rect(0,0,width,height);
    const float inset=18*canvas.scale,size=20*canvas.scale;
    DrawRectangleLinesEx({bounds.x+inset,bounds.y+inset,bounds.width-2*inset,bounds.height-2*inset},canvas.scale,color);
    for(float x : {bounds.x+inset,bounds.x+bounds.width-inset-size})
        for(float y : {bounds.y+inset,bounds.y+bounds.height-inset-size}) {
            DrawRectangleRec({x,y,size,size},color);
            DrawRectangleRec({x+4*canvas.scale,y+4*canvas.scale,12*canvas.scale,12*canvas.scale},kPageIvory);
            DrawRectangleRec({x+6*canvas.scale,y+6*canvas.scale,8*canvas.scale,8*canvas.scale},color);
        }
}

/// @brief 绘制覆盖整个窗口的二级页背景；opacity 为 0～1，半透明时叠在首页上，不清除下层画面。
/// canvas 为 PageCanvas，motion 只借用；所有花纹和边框沿用原透明度，随整层一起混合。
inline void DrawPageSurface(const Canvas& canvas,const ui::MenuMotion& motion,float opacity=1) {
    if(opacity>=1)ClearBackground(Color{18,16,19,255});
    DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),ui::WithOpacity(Color{18,16,19,255},opacity),ui::WithOpacity(Color{62,17,25,255},opacity));
    for(int index=0;index<18;++index) {
        const float x=index%2==0 ? 80+(index%4)*38.0F : canvas.width-140+(index%3)*32.0F;
        const float y=65+index*(canvas.height-100)/18;
        for(int petal=0;petal<5;++petal) {
            const float angle=petal*1.256637F+index*0.3F+motion.Phase()*0.035F;
            // 中心跟随布局，花瓣半径和偏移保持等比，竖屏不会变成椭圆。
            const auto center=canvas.Point(x,y);
            DrawCircleV({center.x+std::cos(angle)*13*canvas.scale,center.y+std::sin(angle)*13*canvas.scale},
                        10*canvas.scale,ui::WithOpacity(Color{208,158,126,15},opacity));
        }
    }
    for(Vector2 center : {canvas.Point(-110,130),canvas.Point(canvas.width+110,canvas.height-110)})
        DrawRing(center,220*canvas.scale,221*canvas.scale,motion.Phase()*3,motion.Phase()*3+300,80,ui::WithOpacity(Color{208,158,126,18},opacity));
    DrawPageFrame(canvas,ui::WithOpacity(Color{149,113,84,180},opacity),canvas.width,canvas.height);
}

/// @brief 页面统一页眉及左下角题签；font 借用，title/subtitle/watermark 为 UTF-8 文本，字号等比。
inline void DrawPageHeader(const Canvas& canvas,Font font,const char* title,const char* subtitle,const char* watermark) {
    const bool compact=canvas.width<1000;
    const float center=canvas.Point(canvas.width/2,0).x;
    ui::DrawCentered(font,title,center,canvas.Point(0,compact?90:72).y,
                     ui::FittedSize(font,title,35*canvas.scale,(canvas.width-120)*canvas.xScale),kPageIvory);
    ui::DrawCentered(font,subtitle,center,canvas.Point(0,compact?146:116).y,
                     ui::FittedSize(font,subtitle,17*canvas.scale,(canvas.width-120)*canvas.xScale),kPageGold);
    ui::DrawText(font,watermark,canvas.Point(compact?64:100,canvas.height-186),
                 ui::FittedSize(font,watermark,52*canvas.scale,(canvas.width-120)*canvas.xScale),Color{173,61,65,255});
}

/// @brief 页面统一页脚；secondLine 可为空，位置贴近全窗口底部，不依赖列表高度。
inline void DrawPageFooter(const Canvas& canvas,Font font,const char* firstLine,const char* secondLine="") {
    const float center=canvas.Point(canvas.width/2,0).x;
    for(int index=0;index<2;++index) {
        const char* text=index==0 ? firstLine : secondLine;
        const float size=(index==0?18:15)*canvas.scale;
        ui::DrawCentered(font,text,center,canvas.Point(0,canvas.height-(index==0?64:31)).y,
                         ui::FittedSize(font,text,size,(canvas.width-80)*canvas.xScale),
                         index==0?kPageGold:Color{163,133,121,255});
    }
}

/// @brief 统一列表选中细线；bounds 为屏幕矩形，weight/reveal 为 0～1，改键与选项共用强调样式。
inline void DrawPageRowRule(const Canvas& canvas,Rectangle bounds,const ui::MenuMotion& motion,float weight,float reveal) {
    ui::FocusLine({bounds.x+12*canvas.scale,bounds.y+bounds.height},
                  {bounds.x+bounds.width-12*canvas.scale,bounds.y+bounds.height},canvas.scale,
                  Fade(kPageGold,reveal),weight,motion.Phase());
}

/// @brief 统一操作按钮；bounds 为屏幕矩形，weight 为统一选择强调（0～1），键鼠选择都由输入层驱动。
/// opacity 为 0～1 的整层透明度，保留底色与字形原有 Alpha；仅用于展示，不改变命中范围。
inline void DrawPageButton(const Canvas& canvas,Font font,Rectangle bounds,const char* label,
                           const ui::MenuMotion& motion,float weight=0,float opacity=1) {
    const DrawContext context{font,canvas.scale,opacity,motion.Phase(),motion.Pulse()};
    Button{bounds}.Draw(context,label,{weight,1});
}
} // namespace koumakyou::ui

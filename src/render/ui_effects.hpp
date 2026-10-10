// 本文件绘制新典风格的文字光晕、太极游标、星芒与细线，坐标统一为屏幕像素。
#pragma once

#include "render/ui_typography.hpp"
#include <algorithm>
#include <cmath>

namespace koumakyou::ui {
/// @brief 按 amount（0～1）混合 RGBA，供选择颜色渐变使用。
inline Color Blend(Color from, Color to, float amount) {
    amount = std::clamp(amount, 0.0F, 1.0F);
    const auto channel = [amount](int a, int b) { return static_cast<unsigned char>(a + (b - a) * amount); };
    return {channel(from.r,to.r),channel(from.g,to.g),channel(from.b,to.b),channel(from.a,to.a)};
}
/// @brief 绘制借用字体及暖色轮廓光；size 为屏幕像素，strength 为 0～2（确认可短暂增强），文字度量不受光晕影响。
inline void GlowText(Font font, const char* text, Vector2 position, float size,
                     Color ink, Color glow, float strength) {
    if (strength > 0.01F) {
        for (int ring = 2; ring >= 1; --ring) {
            const float radius = size * 0.035F * ring;
            for (Vector2 offset : {Vector2{radius,0}, Vector2{-radius,0}, Vector2{0,radius}, Vector2{0,-radius}})
                DrawText(font,text,{position.x+offset.x,position.y+offset.y},size,Fade(glow,strength*0.11F/ring));
        }
    }
    DrawText(font,text,position,size,ink);
}
/// @brief 四角星芒；radius 为屏幕像素，颜色已含展示透明度。
inline void Spark(Vector2 center, float radius, Color color) {
    const Vector2 top{center.x,center.y-radius}, right{center.x+radius,center.y};
    const Vector2 bottom{center.x,center.y+radius}, left{center.x-radius,center.y};
    DrawTriangle(top,left,bottom,color);
    DrawTriangle(top,bottom,right,color);
}
/// @brief 带流动星芒的细线；amount（0～1）控制选中强调，phase 为秒，scale 为画布缩放比例。
inline void FocusLine(Vector2 left, Vector2 right, float scale, Color color, float amount, float phase) {
    DrawLineEx(left,right,std::max(0.6F,scale),Fade(color,0.18F+0.7F*amount));
    if(amount<=0.01F)return;
    const float drift=0.90F+0.025F*std::sin(phase*2.4F);
    const Vector2 star{left.x+(right.x-left.x)*drift,left.y};
    DrawLineEx({star.x-10*scale,star.y},{star.x+10*scale,star.y},scale,Fade(color,amount*0.35F));
    Spark(star,5*scale,Fade(color,amount));
}
/// @brief 绘制旋转太极与外环；radius 为屏幕像素，phase 为秒，background 用于阴阳反色。
inline void YinYang(Vector2 center, float radius, Color color, Color background, float phase) {
    DrawCircleLinesV(center,radius*1.25F,Fade(color,0.6F));
    DrawRing(center,radius*1.4F,radius*1.45F,phase*22,phase*22+280,40,Fade(color,0.35F));
    DrawCircleV(center,radius,color);
    DrawCircleSector(center,radius,phase*30,phase*30+180,24,background);
    const float angle=(phase*30+90)*DEG2RAD;
    const Vector2 offset{std::cos(angle)*radius*0.5F,std::sin(angle)*radius*0.5F};
    const Vector2 a{center.x+offset.x,center.y+offset.y}, b{center.x-offset.x,center.y-offset.y};
    DrawCircleV(a,radius*0.5F,color); DrawCircleV(b,radius*0.5F,background);
    DrawCircleV(a,radius*0.15F,background); DrawCircleV(b,radius*0.15F,color);
}
} // namespace koumakyou::ui

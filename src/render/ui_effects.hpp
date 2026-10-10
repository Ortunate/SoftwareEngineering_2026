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
/// @brief 将太极局部坐标转换为屏幕像素；x/y 以主体半径为单位，angle 为弧度。
inline Vector2 TaijiPoint(Vector2 center, float radius, float angle, float x, float y) {
    const float cosine=std::cos(angle),sine=std::sin(angle);
    return {center.x+radius*(x*cosine-y*sine),center.y+radius*(x*sine+y*cosine)};
}
/// @brief 绘制太极的一个横向色带；两端边界使用单位半径坐标，颜色包含选中淡入的透明度。
inline void TaijiBand(Vector2 center,float radius,float angle,float top,float bottom,
                      float leftTop,float rightTop,float leftBottom,float rightBottom,Color color) {
    const Vector2 a=TaijiPoint(center,radius,angle,leftTop,top);
    const Vector2 b=TaijiPoint(center,radius,angle,leftBottom,bottom);
    const Vector2 c=TaijiPoint(center,radius,angle,rightBottom,bottom);
    const Vector2 d=TaijiPoint(center,radius,angle,rightTop,top);
    DrawTriangle(a,b,c,color);
    DrawTriangle(a,c,d,color);
}
/// @brief 绘制旋转阴阳鱼与印章式外圈；radius 为主体屏幕像素半径，须为正，phase 为秒。
/// color/background 分别为朱红和浅色鱼；共享同一透明度，主体色带不重叠，避免淡出时出现叠色圆斑。
inline void YinYang(Vector2 center, float radius, Color color, Color background, float phase) {
    const float angle=phase*30*DEG2RAD;
    // 同一 S 曲线分别作为两条鱼的边界，保留两个直径为主体半径的完整鱼头。
    // 横向细分固定为 96 段，旋转与 Retina 缩放后仍平滑，不产生圆盘叠画的第三个斑点。
    const auto edge=[](float y) { return std::sqrt(std::max(0.0F,1-y*y)); };
    const auto split=[](float y) {
        const float local=y<0 ? y+0.5F : y-0.5F;
        const float x=std::sqrt(std::max(0.0F,0.25F-local*local));
        return y<0 ? x : -x;
    };
    for(int index=0;index<96;++index) {
        const float top=-1+index/48.0F,bottom=-1+(index+1)/48.0F;
        TaijiBand(center,radius,angle,top,bottom,-edge(top),split(top),-edge(bottom),split(bottom),color);
        TaijiBand(center,radius,angle,top,bottom,split(top),edge(top),split(bottom),edge(bottom),background);
    }
    // 显式细分鱼眼，避免 raylib 按小半径选择过少边数后呈现多边形。
    DrawCircleSector(TaijiPoint(center,radius,angle,0,-0.5F),radius*0.17F,0,360,32,background);
    DrawCircleSector(TaijiPoint(center,radius,angle,0,0.5F),radius*0.17F,0,360,32,color);
    // 细双圈与四处短刻线保持印章感；最小窗口也为主体留出清楚的留白。
    DrawRing(center,radius*1.01F,radius*1.075F,0,360,80,color);
    DrawRing(center,radius*1.19F,radius*1.23F,0,360,80,Fade(color,0.55F));
    for(int index=0;index<4;++index) {
        const float turn=angle*0.35F+index*PI/2;
        const auto inner=TaijiPoint(center,radius,turn,1.32F,0);
        const auto outer=TaijiPoint(center,radius,turn,1.45F,0);
        DrawLineEx(inner,outer,std::max(0.55F,radius*0.055F),Fade(color,0.65F));
    }
}
} // namespace koumakyou::ui

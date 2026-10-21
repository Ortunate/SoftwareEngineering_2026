// 本文件绘制新典风格的文字光晕、选择箭头、太极游标、准星、爱心、星芒与细线，坐标统一为屏幕像素。
#pragma once

#include "ui/typography.hpp"
#include <algorithm>
#include <cmath>

namespace koumakyou::ui {
/// @brief 在已有 RGBA 透明度上叠加 opacity；须为有限值，钳制到 0～1，不能重新显露已完全透明的元素。
inline Color WithOpacity(Color color,float opacity) {
    color.a = static_cast<unsigned char>(color.a * std::clamp(opacity,0.0F,1.0F));
    return color;
}
/// @brief 按 amount（0～1）混合 RGBA，供选择颜色渐变使用。
inline Color Blend(Color from, Color to, float amount) {
    amount = std::clamp(amount, 0.0F, 1.0F);
    const auto channel = [amount](int a, int b) { return static_cast<unsigned char>(a + (b - a) * amount); };
    return {channel(from.r,to.r),channel(from.g,to.g),channel(from.b,to.b),channel(from.a,to.a)};
}
/// @brief 绘制共用三角选择箭头；bounds 为固定屏幕像素命中区，direction 为 -1/1，phase 为更新侧推进的秒数。
/// opacity 为 0～1，hovered 由固定命中区计算；轻微水平晃动仅影响绘制，不能改变输入区域或在绘制时推进动画。
inline void SelectionArrow(Rectangle bounds,int direction,Color color,float opacity,float phase,bool hovered) {
    const float drift=direction*bounds.width*0.06F*std::sin(phase*3.5F);
    const Vector2 center{bounds.x+bounds.width/2+drift,bounds.y+bounds.height/2};
    const float dx=direction*bounds.width*0.28F,dy=bounds.height*0.26F;
    const Vector2 tip{center.x+dx,center.y},top{center.x-dx,center.y-dy},bottom{center.x-dx,center.y+dy};
    DrawTriangle(tip,direction<0?bottom:top,direction<0?top:bottom,
                 WithOpacity(color,opacity*(hovered?1.0F:0.6F)));
}
/// @brief 绘制借用字体及暖色轮廓光；size 为屏幕像素，strength 为 0～2（确认可短暂增强），文字度量不受光晕影响。
inline void GlowText(Font font, const char* text, Vector2 position, float size,
                     Color ink, Color glow, float strength) {
    if (strength > 0.01F) {
        for (int ring = 2; ring >= 1; --ring) {
            const float radius = size * 0.035F * ring;
            for (Vector2 offset : {Vector2{radius,0}, Vector2{-radius,0}, Vector2{0,radius}, Vector2{0,-radius}})
                DrawText(font,text,{position.x+offset.x,position.y+offset.y},size,WithOpacity(glow,strength*0.11F/ring));
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
/// @brief 绘制实心爱心；center 为包围盒中心，size 为正数屏幕像素，宽高随 size 等比缩放。
/// 使用不重叠的三角形填充连续轮廓，半透明时不会因两瓣叠画产生深色接缝，也不依赖字体字形。
inline void Heart(Vector2 center,float size,Color color) {
    const auto point=[&](float angle) {
        const float sine=std::sin(angle);
        return Vector2{center.x+size*0.5F*sine*sine*sine,
                       center.y-size*(13*std::cos(angle)-5*std::cos(2*angle)-2*std::cos(3*angle)-std::cos(4*angle)+2.5F)/29};
    };
    Vector2 previous=point(0);
    for(int index=1;index<=48;++index) {
        const auto current=point(2*PI*index/48);
        DrawTriangle(center,current,previous,color);
        previous=current;
    }
}
/// @brief 绘制回纹四角、断环与中心瞄准点；center 为准确瞄准位置，scale 为正比例，所有装饰等比缩放。
/// accent/ink/mark 分别控制框线、中心点和四向短刻线；调用方提供危险态配色，绘制不读取输入或推进动画。
inline void AimReticle(Vector2 center,float scale,Color accent,Color ink,Color mark) {
    const Color shadow{24,10,19,220};
    const auto point=[&](float x,float y) { return Vector2{center.x+x*scale,center.y+y*scale}; };
    const auto line=[&](Vector2 from,Vector2 to,Color color) {
        DrawLineEx(from,to,3*scale,shadow);
        DrawLineEx(from,to,scale,color);
    };
    // 中央留空，只有瞄准点位于射线原点；深色衬线使浅色雾景和明亮弹幕下的轮廓也可辨认。
    for(int quadrant=0;quadrant<4;++quadrant) {
        const float start=quadrant*90+20,end=start+50;
        DrawRing(center,6*scale,8.5F*scale,start,end,12,shadow);
        DrawRing(center,6.8F*scale,7.6F*scale,start,end,12,accent);
    }
    for(float x : {-1.0F,1.0F})for(float y : {-1.0F,1.0F}) {
        line(point(x*6,y*10),point(x*10,y*10),accent);
        line(point(x*10,y*10),point(x*10,y*6),accent);
    }
    for(float direction : {-1.0F,1.0F}) {
        line(point(direction*11,0),point(direction*14,0),mark);
        line(point(0,direction*11),point(0,direction*14),mark);
    }
    DrawCircleV(center,2.5F*scale,shadow);
    DrawCircleV(center,1.25F*scale,ink);
}
/// @brief 带流动星芒的细线；amount（0～1）控制选中强调，phase 为秒，scale 为画布缩放比例。
inline void FocusLine(Vector2 left, Vector2 right, float scale, Color color, float amount, float phase) {
    DrawLineEx(left,right,std::max(0.6F,scale),WithOpacity(color,0.18F+0.7F*amount));
    if(amount<=0.01F)return;
    const float drift=0.90F+0.025F*std::sin(phase*2.4F);
    const Vector2 star{left.x+(right.x-left.x)*drift,left.y};
    DrawLineEx({star.x-10*scale,star.y},{star.x+10*scale,star.y},scale,WithOpacity(color,amount*0.35F));
    Spark(star,5*scale,WithOpacity(color,amount));
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
    DrawRing(center,radius*1.19F,radius*1.23F,0,360,80,WithOpacity(color,0.55F));
    for(int index=0;index<4;++index) {
        const float turn=angle*0.35F+index*PI/2;
        const auto inner=TaijiPoint(center,radius,turn,1.32F,0);
        const auto outer=TaijiPoint(center,radius,turn,1.45F,0);
        DrawLineEx(inner,outer,std::max(0.55F,radius*0.055F),WithOpacity(color,0.65F));
    }
}
} // namespace koumakyou::ui

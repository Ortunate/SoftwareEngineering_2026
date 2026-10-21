// 本文件提供与具体页面无关的响应式画布；绘制与交互使用同一窗口逻辑像素坐标。
#pragma once
#include "raylib.h"
#include <algorithm>

namespace koumakyou::ui {
/// @brief 设计画布的窗口映射；默认等比居中，Full 将布局双轴铺满，scale 始终用于等比的文字与图案。
/// 窗口及设计尺寸按至少 1 像素处理，最小化时不除零；调用方不得用 xScale/yScale 拉伸立绘。
struct Canvas {
    float scale;
    float xScale, yScale;
    Vector2 origin;
    float width, height;
    Canvas(int windowWidth, int windowHeight, float designWidth = 1100, float designHeight = 760)
        : scale(std::min(std::max(1,windowWidth)/std::max(1.0F,designWidth),
                         std::max(1,windowHeight)/std::max(1.0F,designHeight))),
          xScale(scale),yScale(scale),origin{},width(std::max(1.0F,designWidth)),height(std::max(1.0F,designHeight)) {
        origin={(std::max(1,windowWidth)-width*scale)/2,(std::max(1,windowHeight)-height*scale)/2};
    }
    /// @brief 铺满窗口的设计画布；参数均为逻辑像素，控件矩形使用双轴比例，字体/圆形使用 scale。
    [[nodiscard]] static Canvas Full(int windowWidth,int windowHeight,float designWidth,float designHeight) {
        Canvas canvas(windowWidth,windowHeight,designWidth,designHeight);
        canvas.origin={};
        canvas.xScale=std::max(1,windowWidth)/canvas.width;
        canvas.yScale=std::max(1,windowHeight)/canvas.height;
        return canvas;
    }
    /// @brief 将设计像素坐标转换为窗口逻辑像素，不改变画布状态。
    [[nodiscard]] Vector2 Point(float x, float y) const {
        return {origin.x+x*xScale,origin.y+y*yScale};
    }
    /// @brief 将设计像素矩形转换为窗口逻辑像素，供绘制和鼠标命中共同使用。
    [[nodiscard]] Rectangle Rect(float x, float y, float width, float height) const {
        const auto point = Point(x, y);
        return {point.x,point.y,width*xScale,height*yScale};
    }
};

} // namespace koumakyou::ui

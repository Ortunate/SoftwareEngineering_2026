// 本文件定义贴齐窗口底边的功能按键横条；只展示调用方提供的键位，不接管输入或持有业务状态。
#pragma once
#include "ui/components.hpp"

namespace koumakyou::ui {
/// @brief 单项操作提示；keys/action 为 Draw 期间有效的 UTF-8 指针，keys 可为空串，enabled=false 降低亮度。
struct KeyHint { const char* keys; const char* action; bool enabled=true; };
/// @brief 固定单行高度的底部横条；尺寸为窗口逻辑像素，所有提示横向排列，窄屏也不换行。
/// 每页最多展示六项，切换提示数量不改变横条高度；仅展示，不生成按钮命中区或读取键鼠。
class CommandBar {
public:
    static constexpr int kMaxHints=6;
    /// @brief 构造窗口全宽横条，负值/零尺寸按一像素处理，避免最小化时产生无效坐标。
    CommandBar(float width,float height);
    /// @brief 给页面布局预留横条空间；返回至少一逻辑像素，3D 视口不应调用此方法缩小场景。
    [[nodiscard]] static float ContentHeight(float width,float height);
    /// @brief 横条占用的实际屏幕矩形，底边严格贴齐窗口。
    [[nodiscard]] Rectangle Bounds() const { return bounds_; }
    /// @brief 默认文字及内边距的设计缩放，可直接传给 DrawContext::scale。
    [[nodiscard]] float Scale() const { return scale_; }
    /// @brief 返回第 index 项的固定屏幕区域；count 为 1～6，越界或空列表返回零矩形。
    [[nodiscard]] Rectangle Cell(int index,int count) const;
    /// @brief 借用上下文及最多六项提示绘制横条；超出六项忽略，文字等比缩小，零透明度完全不可见。
    void Draw(const DrawContext& context,std::span<const KeyHint> hints) const;
private:
    Rectangle bounds_{}; float scale_=1;
};
} // namespace koumakyou::ui

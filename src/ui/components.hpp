// 本文件定义不依赖游戏状态的 UI 组件模板；调用方持有选择状态、字体和业务行为。
#pragma once
#include "ui/input.hpp"
#include "ui/motion.hpp"
#include <span>

namespace koumakyou::ui {
/// @brief 默认暗红/金色主题；颜色按值持有，组件不会加载纹理、播放声音或写入配置。
struct Theme {
    Color accent{233,204,139,255},text{252,246,236,255},muted{133,117,123,255};
    Color button{69,32,32,170},panel{38,12,25,225},shade{0,0,0,155};
};
/// @brief 单次绘制上下文；font 借用调用方有效字体，scale 为正比例，opacity 为 0～1，phase 单位为秒。
/// Draw 不推进时钟或修改输入；所有组件矩形为窗口逻辑像素，字体大小和内部边距乘 scale。
struct DrawContext { Font font{}; float scale=1,opacity=1,phase=0,pulse=1; Theme theme{}; };
/// @brief 展示状态与输入独立；weight/reveal 为 0～1，reveal 不改变命中区或组件可用性。
struct Visual { float weight=0,reveal=1; };
/// @brief 标签在固定矩形内的水平对齐方式，不影响鼠标命中区。
enum class TextAlign { Left, Center, Right };
/// @brief 文本标签模板；fontSize 为设计像素，等比适配 bounds 宽度且垂直居中，字符串不转移所有权。
struct Label {
    Rectangle bounds; float fontSize=22; TextAlign align=TextAlign::Left;
    /// @brief text 须为本次调用期间有效的非空 UTF-8 指针；颜色 Alpha 与上下文透明度相乘。
    void Draw(const DrawContext& context,const char* text,Color color) const;
};
/// @brief 无输入的底板模板；roundness 为 0～1，borderWidth 为设计像素，零宽度不画边框。
struct Panel {
    Rectangle bounds; float roundness=0.1F,borderWidth=0;
    /// @brief 使用上下文主题绘制底色及可选边框，无输入和资源所有权。
    void Draw(const DrawContext& context) const;
};
/// @brief 单帧组件反馈；hovered 为几何悬停，focused 为有效指针接管，activated 为首次确认，不自动执行业务。
struct Interaction { bool hovered=false,focused=false,activated=false; };
/// @brief 列表项身份仅在所属列表中唯一；禁用项保留绘制，但键鼠都跳过，bounds 不随绘制动画改变。
struct Item { int id; Rectangle bounds; bool enabled=true; };
/// @brief 列表导航使用纵向、两组方向别名或仅鼠标；None 仍允许当前项的键盘确认。
enum class Navigation { Vertical, BothAxes, None };
/// @brief 列表更新反馈；back 优先，confirmed 每帧至多一次，changed 说明调用方的 selected 已改变。
struct SelectionResult { bool changed=false,confirmed=false,back=false; };
/// @brief 更新调用方持有的选择；items 借用到函数返回，id 须唯一，空列表/全禁用时 selected=-1。
/// 按数组顺序循环且跳过禁用项；只在指针实际移动或点击时接管，空白点击不确认，返回不修改选择。
[[nodiscard]] SelectionResult UpdateSelection(std::span<const Item> items,const MenuInput& input,int& selected,
                                             Navigation navigation=Navigation::BothAxes);

/// @brief 按钮的普通填充和居中文字两种模板，共享输入语义。
enum class ButtonStyle { Filled, Text };
/// @brief 按钮模板；Filled 用于普通操作，Text 用于暂停/结果菜单；输入和绘制共用 bounds。
struct Button {
    Rectangle bounds;
    bool enabled=true;
    ButtonStyle style=ButtonStyle::Filled;
    /// @brief focused 表示键盘是否选中此按钮；返回优先，禁用时不接收输入，按住确认不会重复触发。
    [[nodiscard]] Interaction Update(const MenuInput& input,bool focused=false) const;
    /// @brief label 为本次调用内有效的 UTF-8 文本；只借用上下文与字符串，禁用时自动降低透明度。
    void Draw(const DrawContext& context,const char* label,Visual visual={}) const;
    /// @brief 返回可供 UpdateSelection 使用的相同命中区域；id 由所属列表分配。
    [[nodiscard]] Item Definition(int id) const { return {id,bounds,enabled}; }
};
/// @brief 可自定义内部内容的列表行；id 为列表局部身份，enabled 控制交互而非业务解锁的灰色外观。
struct SelectableRow {
    Rectangle bounds; int id; bool enabled=true;
    /// @brief 与 Button 相同的单帧命中语义；指针接管时将本行 id 写入 selected，便于同页跨栏更新焦点。
    [[nodiscard]] Interaction Update(const MenuInput& input,int& selected) const;
    /// @brief Update 的点击简写，不消费键盘确认；导航和一次性键盘确认应由 UpdateSelection 或所属页面处理。
    bool Select(const MenuInput& input,int& selected) const;
    /// @brief 绘制底色和强调线并返回内容颜色；available=false 只改变外观，便于浏览尚未实现的目录。
    [[nodiscard]] Color Draw(const DrawContext& context,float weight,bool available=true) const;
    /// @brief 返回借用本行几何与可用状态的列表项值，不保存外部引用。
    [[nodiscard]] Item Definition() const { return {id,bounds,enabled}; }
};
/// @brief 固定命中区的三角箭头；direction 为 -1/1，轻微左右晃动只改变绘制位置。
struct ArrowButton {
    Rectangle bounds; int direction; bool enabled=true;
    /// @brief 仅处理指针命中和点击；键盘的上一项/下一项仍由所属列表决定。
    [[nodiscard]] Interaction Update(const MenuInput& input) const;
    /// @brief 读取上下文秒制相位和当前指针，仅绘制晃动与悬停，不修改命中区或输入状态。
    void Draw(const DrawContext& context) const;
};
/// @brief 数值选择器的命中模板；bounds 包含标签/数值/箭头，左右区域只决定增减，不持有或修改业务数值。
struct ValueSelector {
    Rectangle bounds,decrease,increase; bool enabled=true;
    /// @brief 返回 -1/0/1 的调整意图；键盘聚焦时左右调整，确认/行内点击默认增加，左箭头点击减少。
    /// 返回优先，vertical 非零时不处理横向调整；调用方负责步长、钳制、声音和保存。
    [[nodiscard]] int Update(const MenuInput& input,bool focused) const;
    /// @brief 借用上下文绘制方框箭头；current/minimum/maximum 必须使用同一数值单位，边界箭头降低亮度。
    void DrawArrows(const DrawContext& context,int current,int minimum,int maximum,Visual visual={}) const;
};
/// @brief 模态底板模板；viewport/panel 为屏幕矩形，titlePosition 是标题上缘中点，不拥有子控件或页面流程。
struct Modal {
    Rectangle viewport,panel; Vector2 titlePosition;
    /// @brief 绘制遮罩、圆角底板和标题；调用方只将当前模态的输入传给子控件，避免下层接收同帧确认。
    void Draw(const DrawContext& context,const char* title,Color titleColor,float titleSize=43) const;
};
} // namespace koumakyou::ui

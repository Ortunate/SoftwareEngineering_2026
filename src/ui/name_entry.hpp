// 本文件定义独立命名界面的状态、ASCII编辑输入和响应式布局；不依赖存档、游戏或平台输入法。
#pragma once
#include "ui/components.hpp"
#include "ui/layout.hpp"
#include <string>
#include <string_view>

namespace koumakyou::ui {
/// @brief 调用方持有的编辑状态；名称由外部string独占，replaceOnType使下一次合法输入替换整串。
/// selected为字符按钮或操作按钮的局部编号，不用于MenuMotion的16槽权重。
struct NameEntryState {
    static constexpr int kColumns=16,kKeyCount=95,kSave=kKeyCount,kBack=kSave+1,kDelete=kSave+2,kItemCount=kSave+3;
    static constexpr int kRows=(kKeyCount+kColumns-1)/kColumns;
    int selected=kSave;
    bool replaceOnType=false;
};
/// @brief 命名长度按可打印ASCII字节计数，maxLength须大于0；两端空格可编辑，但全空格不是合法名称。
struct NameEntryConfig { std::size_t maxLength=8; };
/// @brief 一帧命名输入；controls仅接受原始方向键、Enter/小键盘Enter、Esc和鼠标，不使用字母菜单别名。
/// text保存完整码点供ASCII过滤，Read消费本帧字符队列；无窗口测试可直接构造快照。
struct NameEntryInput {
    MenuInput controls{};
    std::u32string text;
    bool erase=false,selectAll=false,next=false;
    static NameEntryInput Read();
};
/// @brief 编辑阶段不触发业务；Submit请求调用方校验并保存，Cancel请求返回上级。
enum class NameEntryAction { None, Submit, Cancel };
/// @brief 返回编辑、焦点和按钮激活反馈；Submit仅表达用户意图，合法性及保存结果由调用方处理。
struct NameEntryResult { NameEntryAction action=NameEntryAction::None; bool edited=false,selectionChanged=false,activated=false; };
/// @brief 本次Draw期间借用的UTF-8标签；error可为空，组件不保存这些指针。
struct NameEntryText { const char* title="名称";const char* subtitle="";const char* save="保存";const char* error=""; };
/// @brief 公用命名布局；构造参数为窗口逻辑像素，避让底部CommandBar，全部矩形用于绘制和命中。
struct NameEntryLayout {
    Canvas canvas;
    int width,height;
    bool compact;
    NameEntryLayout(int windowWidth,int windowHeight);
    Rectangle Panel() const;
    Rectangle Field() const;
    /// @brief index为0～94，依次为A～Z、a～z、0～9、其余可打印符号及空格；其他编号不属于字符按钮。
    Rectangle Character(int index) const;
    /// @brief index为0保存、1返回、2删除；局部焦点编号对应NameEntryState::kSave起的三个编号。
    Rectangle Action(int index) const;
};
/// @brief 可独立链接的命名界面；只编辑调用方名称和状态，既不保存文件，也不管理音效或系统输入法。
/// Update只接收快照；Draw/DrawHints只读状态，字体借用DrawContext，绘制须在调用方绘制阶段执行。
class NameEntry {
public:
    explicit NameEntry(const NameEntryLayout& layout):layout_(layout){}
    NameEntryResult Update(std::string& value,NameEntryState& state,const NameEntryConfig& config,const NameEntryInput& input) const;
    void Draw(const DrawContext& context,const std::string& value,const NameEntryState& state,const NameEntryConfig& config,const NameEntryText& text) const;
    void DrawHints(const DrawContext& context) const;
    /// @brief 只接受1～maxLength个0x20～0x7e字符且至少一个非空格；无副作用。
    static bool Valid(std::string_view value,const NameEntryConfig& config);
private:
    NameEntryLayout layout_;
};
} // namespace koumakyou::ui

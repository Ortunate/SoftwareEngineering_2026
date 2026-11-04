// 本文件定义Start、Extra Start及Score查询共用的五难度札绘制组件。
#pragma once
#include "ui/components.hpp"

namespace koumakyou::ui {
/// @brief bounds为窗口逻辑像素，difficulty为Easy～Extra的0～4；不持有字体，不处理解锁或输入。
struct DifficultyCard {
    Rectangle bounds;
    int difficulty;
    bool available=true;
    /// @brief 使用共用名称、中文说明及面数绘制；selected控制纸色与墨色，透明度读取context。
    /// 非法难度不绘制；命中区域始终为bounds，入场位移由页面单独计算。
    void Draw(const DrawContext& context,bool selected) const;
};
} // namespace koumakyou::ui

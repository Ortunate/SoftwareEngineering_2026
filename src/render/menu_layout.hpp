// 本文件定义菜单/弹窗共用的绘制与点击布局；二级页面按窗口双轴铺满，文字和图案仍等比缩放。
#pragma once

#include "raylib.h"
#include <algorithm>
#include <array>

namespace koumakyou::menu {
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

/// @brief 根据窗口逻辑像素宽高选择纵向/横向铺满画布，绘制与点击必须使用相同实例配置。
inline Canvas PageCanvas(int width,int height) {
    const bool compact=std::max(1,width)<std::max(1,height)*1.15F;
    return Canvas::Full(width,height,compact?760:1100,compact?1100:760);
}

inline constexpr int kOptionsCount = 9;
inline constexpr int kNumericOptions = 6;
inline constexpr int kBindingsIndex = 6;
inline constexpr int kResetIndex = 7;
inline constexpr int kBackIndex = 8;
/// @brief 暂停菜单的局部顺序；输入导航、鼠标命中与绘制必须使用相同项数。
inline constexpr int kPauseCount = 3;
inline constexpr std::array<const char*,kPauseCount> kPauseLabels{
    "解除暂停","返回主菜单","从头再来"};
/// @brief 结算操作的局部顺序；输入与绘制共用，结算时默认选择重新开始。
inline constexpr int kResultCount = 2;
inline constexpr std::array<const char*,kResultCount> kResultLabels{"从头再来","返回主菜单"};
// 前三项沿用原有顺序；新增项目位于其后，编号不属于持久化配置格式。
inline constexpr std::array<const char*, kOptionsCount> kOptionLabels{
    "FPS LIMIT", "BGM Vol", "MOUSE SENSITIVITY", "SE Vol", "Player", "Bomb",
    "按键设置", "恢复默认设置", "Quit"};
/// @brief 第 index 行的屏幕矩形；index 为 0～8，菜单的文字与命中共用此处定义。
inline Rectangle OptionRow(const Canvas& canvas, int index) {
    return canvas.width<1000 ? canvas.Rect(48,220+index*76,664,64)
                             : canvas.Rect(310,160+index*48,625,42);
}
/// @brief 数值行的屏幕箭头矩形；index 为 0～5，direction 为 -1（减小）或 1（增大）。
inline Rectangle OptionArrow(const Canvas& canvas, int index, int direction) {
    return canvas.width<1000 ? canvas.Rect(direction<0?420:664,230+index*76,40,40)
                             : canvas.Rect(direction<0?670:886,165+index*48,32,32);
}

/// @brief 开局、音乐、成绩和制作信息的响应式布局；构造参数为窗口逻辑像素。
/// 操作按钮和音乐行返回屏幕矩形；Choice 返回设计矩形供动画与文字定位。
struct PageLayout {
    Canvas canvas;
    bool compact;
    PageLayout(int width,int height):canvas(PageCanvas(width,height)),compact(canvas.width<1000) {}
    [[nodiscard]] Rectangle Back() const { return canvas.Rect(compact?40:70,compact?1010:675,compact?210:170,42); }
    [[nodiscard]] Rectangle Forward() const { return canvas.Rect(compact?380:775,compact?1010:675,compact?340:250,42); }
    /// @brief 选择卡片的设计矩形，index 为 0/1；动画和文字使用它的设计坐标，输入转换后命中。
    [[nodiscard]] Rectangle Choice(int index) const { return {compact?48.0F:640.0F,(compact?680.0F:300.0F)+index*125,compact?664.0F:385.0F,105}; }
    /// @brief 音乐行的屏幕矩形，index 为 0～2。
    [[nodiscard]] Rectangle Track(int index) const { return canvas.Rect(compact?48:280,(compact?280:260)+index*(compact?150:95),compact?664:680,compact?100:70); }
};

/// @brief 改键列表及录入弹窗共用布局；构造参数为窗口逻辑像素，所有方法返回屏幕矩形。
/// 12 项局部编号含 10 个动作、恢复和返回，只有动作使用 Row。
struct BindingsLayout {
    Canvas canvas;
    bool compact;
    BindingsLayout(int width,int height)
        :canvas(PageCanvas(width,height)),compact(canvas.width<1000) {}
    /// @brief 动作行的屏幕矩形，index 为 0～9。
    [[nodiscard]] Rectangle Row(int index) const { return canvas.Rect(compact?48:240,(compact?220:176)+index*(compact?52:31),compact?664:620,compact?48:28); }
    /// @brief 页面底部按钮，index 为 0（恢复）或 1（返回）。
    [[nodiscard]] Rectangle Button(int index) const { return canvas.Rect(compact?48+index*342:240+index*320,canvas.height-130,compact?322:300,42); }
    [[nodiscard]] Rectangle Modal() const { return canvas.Rect((canvas.width-600)/2,canvas.height/2-100,600,200); }
};

/// @brief 游戏叠加层的缩放与暂停、结算命中区域；构造参数为窗口逻辑像素，返回矩形亦为屏幕像素。
/// 独立于 3D 视口，窄屏仍居中且完整显示。
struct OverlayLayout {
    Canvas canvas;
    OverlayLayout(int width,int height):canvas(width,height,760,620) {}
    [[nodiscard]] Rectangle PausePanel() const { return canvas.Rect(135,180,490,118+kPauseCount*48); }
    /// @brief 暂停操作行，index 为 0～kPauseCount-1，绘制与点击共用。
    [[nodiscard]] Rectangle PauseRow(int index) const { return canvas.Rect(170,270+index*48,420,42); }
    /// @brief 结算操作行，index 为 0～kResultCount-1，绘制与点击共用。
    [[nodiscard]] Rectangle ResultRow(int index) const { return canvas.Rect(170,430+index*48,420,42); }
};
/// @brief 标题页的响应式布局；使用完整窗口，方形/竖屏将标题放到菜单上方。
/// width/height 为窗口逻辑像素，须为正数；行数包含 Debug 专用项，绘制和点击必须共用。
/// canvas 坐标原点在窗口左上角，不采用其他菜单页的固定画布居中留边。
struct TitleLayout {
    Canvas canvas;
    float width;
    float height;
    bool compact;
    float menuLeft;
    float menuTop;
    float menuWidth;
    int rowCount;

    TitleLayout(int windowWidth, int windowHeight, int count)
        : canvas(windowWidth, windowHeight), compact(std::max(1,windowWidth)<std::max(1,windowHeight)*1.2F), rowCount(count) {
        windowWidth=std::max(1,windowWidth); windowHeight=std::max(1,windowHeight);
        if (compact) canvas.scale = std::min(windowWidth / 900.0F, windowHeight / 900.0F);
        canvas.xScale=canvas.yScale=canvas.scale;
        canvas.origin = {};
        width = windowWidth / canvas.scale;
        height = windowHeight / canvas.scale;
        menuWidth = compact ? 390.0F : 315.0F;
        menuLeft = width - menuWidth - (compact ? 28.0F : 40.0F);
        // 窄屏先为标题保留 240 像素；最后一行与窗口底部之间保留留白，避免菜单贴住边框。
        menuTop = compact ? std::max(240.0F, (height - count * 45 - 100 + 240) / 2)
                          : 172 + (height - 760) / 2;
    }
    /// @brief 菜单行的窗口点击矩形；index 为 0～rowCount-1，行距为设计像素。
    [[nodiscard]] Rectangle Row(int index) const {
        return canvas.Rect(menuLeft, menuTop + index * 45, menuWidth, 40);
    }
    /// @brief 透明字标的屏幕矩形；纹理尺寸为正数像素，按窗口中心等比缩放。
    /// 宽屏可使用菜单左侧空间放大字标；较窄窗口只占菜单上方，始终避开菜单和边框。
    [[nodiscard]] Rectangle Logo(int textureWidth, int textureHeight) const {
        const float top = compact ? 38.0F : 32.0F;
        const float sideWidth = 2 * (menuLeft - width / 2 - 20);
        const bool besideMenu = !compact && sideWidth >= 400;
        const float maximumWidth = std::min(width - 112, besideMenu ? std::min(680.0F, sideWidth) : 680.0F);
        const float maximumHeight = besideMenu ? 240.0F :
            std::min(compact ? 168.0F : 180.0F, menuTop - top - 12);
        const float factor = std::min(maximumWidth / textureWidth, maximumHeight / textureHeight);
        const float logoWidth = textureWidth * factor, logoHeight = textureHeight * factor;
        return canvas.Rect((width - logoWidth) / 2, top, logoWidth, logoHeight);
    }
    /// @brief 等比铺满窗口的素材裁剪矩形（纹理像素）；纹理尺寸须为正，保留左侧人物。
    [[nodiscard]] Rectangle BackgroundSource(int textureWidth, int textureHeight) const {
        const float cover = std::max(width / textureWidth, height / textureHeight);
        const float sourceWidth = width / cover;
        const float sourceHeight = height / cover;
        return {0, (textureHeight - sourceHeight) * 0.25F, sourceWidth, sourceHeight};
    }
    /// @brief 分层背景的屏幕矩形；尺寸为正数纹理像素，relativeScale/offset 相对于铺满窗口的场景画布。
    /// 环境允许越过窗口边缘以等比覆盖，左边固定；offset 的 x/y 分别以场景宽/高为单位。
    [[nodiscard]] Rectangle LayerFrame(int textureWidth, int textureHeight,
                                       float relativeScale = 1, Vector2 offset = {}) const {
        const float factor = std::max(width / textureWidth, height / textureHeight);
        const float sceneWidth = textureWidth * factor, sceneHeight = textureHeight * factor;
        return canvas.Rect(offset.x * sceneWidth, (height - sceneHeight) * 0.25F + offset.y * sceneHeight,
                           sceneWidth * relativeScale, sceneHeight * relativeScale);
    }
    /// @brief 红月的屏幕矩形；logo 为当前字标屏幕矩形，月球中心与字标中心重合。
    /// 透明素材中月球中心为画布的 (0.34,0.42)、直径约为高度的 0.62；不能用整张透明画布中心定位。
    [[nodiscard]] Rectangle MoonFrame(int textureWidth, int textureHeight, Rectangle logo) const {
        const float diameter = std::min({320 * canvas.scale, logo.width * 0.65F, logo.height * 1.3F});
        const float factor = diameter / (textureHeight * 0.62F);
        const float frameWidth = textureWidth * factor, frameHeight = textureHeight * factor;
        return {logo.x + logo.width / 2 - frameWidth * 0.34F,
                logo.y + logo.height / 2 - frameHeight * 0.42F, frameWidth, frameHeight};
    }
    /// @brief 灵梦层独立等比定位；透明画布左侧约 58% 为人物，窄屏缩小以保留完整人物轮廓。
    [[nodiscard]] Rectangle ReimuFrame(int textureWidth, int textureHeight) const {
        auto frame = LayerFrame(textureWidth, textureHeight, 0.73F, {0.025F, 0.16F});
        if (compact) {
            const float factor = std::min(1.0F, (canvas.Rect(0,0,width-56,1).width) / (frame.width * 0.58F));
            frame.width *= factor;
            frame.height *= factor;
            frame.x = canvas.Point(28,0).x;
            frame.y = canvas.Point(0,220).y;
        }
        return frame;
    }
};
#ifdef KOUMAKYOU_DEBUG_FEATURES
inline constexpr int kTitleRowCount = 11;
#else
inline constexpr int kTitleRowCount = 10;
#endif
}  // namespace koumakyou::menu

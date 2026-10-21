# UI 模块与公共接口

`src/ui` 是独立的即时绘制组件模块。公共入口为 `#include "ui/ui.hpp"`，CMake 目标为 `koumakyou_ui`；新程序可直接 `target_link_libraries(my_app PRIVATE koumakyou_ui)`，无须链接游戏引擎。模块只依赖 C++20 标准库和 raylib，不包含 `Game`、关卡、配置或素材管理器，也不加载文件、播放声音、移动鼠标或修改游戏状态。

## 文件与组件

| 文件 | 接口与职责 |
|---|---|
| `ui.hpp` | 完整公共入口；也可只包含所需的细分头文件 |
| `input.hpp` | `MenuInput` 单帧快照，统一 WASD/方向键、Z/Space/Enter/左键、X/Esc/右键 |
| `layout.hpp` | `Canvas` 等比或全窗口坐标映射，零窗口尺寸保护 |
| `components.hpp/.cpp` | `Label`、`Panel`、`Button`、`SelectableRow`、`ArrowButton`、`ValueSelector`、`Modal`，以及 `UpdateSelection` 列表交互 |
| `command_bar.hpp/.cpp` | `CommandBar` 与 `KeyHint`，全宽贴底的上下文按键提示和内容避让区域 |
| `motion.hpp` | `MenuMotion` 选择渐变、页面进场、确认闪光和可重复动画时钟 |
| `typography.hpp` | UTF-8 文字测量、绘制、居中与宽度适配 |
| `effects.hpp` | 光晕、选择箭头、强调线、星芒与太极装饰 |
| `page.hpp` | 共用页面背景、边框、页眉、页脚及按钮模板 |

`DrawContext` 借用字体，携带缩放、透明度、主题颜色、动画秒数与呼吸强度。`Visual` 提供选中权重和显现进度。控件不会持有字体或纹理句柄的所有权，调用方负责其有效期；绘制必须处于调用方的 raylib 绘制阶段。

所有控件的 `bounds` 都是窗口逻辑像素，输入与绘制使用同一矩形。字体大小和内部边距使用设计像素并乘 `DrawContext::scale`；立绘等业务图片仍由页面布局等比摆放。动画只偏移绘制位置，不改变输入区域；绘制和调整窗口尺寸不会推进组件时钟。

## 交互约定

- `UpdateSelection(items, input, selected, navigation)` 借用 `Item` 列表，更新调用方持有的选中 ID，返回 `changed/confirmed/back`。ID 在该列表内唯一，导航按数组顺序循环；禁用项由键鼠共同跳过，空列表或全部禁用时选中 ID 为 `-1`，不会确认。
- `Button::Update` 返回悬停、指针接管和确认；`Definition(id)` 将同一个按钮的命中区交给列表。`ButtonStyle::Text` 用于暂停及结果操作，`Filled` 用于普通操作。
- `SelectableRow::Update` 适合符卡等自定义内容行：悬停接管时更新本行 ID，调用方据此切换栏目焦点。`Select` 是只返回本行点击的简写；`Draw` 绘制底色和强调并返回内容颜色。
- `ArrowButton` 同时提供点击交互和固定命中区内的轻微晃动绘制。
- `ValueSelector::Update` 仅返回增减意图 `-1/0/1`，`DrawArrows` 负责边界外观；步长、钳制、数值文本、音效及保存由页面负责。
- `Modal` 仅绘制遮罩、圆角底板和标题。调用方只向当前模态传入输入，并在关闭或打开新页面后结束本帧，避免操作穿透。

返回优先于确认；指针只有实际移动或点击时才接管焦点，静止指针不会覆盖键盘导航。确认只响应首次按下，空白点击不确认。`Item::enabled` 控制交互，`SelectableRow::Draw` 的 `available` 只控制灰色外观：尚未实现但允许浏览的目录仍可响应悬停，实际启动限制由业务层判定。

## 统一底部功能按键横条

`CommandBar(width, height)` 接收实际窗口逻辑尺寸，`Draw(context, hints)` 借用最多六个 `KeyHint{keys, action, enabled}`。横条只展示，不处理点击；调用方仍使用 `MenuInput` 和页面路由处理实际功能。高度固定为一行正文加上下内边距（28 个设计像素），所有提示横向排列，窄屏也不换行，文字按各项可用宽度缩小；提示数量变化不改变占位，零透明度不残留底色和文字。

```cpp
const ui::CommandBar bar(windowWidth, windowHeight);
const std::array<ui::KeyHint, 3> hints{{
    {"W/S", "选择"}, {"Z / Space / 左键", "确认"}, {"X / Esc / 右键", "返回"}
}};
bar.Draw({font, bar.Scale()}, hints);
const float contentHeight = ui::CommandBar::ContentHeight(windowWidth, windowHeight);
```

`Bounds()` 给出实际占位，`Cell(index, count)` 用于验证布局，`ContentHeight()` 为页面控件预留安全高度。菜单、开局、符卡练习及弹窗在该高度内排版；战斗仍绘制全窗口 3D，仅底部台词、Boss 条、提示和 FPS 避让横条。极小窗口保留至少一像素内容，最小化不绘制。

游戏仅在 `Game::Draw()` 结束时绘制一次横条，覆盖首页进场、设置/改键及行内录入、普通/整关/符卡练习、音乐室、成绩、制作信息、加载及失败、战斗、对话、暂停、收尾和结算。`src/render/command_bar.cpp` 根据页面提供提示，战斗读取当前自定义绑定。旧页面功能按键说明已统一到横条，符卡的未实现说明和结算倒计时保留为正文信息。

Option 的 Quit、Music Room 的返回项、Score 的返回按钮和 Key Bindings 的返回按钮均移除，且不保留导航槽或隐藏命中区；使用 X/Esc/鼠标右键返回。音乐室只循环三首曲目；改键页包含十项绑定，恢复默认作为同一样式的最后一行，绘制和鼠标命中共用列表布局。Score 的确认键不再兼任返回。横条内的“返回”文字也不作为可点击按钮。

## 最小使用方式

以下代码分别放入调用方的更新与绘制阶段。`font` 由调用方加载，`selected` 与 `motion` 跨帧保存；`buttons` 中的矩形来自同一个窗口布局。

```cpp
#include "ui/ui.hpp"
#include <array>
namespace ui = koumakyou::ui;

int selected = 0;
ui::MenuMotion motion;
const std::array<ui::Button, 2> buttons{{
    {{100, 120, 280, 44}},
    {{100, 180, 280, 44}}
}};

// 更新阶段：dt 为秒；只采集一次输入，业务行为由调用方处理。
const auto input = ui::MenuInput::Read();
const std::array<ui::Item, 2> items{
    buttons[0].Definition(0), buttons[1].Definition(1)
};
const auto result = ui::UpdateSelection(items, input, selected);
if (result.back) { /* 返回上级，然后结束本帧 */ }
else if (result.confirmed) { /* 按 selected 执行业务，然后结束本帧 */ }
motion.Advance(0, selected, dt);

// 绘制阶段：所有字体和状态均借用，Draw 不修改选择或推进动画。
const ui::DrawContext paint{font, 1, 1, motion.Phase(), motion.Pulse()};
buttons[0].Draw(paint, "Start", {motion.Weight(0), motion.Reveal()});
buttons[1].Draw(paint, "Back", {motion.Weight(1), motion.Reveal()});
```

`UpdateSelection` 支持任意唯一整数 ID；`MenuMotion` 的权重容量为 16，`Weight` 仅接受局部槽号 0～15，越界返回零。使用任意 ID 或分页目录时，调用方须映射为当前列表的局部动画槽。`Advance` 的页面编号只用于区分展示生命周期，不写入配置；同页两栏分别保存各自的 `MenuMotion`，焦点递进不更换页面编号。

## 游戏侧接入与职责

首页、音乐室、Option、键位设置、暂停和结果菜单使用 `UpdateSelection`；数值设置使用 `ValueSelector`；Start/Practice Start 与 Spell Practice 的角色箭头使用 `ArrowButton`；符卡同页两栏使用 `SelectableRow`；暂停和练习结果使用 `Modal` 和文字按钮。公共画布、字体、装饰和页面模板已从 `src/app`、`src/render` 移入本模块，旧路径不再保留重复实现。

`src/render/*_layout.hpp` 保留具体页面区域、标签与项数，`src/render/*_render.cpp` 负责组合控件与业务内容。`Game` 仍负责页面路由、练习解锁、角色选择记忆、存档、音效和加载等业务行为；首页专用入场、开局流程混合与对话发言动画保留在页面层，不反向引入通用 UI 模块。

## 验证

```bash
cmake --build --preset macos-release --target koumakyou_ui_tests koumakyou_tests
ctest --preset macos-release --output-on-failure
cmake --build --preset macos-release --target koumakyou_settings_ui_tests
./build/macos-release/koumakyou_settings_ui_tests output/stage1-review/ui-module
```

`koumakyou_ui_tests` 只链接 `koumakyou_ui` 和 raylib，不创建窗口，也不加载游戏或素材；验证横条在极小/宽/窄窗口的单行占位、横向排列与边界，以及独立导航、禁用与空列表、返回优先、重叠命中、静止指针、跨栏焦点、箭头、数值选择及画布/时钟。原生窗口测试先在不创建 Game 的条件下用默认字体绘制全部基础组件，核验零透明度不会泄漏底色、文字或光晕，绘制不推进时钟也不移动指针，并输出 `ui-components.png`；可用 `--components-only` 单独执行。完整测试再通过正式页面验证模态和两栏交互、全部键鼠别名、进场动画及五种窗口比例。

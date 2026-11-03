# UI 模块与公共接口

`src/ui` 是独立的即时绘制组件模块。公共入口为 `#include "ui/ui.hpp"`，CMake 目标为 `koumakyou_ui`；新程序可直接 `target_link_libraries(my_app PRIVATE koumakyou_ui)`，无须链接游戏引擎。模块只依赖 C++20 标准库和 raylib，不包含 `Game`、关卡、配置或素材管理器，也不加载文件、播放声音、移动鼠标或修改游戏状态。

## 文件与组件

| 文件 | 接口与职责 |
|---|---|
| `ui.hpp` | 完整公共入口；也可只包含所需的细分头文件 |
| `input.hpp` | `MenuInput` 单帧快照，统一 WASD/方向键、Z/Space/Enter/左键、X/Esc/右键 |
| `layout.hpp` | `Canvas` 等比或全窗口坐标映射，零窗口尺寸保护 |
| `components.hpp/.cpp` | `Label`、`Panel`、`Button`、`SelectableRow`、`ArrowButton`、`ValueSelector`、`Modal`，以及 `UpdateSelection` 列表交互 |
| `name_entry.hpp/.cpp` | `NameEntry`公用ASCII命名界面，包含快照输入、编辑状态、字符按钮、操作意图与响应式布局 |
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

## Score 查询

从主菜单进入Score先显示Easy、Normal、Hard、Lunatic、Extra五种难度札，使用`ui::DifficultyCard`，名称、配色、中文说明、面数和花饰与Start／Extra Start保持一致。五张卡片错峰滑入，确认后进入该难度成绩页；查询不要求已有成绩或关卡解锁。返回先退到难度选择，再返回主菜单。成绩浏览使用公用箭头翻页及全局按键提示。

成绩页按灵梦·灵之御札、灵梦·梦之御札、魔理沙·魔之御札、魔理沙·恋之御札顺序分为四页。每页并排展示该机体的标准模式与挑战模式前十名，完整机体名和各自模式名显示在对应列表上方。左右方向键、A/D及列表两侧垂直居中的公用箭头循环翻页，底部显示当前页码；没有机体选项卡或模式切换。难度只在上一层选择，查询不改开局记忆、实际自机或存档。

`render/score_layout.hpp`统一双栏表格、上方信息、页码和箭头的布局及命中区域。`ScoreViewStep`区分难度选择与成绩查看；本局命名保存后的成绩页直接显示本局难度和机体，并仅高亮本局模式的保存名次，确认／返回继续原Demo选存流程。

`koumakyou_postgame_lifecycle_tests <截图目录> --score-only`验证五难度×四页×两模式、真实菜单入口、键鼠循环翻页、逐级返回、只读存档与六种窗口比例；`--names-only`继续覆盖命名后直接查看成绩及Demo交接。

## 公用命名界面

`NameEntry`同时用于分数保存与Demo（Replay）保存命名，组件只依赖独立UI库。调用方持有`std::string`与`NameEntryState`，通过`NameEntryConfig{maxLength}`设置长度：两入口均只接受可打印ASCII（0x20～0x7e），不能保存空名称或全空格名称；两者上限均为8。

`NameEntryInput::Read()`读取原始方向键、Enter/小键盘Enter、Esc/右键、Backspace（含重复）、Ctrl+A、Tab及字符队列。字母WASD、Z、X和空格仅作为文字，不使用普通菜单别名；非ASCII码点忽略。无窗口测试可以直接构造输入快照。返回优先于编辑/确认，Ctrl+A标记下一次有效字符替换整串；直接输入后焦点回到保存按钮。屏幕字符按钮、空格、删除和物理键盘共用同一编辑逻辑。

```cpp
// value和state由页面跨帧保存；初次打开、预填名称时可设置replaceOnType=true。
ui::NameEntryState state{.replaceOnType=true};
std::string value="PLAYER";
ui::NameEntry entry({windowWidth,windowHeight});
const ui::NameEntryConfig config{8};
const auto result=entry.Update(value,state,config,ui::NameEntryInput::Read());
if(result.action==ui::NameEntryAction::Cancel) { /* 返回选存，保留value */ }
if(result.action==ui::NameEntryAction::Submit && ui::NameEntry::Valid(value,config)) {
    /* 由调用方保存；失败保留value并提供error，成功后再切换页面。 */
}
// 以下仅在绘制阶段调用；重建窗口布局不会重置名称或焦点。
entry.Draw(context,value,state,config,{"名称","","保存",error.c_str()});
entry.DrawHints(context);
```

`NameEntryLayout`返回输入框、95个可打印ASCII字符按钮（16列，涵盖大小写、数字、符号和空格）和三个操作按钮的窗口像素区域，横竖屏共用同一布局。`Draw`和`DrawHints`只读调用方状态并借用字体，不保存文件、不播放声音、不修改页面、不控制系统输入法。保存格式仍归业务层管理，历史录像的UTF-8元数据读取保持兼容；新建名称通过ASCII界面约束后保存。

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

Option 的 Quit、Music Room 的返回项、Score 的返回按钮和 Key Bindings 的返回按钮均移除，且不保留导航槽或隐藏命中区；使用 X/Esc/鼠标右键返回。音乐室浏览现有17首曲目，左右方向键及公用箭头翻页，上下方向键和滚轮选曲；改键页包含十项绑定，恢复默认作为同一样式的最后一行，绘制和鼠标命中共用列表布局。从主菜单浏览 Score 时确认键进入所选难度，成绩页的确认键不兼任返回；本局保存后的 Score 确认键继续进入 Demo 保存询问。横条内的“返回”文字也不作为可点击按钮。

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

命名界面一键入口：macOS双击`tools/test_name_entry.command`，或执行`python3 tools/test_name_entry.py`。脚本自动配置并构建所需测试目标、运行独立UI单元测试，然后打开可操作窗口；F1切换分数命名，F2切换Demo命名，F12截图，F10或关闭窗口退出。两种保存均只写入临时目录，退出清理，不影响实际记录。

`python3 tools/test_name_entry.py --auto`运行两个实际入口、字符按钮、ASCII过滤、统一8字符限制、取消保留及保存失败重试的自动回归；`--smoke`短暂打开窗口并依次截图两种场景后自动退出。截图及`run.log`默认保存在`output/name-entry-review/`，可用`--output`指定路径；`--build-dir`指定已有CMake构建目录，默认`build-debug`。脚本不依赖当前工作目录，路径含空格或中文也可运行，需已安装Python 3和CMake；首次配置还需C++20编译器及raylib下载环境。

`koumakyou_ui_tests` 只链接 `koumakyou_ui` 和 raylib，不创建窗口，也不加载游戏或素材；验证横条在极小/宽/窄窗口的单行占位、横向排列与边界，以及独立导航、禁用与空列表、返回优先、重叠命中、静止指针、跨栏焦点、箭头、数值选择及画布/时钟。原生窗口测试先在不创建 Game 的条件下用默认字体绘制全部基础组件，核验零透明度不会泄漏底色、文字或光晕，绘制不推进时钟也不移动指针，并输出 `ui-components.png`；可用 `--components-only` 单独执行。完整测试再通过正式页面验证模态和两栏交互、全部键鼠别名、进场动画及五种窗口比例。

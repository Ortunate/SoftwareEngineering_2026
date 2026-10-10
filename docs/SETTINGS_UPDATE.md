# Settings 更新说明与验收

## 整合基线

本次以 `dev/koumakyou` 的 `374dbbf` 为基础整合 Settings。保留道具掉落与持续追踪、Power 成长、直射/追踪弹以及 Debug 射击测试场；没有用旧版文件覆盖这些功能。

鼠标灵敏度通过 `PlayerInput.mouseSensitivityScale` 传入封装后的 Player。F3～F6 保留给现有调试功能，避免重新绑定时同时触发。新增灵敏度倍率回归测试，同时将玩家测试的大型弹幕池改为堆上分配，避免 Windows 默认栈空间不足。

## 修改范围

| 模块 | 修改 |
|---|---|
| config/key_bindings.hpp | 键盘/鼠标统一编码、合法性校验、输入查询、冲突交换；Focus 默认 Mouse 4；射击/Bomb 副键可编辑 |
| config/settings.* | BGM 音量、灵敏度保存；version=1 升级；无效值回退 |
| app/game.*、app/input.cpp、app/constants.hpp | 设置应用、玩家重置后保持灵敏度、鼠标录入防误触、设置页布局 |
| resources/asset_manager.* | 当前及未来加载的三类 BGM 统一应用音量 |
| render/game_render.cpp | 音量/灵敏度数值、鼠标键名称和录入提示 |
| CMakeLists.txt、src/windows_utf8.manifest | Windows/MSVC 自动嵌入 UTF-8 清单；测试采用 UTF-8 编译 |
| tests/settings_tests.cpp、tests/settings_ui_tests.cpp | 设置边界、迁移、鼠标绑定及隐藏窗口交互回归 |

## 验证结果

- VS 2026 / CMake 4.4.3 / raylib 6.0 的 Release 构建通过。
- CTest 核心测试通过，覆盖原玩法/关卡测试及新增设置持久化、非法输入回退、版本迁移、键鼠冲突交换。
- 隐藏窗口回放通过：音量 0～100、灵敏度 0.1～3.0、点击录入防误触、鼠标侧键绑定、鼠标/键盘重绑定、Esc 取消、持续按住/首次按下/松开分发、重新创建游戏后进入关卡。
- 900×620 最小窗口的设置页、12 行绑定列表和改键弹窗截图已检查，文字无遮挡。
- 中文素材路径使用应用 UTF-8 清单；未更改系统编码或音乐文件名。
- 物理鼠标侧键与厂商驱动的映射仍需在用户设备上确认。部分驱动会把侧键转换成键盘宏，此时界面显示实际收到的键码。

## 手工验收

1. 在 OPTIONS 调整音乐为 0%，确认静音；恢复 50%，确认标题音乐恢复。开始关卡并进入 Boss，确认音量设置保持。
2. 比较 0.5x/1.0x/2.0x 下相同鼠标位移的视角变化；重开、返回主菜单再开始，设置应保持。
3. 按住默认 Mouse 4，确认 Focus 低速；松开恢复。无侧键时在 KEY BINDINGS 改为 C。
4. 将 Focus 改成左键：FIRE (ALTERNATE) 应与原 Focus 键交换，左键不应同时触发射击。
5. 测试鼠标右键、中键和另一颗侧键录入，Esc 取消不改变原绑定。
6. 关闭后从同一目录重启，确认全部设置恢复。RESET DEFAULTS 只恢复绑定。

## 复现测试

配置并构建后执行：

```powershell
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target koumakyou_settings_ui_tests
.\build\Release\koumakyou_settings_ui_tests.exe
```

图形集成测试需要 OpenGL 和 raylib 自动化事件支持；创建隐藏窗口并静音，使用独立临时配置，不覆盖玩家设置。源码仍默认下载 raylib 5.5，本次实际验证使用6.0，未进行 macOS 构建验证。

## 2026-10-01 箭头点击区域修正

- 三项数值设置使用独立的 32×32 像素箭头按钮，绘制与命中测试共用同一个矩形。
- 点击标签、数值、行内空白或按钮外不会改变数值；按钮有悬停高亮，达到边界的方向显示为灰色。
- FPS 在 30 和 Unlimited 两端停止，不再循环。
- 已通过 900/1100 两种窗口宽度的鼠标回放：按钮内增减、按钮外一像素不触发、标签/数值/空白不触发、FPS 上下界不跳转；原设置交互和核心测试通过。

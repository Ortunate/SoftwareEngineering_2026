# 结局与通关成绩

当前完整流程为：Normal 普通开局连续通过一至六面 → 六面奖励明细 → 退场及战斗资源释放 → 结局加载与播放 → 最高分选存 → 八字符命名 → 对应排行榜。制作人员演出未接入。整关练习、符卡练习、预览及调试直接启动末关不会触发主线结局。

## 原作条件与内容来源

条件以本机《东方红魔乡：新典》可执行文件及 `th06ED.dat`、`th06IN.dat` 为准。核对版本 SHA-256 为 `48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82`；分支指令证据、六份 END 哈希、中文翻译哈希和素材来源记录在 [native_reference.json](../assets/source/endings/native_reference.json)。导入脚本拒绝未经核对的 EXE 或 END 操作码。

| 条件 | 结局 |
| --- | --- |
| Easy，或续关次数大于零 | 对应角色的坏结局 |
| 挑战模式且累计 Miss 大于 5 | 对应角色的坏结局；5 次仍是好结局 |
| Normal／Hard／Lunatic，无续关，且未触发上述条件 | 对应角色和御札的好结局 |
| 未完整通过主线，或 Extra | 不选择这六种结局 |

当前实际开放 Normal，尚无续关入口。选择器保留原生难度与续关规则，不能据此把未开放难度视为可玩。以后接入续关必须递增 `runContinues_`，不能将挑战模式等同于续关。`clearedMainlineStages_` 只接纳当前连续前缀的下一面；正常切关保留它，重开或返回主菜单清除。

| 角色／御札 | 好结局脚本 | 坏结局脚本 |
| --- | --- | --- |
| 灵梦 · 灵之御札 | `end00.end` | `end00b.end` |
| 灵梦 · 梦之御札 | `end01.end` | `end00b.end` |
| 魔理沙 · 魔之御札 | `end10.end` | `end10b.end` |
| 魔理沙 · 恋之御札 | `end11.end` | `end10b.end` |

中文取 `localization.msgpack` 的官方 `zh-CN`，保留换行、叙述、人物颜色和结束文字。15 张背景直接保留原生 3840×2160 DDS，使用现有 DX10/BC7 解码器；每条分支仅加载实际需要的一或四张。结局曲来自 `bgm/th06_17.opus`，离线重封装后转换为 MP3。运行时只依赖仓库内的内容表、素材和音乐，不读取安装目录、EXE 或解包文件。

经典原作的 [Ending.cpp](https://github.com/GensokyoClub/th06/blob/master/src/Ending.cpp)、[ResultScreen.cpp](https://github.com/GensokyoClub/th06/blob/master/src/ResultScreen.cpp) 与 [GameManager.hpp](https://github.com/GensokyoClub/th06/blob/master/src/GameManager.hpp) 用于辅助核对 END 操作、八字符命名和前十插入方式；新典特有的挑战 Miss 条件以本机二进制为准。

## 播放、交接与操作

`src/narrative/ending.*` 不持有窗口、GPU 或音乐资源。内容表为静态只读数据，Player 独占游标和可见文字，按 60 Hz 内容帧累计秒数；换显示帧率不改变等待或淡白时长。原生初始等待 120 帧、淡入 180 帧、文本间隔 70/12 帧、页面最长等待 1200 帧、最短确认等待 4 帧及结尾淡出 240 帧均保留。`@Fdata/staff00.end` 替换为完成事件。

确认键 Z／Space／Enter／左键推进当前等待，长按缩短文本间隔。首次观看不允许整体跳过；完成过的同一结局允许 Ctrl 快进、X／Esc／右键跳过。观看标记与是否保存成绩分开，放弃本轮分数仍会记住已观看。文本根据窗口宽度换行，图片保持原始画幅，底部操作栏不被淡白覆盖。

末关先清演员、弹幕、Boss、道具和效果等资源借用者，再卸载战斗素材，复用异步 Loading 通道加载结局。播放结束释放结局专属资源，恢复标题音乐和用户音量；音乐淡出只乘临时系数，不改设置。成绩、Miss、Bomb、Graze 和完成时间在进入选存时形成快照，之后不再推进战斗或增加分数。

## 最高分选存与命名

- 标准／挑战与四种御札分别保存 Normal 完整主线的前十名。排名由分数自动确定；同分新成绩排在旧成绩前，末位移出。
- 选存页提供“保存并命名”和“不保存，返回主菜单”。未进入前十时禁用保存，保留旧记录。命名页取消返回选存并保留名字。
- 名字为 1～8 个可打印 ASCII 字符，不能全为空格。命名仅支持直接键盘输入，不提供虚拟键盘；首次输入替换上次名字，Backspace 删除，Enter／数字键盘 Enter 保存。WASD、Z、X 和空格在这里都是文字，Esc／右键返回；Ctrl+A 重新选择整段名字。方向键和鼠标左键不改变名字或触发保存。
- 确认名字后才把本轮成绩插入排行榜；成功后显示对应模式、御札和高亮名次。Score 入口可按上下切模式、左右切御札。保存后的排行榜可确认返回首页。

独立文件 `koumakyou_scores.dat` 保存完整通关成绩、最近名字与六个观看标记；写入同目录 `.tmp`、检查写入和关闭结果，再原子替换。保存失败保留旧文件、本轮成绩和已输入的名字，页面提示重试。加载按行校验，忽略损坏行并稳定整理排名。存档文件已加入 `.gitignore`。

原 `koumakyou_settings.cfg` 和分关 `StageHighScores` 沿用原有口径；分关成绩仍在关卡结算时自动更新。旧 `best_score` 缺少完整通关身份和名字，不伪造迁入新排行榜。当前未实现 Replay 存储。

macOS 游戏窗口仍不启动 IME，局部视图子类将可打印 ASCII 交回 GLFW 字符回调，保留原物理按键和系统输入源；Windows 沿用窗口 IMM 解除策略。其他页面不消费命名文字，进入命名时清空确认键的字符队列，避免页面切换穿透。

## 重新导入

使用已有 `thdat-nc` 将六份 END、`end00_4k.dds`～`end13_4k.dds`、`end00b_4k.dds` 解至临时目录，同时解出 `th06IN.dat` 的 `localization.msgpack`。将 DDS 按报告中的文件名映射原样复制到 `assets/runtime/endings/native/`。不导入 `staff*` 文件。

```bash
# GAME 为本机合法安装目录，EXTRACTED 为上述临时解包目录。
python3 tools/rewrap_native_opus.py "$GAME/data/bgm/th06_17.opus" /tmp/ending.ogg
ffmpeg -i /tmp/ending.ogg -codec:a libmp3lame -q:a 2 assets/runtime/endings/ending.mp3
python3 tools/import_endings.py "$GAME/th06nc.exe" "$EXTRACTED" "$EXTRACTED/localization.msgpack" \
  --output src/narrative/ending_content.inc \
  --report assets/source/endings/native_reference.json \
  --runtime-root assets/runtime --source-music "$GAME/data/bgm/th06_17.opus"
```

EXE 文件名须按实际安装版本指定。不依赖 Python 第三方库；导入器复用已有 PE 解析器，只验证已知分支，不从运行时反汇编生成选择逻辑。中文字体子集扫描范围包含 `.inc`；使用 `tools/subset_ui_font.py` 从 Noto Serif CJK SC SemiBold／Bold 同步重建 `ui_charset.txt` 与两份字体，不能只更新结局表而遗漏字体。

## 验证

```bash
cmake --build build-debug --target koumakyou koumakyou_tests koumakyou_postgame_tests -j 6
ctest --test-dir build-debug --output-on-failure
cmake --build build-debug --target koumakyou_postgame_lifecycle_tests koumakyou_lifecycle_tests koumakyou_window_input_tests -j 6
./build-debug/koumakyou_postgame_lifecycle_tests /tmp/ending-review
./build-debug/koumakyou_lifecycle_tests /tmp/campaign-review --campaign-only
./build-debug/koumakyou_window_input_tests
```

无窗口测试覆盖六分支、Miss 5/6、Easy／续关／Extra／非法输入、不同显示帧率、首次观看限制、前十插入与分组、64 位分数、损坏文件、CRLF 和写入失败。六份原始脚本独立推导的不输入完成时长为 24740、28550、34900、36170、6890、6960 内容帧。

图形测试使用真实高清背景与字体，检查六结局资源交接、所有译文码点、真实字符队列、命名特殊键、非 ASCII 过滤、方向键和左键不编辑名字、选存、失败后重试、存档回读及 390×844／800×600／1100×760／2560×1440／3440×1440 布局；完整主线测试验证连续六关出口。图形用例需要桌面会话，数据写入临时目录，不改用户存档。

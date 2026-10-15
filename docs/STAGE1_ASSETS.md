# 一面新增素材

标题与对话立绘使用内置 ImageGen 生成；游戏界面文字与悬浮卡片由程序绘制，战斗角色、弹幕、道具及击破动画使用本机新典的原始素材。战斗 DDS/ANM 的完整目录与接口见 [原始战斗素材接口](NATIVE_BATTLE_ASSETS.md)。

| 文件 | 用途与生成提示 |
|---|---|
| `assets/runtime/common/title_reimu_modern.png` | 旧合成首页插画，仅在独立图层缺失时回退使用 |
| `assets/runtime/common/title_layers/moon.png` | 红月和半透明云雾，周围保留 Alpha |
| `assets/runtime/common/title_layers/shrine.png` | 神社、鸟居、石阶与植被，不含人物或月亮 |
| `assets/runtime/common/title_layers/reimu.png` | 独立灵梦透明层，托腮及膝上手部已按反馈重绘；完整提示见 `TITLE_BACKGROUND_LAYERS.md` |
| `assets/runtime/stages/stage1/portraits/reimu/*.png` | 灵梦 6 张原动作/表情的现代重绘，使用自然手指比例；a1 全身图用于开局选择 |
| `assets/runtime/stages/stage1/portraits/rumia/*.png` | 露米娅 4 张差分，保留金发、红结、黑白服装及平伸双臂；a0 用于默认对话，b0 用于符卡 |

PNG 保留透明通道，生成方式为内置 ImageGen。立绘的原始 DDS 作为重绘参考，运行时立绘使用新绘制的 PNG；完整提示词保存在 `assets/source/portraits/prompts.json`。旧 `reimu_cutout.png`、`rumia_cutout.png` 与 `*_modern.png` 暂保留供比较，不再作为对话、菜单和 Boss 的立绘加载。

灵梦按 a0、a1、b0、b1、c0、c1 对应表情 0–5，露米娅按 a0、a1、b0、b1 对应 0–3。灵梦 a1 为已修正手指的样图；露米娅保留原素材右侧袖子出画面的构图。原 PNG 尺寸分别为 976×1612 和 887×1774，不重采样。对话通过清单中的 `dialogueCrop` 等比绘制上部；符卡使用完整图；世界露米娅使用独立 stg1enm2 动画，判定锚点为 `(0.5,0.5)`。

10 张差分在进入 Stage 1 时加载，退出时释放；世界露米娅由 DDS/ANM 图集缓存加载，与对话 PNG 独立。开局选择页独立持有通用生命周期的灵梦 a1 纹理，不受关卡卸载影响。

## 新典界面图集

`reference_frame.png` 为 CM 包的 `frame_zh-CN.dds` 格式转换，`reference_stage1.png` 为 `frame_stage1_zh-CN.dds` 格式转换。没有重采样或重新绘制；两者保留作原作界面参考。全窗口战斗界面不再加载侧栏图集，改为按关卡元数据绘制悬浮题签与状态卡。

## 音效

当前运行时 WAV 来自本机新典 IN 包，保留原始波形：

| 运行时名 | 源文件 |
|---|---|
| menu_move / menu_confirm / menu_cancel | select00 / ok00 / cancel00 |
| shoot / enemy_down / hit | plst00 / enep00 / pldead00 |
| bomb / graze / pickup | gun00 / graze / item00 |
| spell / clear / full_power | kira00 / extend / powerup |
| reference_6 / 7 / 8 / 9 | power1 / tan00 / tan01 / tan02 |
| reference_16 / 18 | lazer00 / enep01 |

`reference_*` 按原始帧指令 106 的编号播放。`tools/generate_sound_assets.py` 仍保留为无参考素材时的合成替代工具；运行它会覆盖同名 WAV，不是当前素材的重建命令。

## 中文字体

`ui_font.otf` 是 [Noto Serif CJK SC](https://github.com/notofonts/noto-cjk/tree/main/Serif) SemiBold（字重 600）的子集，依据 SIL Open Font License 使用，许可全文见同目录 `FONT_LICENSE.txt`。子集字体改名为 KoumakyouUISerif。中文采用宋体笔画，英文保留衬线，适度加粗以接近新典菜单与对话的风格。

标题、选项、开局选择、HUD、对话、暂停、结算、提示消息和按键设置均使用这一字体，通过 `render/ui_typography.hpp` 共用字号 1.5% 的字距，并以相同度量处理居中与右对齐。运行时以 128 像素加载字形，改善 2× Retina 显示；ASCII 数字调整为等宽前进量并居中，避免分数更新时宽度跳动。此调整不修改 OTF 字形。

新增中文界面文字后，需以原字体和 fonttools 重建：

```bash
python3 tools/subset_ui_font.py /path/to/NotoSerifCJKsc-SemiBold.otf src assets/runtime/common
```

生成器同时保存 `ui_charset.txt`；运行时按该清单加载字形，避免只加载默认拉丁字符。

加载页补入的“女、祈、祷”使用 Google Fonts 提供的同源 [Noto Serif SC](https://fonts.google.com/noto/specimen/Noto+Serif+SC) 600 字重字形，按现有 1000 units/em 转入子集字体；原有 UI 字形和度量保持不变，沿用同一 SIL OFL 许可。后续使用完整 Noto Serif CJK SC 原字体运行上述生成器，也会包含这三个字。

全窗口 HUD 的关卡题签另以同一官方字形来源补齐“幻、卷”；其他现有字形保持不变。重建时仍可使用上述完整原字体与生成器。

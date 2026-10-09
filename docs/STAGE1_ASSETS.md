# 一面新增素材

本批素材用于现代化重绘版本。标题与立绘使用内置 ImageGen 生成；游戏界面文字由程序绘制，战斗边框与花纹使用本机参考游戏的原始图集。

| 文件 | 用途与生成提示 |
|---|---|
| `assets/runtime/common/title_reimu_modern.png` | 宽幅现代动画插画：象牙白与朱红色调，红月、神社、樱花，灵梦坐在左侧；右侧留出菜单空间，无文字 |
| `assets/runtime/stages/stage1/reimu_cutout.png` | 以项目既有灵梦立绘为参考，保留服装、姿势和御币，移除背景，输出真实透明通道 |
| `assets/runtime/stages/stage1/rumia_cutout.png` | 以项目既有露米娅立绘为参考，保留角色轮廓和服装，移除背景，输出真实透明通道 |

PNG 保留透明通道，原始立绘保留在目录中供比较。生成方式为内置 ImageGen，并非精确复制参考游戏像素。

## 新典界面图集

`reference_frame.png` 为 CM 包的 `frame_zh-CN.dds` 格式转换，`reference_stage1.png` 为 `frame_stage1_zh-CN.dds` 格式转换。没有重采样或重新绘制；图集来源坐标按 ANM 的 sprite 定义使用，原生 3D 视口适配到其透明场地窗口。

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

`ui_font.otf` 是 [Noto Serif CJK SC](https://github.com/notofonts/noto-cjk/tree/main/Serif) Regular 的子集，依据 SIL Open Font License 使用，许可全文见同目录 `FONT_LICENSE.txt`。子集字体改名为 RedMistUISerif。

新增中文界面文字后，需以原字体和 fonttools 重建：

```bash
python3 tools/subset_ui_font.py /path/to/NotoSerifCJKsc-Regular.otf src assets/runtime/common
```

生成器同时保存 `ui_charset.txt`；运行时按该清单加载字形，避免只加载默认拉丁字符。

# 一面新增素材

本批素材用于现代化重绘版本。标题与立绘使用内置 ImageGen 生成；游戏界面文字、边框、花纹由程序绘制。

| 文件 | 用途与生成提示 |
|---|---|
| `assets/runtime/common/title_reimu_modern.png` | 宽幅现代动画插画：象牙白与朱红色调，红月、神社、樱花，灵梦坐在左侧；右侧留出菜单空间，无文字 |
| `assets/runtime/stages/stage1/reimu_cutout.png` | 以项目既有灵梦立绘为参考，保留服装、姿势和御币，移除背景，输出真实透明通道 |
| `assets/runtime/stages/stage1/rumia_cutout.png` | 以项目既有露米娅立绘为参考，保留角色轮廓和服装，移除背景，输出真实透明通道 |

PNG 保留透明通道，原始立绘保留在目录中供比较。生成方式为内置 ImageGen，并非精确复制参考游戏像素。

## 音效

`tools/generate_sound_assets.py` 使用标准库合成 44.1 kHz、单声道、16-bit PCM WAV，固定随机种子，覆盖菜单移动/确认/取消、射击、击破、受击、Bomb、擦弹、拾取、符卡、通关、满火力共 12 种事件。运行 `python3 tools/generate_sound_assets.py assets/runtime/common/se` 可重建。音效是适配三维版的合成替代音色，尚未逐项比对原作波形。

## 中文字体

`ui_font.otf` 是 [Noto Serif CJK SC](https://github.com/notofonts/noto-cjk/tree/main/Serif) Regular 的子集，依据 SIL Open Font License 使用，许可全文见同目录 `FONT_LICENSE.txt`。子集字体改名为 RedMistUISerif。

新增中文界面文字后，需以原字体和 fonttools 重建：

```bash
python3 tools/subset_ui_font.py /path/to/NotoSerifCJKsc-Regular.otf src assets/runtime/common
```

生成器同时保存 `ui_charset.txt`；运行时按该清单加载字形，避免只加载默认拉丁字符。

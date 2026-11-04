# 芙兰朵露应用图标

`flandre.png` 为内置 `image_gen` 生成、经用户审查后指定接入的原图，1254×1254 RGB。保留金发红瞳、白帽红缎带、黄领结与彩晶翼，以深红背景突出头像。原图直接复制生成输出，不裁剪、不重绘；完整提示词及参考来源见 `prompts.json`。

| 文件 | 用途 |
|---|---|
| `flandre.png` | 已审查的原始图像，保留原分辨率 |
| `koumakyou.ico` | Windows 可执行文件资源，包含 16、24、32、48、64、128、256 像素帧 |
| `koumakyou.icns` | macOS 应用包图标，包含 16–1024 像素及 Retina 图块 |
| `../../runtime/common/app_icon.png` | 256×256 运行时图标，随 `copy_assets` 分发 |

在 macOS 项目根目录运行 `python3 tools/generate_app_icons.py` 可从原图重新生成三个衍生文件；只调用系统 `sips` 缩放，再按 ICO/ICNS 格式封装 PNG，不需要 Python 第三方依赖。原图不变；重建会覆盖三个衍生素材。

运行时从可执行文件旁的 `assets/common/app_icon.png` 读取，独立于工作目录与关卡资源。Windows/Linux 使用 raylib 窗口图标；macOS 使用 AppKit 设置应用 Dock 图标。调用发生在窗口创建后，临时图像及时释放。macOS 打包还把 ICNS 放入 `Contents/Resources` 并写入 `CFBundleIconFile`，用于 Finder 和启动前的图标。Windows 只给正式游戏嵌入图标，保留所有工具与测试原有 UTF-8 清单。

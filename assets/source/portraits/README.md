# 现代重绘立绘来源

使用内置 ImageGen，参考用户本机《东方红魔乡：新典》原始立绘，同时以用户认可的灵梦样图和手指修正版作为统一画风参考，逐张保留动作与表情。

运行时成品分别位于 `assets/runtime/common/portraits/reimu/`（灵梦 6 张，供菜单及各关对话复用）和 `assets/runtime/stages/stage1/portraits/rumia/`（露米娅 4 张）。PNG 原样复制，未覆盖生成图像或原始参考。`prompts.json` 保存本批九张的完整提示词，以及已认可样图的文件来源；`destination` / `file` 为相对项目根目录的成品路径，`original` 保留制作时的原始参考文件名；本机绝对路径已移除。

原始 DDS/ANM 参考包、审阅总览与 ZIP 属于本机工作产物，不进入源码；角色重绘和使用约定见 `docs/STAGE1_ASSETS.md`。

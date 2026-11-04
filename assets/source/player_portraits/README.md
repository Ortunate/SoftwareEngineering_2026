# 自机选人立绘的分离与现代重绘

使用内置 image_gen，以本机《东方红魔乡：新典》的灵梦、魔理沙选人立绘为姿势与角色参考，以项目既有现代灵梦立绘为画风参考。完整生成与边缘清理提示词、图集坐标、PNG 尺寸和 SHA-256 见 `prompts.json`。

| 角色 | 原图分离参考 | 重绘成品 |
|---|---|---|
| 博丽灵梦 | `reimu_original.png` | `assets/runtime/common/spell_practice/reimu_modern.png` |
| 雾雨魔理沙 | `marisa_original.png` | `assets/runtime/common/spell_practice/marisa_modern.png` |

原图来自 `th06TL.dat` 的 `slpl00b_4k_zh-CN.dds`、`slpl01b_4k_zh-CN.dds`，从 4096×4096 图集中按 `(1024, 0, 1504, 1396)` 无缩放裁出人物区域，去除下方人物信息栏，原始像素和 Alpha 保留。原图与重绘分开存放，游戏不依赖临时解包目录。

重绘保留灵梦侧举御币、另一手垂下的站姿与温和微笑，以及魔理沙侧坐扫帚、双腿并拢的姿势；保留红白巫女服、红色大蝴蝶结，以及金发侧辫、黑白魔女帽、围裙和扫帚等特征。灵梦原图被图集文字区域遮挡的少量脚部依现有服装补全。两张成品逐张检查手部、全身构图和透明边缘，不包含背景、文字、评级或界面元素。

成品为真正 RGBA 透明 PNG，保留生成文件的分辨率与透明通道，未二次放大。`CommonAssets::spellPortraits` 加载它们用于 Start／Practice Start 的前景和 Spell Practice，灵／梦共用灵梦图、魔／恋共用魔理沙图；纹理由通用素材模块持有并释放，缩小时使用 mipmap 和三线性过滤。首页背景和关卡对话差分仍使用各自素材。

Start／Practice Start 的半透明后层独立使用不同姿势的图：灵梦使用 `assets/runtime/common/portraits/reimu/face00a_1.png`，保留裁法、色调和透明度；魔理沙使用 `assets/runtime/common/portraits/marisa/face01a_1.png` 的现代提围裙姿势，不是骑扫帚前景的放大版。两位角色的后层均以 27% 透明度绘制上部。

魔理沙原典后层按 `slpl01a.anm` 的 sprite 0 从 `slpl01a_4k.dds` 提取，保留为本目录 `marisa_backdrop_original.png`，不再用于运行时。其尺寸、坐标和校验信息在 `prompts.json` 的 `backdrops` 中；魔理沙六张对话差分及完整提示词另见 `assets/source/portraits/marisa/README.md`。

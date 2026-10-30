# 掉落物薄板模型

七款分别为小 P、点数、大 P、Bomb、满火力 F、残机 1UP 和星点，对应原始 Item(0…6)。七款模型现已接入关卡共享资源和原始 Item(0…6) 绘制映射；DropSystem 现有小 P、大 P、点数和星点已使用模型。Bomb、满火力及残机的模型可通过原始 Item 外观绑定，但不在本次修改中新增自然掉落或奖励规则。

## 资产

`assets/items/<名称>/` 提供 Blender 源文件、glTF/bin、原始裁图 surface.png、裁图来源 source.json、尺寸和轴向描述 asset.json，以及 REVIEW.md。正反面分别映射原图，背面文字正读；实体轮廓按原图 Alpha 主区域的凸包挤出，轻微倒角，透明边缘保留原柔光。牌面主轮廓高度约 1，厚度 0.06，倒角 0.01；PNG 外沿透明余边不计入实体尺寸。本地 +Z 正面，+Y 为竖直旋转轴。模型不提供危险碰撞体，未来拾取判定继续由玩法管理。

每款不到 1000 三角面，不运行性能测试。原图直接裁自项目高清 etama3.dds，遵循原素材的权利归属，不额外重绘或拉伸。

## 审阅入口

双击根目录 `Review-Items.cmd`：TAB 选择、V 切换总览、空格暂停、1 贴图/2 实体/3 线框、F/S/B 正面/侧面/背面、R 归零。检查固定方向时先暂停。总览和单款均按自身 Y 轴旋转，不绕中心公转。

`Review-Items.cmd -Capture` 重新渲染，`Review-Items.cmd -Generate -Capture` 从原图重建后渲染。工具、依赖和临时环境均在项目根目录隔离；Windows PowerShell 启动脚本使用 UTF-8 BOM。

输出 `output/projectile-review/items/overview.gif` 和七款同名 GIF。每张四秒、100 帧、25 fps、无限循环；另有 front/side/back PNG。帧序列由 raylib 加载正式 glTF 输出，再使用项目内 Pillow 编码。GIF 合成同时检查资源路径、面预算与实体闭合边拓扑。

2026-10-29 七款薄板及现有掉落物局内表现已人工验收通过。重点检查正面还原、侧面厚度、背面文字、图案比例、透明边缘，以及七款区分度；自动检查不代签。

## 场内旋转

DropSystem 使用每个 Pickup 的模拟年龄计算绕本地 +Y 长轴的旋转：90 度/秒，四秒一圈，出生和池复用从零开始。旋转不依赖相机，不改变上抛、下落、追踪和拾取半径；暂停不推进年龄，因此姿态冻结。模型按照原外观世界高度缩放，保留动画不透明度，参加现有统一透明排序。资源随关卡逐款加载，取消/失败和卸载统一释放；正式打包只复制 glTF/bin、PNG 和 asset.json。

运行 Run-Model-Game.cmd 在游戏中查看；output/projectile-review/items/items-runtime.png 是正式绘制接口的七款集成截图。

## 局内 GIF

output/projectile-review/items/drops-ingame.gif 为六秒 Game::Draw 实际场景：前三款抛落并旋转，4.4 秒触发吸取，同时通过敌弹转换生成星点；HUD 显示真实拾取奖励。固定 25 fps 模拟，编码会合并末段相同帧而保持总时长。复现：构建 koumakyou_lifecycle_tests，以截图目录前缀和 --drops-capture 运行，再运行 tools/item_review/encode_ingame.py（参数为仓库根目录）。本场景仅展示已接入玩法的四款；不会伪造其余三款自然掉落。

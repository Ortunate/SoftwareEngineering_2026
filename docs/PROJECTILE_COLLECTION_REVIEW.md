# 普通弹体模型与独立审阅

本组为 Type 0–9 普通敌弹，共 118 个有效颜色组合；颜色通过实例参数变化，不复制模型。苦无及其他八类已由用户验收通过，泡弹待最终复验。具体记录以各目录 asset.json 为准。激光、自机弹和 Bomb 不属于本组。

## 文件组织

所有正式资源位于 `assets/projectiles/`，每个 `*_classic/` 目录对应一个弹体类型：

| Type | 目录 | 弹体 | 颜色数 | 显示 / 碰撞三角形 |
| --- | --- | --- | ---: | ---: |
| 0 | pellet_classic | 粒弹 | 16 | 120 / 120 |
| 1 | ring_orb_classic | 环形小玉 | 16 | 308 / 168 |
| 2 | rice_classic | 米弹 | 16 | 120 / 120 |
| 3 | orb_classic | 实心小玉 | 16 | 308 / 168 |
| 4 | kunai_classic | 苦无 | 16 | 146 / 146 |
| 5 | needle_classic | 针形弹 | 16 | 32 / 32 |
| 6 | large_orb_classic | 大玉 | 8 | 512 / 288 |
| 7 | fireball_classic | 火球 | 1 | 312 / 80 |
| 8 | knife_classic | 小刀 | 8 | 122 / 122 |
| 9 | bubble_classic | 泡弹 | 5 | 696 / 288 |

每个目录包含六个文件：

- `<目录名>.blend`：可编辑制作源文件，保留分部件结构；材质预览为制作辅助。
- `<目录名>.gltf` 和同名 `.bin`：配套的运行时显示模型，必须一起使用；每类一个共享网格、一个材质。
- `collision.bin`：KCOL0001 格式碰撞数据，闭合部件取并集，保留真实孔洞。
- `asset.json`：类型编号、面数、颜色数量、部件角色、碰撞语义及人工验收状态。
- `REVIEW.md`：本类结构与人工检查项目。

`references/` 是原作对照图片和来源说明，仅供查看器对照，不作为模型贴图。当前模型使用纯色、顶点遮罩和程序化着色，无额外纹理目录。被否定的银色写实苦无及其 2K 贴图、专用制作和查看工具已移除；旧版内容可从 Git 历史查阅。

## 形体与碰撞约定

+Y 为弹尖，+Z 为正面；球体显示直径为 1，方向弹总长约为 1。实例只允许正的统一缩放和单位四元数。模型归一化尺寸不代表游戏世界尺寸。

实体弹碰撞从最终几何导出；小刀使用圆柱柄、沿正背贯通的纵向孔，孔内为空。三种 orb 的外球体整体参与碰撞；环形小玉是白核、暗间隔、彩色环带，实心小玉是紧凑连续柔边，大玉是更宽的柔边。原图源矩形比例为 1∶1∶2，对照时保留家族相似性。

泡弹显示由半径 0.19 的暗中心球、外半径 0.5 的透明球壳、48 根静态三维尖丝组成。**碰撞覆盖白色光圈内缘以内的整个体积，半径 0.40，包含尖丝区域及空隙；外白色光圈无碰撞。** 碰撞网格采用外球壳相同拓扑缩放 0.8，单独导出，不加入显示模型。暗中心球不是碰撞范围。

火球只有稳定热核参与碰撞，动态外焰为装饰。原 ANM 在 0/5/10/15 内容帧换相，第 20 帧跳回，三维形变按 60 Hz 的 1/3 秒循环。

球壳、尖丝与火焰采用加色，泡暗球乘法衰减背景；透明层深度测试开启、深度写入关闭，按固定角色顺序绘制，不做逐弹深度排序。高覆盖泛白和浅背景对比度仍是边界，详见[性能记录](PROJECTILE_COLLECTION_PERFORMANCE.md)。

## 独立查看与制作

双击 `tools/windows/Review-Projectiles.cmd` 查看全组，`tools/windows/Review-Kunai.cmd` 查看苦无单件。查看器不启动游戏。Tab 选类型，C 切颜色，T 全色，A 十类总览，V 三种 orb 等尺寸/原图比例对照。鼠标旋转，滚轮缩放。

1/2/3 为成品/灰模/线框，4/5 为碰撞单显/叠加，6 为本体，7 为全部装饰，8 为球壳，9 为尖丝/火焰。G 开关光效，B 切背景，O 看重叠。P 开关探针，H 置于中心或柄孔，J 切泡弹内部体积/白光圈/外部（默认探针预期 CONTACT/CLEAR/CLEAR）。方向键与 PgUp/PgDn 移探针，+/- 调半径。Space 暂停，逗号/句号逐帧，F12 截图。

`tools/windows/Review-Projectiles.cmd -Action capture-collection` 导出 209 张审阅截图；`-Action test` 构建并执行资产检查及两项 CTest；`tools/windows/Stress-Projectiles.cmd` 跑完整压测。输出均在忽略的 `output/projectile-review/` 和 `build/projectile-review/`。

制作脚本位于 `tools/projectile_review/`：`create_classic.py` 制作苦无，`create_collection.py` 制作其余九类，后者支持 `--only-bubble` 单件返修。全组重新生成会重置对应人工验收状态。工具依赖保存在项目 `.tools/`、`.deps/`，缓存位于 `.cache/`，启动器在子进程设置环境。

## raylib 接入接口

使用 raylib 5.5 / OpenGL 3.3，完整效果需配套 `collection_renderer.*` 与 `shaders/collection.*`；普通 DrawModel 不会解释顶点角色编码。COLOR_0.r 保留历史亮面数据，运行时不再用它划分固定白面；g 为角色/2（实体、壳、装饰），b 为火焰相位。实体根据世界法线与视线夹角计算柔和明暗和高光，避免固定光向或模型转向带来的亮暗割裂；球玉、球壳、火焰保留各自的发光分布。只有火球装饰形变，泡尖丝静态。

光影回归使用 `./build-debug/projectile_runtime_tests . /tmp/projectile-lighting`：在隐藏图形窗口内比较十类弹体随相机共同旋转后的像素一致性，并检查针形弹正背面对称和独立苦无渲染器的相机输入。输出 `projectile-lighting-views.png` 的三行分别为弹体转动 0°、90°、180°，用于检查转向后的体积与明暗。

构造 `CollectionRenderer(projectRoot, sharedCapacity)`，为每个 `CollectionInstance` 设置 type、transform、AppearanceFor(type,color)、shellOpacity 与 phase。先 Prepare(instances,camera,aspect)，再在 BeginMode3D/EndMode3D 中 Draw(seconds,glow)。秒数非负有限、透明度 0–1；光效强度 0–2、宽度 0–0.025。资源在窗口上下文存在时创建和销毁。

8192 是全部弹体共享容量。共享实例 GPU/CPU 打包缓冲各 768 KiB，可见索引 32 KiB；无逐弹 GPU 对象。实体类带光效 2 次绘制、三种 orb 3 次、泡弹 4 次、火球 3 次；混合弹种调用数累加。Collision(type) 返回碰撞资产，配合每线程复用 CollisionScratch 和 TouchesInstance 查询，显示剔除不改变物理状态。

正式游戏命中、擦弹、清弹和连续运动碰撞尚未接入，独立模型测试不能证明高速运动没有跨帧穿透。

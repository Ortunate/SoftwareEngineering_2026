# 原版风格苦无：人工验收通过

参考根目录 `kunai.png` 的 Type 4 / C00–C15：宽三角刃、短连接部、穿孔尾环、白亮面和彩色侧面。代表色为 C02 红色。导出坐标 +Y 指向弹尖、+Z 为正面，标准化长度约 1；原图尖朝下仅是显示朝向，接入游戏时由旋转决定发射方向。

本件不使用金属磨损、缠柄或 PBR 纹理。白亮面由 glTF `COLOR_0.r` 遮罩控制；颜色和光效来自参数，不把原图拉伸到模型上。Blender 源文件保留三个独立封闭部件，显示导出为一个网格、一个材质、146 三角形。原银色版本 11,540 面保持不变，本体面数减少约 98.7%。

## 人工查看

双击根目录 `Review-Kunai.cmd`。原银色版用 `Review-Kunai.cmd -Action silver`。两者均独立于游戏和玩家数据。

| 操作 | 功能 |
| --- | --- |
| 鼠标左拖 / 滚轮 | 旋转 / 缩放 |
| 1 / 2 / 3 | 成品 / 灰模 / 线框 |
| 4 / 5 | 实际碰撞网格单显 / 叠加 |
| C / 点击色块 | 切换单件颜色 |
| T | 16 色同屏，仍共享一个网格 |
| G / Q、E / [、] | 光效开关 / 强度 / 宽度 |
| B | 深浅背景 |
| P / H / J | 开关探针 / 孔洞 / 环体接触预设 |
| 方向键 / PgUp、PgDn / +、- | 探针 XY / Z / 球半径 |
| Space / R / F12 | 自动旋转 / 重置相机 / 截图 |

绿色 CLEAR 表示无接触，橙色 CONTACT 表示接触。本体碰撞是三个闭合部件的实体并集，环中心保留孔洞，不使用整个外包盒作为精确碰撞。16 色总览不显示单件探针。

截图：`Review-Kunai.cmd -Action capture`，输出 `output/projectile-review/classic/` 的正侧背面、灰模、碰撞、孔洞/环接触、16 色及光效/背景图。人工重点检查轮廓是否贴近参考，远看是否仍能区分颜色，侧面是否有厚度，环孔是否真实为空，光效是否过强。

2026-10-27，用户明确反馈“我和 leader 都觉得行”，本版美术人工验收通过。自动检查不代替美术验收；后续修改或重新生成模型仍需重新验收。

## raylib 接入约定

资源可由 raylib 5.5 直接 `LoadModel` 读取。正确呈现白亮面和 16 色应配合 `tools/projectile_review/classic_renderer.*` 及 `shaders/classic.*`，默认 `DrawModel` 不具备该颜色遮罩/光效接口。当前实现要求 OpenGL 3.3，尚未提供 GLES/WebGL 后端。

```cpp
// 在 InitWindow 之后创建，CloseWindow 之前销毁；实例数组由调用方复用。
review::ClassicRenderer renderer(gltfPath, shaderDirectory, BulletSystem::kCapacity);
review::ProjectileInstance item;
item.appearance = review::ClassicPalette(2);
item.position = worldPosition;
item.rotation = unitQuaternion;  // 模型 +Y 转到发射方向
item.scale = worldLength;        // 正的统一缩放，不支持非统一缩放
renderer.Prepare(instances, camera, renderWidth / float(renderHeight));
BeginMode3D(camera);
renderer.Draw(true);
EndMode3D();
```

`ProjectileAppearance` 包含 `bodyColor`、`highlightColor`、`glowColor`、`glowStrength`（0–2）、`glowWidth`（0–0.025 标准化长度）。本体不透明，光效为附加加色轮廓壳：深度测试开启、深度写入关闭，不是屏幕空间 bloom，不会改变碰撞。每批本体 1 次绘制，光效开启共 2 次；单实例数据 96 字节，8192 容量 GPU 实例缓冲为 786,432 字节。缓冲在构造时一次创建，逐帧只更新内容。16 色不复制模型、材质或缓冲。

`Prepare` 只剔除显示；物理状态仍由游戏维护。碰撞可加载共享 `CollisionAsset`，每个查询线程预分配一个 `CollisionScratch`，调用 `TouchesInstance` 对包围球初筛后查询真实网格 BVH。统一缩放同时作用于模型和碰撞；调用者必须提供有限坐标、单位四元数和非负球半径。该接口是离散球接触，不提供高速运动的连续扫掠；接入游戏仍需考虑跨帧穿透。

当前未改游戏渲染、弹道或碰撞；该模块提供可接入的数据和示范，尚不能把独立测试结果当作完整关卡性能。

## 性能测试与预算

根目录 `Stress-Kunai.cmd` 或 `Review-Kunai.cmd -Action stress`。运行约 8.5 分钟，需保持窗口可见且不最小化。无 MSAA、关闭垂直同步和帧率限制，1920×1080，13 个场景各预热 3 秒、采样 10 秒、重复 3 次。测试结束后查看 `output/projectile-review/performance/results.csv` 与 `hardware.txt`。

场景覆盖 1024/4096/8192 发，静态/运动、单色/16 色、光效关闭/开启、密集重叠、极近屏幕覆盖、75% 显示剔除、8192 个碰撞候选，以及同数量的简化二维 sprite 基线。为控制总时长采用代表性组合，不是所有维度的完整笛卡尔积。CPU 更新为合成运动，不包含真实技能脚本；碰撞为每帧对一个球体查询。

CSV 分别记录整帧、更新、碰撞、打包上传、绘制 CPU、异步 GPU 的均值/P50/P95/P99，以及数量、可见数、绘制次数、三角形和窄相次数。GPU 查询忙时跳过并统计，不在帧内阻塞取结果；整帧包含提交窗口，截图编码不在测量区间内。sprite 基线通过 raylib 批处理提交，绘制次数按默认批容量估计；模型通道绘制次数为实际调用计数。两者提交方式不同，用于区分三角形/GPU 成本与 CPU 提交成本，不能据此声称 3D 普遍比 2D 更快。

普通 8192 场景目标为整帧 P95≤16.7 ms、GPU P95≤6 ms、更新与碰撞 CPU 合计≤2 ms。极端场景单列，不偷减数量。当前仓库未声明最低硬件，因此测试只验证所记录机器；低端 GPU 和完整游戏需另行实测。面数只是预算一部分，透明覆盖率、提交方式、碰撞候选数同样决定压力。

重建：`Review-Kunai.cmd -Action generate-classic` 后运行 `Review-Kunai.cmd -Action test`。重新生成会将资产状态重置为待验收。依赖、构建与运行数据全部留在项目目录。

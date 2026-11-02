# 渲染压力与 macOS Apple Silicon 优化分析

分析日期：2026-11-01。代码基线：`676c0b9`。

本文保留优化前的静态分析；后续实现及实测对照见 [渲染优化与基线压测](RENDER_OPTIMIZATION_RESULTS.md)。

## 1. 结论与证据边界

可以优化，而且现有架构已经具备继续优化的基础：普通敌弹使用共享网格、持久分配的实例缓冲和视锥剔除，不透明实体先写深度，透明层统一排序。优先方向是**减少重复准备和提交、控制透明覆盖、改善帧调度**，之后才是进一步降低几何复杂度和评估 Metal 后端。

这里的“持久分配”仅指缓冲对象复用，不代表使用了 persistent mapping。

本报告基于源码、构建缓存、第三方后端实现、本机硬件信息及 Apple 官方资料。未进行运行时 CPU/GPU 采样、功耗测量或优化前后对照，因此以下优先级属于工程判断，不能解读为已证实的耗时排行；不提供没有测量依据的提升百分比。

本机查询结果：MacBook Air，Apple M5，10 核 CPU、8 核 GPU、16 GB 内存。`build-debug/koumakyou` 是 arm64 可执行文件，未发现需要先解决的 x86_64 架构问题。`build-debug` 为 Debug、`build` 为 Release；这只能说明缓存配置，不能保证已有二进制对应最新源码。

## 2. 当前渲染结构

调用主线：

```text
main：Update → BeginDrawing → Clear → Game::Draw → EndDrawing
  DrawWorld：天空盒 → 场景 → 场地网格
    CombatRenderer::Prepare：普通弹转换为模型实例
    SpriteRenderer::Draw：不透明模型 → 透明命令构建/排序 → 混合绘制
  HUD、对话、演出与菜单叠加
```

当前依赖固定在 raylib 5.5；本地构建使用 `GRAPHICS_API_OPENGL_33`，链接 OpenGL.framework。正式敌弹模型路径是 `CollectionRenderer`，不能把独立 `ClassicRenderer` 的实现或统计直接当成全游戏路径。

已有合理设计应保留：

- `CollectionRenderer` 按十种弹体共享几何，使用 `glDrawElementsInstanced`，在 `Prepare` 中剔除。
- 正常世界绘制直接进入当前 framebuffer，没有为了每层效果频繁创建和切换 FBO。
- 普通敌弹模型、碰撞素材和实例资源复用；主要工作向量复用容量。
- 主体、透明壳、外辉光分离，有进一步按材质和质量档位组织的空间。
- 战斗模拟固定 120 Hz，显示帧率已有独立配置；纯渲染优化无需降低逻辑频率。

## 3. 优化机会与优先级

优先级：P0 为测量基础，P1 为先实施的局部优化，P2 为结构优化，P3 为后端演进。收益指作用方向，均待实测。

| 优先级 | 项目 | 主要降低的压力 | 工作量 / 风险 |
|---|---|---|---|
| P0 | Release 帧时间、提交次数、上传量与功耗基线 | 识别真正瓶颈 | 小至中 / 低 |
| P1 | 缓存 uniform location、收敛 GL 状态保存边界 | CPU 和驱动调用 | 小至中 / 状态交接需验证 |
| P1 | 可选 MSAA、光晕质量及菜单/后台帧率策略 | GPU、耗电 | 中 / 画质与帧节奏变化 |
| P1 | 更早保守剔除，减少无效透明命令 | CPU 排序、准备 | 中 / 防止错误剔除 |
| P2 | 每帧统一实例准备与上传，按区间绘制 | CPU、上传与潜在同步等待 | 中至大 / 透明语义敏感 |
| P2 | 激光片段、自机弹、掉落物实例化 | CPU 提交、驱动调用 | 中至大 / 保持交错顺序 |
| P2 | 场景网格缓存、可选视觉 LOD | CPU 几何生成、顶点/片元 | 中 / 保留外观与判定 |
| P3 | 完整 Metal 或跨平台 Metal 后端 | 驱动开销、带宽及架构利用 | 大 / 跨后端一致性 |

### 3.1 普通敌弹已经合批，但同帧存在重复准备与上传

证据：[`combat_renderer.cpp`](../src/render/combat_renderer.cpp) 第 13–31 行、[`collection_renderer.cpp`](../src/render/projectiles/collection_renderer.cpp) 第 68–116 行。

同一个普通弹会进入透明列表；已完成出生演出且 Alpha=255 的弹还进入不透明列表。不透明主体绘制调用一次 `CollectionRenderer::Prepare`；透明队列中连续的模型命令调用 `DrawGroups`，再次准备分组。半透明出生弹还会为主体单独准备一次。

这不是无意义地重复绘制同一层：主体、壳和辉光承担不同外观。问题是实例校验、相机矩阵/视锥计算、打包和上传也跟着重复。透明命令被演员、道具或自机弹打断时，准备次数进一步增加。

建议把接口拆成“本帧准备一次”和“绘制指定区间/层”。预先计算相机数据、可见实例、类型区间和透明顺序，统一上传后反复引用相应区间。部分层可共享属性；需要不同排列时使用明确的多区间布局，不能假设一份类型排序能够同时满足所有 Alpha 顺序。

`Packed` 包含 24 个 float，按当前布局为 96 字节。8192 个实例的一次完整上传是 768 KiB；60 FPS 下约 45 MiB/s。重复两次约 90 MiB/s，尚不含其他实例及 CPU 复制。这是理论数据量，不是实测带宽；对 Apple Silicon 更应检查大量小调用和重用区间导致的等待，而不能仅凭这个数认定内存带宽不足。

当前多次 `glBufferSubData` 都从同一缓冲偏移 0 写入。驱动负责正确性，但可能产生等待或内部重命名。可比较帧内追加区间、跨帧轮转缓冲、整帧 orphan 后上传等方案。避免每个小批次都 orphan，也不能使用不受保护的 unsynchronized 写入。

### 3.2 激光有实例化实现，但正式入口仍逐片段调用

证据：[`sprite_renderer.cpp`](../src/render/sprite_renderer.cpp) 第 349–419 行、[`combat_renderer.cpp`](../src/render/combat_renderer.cpp) 第 44–47 行、[`laser_renderer.cpp`](../src/render/projectiles/laser_renderer.cpp) 的 `Draw`。

透明排序会在相关精灵深度处切分激光，Boss Bomb 巨束还固定切成至少 12 个轴向区间。敌方模型激光每段通过 `std::span(&item,1)` 传入 `LaserRenderer`，再次设置矩阵、查询状态、上传单实例；默认成品模式开启光晕时，每段进行三个 pass。

更关键的是：激光顶点着色器仍生成整条光束，片元着色器通过 `clipFrom/clipTo` 和 `discard` 保留片段。增加片段数会重复提交整束几何，并可能处理大量最终丢弃的片元。Bomb 和自机激光也应检查同类裁切模式。

建议把片段范围放进实例数据，批量准备片段；优先合并排序队列中相邻且材质兼容的片段。进一步使几何在顶点阶段直接对应区间，减少整束重复光栅化；端盖、法线、整束 UV、轴向亮度和片段边界必须与原有结果一致。

已有投影重叠过滤和精灵深度区间筛选，不应重新建议无条件按所有对象切分。`sprite_overlap_tests.cpp` 已覆盖相关规则，还包含相机平面交叉下产生 1025 段的保守案例。这证明极端分段必须有性能预算，但不能通过硬截断段数破坏遮挡。

### 3.3 GL 状态查询与 uniform 查找位于频繁路径

证据：[`gl_state.hpp`](../src/render/projectiles/gl_state.hpp)、[`model_draw.hpp`](../src/render/projectiles/model_draw.hpp)、`CollectionRenderer::Draw`、`LaserRenderer::Draw`。

每次构造 `GLState` 会执行 12 次 `glGetIntegerv`、3 次 `glIsEnabled` 和一次 `glGetBooleanv`，结束时又逐项恢复。uniform location 也在每次绘制、类型循环以及 `Real/Int/Vec` 辅助调用中按名称查找。

建议在着色器加载后缓存 location，并在着色器重建后更新缓存；相机、时间等公共 uniform 每个必要批次设置一次。把完整状态保存尽量放在明确的渲染阶段边界，内部采用已知状态或状态缓存。

状态查询不必然造成 GPU 硬同步，但调用密度确定存在，应通过 CPU profile 判断代价。不能直接删除保护：当前 raw GL 与 raylib/rlgl 混用，必须保留 flush 时机、程序/VAO/缓冲绑定、深度写入、混合和剔除的交接契约，并避免缓存与实际 GL 状态脱节。

### 3.4 像素覆盖、透明层和抗锯齿

证据：[`main.cpp`](../src/main.cpp) 第 77 行、[`game_render.cpp`](../src/render/game_render.cpp) 第 104–123 行、`CollectionRenderer::Draw` 及 `collection.fs`。

默认窗口请求 4× MSAA；世界按当前 framebuffer 实际尺寸全窗口绘制。主体之外还有透明壳、外辉光、激光和 Bomb 多层，大面积覆盖时可能偏向片元/混合压力。

建议提供可比较的质量选项：MSAA 关闭/4×、外辉光关闭/完整、背景细节、世界渲染比例。优先保持弹体主体和危险边界清晰，先降低装饰性光晕；UI 可保持原生分辨率。

若世界目标每轴使用 0.75 倍像素，像素数量为原来的 56.25%；这不等于总帧时间下降 43.75%，因为排序、提交、模拟和合成成本仍在。缩放目标还会增加一个 FBO 和合成步骤，可能抵消收益，需在片元瓶颈场景验证。

本轮未查询运行窗口的真实像素尺寸，不能断言当前已经在 2× Retina 下运行。应记录 `GetScreenWidth/Height`、`GetRenderWidth/Height`、DPI 和实际 sample count；若每轴确实 2×，像素数为逻辑尺寸的 4 倍。4× MSAA 也不能简单解释成片元着色器执行量再乘四。

**截图基准有实质差异：**`main` 的截图入口创建 `LoadRenderTexture`，而 raylib GLFW 后端明确只对主 framebuffer 请求 MSAA，用户 FBO 不自动启用。现有截图还先快进模拟、最终只画四帧并读回图片。因此截图适合视觉回归，不能替代窗口连续渲染的性能基准。

### 3.5 自机弹、掉落物与背景

证据：`CombatRenderer::DrawBullet/DrawItem`、`model_draw.hpp`、[`stage1/scenery.cpp`](../src/stages/stage1/scenery.cpp)、[`stage6/scenery.cpp`](../src/stages/stage6/scenery.cpp)。

自机弹逐个计算姿态、设置参数并绘制实体/加色层；道具逐个 `DrawMesh`。持续射击和大量掉落值得独立测量，再按共享模型和材质实例化。必须保持它们相对演员、弹幕的透明交错顺序。

背景每帧通过 `DrawCylinderEx`、`DrawSphere`、`DrawCube` 等接口生成和提交简单几何。一面 36 棵树使用 108 次圆柱调用；六面的球体、塔楼也有静态重复几何。可把球、树、塔楼和场地线框缓存为 GPU 网格，静态对象合并，运动背景只更新变换。

这些立即模式 API 可能被 rlgl 合批，不能把 108 次函数调用直接写成 108 次 GPU draw call。缓存主要减少 CPU 几何生成，实际提交收益另测。

### 3.6 更早剔除和 CPU 数据遍历

证据：`CombatRenderer::Prepare`、`SpriteRenderer::Draw`、[`fixed_pool.hpp`](../src/core/fixed_pool.hpp)。

普通弹的视锥剔除目前在 Collection 准备阶段，而透明队列此前已经接收每颗模型弹并参与排序。弹池容量 16384，`ForEachActive` 总是扫描全部槽位；渲染主线至少经过模型准备、激光收集、普通弹处理三次全池扫描。

建议一次生成本帧只读绘制快照，缓存资源绑定、姿态、深度和保守包围体；在排序前剔除确定不可见的弹体和装饰层。包围体必须覆盖辉光、火焰形变、ANM 偏移和激光全长，不可用中心点代替。

若 profile 证明固定池遍历成为瓶颈，再考虑维护活动索引。改变池分配或遍历顺序可能影响同深度稳定顺序、发弹顺序和随机行为，宜先只优化渲染快照，不改模拟池。

另有容量边界需要处理：普通弹池为 16384，Collection 和 Laser 实例容量各为 8192；Collection 在剔除前检查输入数量，超出便抛异常。不能把池满前的渲染失败当作可接受的降载方式。应统一容量或采用保持排序语义的分块，并增加超过 8192 的压力验证。

## 4. macOS Apple Silicon 专项

### 4.1 保留 OpenGL 时就能实施的优化

1. 使用原生 arm64 的 Release/RelWithDebInfo 采样，本机已具备 arm64 基础。LTO 可作为单独实验，但不会自动解决提交碎片或片元覆盖。
2. 缓存 location、减少状态查询、减少重复打包与上传；这些针对当前路径，跨平台也有收益。
3. 用 60 FPS 作为节能对照。已有 30/60/120/144/240/不限帧设置；高刷新或不限帧会增加每秒绘制量。不要通过把战斗逻辑从 120 Hz 降到 60 Hz 节能。
4. 优化帧调度。当前入口没有请求 `FLAG_VSYNC_HINT`，本地 raylib GLFW 默认关闭 VSync；`SetTargetFPS` 通过等待限速，默认 `SUPPORT_PARTIALBUSY_WAIT_LOOP` 会把等待预算的约 5% 留给忙等。这个比例是等待时间的比例，不是整个进程实测 CPU 占用。比较当前限速、VSync 和基于显示节奏的阻塞等待，验证输入延迟、撕裂、重复等待与高刷新屏行为。
5. 菜单、暂停、失焦或最小化时按需要降频/冻结世界绘制；暂停菜单本身仍有动画，完全缓存世界须处理窗口尺寸、观察相机和资源失效。后台恢复时避免把长间隔当成待补算战斗时间。
6. 统一内存并不免除 CPU/GPU 访问同步；不能认为反复覆盖同一实例缓冲必然零成本。

避免直接套用桌面 OpenGL 4.3/4.4 的 compute、SSBO、persistent mapped buffer 方案。Apple 官方说明其 OpenGL 支持到 4.1，非核心功能要查询扩展；本项目配置更是 GL 3.3。是否存在某个扩展应以运行时检测为准。[Apple：让 Metal app 在 Apple 芯片 Mac 上运行](https://developer.apple.com/cn/videos/play/wwdc2020/10631/)

不要全局启用 `-ffast-math` 或为了向量化改变模拟浮点表达式；项目依赖录像、随机顺序和独立行为差分。NEON/SIMD 可针对确认的纯渲染打包热点评估，当前证据不足以把它列为首选。

### 4.2 Metal 的具体价值和适用条件

Apple 已从 macOS 10.14 起弃用 OpenGL；迁移 Metal 有长期维护与架构利用价值，但不会自动消除重复绘制、排序或过度覆盖。[Apple OpenGL 指南](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_intro/opengl_intro.html)

Apple GPU 使用基于 tile 的延迟渲染。对本项目，合理方向是在尽量少的 render pass 中完成场景、主体、透明层和 HUD；正确配置附件 load/store，避免不必要的中间目标写回。瞬态深度和 MSAA 附件可按支持情况使用 memoryless，MSAA 使用片上 resolve；最终颜色和仍需后续采样的数据必须保留。这是 Metal 路径的能力，不能直接作为当前 OpenGL 已有收益。[Apple GPU 架构说明](https://developer.apple.com/videos/play/wwdc2020/10602/)

| 专项 | 对项目的具体做法 | 必须保留的条件 |
|---|---|---|
| 动态实例缓冲 | shared `MTLBuffer`，每帧使用独立区间，2～3 帧在途资源以完成信号回收 | 不覆盖 GPU 尚在读取的数据；控制帧队列避免增加输入延迟 |
| 静态网格和纹理 | 评估 private 存储，加载阶段完成上传 | 统一内存下也要匹配访问模式，不能假设 private 总有收益 |
| Pipeline 管理 | 预创建主体/壳/辉光状态，常见变体预热 | 限制组合数量，避免运行中编译卡顿 |
| Shader 精度 | 色彩与光效参数评估 `half`，位置/投影保留 `float` | 比对亮度、渐变、近裁面和远距离误差 |
| Shader 变体 | 按材质和 pass 使用有限 function constant 变体 | 不把 uniform 分支误认为一定存在严重线程发散 |
| 压缩纹理 | 大背景和立绘评估 ASTC/BC，按设备支持选择 | 小字、细线、Alpha 边缘单独做画质回归 |

Apple 对动态缓冲推荐多缓冲以平衡 CPU/GPU 并行；shared 资源仍需同步访问。存储模式应按实际硬件和资源用途选择。[多缓冲](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/TripleBuffering.html)、[资源存储模式](https://developer.apple.com/documentation/metal/setting-resource-storage-modes)、[shared 访问约定](https://developer.apple.com/documentation/metal/mtlstoragemode/shared)

对颜色和光效采用较低精度是可评估的优化，但精度必须验证；着色器半精度不能推广到战斗模拟。[Metal Shader 最佳实践](https://developer.apple.com/videos/play/tech-talks/111373/)

当前 DDS 加载器把 BC 数据解码成 RGBA8，再通过 `LoadTextureFromImage` 上传。因此源文件压缩不等于运行时纹理已压缩。大纹理采用硬件压缩可能减少占用与采样带宽，但须新增匹配后端的上传路径；不能假设 Metal 的 ASTC 支持等于现有 macOS GL 路径也支持。普通敌弹主体是程序化着色，纹理压缩对它的直接帮助有限。[Apple 纹理工具与格式说明](https://developer.apple.com/videos/play/wwdc2021/10157/)

### 4.3 迁移成本与推荐路径

Metal 不是 CMake 开关。项目既有 raylib UI/场景绘制，又有直接 GL 调用、VAO、GLSL 和 framebuffer 状态；仅把敌弹换成 Metal，仍需解决两套 API 的目标共享、同步和合成，风险较高。

推荐先提取与 API 无关的帧数据：可见实例、材质/pass、透明排序、片段范围、相机与灯光参数，再由后端消费。随后比较：

- 继续 raylib/OpenGL：局部改动成本最低，适合先取得收益。
- 原生 Metal 后端：macOS 控制力最好，但需覆盖世界、精灵、UI、资源与截图，并维护另一套后端。
- 支持 Metal 的跨平台抽象：可减少多平台后端维护；仍需重写接口适配和 shader，选型与功能覆盖需另做验证。

当局部优化后仍出现明显驱动提交瓶颈、目标设备无法满足帧时间/功耗预算，或项目确定长期优先支持 macOS 时，再推进 Metal 原型。首个原型应复现“普通弹 + 透明演员交错 + 激光切分 + UI”，不能只用纯不透明实例展示迁移收益。

不建议优先引入 GPU 驱动间接绘制、GPU 全量模拟或复杂 OIT。当前弹量先用 CPU 准备加实例化即可评估；OIT 会改变透明外观并增加附件、带宽与验证成本。纯加色在特定条件下可交换，不代表可以跨过 Alpha 演员或泡内球的乘法吸收层任意重排。

## 5. 可执行的验证方案

### 5.1 固定场景与指标

以 Release 或 RelWithDebInfo、固定输入/种子/相机采样；Debug 仅做功能检查。至少包括菜单、暂停、普通道中、密集混合敌弹、持续自机射击、大量掉落、多激光穿过演员、Bomb 近景、关卡反复加载/卸载。主视角和回放观察视角分别覆盖。

每组预热后采样 30～60 秒，重复三次；再做至少 10 分钟的热稳定运行。记录机器/系统版本、供电与低电量模式、实际像素尺寸、MSAA、帧率上限、活动弹数和场景内容，比较前后使用同样条件。

| 指标 | 目的 |
|---|---|
| Update、绘制准备、排序、提交的 CPU 时间 | 区分模拟、渲染准备与驱动压力 |
| GPU 帧时间、整帧 P50/P95/P99、超预算帧数 | 观察稳定性和尾部卡顿 |
| draw call、状态查询/切换、实例上传次数与字节数 | 验证结构优化确实减少工作 |
| 活动/可见实例、透明命令、连续模型批次数、激光片段数 | 解释场景复杂度和批次碎片 |
| CPU 占用、GPU 活跃、内存及能耗趋势 | 验证“降低压力”，而非仅提高最高 FPS |

60 FPS 预算为 16.67 ms，120 FPS 为 8.33 ms。CPU/GPU 流水并行，不应简单相加判定帧时间。分别做不限帧吞吐实验和固定 60 FPS 能耗实验；限帧下 FPS 相同也可能已经显著省电。

当前 `CollectionRenderer::Stats` 与 `LaserRenderer::Stats` 是最近一次调用的统计，`Draw` 还会重置 draw/triangle 计数；不得直接当成整帧总数。应在帧级累计，连同 raylib、UI 和其他模型提交统一统计。`LastBeamSegmentCount` 可作为现有切分指标复用。

当前 GL 路径先用 Instruments Time Profiler/System Trace 和帧级计数；GPU timer query 须先检测能力，并延迟读取结果，避免测量引入等待。Metal 后端再使用 Metal System Trace、GPU capture 和 GPU counters；不能保证 Metal 调试工具能按自定义 pass 完整解释当前 OpenGL 路径。[Apple 性能分析](https://developer.apple.com/documentation/xcode/analyzing-the-performance-of-your-metal-app)、[GPU counter 分析](https://developer.apple.com/documentation/xcode/analyzing-apple-gpu-performance-using-counter-statistics)

用分辨率、外辉光、MSAA、激光数和透明交错数量分别做单变量实验：降低像素数有效通常指向片元/带宽方向；固定几何而减少批次数有效通常指向提交方向。它们只是定位线索，最终用 CPU/GPU 时间确认。

### 5.2 正确性与验收

优化前后同场景对照：同深度稳定顺序、演员与弹体交错、泡内球吸收、半透明出生、激光前后穿插、近眼 Bomb、Retina 与离屏尺寸、暂停恢复、资源重载及容量边界。

沿用现有 `stage_tests`、`behavior_tests`、弹体方向、透明遮挡、模型运行时和生命周期检查。涉及纯渲染缓存也要验证录像兼容边界；涉及模拟池或浮点运算时，必须继续满足 60 Hz 独立参考、120 Hz 半帧逻辑与不超过 `5e-4` 世界单位的既有误差限制。

实施后运行项目约定的构建和 CTest；`koumakyou_sprite_overlap_tests`、`projectile_runtime_tests`、`koumakyou_lifecycle_tests` 是按需图形目标，应另行构建并按各测试入口参数执行，不能把默认 CTest 通过理解为它们也通过。

## 6. 建议实施顺序

1. **建立基线**：补齐帧级统计、标准场景和窗口连续采样入口，确认 CPU/GPU 主导项；处理 8192/16384 容量边界。
2. **局部优化**：缓存 location、相机/资源绑定数据，保守提前剔除，减少状态保护频次；提供 MSAA/辉光质量选项，比较帧调度与后台策略。
3. **组织批次**：普通弹统一帧内准备与上传，激光片段参数实例化，自机弹/道具合批；保留排序、深度和混合契约。
4. **针对采样结果优化 GPU**：必要时加入场景网格缓存、视觉 LOD、世界分辨率缩放和 shader 变体；碰撞几何不随视觉 LOD 改变。
5. **评估 Metal 原型**：以相同场景、画质、分辨率、帧率和热状态比较 CPU/GPU 时间、能耗及维护成本，再决定正式迁移。

本次仅新增分析文档，未修改渲染实现，也未执行构建、CTest 或图形性能测试。

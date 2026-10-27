# 弹体制作方法与独立验收

当前分支从 `dev/koumakyou` 的 `2a31916` 创建。按最新反馈，Type 4 苦无增加原版风格低面数版本，以根目录 `kunai.png` 为参考，采用纯色和可调颜色光效；原银灰样品保持不变。新版于 2026-10-27 经用户与 leader **人工验收通过**。不复用 `heny/projectile-models`，不修改正式游戏的渲染、弹道和碰撞。

## 当前执行方法：先满足轮廓与弹幕预算

新版完整操作和接口见 [kunai_classic/REVIEW.md](../assets/projectiles/kunai_classic/REVIEW.md)。下面的 PBR 材质、贴图通道和银色棚灯操作保留为早期银色样品说明，使用 `Review-Kunai.cmd -Action silver` 访问；默认启动器现为新版查看器。

- 苦无本体预算 128–160 三角形，硬上限 192；当前为 146（三角刃 6、短连接部 12、穿孔环 128）。颜色用顶点遮罩和实例参数，纯色版本不额外制作或加载纹理。后续有实质表面纹样需求的弹体才独立制作贴图。
- 实体部件先分别做闭合几何，导出时合并一个显示网格/材质；碰撞保留部件并集及孔洞。碰撞三角形直接来自最终网格。光效外壳不计入实体碰撞。
- 8192 是 `BulletSystem::kCapacity` 的总池容量，包含共享池内弹幕；按共享网格、持久实例缓冲和显示剔除设计，不为每发复制模型或创建 GPU 缓冲。
- 每件先提交需求、建模、独立验收。16 色是同一个苦无的参数预览，不是继续制作其他弹体。人工未通过前不扩展到下一类。
- 高密度常见形体建议以本件 128–160 面作为第一档预算，不视为其他形体的统一强制面数；透明球壳需要另测过度绘制成本。更复杂弹体须单独验证数量和屏幕覆盖率。
- 性能按可见 1920×1080、无垂直同步/限帧、预热 3 秒+测量 10 秒、每例 3 次执行。普通 8192 场景以整帧 P95≤16.7 ms 为验收目标，GPU≤6 ms、更新+碰撞 CPU≤2 ms 为阶段目标；极近/重叠/大量窄相碰撞单列。完整游戏仍需接入后复测。

## 工具隔离

所有新增工具与运行数据位于仓库根目录：

| 目录 | 内容 |
| --- | --- |
| `.tools/` | Blender 4.5.3、CMake 3.31.5、MinGW 16.2.0 的独立副本 |
| `.deps/` | raylib 5.5 源码及其附带的查看器字体 |
| `.cache/` | 下载包、临时目录、Python 与工具缓存 |
| `.local/` | Blender 用户资源、查看器设置、日志 |
| `build/projectile-review/` | 独立查看器和碰撞测试程序 |
| `output/projectile-review/` | 自动检查报告、截图及转台序列 |

这些目录不提交。模型、贴图、碰撞资产、制作脚本、查看器代码、版本锁与本文提交到仓库。

首次在另一台 Windows x64 机器使用，运行根目录 `Setup-Projectile-Tools.cmd`。脚本按 `tools/projectile_review/toolchain.lock.json` 下载便携包并验证 SHA-256；Blender 官方下载失败时使用已列出的镜像，校验值不变。需要网络、系统 PowerShell、curl 和 tar，不调用全局包管理器。

当前工作区的 CMake 与 MinGW 从已有安装只读复制到 `.tools/`，实际构建使用独立副本；锁文件记录其可执行文件 SHA-256 和对应原始发行包。Blender 与 raylib 使用下载的校验包。没有修改原工具目录、系统 PATH、文件关联、注册表或用户 PowerShell 配置。

根目录启动器通过新 PowerShell 子进程调用脚本。进程内将工具 PATH、TEMP/TMP、Blender 用户资源及 Python 缓存导向项目；退出后不污染调用者。Blender Python 不加载用户级包；CMake 禁用包注册表查找和写入。不要在日常交互式终端 dot-source `environment.ps1`。

## 单件制作流程

1. **需求卡**：确定具体类别和颜色，记录现实结构参考、材质、轮廓、动画和碰撞语义。每轮只做一个具体样品；未经人工通过不扩展同类颜色或其他类别。
2. **实体结构**：Blender 创建有厚度的封闭部件。轮廓、刃脊、缠绕、孔洞用几何表达，细小纤维与磨痕使用贴图。
3. **独立材质**：制作 Base Color、Normal、Roughness、Metallic、AO，并导出 ORM。底色不含固定照明高光；不使用原游戏精灵拉伸成模型表面。
4. **可复现导出**：保留 `.blend` 和外置 PNG，导出 glTF；同时从最终几何导出碰撞数据，不能另造一个覆盖孔洞的包围体冒充碰撞。
5. **自检**：模型闭合、退化三角形、UV/法线、资源路径、导出网格与碰撞顶点一致性、接触语义、不同背景和观察角度。
6. **人工验收**：在独立查看器旋转、放大并检查贴图和真实碰撞。记录用户意见并修改当前件，直至明确通过。

未来光球、玉和大泡要求外层半透明球壳；整球内部参与碰撞，外部光晕/拖尾不参与。它们尚未制作。

## 独立查看器

以下为银色样品操作，运行 **`Review-Kunai.cmd -Action silver`**。程序不链接 `koumakyou_engine`，不会启动关卡、读取玩家记录或改变游戏设置。

| 操作 | 功能 |
| --- | --- |
| 左键拖动 / 右键拖动 / 滚轮 | 旋转 / 平移 / 缩放 |
| 1 / 2 / 3 | 成品材质 / 灰模 / 线框 |
| 4 / 5 | 碰撞体单显 / 实际碰撞网格叠加 |
| T | 独立贴图通道，顶部选择 Base Color、Normal、Roughness、Metallic、AO |
| Front / Side / Back | 正、侧、背视角 |
| Q / E | 旋转棚灯；左侧 +/- 调节曝光 |
| P | 开关球形探针 |
| Tip / Blade / Grip / Ring / Hole | 探针预设位置 |
| 方向键 / Page Up / Page Down | 探针沿模型 X/Y / Z 移动 |
| 键盘 + / -，Shift | 探针半径增减；Shift 加快移动 |
| Space / R / F12 | 自动转台 / 重置 / 保存当前截图 |

接触显示红色 CONTACT；无接触为绿色 CLEAR。Gap 是表面距离减探针半径，负值表示相交。模型单位为标准化长度，**不是米，也不是已确定的游戏世界尺寸**。界面英文仅用于该独立技术审阅工具。

程序支持明亮、深色、中性背景。实时材质采用独立法线与 ORM、GGX 直接照明及解析棚灯环境反射；不依赖 Blender 离线渲染。截图和性能结果不能代替人工美术验收。

## 重建与检查

从仓库根目录运行，环境设置只发生在这些子进程中：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/projectile_review/run.ps1 -Action generate
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/projectile_review/run.ps1 -Action test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/projectile_review/run.ps1 -Action capture-silver
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/projectile_review/run.ps1 -Action benchmark
```

`generate` 生成银色样品几何和 2K 材质；`generate-classic` 只生成新版纯色低面数资产。均使用 Blender 附带 Python，不需要系统 Python。`test` 构建独立工具，运行两版资产检查与 CTest。`capture-silver` 输出银色十二张审阅图及 24 帧转台；`capture` 输出新版十三张审阅图。`benchmark` 是旧版单件隐藏窗口计时；8192 发测试使用 `stress`，`stress-quick` 仅用于检查测试程序能运行，不作性能结论。

Blender 工程、glTF、纹理和碰撞是本任务的正式资产，应提交；临时烘焙文件、构建产物、截图缓存不提交。

## 参考与权利说明

- 现实结构参考：[伊贺流忍者博物馆忍具目录](https://www.iganinja.jp/about/museum/pdf/collection_ningu.pdf)。本件是游戏用现实材质苦无设计，不宣称复原某一件馆藏。
- 东方氛围参考：[红魔乡 New Classic 官方站](https://touhou-project.news/shanghai-alice-reprise/)、[不可思议的幻想乡 FORESIGHT 官方画廊](https://fushigen4.jp/gallery/)。没有复制页面图片到模型贴图。
- glTF 材质约定：[Khronos glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)。颜色图使用 sRGB，法线及 ORM 使用线性数据。
- 查看器字体使用 raylib 附带的 Anonymous Pro Bold（Mark Simonson，SIL Open Font License）；来源说明位于 `.deps/raylib-5.5/examples/text/resources/LICENSE.md`。

验收状态只能依据用户明确反馈更新，不能由自动测试或代理代签。新版苦无已由用户与 leader 明确通过；重新生成或修改模型后应重新验收。

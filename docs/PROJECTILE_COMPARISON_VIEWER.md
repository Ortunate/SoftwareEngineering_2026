# 原作素材与 3D 弹体对照查看器

在仓库根目录运行：

```bash
python3 tools/compare_projectiles.py
```

脚本自动配置 CMake、按需构建 `projectile_comparison` 并打开中文桌面窗口。需要 Python 3.9+、CMake、项目现有的 C++20/raylib 构建环境与 OpenGL 图形会话；无需 Blender 或额外 Python 包。优先复用本项目的 raylib 源码。

macOS 可双击 `tools/Compare-Projectiles.command`；Windows 可双击 `tools/windows/Compare-Projectiles.cmd`。已经构建后，使用 `--no-build` 跳过配置与编译。

## 覆盖范围

| 类别 | 查看项 | 颜色／档位／素材层变体 |
|---|---:|---:|
| 普通敌弹 | 10 | 118 |
| 敌方激光 | 2 | 32 |
| 灵梦自机弹 | 3 | 3 |
| 魔理沙主弹、导弹、激光、爆破 | 4 | 12 |
| 四种 Bomb | 4 | 15 |
| 附加：两名角色的左右副机 | 2 | 4 |
| 合计 | 25 | 184 |

全部 24 份正式模型资源都可查看。封魔阵沿用生产环境的贴图平面几何，界面明确标注没有独立网格；Bomb 展示单体模型与各素材层，不模拟整场七球、64 星或六面阵的完整编排。

梦想封印右侧同步切换 ANM 133–136 对应的蓝、青、绿、红四色；星群同步切换三种配色。光炮右侧保留组合单体材质，左侧可切换原作各图层。

左侧直接从仓库的原作 DDS/ANM 裁取素材，保留纹理 Alpha，可选择应用脚本 RGB；为了看清素材图，不叠加原作淡入淡出、位移与旋转。下方显示图集、ANM、精灵编号及脚本缩放、Alpha，颜色按钮也使用实际素材缩略图。

右侧复用当前 `ProjectileAssets`、弹体集合、激光、玩家弹和 Bomb 渲染路径，包括目前的平滑法线与抛光材质。副机核心固定在世界轴上供查看厚度和背面，保留左右镜像、自转规则。光束使用适合查看的代表长度，各项自动适配取景，不表达不同弹种在游戏中的相对大小或射程。原图方向保持不变，模型采用其正式局部轴；可用视角滚转比较朝向差异。

查看器只加载素材，不启动游戏、音频、关卡时间轴，也不修改玩家设置、成绩或素材文件。

## 操作

| 操作 | 控件 |
|---|---|
| 转动视角 | 在右侧模型区按住鼠标左键拖动 |
| 缩放 | 模型区鼠标滚轮 |
| 绕视线滚转 | `Q` / `E`，每次 15° |
| 正面／侧面／背面／俯视 | `1` / `2` / `3` / `4` |
| 重置视角与缩放 | `R` |
| 切换弹种 | 点击左侧列表，或 `Tab`、`↑`、`↓`；`Shift+Tab` 反向 |
| 切换颜色、档位或素材层 | 点击缩略图，或 `C`、`←`、`→` |
| 播放／暂停 | `Space` 或点击播放文字 |
| 前后逐帧 | `,` / `.`，按 60 Hz 内容帧采样 |
| 定位动画帧 | 点击或拖动下方时间条 |
| 线框、网格、深浅背景 | `W` / `G` / `B` |
| 原图／脚本 RGB 着色 | `N` |
| 审阅亮度／局内 Alpha | `A`；原图面板始终保留自身纹理 Alpha |
| 导出当前对照图 | `F12` |
| 退出 | `Esc` 或关闭窗口 |

初始弹种、变体与视角也可由参数指定，变体编号从零开始：

```bash
python3 tools/compare_projectiles.py --entry marisa-laser --variant 2 --view side
python3 tools/compare_projectiles.py --entry bomb-spark --view back --no-build
```

有效弹种标识见 `tools/projectile_review/comparison_catalog.hpp`，例如 `pellet`、`rice`、`kunai`、`bubble`、`laser-round`、`reimu-gold`、`marisa-missile`、`bomb-stars`、`reimu-option`。

## 全目录检查与截图

```bash
python3 tools/compare_projectiles.py --verify
# 或指定输出目录
python3 tools/compare_projectiles.py --no-build --verify --capture-dir /tmp/projectile-comparison
```

`--verify` 创建隐藏图形窗口，对 184 个变体检查原图与模型非空，并为每项的第一个变体额外检查正、侧、背、俯视，共 284 组样本。会导出 25 张主对照图、额外四张梦想封印颜色差分图与 `report.json`，记录原图绑定和像素检查结果。检查还验证全部 24 个模型编号都已覆盖。`passed` 仅表示覆盖与显示检查成功，不代替对造型、颜色和三维适配的人工审阅。

默认输出目录为已忽略的 `output/projectile-review/comparison/`；交互模式的 F12 截图也写入此处。可用 `--capture-dir` 修改，用 `--build-dir`、`--config` 选择其他构建目录／配置。

启动器的无窗口参数测试：

```bash
python3 -B tests/test_projectile_comparison_launcher.py
ctest --test-dir build-debug -R projectile_comparison_launcher_tests --output-on-failure
```

2026-11-04 在 macOS 实测：自动配置／编译／全目录检查的一键流程通过，25 项、184 变体、24 份模型及 284 组画面检查正常；macOS 双击入口的交互绘制循环可正常运行和退出。启动器四项单元测试与对应 CTest 通过。

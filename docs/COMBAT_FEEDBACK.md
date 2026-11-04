# 弹体材质、近镜头可读性与战斗提示音

## 光滑材质

`render/projectiles/smooth_normals.hpp` 在资源加载时对同位置、同显示角色的三角形按角度加权平滑法线，超过 60° 的折角保留硬边；实体与外壳不跨 `COLOR_0.g` 角色混合。普通敌弹集合、兼容苦无、敌激光和自机/Bomb 显示网格采用相同规则。副机继续使用原材质。

片元高光采用柔和底层反射叠加更集中的镜面峰值，减小大面积白色覆盖，保留弹色。光效随观察角度变化，不增加固定世界光向的暗面；点积钳制到 `[0,1]`，防止轮廓幂函数在浮点越界时产生无效颜色。灵梦实体复用自机材质通道，保留原 glTF 贴图、UV 和 Alpha。

法线只在加载时计算和上传，局内没有逐帧重算法线；原网格位置、索引、面数及 KCOL 碰撞文件不变。

## 近自机弹幕淡化

`render/projectile_visibility.hpp` 按弹体显示表面到自机的世界距离计算 Alpha 系数。表面距离不超过 `0.15` 时达到最低透明度，从 `0.15` 到 `1.25` 使用 smoothstep 平滑恢复，超过 `1.25` 保持原始 Alpha。普通近距离敌弹保留至少 `35%`；正在逼近的危险弹与生效光束保留至少 `65%`，不会因改善后方视野而隐去近身威胁。

普通弹使用显示外形半径，光束使用自机到整段胶囊的最近距离；大弹不会等到中心贴近后才开始淡化。计算只读实际自机位置，回放的自由观察相机不改变弹幕 Alpha。模型、原生公告板、旧球体回退和敌激光共用该规则；碰撞、擦弹、寿命和弹池数据不受影响。

显示包围球在资源加载时缓存，不借用碰撞半径。淡化后的主体退出不透明深度通道，并逐实例保留远到近的透明队列顺序；只有不透明主体的装饰继续按弹种合批，防止不同弹种重排造成混色反转。显现动画 Alpha 与距离系数相乘。

## 音效资源与触发

新增声音由 `tools/generate_sound_assets.py` 确定性合成，格式为 44.1 kHz、单声道、16 位 PCM WAV，不是从原作提取的声音。已随运行时素材分发，游戏不依赖 Python。

| 资源名 | 路径 | 时长 | 触发 |
|---|---|---:|---|
| `life_up` | `common/se/life_up.wav` | 0.55 秒 | 收取 Life/1UP 道具；同帧聚合奖励优先提示加命 |
| `countdown` | `common/se/countdown.wav` | 0.12 秒 | 活动符卡真实剩余 10～6 秒的整秒边界 |
| `countdown_urgent` | `common/se/countdown_urgent.wav` | 0.18 秒 | 活动符卡真实剩余 5～1 秒的整秒边界，使用更高音调 |

三者由通用音效表加载并沿用 SE 音量设置。普通拾取继续使用 `pickup`；加命提示优先于同帧普通拾取，避免被其掩盖。

`gameplay/effects/combat_audio.hpp` 使用技能的实际秒时钟，取剩余秒的向上整数边界，并容许 `1e-4` 秒的浮点舍入误差。只在 `Game::UpdateBoss` 推进战斗时触发，绘制或暂停不会重复计数；非符卡、无计时、归零及技能结束时静音。跨越多个整秒的一次更新只播放当前提示，不补播积压声音；换符或时钟归零时不继承上一符的倒计时。

重新生成合成素材用于审阅：

```bash
python3 tools/generate_sound_assets.py /tmp/combat-sounds
```

## 验证入口

```bash
cmake -S . -B build-debug
cmake --build build-debug --target koumakyou koumakyou_tests combat_feedback_tests \
  projectile_runtime_tests koumakyou_sprite_overlap_tests koumakyou_lifecycle_tests -j 6
ctest --test-dir build-debug --output-on-failure
./build-debug/projectile_runtime_tests assets/runtime /tmp/polished-models
./build-debug/koumakyou_sprite_overlap_tests /tmp/near-fade-overlap assets/runtime
./build-debug/koumakyou_sprite_overlap_tests --fade-only
./build-debug/koumakyou_lifecycle_tests /tmp/combat-audio --combat-audio-only
./build-debug/koumakyou_lifecycle_tests /tmp/polished-shots --player-shots-only
./build-debug/koumakyou_lifecycle_tests /tmp/polished-marisa --marisa-only
./build-debug/koumakyou_lifecycle_tests /tmp/polished-bombs --bomb-only
```

`combat_feedback_tests` 无窗口检查曲面连续、硬边和显示角色隔离，以及 30/60/120/144/240 FPS 的十次倒计时提示。模型图形用例检查相机旋转后的颜色一致性、分批像素、原碰撞资源与激光 MSAA 接缝。音频集成测试初始化真实设备并强制静音，检查 WAV 加载、1UP/普通拾取、两级倒计时及暂停/结束门禁，不修改用户设置或成绩。

近自机淡化由 `projectile_direction_tests` 检查远近系数、逼近/离开下限、大弹和线段距离；`koumakyou_sprite_overlap_tests` 用独立观察相机读取 35%/65% 实际像素。`projectile_runtime_tests` 将不同弹种的统一队列与逐弹绘制比较，覆盖正常/显现期 Alpha、主体/装饰顺序，并验证绘制不改战斗 Alpha。

2026-11-04 近自机淡化验证：游戏、核心测试、模型运行时、重叠测试、生命周期目标及相关查看/压测工具构建通过；全量 CTest 9/9 通过（398.93 秒），正式弹体运行时和完整图形重叠回归通过。截图位于 `/tmp/projectile-runtime-fade-final/` 和 `/tmp/projectile-fade-overlap-final/`，不进入仓库。

2026-11-04 验证：构建完成，CTest 8/8 通过（411.40 秒）；模型、透明遮挡、灵梦/魔理沙射击、四种 Bomb 和静音音频集成检查通过。三个新增 WAV 与生成器输出逐字节一致，均非静音且未削波。

所有检查结束后，对 `marisa-lasers` 另做一轮 Debug、4×MSAA 短测（预热 2 秒、采样 6 秒）：窗口 P95 为 9.054 ms，绘制 1060 次，帧内 uniform 名称查询为 0。此轮只作负载冒烟检查，不计算与历史版本的收益比例。本机日志与截图位于忽略目录 `output/render-performance/polished-audio-20261104/`。

# 敌方激光建模与独立验收

两套激光资源分别位于 `assets/projectiles/laser_default/`、`laser_round/`；每套 16 色，共 32 种素材选择。默认套组按连续白芯彩边条带适配；第二套根据圆形光斑做轴向亮度渐收、圆润端帽的三维适配，两套共享胶囊轮廓而材质分布不同。第二套在当前游戏未绑定，不宣称已还原其原作关卡用途。两套人工验收均待完成。

## 资源与参数

每套有 `.blend`、`.gltf`、同名 `.bin`、`asset.json`、`REVIEW.md`。原图复制到 references，来源为仓库目录的 etama3_2 ANM 0/1、S0–15/S16–31。每套由 144 面主体和 144 面外光层组成，共 288 面，硬上限 384；三通道带光效实际提交 432 面/实例。没有拉伸精灵贴图或独立 PBR 纹理。

源模型 +Y 为轴线，端点在 Y=0/1；Y<0 和 Y>1 是单位半径端帽偏移。运行时独立重建中段长度与端帽半径。普通 DrawModel 会显示制作模板比例，需配套 laser_renderer 与 laser.vs/fs 才是最终效果。

危险体为同端点、同半径的解析胶囊（端点外各扩展半径），外光层半径为主体的 1.28 倍，光晕为 1.45 倍，均不参与碰撞。碰撞不是固定网格，因此不保存固定 collision.bin，参数定义在 asset.json 中。显示为低面数内接曲面，端帽及圆周与解析曲面的离散偏差约不超过半径的 7%，不声称逐三角形完全重合。

`LaserInstance` 的 origin、length、radius 使用世界单位，direction 必须为单位向量；variant 0..1、color 0..15、glowStrength 0..2。age/warning/growEnd/dangerEnd/end/fadeSeconds 单位秒，时点单调。危险时窗为 [warning,dangerEnd)，end 后隐藏。视觉宽度遵循现有 Bullet::BeamVisualScale，独立于危险时窗；默认预警 2 秒、持续至第 5 秒、收束至第 6 秒。fadeSeconds 独立控制收束时长，危险截止与淡出起点可不同；接入时分别映射实际脚本字段。

构造 `LaserRenderer(root,totalCapacity)`，在 BeginMode3D 中调用 Draw(instances,camera,aspect,mode,glow)，窗口关闭前析构。mode 0成品/1灰模/2主体/3外光；查询用 LaserTouches，探针半径非负，非法参数抛异常。实例缓冲构造时预分配，每实例 64 字节，8192 容量为 512 KiB GPU 与同量 CPU 打包空间；无逐弹 GPU 对象，无每帧创建网格。无视锥剔除，全部活动光束提交；每套带光效最多三次绘制，两套共六次，与普通弹共用总数预算时由调用方分配容量。

## 独立查看

双击 `Review-Lasers.cmd`；不启动关卡、不改现有游戏渲染或碰撞。

- Tab 选套组，C 切颜色，T 展示 16 色，V 同色并排，O 多束交叉。
- 1成品、2灰模、3主体、5外光；4碰撞叠加，6仅胶囊。绿色表示当前危险，灰色表示当前无害。
- Q/E 改中段长度，Z/X 改半径；鼠标旋转、滚轮缩放，可沿轴观察或进入束体。
- Space 暂停/播放，逗号/句号逐 1/60 秒，R 返回持续期。
- H 循环端帽/外光/轴身探针位置，方向键与 PgUp/PgDn 移探针。橙色 CONTACT，绿色 CLEAR；预警与退场均 CLEAR。
- G 光效，B 背景，F12 截图。

`Review-Lasers.cmd -Action capture-lasers` 输出 32 张固定审阅图到 output/projectile-review/lasers，包括两套、正侧端视、分层、碰撞、色表、预警/展开/持续/收束、浅底、交叉与入束。

## 生成与检查

`Review-Lasers.cmd -Action generate-lasers` 用项目内 Blender 生成两套资产；重新生成保留已确认的人工验收记录。`-Action test` 统一构建和检查当前普通弹、苦无及激光：闭合网格、面预算、有限坐标、角色分层、胶囊轴身/端帽/外光探针、旋转、非法输入、时窗边界和游戏视觉宽度一致性。

`Stress-Lasers.cmd` 跑完整实验：两套各 32/128/512 束、两套极近、512 束交叉、512 束加 7680 普通弹混合、8192 束容量极限；11 场景各三次，1920×1080、无 MSAA/垂直同步/限帧，预热 3 秒采样 10 秒。记录整帧、运动和胶囊探针 CPU、异步 GPU 均值/P50/P95/P99，调用和 GPU 查询跳过数。极近和 8192 束是单列压力，不能当作正常关卡数量。

原始结果及报告在 output/projectile-review/lasers/{performance.csv,performance.md,hardware.txt}；普通目标整帧 P95≤16.7 ms、GPU≤6 ms、更新/碰撞 CPU≤2 ms。本轮完整 33 次实验已完成，普通场景全部通过；套组 0/1 的 512 束整帧 P95 为 0.211/0.246 ms，极近为 2.318/2.522 ms，混合场景 0.752 ms，8192 束极限 3.546 ms。该测量使用默认收束时长 1 秒，后续接口将收束时长独立为 fadeSeconds，默认表现不变；最终接口另已完成时序回归。不能用短时诊断代替完整结果。工具、依赖、缓存继续隔离在项目根目录。

按用户最新指示，之后不超过 1000 面的模型不再执行性能相关测试。本轮每套 288 面，保留已经完成的结果，不追加性能测量。几何、碰撞及时序三项 CTest 和激光资产验证通过；32 张最终审阅截图已生成，两套均已获用户人工验收通过。

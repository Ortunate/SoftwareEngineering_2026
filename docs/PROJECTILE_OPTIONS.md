# 副机三维资产

灵梦与魔理沙副机外观已由用户验收。每款为 528 三角面的真实球体，独立 512×512 surface.png，正面 +Z、上方 +Y、半径 0.42。副机不参与碰撞，不导出 collision.bin。

灵梦保留红白太极及阴阳点。魔理沙采用白球、紫蓝绿渐变圆角五星双层轮廓，内层加粗，外层向外柔化。模型纹理为独立绘制；球体正反面共用正交投影，侧面会随球面压缩纹样。

每个资产目录保存 .blend 源文件、glTF/bin、surface.png、asset.json 与 REVIEW.md。元数据保存验收指纹；模型、纹理或查看器变化会要求复验。

双击根目录 Review-Options.cmd 独立查看；Tab 切换、V 总览、T 双侧、鼠标拖动与滚轮调整相机；1 白模、2 实体、3 最终效果、G 光晕、4 显示边界、B 背景、F12 截图。A 将模型直径缩为游戏基础显示高度 0.42。

Space 运行独立查看器演示旋转，灵梦两侧反向、魔理沙同向，演示速度为 90 度/秒；正式游戏已经接入，使用原 ANM 时序及文末描述的左右模型变换。白模模式用于几何查看，不演示旋转。光晕由查看器径向透明贴图与加色混合提供，随相机朝向，不是模型硬壳。

使用 run.ps1 的 generate-options、options、capture-options 动作重建和查看；截图在 output/projectile-review/options。全部依赖继续隔离在项目内，不执行性能测试。

## 局内自转与 Focus GIF 验收

output/projectile-review/options-ingame/reimu-options.gif 与 marisa-options.gif 使用真实 Player 更新、原 ANM 游标和 Game::Draw 输出。每段八秒：前两秒普通状态，中间三秒按住 Focus，最后三秒松开恢复；自转全程持续。自动检查收拢间距约为普通状态三分之一、松开恢复及原生角度变化，人工验收独立进行。

复现：构建 koumakyou_lifecycle_tests，以 output/projectile-review/options-ingame 和 --options-capture 为参数运行，再运行 tools/projectile_review/encode_options_ingame.py（参数为仓库根目录）。

局内左右差异：灵梦左侧（原 ANM script 128）在自转前沿本地 X 镜像模型；魔理沙右侧（script 129）对原生旋转角取反。变换只作用于本次模型显示，不修改共享资产、ANM 游标、Focus 位置和缩放。

2026-10-29 两款副机的局内自转与 Focus 动画经人工验收通过，包含灵梦左侧镜像及魔理沙右侧反向旋转。

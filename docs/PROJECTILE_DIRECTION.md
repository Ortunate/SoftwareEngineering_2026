# 弹幕方向与模型坐标约定

Bullet::Velocity() 与 Direction() 为只读值接口。SetVelocity(v) 在非零有限速度下同步 direction=normalize(v)；零速度保留上次方向，静止出生默认世界 +Z。SetInitialDirection 只允许静止对象设置非零朝向。固定激光用 SetBeamDirection 指定束轴，与普通弹共用 direction；BeamEnd、胶囊判定与绘制不再维护第二份 beamDirection。

生成、加速度、追踪、延迟转向、导弹提前返回路径、命中减速等速度写入均使用 SetVelocity。对象池复用 Bullet{}，恢复默认方向。Spawn 拒绝非有限速度；SpawnBeam 拒绝零方向。原运动积分、无害期与伤害时窗保留。

全部 24 款资产在 asset.json 中声明 localForward、localUp、surfaceFront，生成脚本同步写入。尖端和束轴采用实际导出 +Y；球形模型采用规范 +Y；副机采用图案正面 +Z。localUp 与正向正交。surfaceFront 是图案面的朝向，不能用来代替运动正向。此轮未重建网格、纹理或碰撞文件。

正式 CPU 目录 src/resources/projectile_catalog.hpp 提供 DescribeProjectileModel(name)。src/render/projectile_pose.hpp 的 ProjectilePoseBuilder::BuildPose 接收模型方向、Bullet::Direction()、位置和正常缩放。首帧确定参考横滚，后续传入上帧四元数做最小旋转传输；180 度掉头使用上帧参考轴，自转只允许绕当前运动方向，rollDelta 为本帧弧度增量。

```cpp
auto pose = ProjectilePoseBuilder::BuildPose(
    DescribeProjectileModel(assetName).direction,
    bullet.Direction(), bullet.position, normalScale, previousRotation);
// 显示与碰撞共用 pose；spawn 仅在显示阶段额外改变 scale/alpha。
```

Review-Effects.cmd 已引用正式姿态计算：十类模型连续转向，绿色线段标示世界正向，D 开关方向线，C 叠加使用同一正常姿态的碰撞网格（不跟随 spawn 额外放大）。副机/Bomb 的方向由其演员或效果状态提供，不要求继承 Bullet。游戏现已接入正式三维模型批次和网格碰撞；速度/束轴、连续正常姿态与危险几何同步。Bullet::Orientation() 保存规范本地 +Y 的连续姿态，模型坐标适配后由渲染与碰撞共同读取。

方向测试覆盖速度归一化、停下保持、反向、非法输入、激光端点、池槽复用及全部模型的六轴/连续转弯对齐。未进行性能测试。

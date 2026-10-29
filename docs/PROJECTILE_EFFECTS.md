# 传入模型的公用显现效果

用户已验收通过：模型本身由大缩小，透明度由高变低（逐渐显现），结束后保持最终状态。已接入游戏运行时，敌弹显现读取 spawnVisualSeconds，不改变运动和无害时窗。

公用函数为 SpawnRenderer::Draw(const Model&, const SpawnParams&)，直接接收调用方加载的 raylib Model，使该模型从大缩小、透明度由高到低（alpha 从 0 到 1），到期保持正常大小和完整显示。没有额外球壳，也不自动显现或保留另一份正常弹体。defeat 三维效果已取消。

```cpp
review::SpawnRenderer spawn(projectRoot); // 有效 OpenGL 上下文内创建，持有公用 shader
review::SpawnParams p;
p.position = position;
p.scale = normalScale;
p.age = ageSeconds;
p.duration = 0.65F;
p.startMultiplier = 2.4F;
p.endMultiplier = 1.0F;
// BeginMode3D(camera) 内：
spawn.Draw(model, p);
```

模型由调用方持有和释放。Draw 不修改模型、材质、网格或 transform，不改变运动和碰撞；保留模型自身 transform、纹理和材质基础色，用统一 Alpha 混合 shader 作为显现阶段的绘制通道，不执行调用方模型原有的自定义 shader。多个模型共享一个 SpawnRenderer，各自传入独立年龄即可。

SpawnParams 的位置使用世界单位，rotation 为单位四元数，scale 是模型正常统一缩放。age>=0、duration>0，以秒计；startMultiplier>=endMultiplier>0，opacity 在 [0,1]。采样采用平滑缩放曲线，alpha 单调上升（透明度下降）；结束后保持最终状态继续绘制，或由调用方切回正常绘制，避免重复绘制两份。颜色 tint 默认白色乘数。projectileMask 默认关闭，仅展示本仓库普通敌弹时开启，以解释 COLOR_0 高光与部件遮罩；普通带贴图模型保持默认。

Review-Effects.cmd 用十款已建模敌弹作为实际 Model 入参展示。Tab 切换、V 总览、Space 暂停、R 重播、G 对照原模型、B 背景、鼠标拖动/滚轮改变相机，1/2/3 设置 10/16/32 帧时长，F12 截图。默认 0.65 秒用于看清过程。

capture-effects 渲染原帧，encode_native_effects.py ROOT effects-3d 编码 output/projectile-review/effects-3d/spawn-model-reveal.gif。GIF 中模型自身从大缩小、从透明变得不透明并保持显示，3 秒循环。不生成额外三维资产，不做性能测试。

编码器使用项目 .tools/blender-4.5.3-windows-x64/4.5/python/bin/python.exe 与隔离在 .deps/gif-py311 的 Pillow 13.0.0。native_effects_preview 可重渲染原 DDS/ANM 的参考帧；编码器不带 effects-3d 参数时编码原素材预览。参考素材与本次三维 spawn 分开保存，不把已取消的三维 defeat 效果作为交付内容。

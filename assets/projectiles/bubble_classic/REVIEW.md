# 收紧内部球、加厚白亮外球壳与静态三维放射尖丝

状态：待人工验收。Type 9，5 个有效颜色，共享一个网格，代表色 C01。

几何总数 696 三角形，实体碰撞 288 三角形。预算硬上限 768。

| 部件 | 三角形 | 碰撞角色 |
| --- | ---: | --- |
| InnerVisualSphere | 120 | 显示装饰 |
| TransparentOuterShell | 288 | 显示装饰 |
| StaticRadialFilament_00 | 6 | 显示装饰 |
| StaticRadialFilament_01 | 6 | 显示装饰 |
| StaticRadialFilament_02 | 6 | 显示装饰 |
| StaticRadialFilament_03 | 6 | 显示装饰 |
| StaticRadialFilament_04 | 6 | 显示装饰 |
| StaticRadialFilament_05 | 6 | 显示装饰 |
| StaticRadialFilament_06 | 6 | 显示装饰 |
| StaticRadialFilament_07 | 6 | 显示装饰 |
| StaticRadialFilament_08 | 6 | 显示装饰 |
| StaticRadialFilament_09 | 6 | 显示装饰 |
| StaticRadialFilament_10 | 6 | 显示装饰 |
| StaticRadialFilament_11 | 6 | 显示装饰 |
| StaticRadialFilament_12 | 6 | 显示装饰 |
| StaticRadialFilament_13 | 6 | 显示装饰 |
| StaticRadialFilament_14 | 6 | 显示装饰 |
| StaticRadialFilament_15 | 6 | 显示装饰 |
| StaticRadialFilament_16 | 6 | 显示装饰 |
| StaticRadialFilament_17 | 6 | 显示装饰 |
| StaticRadialFilament_18 | 6 | 显示装饰 |
| StaticRadialFilament_19 | 6 | 显示装饰 |
| StaticRadialFilament_20 | 6 | 显示装饰 |
| StaticRadialFilament_21 | 6 | 显示装饰 |
| StaticRadialFilament_22 | 6 | 显示装饰 |
| StaticRadialFilament_23 | 6 | 显示装饰 |
| StaticRadialFilament_24 | 6 | 显示装饰 |
| StaticRadialFilament_25 | 6 | 显示装饰 |
| StaticRadialFilament_26 | 6 | 显示装饰 |
| StaticRadialFilament_27 | 6 | 显示装饰 |
| StaticRadialFilament_28 | 6 | 显示装饰 |
| StaticRadialFilament_29 | 6 | 显示装饰 |
| StaticRadialFilament_30 | 6 | 显示装饰 |
| StaticRadialFilament_31 | 6 | 显示装饰 |
| StaticRadialFilament_32 | 6 | 显示装饰 |
| StaticRadialFilament_33 | 6 | 显示装饰 |
| StaticRadialFilament_34 | 6 | 显示装饰 |
| StaticRadialFilament_35 | 6 | 显示装饰 |
| StaticRadialFilament_36 | 6 | 显示装饰 |
| StaticRadialFilament_37 | 6 | 显示装饰 |
| StaticRadialFilament_38 | 6 | 显示装饰 |
| StaticRadialFilament_39 | 6 | 显示装饰 |
| StaticRadialFilament_40 | 6 | 显示装饰 |
| StaticRadialFilament_41 | 6 | 显示装饰 |
| StaticRadialFilament_42 | 6 | 显示装饰 |
| StaticRadialFilament_43 | 6 | 显示装饰 |
| StaticRadialFilament_44 | 6 | 显示装饰 |
| StaticRadialFilament_45 | 6 | 显示装饰 |
| StaticRadialFilament_46 | 6 | 显示装饰 |
| StaticRadialFilament_47 | 6 | 显示装饰 |
| CollisionInsideHalo | 288 | 实体碰撞 |

碰撞约定：白色光圈内缘以内的整个球体有碰撞（半径 0.40）；白色光圈不参与。颜色、透明度和外部光效不改变碰撞。

无拉伸原精灵贴图；纯色、顶点亮面遮罩与独立程序化表面着色。源文件保留分部件及材质预览，glTF 合并网格，角色由 COLOR_0.g 编码，需配套运行时着色器解释。

## 本件人工检查

- [ ] etama4 S0–4 的暗透中心、白亮外缘和密集放射尖丝清楚；白色款尖丝弱化
- [ ] 48 根静态尖丝分布于两球之间，正侧背均有真实空间层次
- [ ] 内球 CONTACT；球间、尖丝和外壳 CLEAR；关闭显示层不改变碰撞
- [ ] 本类所有有效颜色、深浅背景、常用镜头尺寸及放大视图均检查
- [ ] 独立碰撞单显与叠加已检查，明确同意本件通过

使用 tools/windows/Review-Projectiles.cmd 选择本件。参数、接口及统一测试记录见 [全组交付说明](../../../docs/PROJECTILE_COLLECTION_REVIEW.md)。本轮人工验收尚未通过，自动检查不能代签。

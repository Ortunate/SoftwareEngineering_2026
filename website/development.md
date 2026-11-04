# 开发 Wiki

这里按实现主题组织仓库文档。每篇导入文档都保留源文件链接和构建提交；原文中的历史数据与规划内容带有适用范围说明。

## 从整体开始

- [课程 NABCD](/reference/course/NABCD)：2026 年 11 月 4 日更新的需求分析及功能边界。
- [原型系统设计](/reference/course/PROTOTYPE_SYSTEM_DESIGN)、[概要设计](/reference/course/OUTLINE_DESIGN)、[详细设计](/reference/course/DETAILED_DESIGN)：开发分支新增的课程设计材料。
- [当前实现架构记录](/reference/DESIGN)：模块职责、对象关系与运行流程，注意页首时效说明。
- [目标系统设计](/reference/SOFTWARE_DESIGN)：需求到设计的映射、接口和总体规划。
- [构建与运行](/reference/README)：平台工具链、启动入口和测试方式。

## 深入运行时

| 主题 | 阅读入口 |
| --- | --- |
| 关卡、时间轴和行为 | [关卡移植方法](/reference/STAGE_PORTING_METHOD)、[Boss 配置](/reference/BOSS_START_CONFIG) |
| 三维弹体与碰撞 | [正式运行时接入](/reference/PROJECTILE_RUNTIME)、[方向约定](/reference/PROJECTILE_DIRECTION) |
| 模型、素材和生命周期 | [建模方法](/reference/PROJECTILE_MODELING_METHOD)、[素材清单](/reference/NATIVE_BATTLE_ASSETS) |
| 菜单与交互 | [UI 模块](/reference/UI_MODULE)、[角色选择](/reference/START_CHARACTER_SELECTION) |
| 录像与确定性 | [Replay](/reference/REPLAY)、[文件协议](/reference/REPLAY_PROTOCOL) |
| 测试与优化 | [测试和性能索引](/testing) |

完整分类位于左侧导航。尚无仓库依据的开发说明保留在下方，后续补齐时应关联真实源码。

## 待补开发资料

::: info 待补充
面向最终交付版本的统一接口总览、完整测试报告及发布验收记录。
:::

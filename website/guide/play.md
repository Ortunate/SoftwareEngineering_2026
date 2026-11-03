# 玩法与模式

## 在空间中躲避

战场允许沿 X、Y、Z 三轴移动，摄像机位置就是玩家判定点。观察方向与移动路线共同决定如何穿过弹幕空隙。三维操作与碰撞包含项目适配，不能将原作平面攻略直接视为等价解法。

## 角色与御札

灵梦与魔理沙共四种御札组合，主副弹和 Bomb 各有表现。具体弹种、火力表和实现边界见 [灵梦弹体](/reference/PROJECTILE_REIMU)、[魔理沙弹体](/reference/PROJECTILE_MARISA)及 [Bomb 对照](/reference/PLAYER_BOMBS_REFERENCE)。

## 主线与 Extra

- 主线接入 Easy / Normal / Hard / Lunatic；Easy 在五面结束，其余难度推进至六面。
- Extra 使用独立路线与入口。
- 标准模式与无限残机挑战分开处理记录，具体行为以游戏入口说明为准。

逐关覆盖和三维适配范围见 [关卡接入记录](/reference/STAGES_2_6_REFERENCE)、[Extra](/reference/STAGE_EXTRA_REFERENCE)与 [难度接入](/reference/DIFFICULTY_INTEGRATION)。

## 练习

Practice Start 直接选择可玩关卡。Spell Practice 提供符卡条目，实际可进入范围受到实现状态及解锁条件限制；目录中存在条目不代表所有组合已可游玩。

符卡解锁、取得与成绩按角色／御札组合记录。详见 [符卡练习](/reference/SPELL_PRACTICE_REFERENCE)。

## Power、擦弹与 Bomb

收集道具增加 Power 和分数，受击会降低火力。Bomb 同时涉及攻击、保护和清弹；不同御札效果有独立实现。规则数值与当前接入边界以 [运行说明](/reference/README)和 [战斗数据](/reference/NATIVE_COMBAT_DATA)为准。

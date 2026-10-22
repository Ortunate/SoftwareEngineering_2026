# Start／Practice Start 双角色选择

主人公阶段提供博丽灵梦、雾雨魔理沙两个候选。WASD／方向键循环切换，点击前景立绘、人物信息或按 Z／Space 确认进入御札页。人物两侧与 Spell Practice 共用金色三角箭头，轻微左右晃动，悬停强调，点击仅切换角色，不会同时进入下一页；晃动不改变固定命中区域。标题下方不再显示姓名选择按钮，其原区域也不响应交互；X／Esc／右键返回上一步。

两层立绘分别加载：前景使用 `spell_practice/reimu_modern.png` 或 `marisa_modern.png`。灵梦后层仍是原有 `face00a_1.png`，原裁剪、色调与 27% 透明度沿用；魔理沙后层使用对应原典 `slpl01a` 提围裙姿势的现代重绘 `common/portraits/marisa/face01a_1.png`，截取上部，以与灵梦相同的色调和 27% 透明度融入界面，不复用骑扫帚的前景。来源与提取坐标见 `assets/source/player_portraits/README.md`。

人物前景完整等比显示，后层按独立区域覆盖并裁切。角色切换时，两层、称号、姓名、速度／范围／攻击力及御札摘要使用同一展示权重交叉淡入；箭头只在主人公阶段响应。灵梦评级为 3／4／3，魔理沙为 4／1／4，御札、Bomb 描述共用完整角色目录。

两位角色均可实际开局。魔理沙的魔之御札使用主弹和加速导弹，恋之御札使用主弹和持续激光，分别绑定 player01 原生动画，并以原生星形副机替换灵梦太极圈。最终确认统一显示 START GAME，键盘与鼠标进入相同加载流程；详见 [魔理沙御札对照](MARISA_SHOTS_REFERENCE.md)。灵梦保持既有射击参数。

配置 version=8 新增 `last_character=0/1`；`last_shot=0/1` 为角色内御札编号，其他选择和符卡记录保留。旧配置默认灵梦，无须复制或迁移符卡解锁。选择时立即保存，取消、返回首页与重启均恢复角色和御札；独立 Spell Practice 继续使用自己的四组合记忆。

核心测试覆盖布局和角色配置往返；窗口测试覆盖两个入口、八种方向键、已删除姓名区域不响应、箭头悬停与点击不穿透、切换淡入、重启记忆、魔理沙实际机体开局，以及五种窗口比例。专项运行：

```bash
cmake --build --preset macos-release --target koumakyou_settings_ui_tests
./build/macos-release/koumakyou_settings_ui_tests output/stage1-review/start-characters --start-characters-only
```

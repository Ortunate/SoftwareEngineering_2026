# 新典原始战斗素材与动画接口

战斗演员、敌弹、玩家弹、激光、P 点、分数点、星点和击破动画直接读取本机《东方红魔乡：新典》的 DDS 与 ANM。对话、菜单和符卡立绘继续使用已经完成的现代重绘差分。

## 文件与来源

`assets/runtime/battle/native/` 保存从本机 `th06CM.dat` / `th06ST.dat` 提取的 25 张标准 DDS 和对应的 25 份 ANM，共 160.87 MiB。文件逐字节复制，没有裁图、缩放、重绘或转成 PNG；`checksums.sha256` 保存全部 50 个原件的 SHA-256。标准分辨率是原包已有版本；未重复收录同图的 `_4k` 版本。构建将整个目录复制到可执行文件旁的 `assets/battle/native/`，运行时不依赖本机游戏安装目录、解包工具或 Python。

| 图集文件名（省略扩展名） | 目录编号 / 内容 |
|---|---|
| etama3 | Bullets：普通弹、各类颜色、显现动画和七类道具 |
| etama3_2 | BeamBullets：激光图集 |
| etama4 | BulletEffects：大泡、显现、爆散、粒子等特效 |
| player00 / player01 | Reimu / Marisa：自机、移动与收势、玩家射击和 Bomb 演出图集 |
| eff00 | Effects：通用动画特效 |
| stg1enm / stg1enm2 | Stage1Enemies / Rumia |
| stg2enm / stg2enm2 | Stage2Enemies / Cirno |
| stg3enm / stg4enm | Stage3Enemies / Stage4Enemies（含对应关卡角色） |
| stg5enm / stg5enm2 | Stage5Enemies / Sakuya |
| stg6enm / stg6enm2 | Stage6Enemies / Remilia |
| stg7enm / stg7enm2 | ExtraEnemies / Flandre |
| eff01…eff07 | Stage1Effects…ExtraEffects：各关卡背景/符卡演出 |

目录包含 1084 个精灵定义和 347 个有效脚本。`stg7enm.anm` 的编号 71 有重复项，按原加载规则由后一项覆盖前一项。图集中的未使用角色、自机、道具与特效均已登记并可加载、播放、绘制；这不意味着当前已经实现二面以后的关卡或魔理沙玩法。

解包工具为 [thdat-nc](https://github.com/Renko6626/thdat-nc)。ANM 指令与特效编号参照 [th06 动画管理器](https://github.com/GensokyoClub/th06/blob/master/src/AnmManager.cpp)、[特效管理器](https://github.com/GensokyoClub/th06/blob/master/src/EffectManager.cpp)核对；项目自行实现解析、播放和绘制。BC7 解码使用 MIT 许可的 [bcdec](https://github.com/iOrange/bcdec)，许可说明见 `third_party/bcdec/README.md`。游戏素材沿用原作品素材，bcdec 许可不适用于游戏素材。

## 接口分层

- `resources/battle_catalog.*`：稳定 `Atlas` 编号、原始路径，以及弹幕、显现、自机与道具的原编号映射。`Catalog()` 返回全部素材，登记不占 GPU 内存。
- `resources/dds_image.*`：直接解码 BGRA32、RGBA32、BC1/2/3/7 为保留 Alpha 的 RGBA8 图像。`eff01…eff07` 是 DX10 BC7；非四倍数边长的边块也可读取，例如 eff04 的 413×407。
- `resources/animation.*`：读取版本 0、外置 DDS 的 ANM，执行精灵切换、循环、随机精灵、镜像、XYZ 旋转、缩放、Alpha/位移插值、普通/加色混合、UV 滚动、停止和标签中断。`Player` 不持有纹理。
- `AssetManager::LoadBattleAtlas()`：按需读取并缓存一套 DDS/ANM，多种颜色和多个实例复用同一张纹理；在有效图形上下文中调用。CPU 档案共享只读，GPU 句柄仅由管理器释放。
- `render/animated_sprite.*`：当前帧的 2D 绘制、相机朝向的 3D 公告板以及激光带。绘制不推进动画；运动方向由调用方提供给 autoRotate。

ANM 坐标采用自身逻辑像素，与 DDS 实际分辨率分别处理，UV 由逻辑尺寸计算。角度是弧度，时间采用固定 60 Hz；`Advance(seconds)` 接收非负有限秒数并保留不足一帧的余量。原脚本编号、精灵编号、颜色偏移和关卡 `VisualId` 属于不同编号空间。

每个演员/弹幕拥有自己的 `battle::Cursor`。弹幕固定池在系统构造时一次性分配到堆，防止新增动画状态占满 Windows 默认线程栈；运行期生成不新增槽位内存。`Sync(archive, selection, age, revision)` 的 age 是本次动作开始后的秒数；选择改变、年龄回退或 revision 改变时重启，同龄重复同步不会推进。暂停时不推进年龄。`position` 与 `positionOffset` 是独立的原像素通道，绘制器将它们作为外部锚点的局部偏移；3D 通过 `height/source.height` 换算世界单位。

`LoadBattleAtlas` 返回借用引用，在 `LoadStage`、`UnloadStage` 或管理器销毁后不得再使用。动画实例借用档案，切换关卡时必须先清空演员、弹幕和效果。`DrawAnimatedBillboard` / `DrawAnimatedBeam` 关闭背面剔除，调用方应在整批提交后恢复；`SpriteRenderer` 已统一处理透明排序、不写深度和状态恢复。

## 使用尚未出场的素材

下面的实例应保留到下一帧，管理器与图形上下文须覆盖它的使用寿命；暂停时跳过 `Advance`。

```cpp
// 加载魔理沙的原待机动画；不会开启或改变玩法。
const auto& atlas = assets.LoadBattleAtlas(battle::Atlas::Marisa);
anm::Player animation;
animation.Start(*atlas.archive, battle::Player(battle::Atlas::Marisa, 0).script);

// 每帧更新后，在 BeginDrawing/EndDrawing 内按原像素比例绘制。
animation.Advance(deltaTime);
DrawAnimatedSprite2D(atlas.texture, *atlas.archive, animation.Sample(), {320, 240});
```

新关卡也可以在 `VisualDefinition` 填写 `.animation={Atlas, script, spriteOffset, initialSprite}`，由清单加载后使用统一 `SpriteRenderer`。`initialSprite=-1` 表示由脚本选精灵；部分特效由外部选择初始精灵，应显式传入。未知精灵/脚本及损坏文件抛异常。

`EnemyBullet(type, color)` 接收类型 0–9、颜色 0–15。大玉/刀弹为八色、火球一色、大泡五色，颜色循环到对应有效色。`BulletSpawn` 接收 10/16/32 帧显现时长。`Item(0…6)` 包含 P、点、大 P、Bomb、满火力、残机与星点，后三类未使用道具也可直接绑定。

## 当前一面接入

一面清单将妖精主动画绑定 stg1enm，战斗露米娅绑定 stg1enm2 的 128 起脚本，世界角色与对话立绘使用不同编号和纹理。ECL 97/98 驱动待机、左右移动和收势并重置对应动作时钟；ECL 100 保留小爆散、旋转爆散、大爆散选择。实际敌弹保留类型和颜色，显现动画结束后切换主体；激光使用原纹理，P/点/星点使用原道具精灵，大 P 单独绑定。

击破通过清单 `defeatVisual` 或演员覆盖值选择独立 ANM，删除指令结束效果，错误的长循环绑定最多保留 1 秒；自然离场和关卡清场保持原演员最后一帧的淡出快照。原始角色和弹幕以连续 Alpha 绘制，按距离合并排序。第一人称视角没有镜头前的玩家身体，自机图集的身体动画提供给未来玩法及预览接口；当前使用其中的射击动画。三维射击、碰撞、拾取、Bomb 规则及演员入场/退场淡出仍沿用项目设计，ANM 负责图像与局部动画，不代替完整原作场景或所有外部粒子运动回调。

## 检查与审阅

```bash
cmake --build --preset macos-debug --target koumakyou koumakyou_tests native_asset_preview koumakyou_lifecycle_tests
ctest --preset macos-debug
./build/macos-debug/native_asset_preview output/stage1-review/native-battle-gallery
./build/macos-debug/koumakyou_lifecycle_tests output/stage1-review/native-battle-integrated/game
(cd assets/runtime/battle/native && shasum -a 256 -c checksums.sha256)
```

后两个可执行文件需要桌面图形会话。预览工具调用正式加载/播放/绘制接口，将所有脚本按第 0、12、32 帧导出，背景脚本的绝对位置居中以便审阅，并检查露米娅身体区域确实有渲染像素。核心测试读取全部 25 套素材、运行所有有效脚本并触发中断，核对真实角色帧序、颜色映射和显现时序。图形生命周期测试覆盖整关、两张符卡练习、暂停、清场及资源释放。审阅截图位于忽略目录，不入库。

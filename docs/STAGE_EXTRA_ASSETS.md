# Extra正式素材接入准备

素材来自与本关ECL/EXE一致的新典本机数据；运行时文件不包含原始安装路径，也不从安装目录加载。生产manifest、第三音乐流和12张静态MSG对白的实际播放已通过验收；条件放行与结算双边界已接入353条事件。背景三维适配及图形回归已完成，正式入口已使用独立Extra身份注册；入口与成绩隔离的最终验收见`STAGE_EXTRA_REFERENCE.md`，原生动态rank仍须继续核对。

## 立绘与背景

原始参考从th06ST.dat的`face12a/b/c.anm`及各自`_4k.dds`按ANM逻辑裁图导出，尺寸为1400×2020。当前三张运行时立绘经内置 `image_gen` 重绘为1024×1536透明RGBA，保留各自动作、神态、服饰与角色装饰，重新塑造面部及双手，统一项目现代立绘画风；闭眼差分保留汗滴、战损和绷带。完整提示词、原图身份和成品校验见 `assets/source/portraits/flandre/`。加载器继续沿用`FindDialogueContent`的Alpha>16取景规则，不按纹理分辨率倍增世界尺寸；原始导出时的边缘验证记录与当前重绘成品的验证应分别理解。

重绘后三张已重新通过 `--extra-assets-only` 图形专项：同步/异步加载、1100×760、1280×720、390×844取景锚点、共享纹理和卸载所有权检查。横屏睁眼及竖屏闭眼截图已目视核验，透明区域无图集底色。

背景直接保留`stg7bg_4k.dds`及`stg7bg.anm`为background.dds/anm，未改变字节。源stage7.std仅留参考侧，SHA-256为`f4c418656897cb29f67c4792e523912bd5e7910f9e2de526d042f0017775d617`；生产不能读取STD并据其生成编排，三维场景须另行明确适配。

## 三个源音乐槽

stage7.std的656/784/912字节处分别绑定`th06_14.opus`、`th06_15.opus`、`th06_16.opus`。官方中文标签依次为“魔法少女们的百年祭”“U.N.Owen就是她吗？”“绯红恶魔姗姗而至”。MSG分支0/10等切至槽1，3/13在223帧切至槽2，5/15在0帧切至槽2；不能假定本关只有两首音乐。

使用统一 `tools/import_bgm.py` 将原始 Opus 解码为 `stage_theme/boss_theme/event_theme.flac`，并从同包 `.pos` 导入各自循环区间；ffmpeg 解码后的 PCM 与 FLAC 回解 PCM 逐曲哈希一致。完整时长仍分别为166.8575、174.7375、117.0575秒，实际循环按旁路 `.loop` 的原作区间执行，前奏只播放一次。来源、单位和 EOF 策略见 [BGM 原作循环](BGM_LOOP_REFERENCE.md)。第三槽通过 `AssetManager::PlayStageTrack(2)` 播放，event_theme 仅表明独立事件音乐资源，不预设它属于何种结算模式。

## 运行素材校验

初次接入时八个文件与临时转码/导出产物的SHA-256一致。当前三张立绘已替换为现代重绘；下表同步列出当前运行时文件的大小与哈希，不能替代正式manifest加载和整关视觉/音频验收。

| 路径 | 字节 | SHA-256 |
|---|---:|---|
| `stages/stage_extra/background.anm` | 352 | `23ba803629eb7e673574280707745cb15e156d8ab766063f315eca722779abb7` |
| `stages/stage_extra/background.dds` | 1048724 | `a3663c629e9605bf535edc2bd0bb27895b233e0517712e5463d98dee9ec01025` |
| `stages/stage_extra/boss_theme.flac` | 22686215 | `740cc06045db4d2b12172a31372a74fe6aa88892af7299b461af40bea7413a1e` |
| `stages/stage_extra/event_theme.flac` | 15491504 | `571ae41e0240f4f27430069de5ee580aa481aa10fa93d4d55ac8fe8e4530a12f` |
| `stages/stage_extra/portraits/flandre/face12a_0.png` | 2031809 | `10ae92fd9e5d846919a62e6b0f150ca0d4e883ff7de6ea930c14a12970ae8325` |
| `stages/stage_extra/portraits/flandre/face12b_0.png` | 2067919 | `c111e0140e6635932e547ea20b367357e1449b770873177068738a503dca94cb` |
| `stages/stage_extra/portraits/flandre/face12c_0.png` | 2088680 | `b11a6da1430ee7c383f3dcaf1dca053bef873d421e3fbeedc705bf470e741bc0` |
| `stages/stage_extra/stage_theme.flac` | 21602260 | `5426089eacade5755a84f5eb2959abf65f93b8737edcd6d9a7905c2f97602bc4` |

## 提取及转码输入身份

| 输入文件 | SHA-256 |
|---|---|
| `stg7bg_4k.dds` | `a3663c629e9605bf535edc2bd0bb27895b233e0517712e5463d98dee9ec01025` |
| `stg7bg.anm` | `23ba803629eb7e673574280707745cb15e156d8ab766063f315eca722779abb7` |
| `face12a_4k.dds` | `ec9a4b96ae7610759108aeaefc24f9d18a0ecf4c600ef4004e87455ef9cfefb2` |
| `face12a.anm` | `5d0f68dd4071bae7ea284c821c64c48b550d1238c43f1f197b42303b5af0639b` |
| `face12b_4k.dds` | `77da0a6f0690d44ffd0eeee42e58ae1e0fc1ba43b4a094e0707f8f6d0c00067c` |
| `face12b.anm` | `3d5bc8cd5465b9e62c8cc947b7ff8710274f140487d79ab1713a6d94add91c5a` |
| `face12c_4k.dds` | `dcb4704eae9dfd73f333ed3fc5c0b4754ef9c9308f73d03b471abd7519aec8fd` |
| `face12c.anm` | `f88d47c25580b352d838206043306440efea8dfadf00b310c531965f1852781a` |
| `th06_14.opus` | `273dd53a7f56c4693b7b3f7b8bce4039ba9bde4e2d238a5cb993f3294c9ece42` |
| `th06_15.opus` | `75aa1e465eee30ea68aef6b1c4247b8143bc85ab825b27898050bf2aa108cfbd` |
| `th06_16.opus` | `2ecfd83f037143b9eb99afda8bf4add0102e8a2eae40e29aa7a548d2d1520fbd` |

## 正式清单与音乐生命周期

`stage_extra/manifest.*`提供静态StageExtraManifest，CommonBattleVisuals只共用定义与路径，不跨关卡借用已加载纹理。普通演员0/3、发射体/辅助光圈13与道中帕秋莉65均绑定ExtraEnemies；芙兰朵露160绑定Flandre。kFlandre沿用kBoss，kPatchouli及其四立绘/背景追加在公共槽之后，清单稠密无重复；道中工厂已切到kPatchouli，不能再用kBoss同时代表两名角色。Flandre表情按原生稀疏编号绑定0=c、2=a、3=b，未使用的1号公共预留槽复用c，按路径共享同一GPU纹理所有权；帕秋莉沿用已核对face08文件路径，但由本关资源管理器独立加载/释放。

StageAssetManifest新增可选eventBgm，旧清单默认空值。AssetManager独占Event播放器并与Stage/Boss流一起加载、音量调整、更新、暂停和卸载；PlayStageTrack接受0/1/2，-1停止，其他索引拒绝且保留正在播放的曲目。同槽重复选择不重启；空槽请求停止旧曲，不沿用上一关流。旧PlayStageMusic/PlayBossMusic接口保留。

`--extra-assets-only`已在真实图形/音频上下文（强制静音）通过同步及异步加载、异步取消、三个独立 PCM 播放器/真实播放/重复选择/暂停恢复/停止、卸载清空、旧关卡空第三槽隔离。原生背景1024×1024纹理、两个独立Boss档案、全部Flandre姿态有效；原始三张1400×2020立绘在1100×760、1280×720、390×844下均无超过2像素的侧边/底边锚点空隙，0/1槽正确共享c纹理。弱引用随卸载释放，不重复销毁共享纹理。

这些验证确认资源和音乐播放接口可用，未把背景纹理当作已实现的三维关卡，也未把合成立绘边缘测试视作真实MSG对话验收。正式清单下四机体×击破/超时的8条完整道中，以及首机体主战20技能/10符与后继5技能/3符的两条连续遭遇均通过；峰值与旧隔离清单一致。GPU透明遮挡、旧Bomb音频与公共资源生命周期回归也已通过。

声效仍沿用项目既有合成公共音效，不因本次音乐接入而宣称已还原原生SE。清单当前没有reference_5和reference_22，角色行为虽报告这些源音效号，播放端会忽略未配置项；正式Extra音效映射须先核对EXE索引/原包，不能随意将其指向相似声音。

## MSG立绘编号修正与演出

原生RVA0x3f38f/0x3f3ae把face12a/b加载在精灵0x4c8/0x4c9；MSG2于0x40964以对手基址0x4c6加表达式选图。0x40769排除道中MSG2/12，其余Extra对白在0x4078f把face12c加载到0x4c6。故真实表情为0=c、2=a、3=b；此前按文件顺序0=a、1=b、2=c的准备清单映射已纠正。独立EXE锚点、分派、文件名字节及篡改拒绝纳入工具，29项Python及真实EXE核验通过。

对白立绘中断2不是无操作：源face00a.anm（SHA-256 `8692054912325c3e5c4eed529beac1aae8d7f17849d768b6cb4922bc1a66faa2`）的脚本0/2在标签2以19指令执行30帧缓出到场外，随后21暂停而不删除。公共DialogueMotion已明确支持此退场，Extra条件前奏943…973帧的中途/终点另有断言；标签5仍对应源动画最终结束。

## 背景三维适配

只读展开同源`stage7.std`得到4类对象、49张面片与60个有效实例；对象0由8×4张64像素平面构成，雾片对象1为192×192，两组侧边卷纹分别为64×40与64×60。12组实例沿源纵向每256像素重复，并以三层深度排列装饰。源相机/雾颜色脚本仍只留参考侧，不在运行时解释或生成事件。

`stg7bg.anm`的256×256逻辑图集中，地砖裁图为`(17,17,32,32)`、白雾为`(96,1,64,62)`、卷纹为`(168,1,64,47)`；第三、四脚本使用同一卷纹精灵并向相反方向旋转。现用已有DDS解码器离线导出三张精灵并人工检查，确认其青绿色砖纹、白雾与透明卷纹形态。生产直接按归一化UV借用正式1024×1024纹理，不增加转换PNG或将高清尺寸当世界比例。

`stage_extra/scenery.*`以世界单位重新安排地面、侧墙与雾层：可玩体积X/Y为±10、Z为±11.5，地面Y=-10.8，卷纹X=±16，雾层X=±24。以冻结暂停后的秒数控制0.7世界单位/秒的纵向滚动，不复制原相机轨迹或原逐帧旋转。半透明装饰从远到近提交并关闭深度写入，结束恢复深度写入/背面剔除，避免透明图集边缘挡住后绘制的演员与弹体。

StageContent的DrawScenery在调用期间借用const AssetManager；一至六面的纯几何场景仍沿用原实现，Extra仅查询自己的kBackground。内容不缓存纹理、不加载或释放素材，空纹理安全跳过。完整依赖构建通过。图形专项覆盖同一时刻逐像素重绘一致、实际滚动、暂停、四个观察位置（含反向观察）、竖屏和卸载后空资源绘制；已检查实际截图。背景版八条条件后继路径及GPU透明混合/遮挡回归均通过，追加战峰值仍为328弹，结算前360帧锁定与一次性加分保持。最终背景版两角色完整时间轴复验通过，分别543.317/463.033逻辑秒、峰值2765弹，零池丢弃；最终GPU及演员资源生命周期专项通过。背景版全量CTest 4/4通过（480.75秒）；期间最后只补齐反向远景，随后完整构建和四视角/时间轴/GPU/生命周期复验均通过。

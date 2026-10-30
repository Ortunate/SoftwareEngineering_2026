# Replay v2 协议与扩展接口

整数使用小端 u32；64 位分数编码为低、高两个 u32。浮点为 IEEE754 float32 原位编码。字符串编码为 u32 字节数及 UTF-8 字节，最长 512 字节。文件末尾 FNV-1a 校验覆盖前述全部字节，仅用于损坏检查。

字段顺序：magic `0x3152504b`、format=2、构建字符串；名称、路线、角色、御札、规则、模拟版本、内容版本、构建信息八个字符串；续关次数；片段数及片段；扩展项数及扩展；开局适配字段；步数；终局摘要；逐步输入；校验和。扩展项为 type/length/payload，高位为必需标志，未知必需项拒绝，未知可选项按长度跳过。未知更高格式拒绝，不尝试猜测。v1 文件提示使用旧构建，不覆盖或迁移原文件。

片段顺序：stage/entry/target 三个稳定字符串，difficulty、begin、end、seed、lives、bombs、power、rank，入口累计 score，出口 Result。Result 为 score、lives、power、misses、bombs、won；bombs 是本段使用次数。入口 bombs 为当前持有数量。begin/end 使用全局输入数组的半开区间，片段必须连续、非空并覆盖输入。最多 64 片段、2,000,000 步；存档总长度有独立上限，先检查长度再分配。

开局适配字段沿用首版的 seed/shot/lives/bombs/phase、practice/spell/challenge 位集合，仅用于当前运行时适配；未来内容必须以片段入口快照、target 稳定 ID 和目录解析器为依据，不把 phase 当作跨关卡身份。输入为 dt、mouseX、mouseY、sensitivity 四个 float32 和 u32 操作位（移动六项、focus、fire、bomb、confirm、fast）。dt 单位秒且在 (0,0.05]。全局录像时间为输入 dt 之和，不包含加载、暂停、命名和观战镜头时间。

难度稳定值：Easy=0，Normal=1，Hard=2，Lunatic=3，Extra=4。主线路线使用前四项；`extra` 路线使用 Extra。`stage1`…`stage6`、`extra` 为预留关卡键；路线目录决定实际顺序和终点，禁止使用 StageId 枚举序号或关卡号加一。Extra 的追加阶段可作为本段 target/事件，或路线目录显式登记的后续片段，不能冒充主线第七面。

`BeginSession` 清空会话并固定机体/规则；`BeginStage` 记录入口快照；`EndStage` 记录区间和出口摘要；`FinishSession` 固定最终结果。关卡加载失败不创建片段，加载耗时不计入输入；只有终局询问确认后调用 Save。续关次数非零时 Save 拒绝。`CanPlay` 是当前运行时的内容解析边界，当前仅允许 Normal 一面及其练习入口；未来登记更多内容时必须同时接通目录解析、入口恢复和段间切换，未知内容绝不降级为一面。

模拟版本与内容版本用于播放资格，构建版本仅展示。当前仍保守绑定编译器和平台，不承诺跨平台浮点一致；模拟签名覆盖玩法、关卡、脚本及总控，内容签名覆盖运行时素材。观察镜头/控件单独文件不影响模拟签名。新模拟行为须调整兼容签名，格式变化须升级格式版本。

录像名称最多 32 个 Unicode 标量，去除首尾 ASCII 空格，拒绝空串、控制字符和非法 UTF-8。名称不作为路径；重名允许，文件名由时间及唯一后缀生成。文件先写临时文件，关闭成功后发布。失败保留待保存数据，不能把失败当作已经保存。

可选扩展 type=1、length=8：开局时刻 recordedAt，按低/高两个 u32 编码为 UTC Unix 秒，合法非零范围至 253402300799（9999 年末）。每文件至多一项；省略时内存值为 0，展示“未记录”。不从文件名或文件修改时间推测。此扩展不改变 v2 核心字段布局，旧读取器可按长度跳过。新局同时冻结 simulation/content/build 版本信息；展示根据文件中的 stage/difficulty/character/shot/route/rules 等稳定 ID 解析，未知 ID 保留原文，不能套用当前菜单或默认一面。

当前一面运行适配中，入口 lives 沿用原始 Player 设置的额外残机数，不含正在使用的一条；详情页 Player 展示该原始配置值。出口 Result.lives 则是终局剩余总残机数，二者不能混用。

与开发分支弹道辅助接入后，输入操作位 bit 11 保存 player.aimAssistEnabled。录制读取本机设置，播放只使用录像位，不受观看者的辅助设置影响。未设置该位的旧输入视为关闭；模拟签名仍决定是否允许实际播放。

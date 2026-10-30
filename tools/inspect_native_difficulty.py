#!/usr/bin/env python3
# 本文件只读核验指定新典 EXE 的难度锚点、rank表、Boss组合/扩展指令及结算标签，不生成运行时数据。
import argparse
import hashlib
import json
from pathlib import Path
import struct

from export_native_combat_data import PeImage

EXPECTED_SHA256 = '48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82'
# 锚点只证明相关指令与本次分析一致，不能单独推导未追踪的入口、边界和模式条件。
ANCHORS = {
    # 光束创建时清局部计数；展开结束同一步转持续0帧，先按counter%12传擦弹开关，再递增时钟。
    0x10f21: 'c7430c00000000c7430819fcffff8b47188943108b471c8943148b47408983880200008b47108943048b470889836c020000488bc38b573085d28993700200008b4f34898b680200008b4f38894b188b0f898b8c0200008b4f04890b0f94c1888b90020000',
    0x1104d: '4533ed',
    0x11da3: '0fb6937c020000',
    0x11e18: '85d20f84c100000083ea010f845201000083fa010f84b8010000',
    0x11e44: '8b43f8488d530c488b0dde8daa008943f4ffc08943f8e8c151ffff',
    0x11ee8: '448b835c020000448b4bf8742c',
    0x11f5f: '453bc80f8ccafeffff44896bf8fec2c743f419fcffff88937c020000b8abaaaa2a4c8d8b64020000f76bf84c8d442430d1fa8bc2c1e81f03d08d0452c1e002488d5424403943f80f94c088442428f30f11542420e8f8a205008b83540200003943f80f8c6bfefffffe837c02000044896bf8c743f419fcffff',
    0x6c3f1: '0f2ff80f86df000000803d87544e00000f852e020000',
    0x6c4d9: '80bc24f8000000000f844e010000',
    # 普通弹/光束的擦弹事件：两个计数各自饱和，随后独立加500分；本事件块不修改rank。
    # 这里只固定已触发擦弹后的结算，不扩大到相交几何、全局计时或效果池回调的完整语义。
    0x118dc: '44382dd5255400752a8b05318453003d9f8601007d08ffc08905228453008b05208453003d3f420f007d08ffc0890511845300f30f100511fe53004c8d442430f30f584338f30f585330f30f585b34458bccc7442420ffffffffba08000000f3410f59c7f3410f59d7f3410f59dff30f11442438f30f11542430f30f115c2434e81f9601008b05d194aa0048810576ba5200f401000083e0bf0fbae80744382d642954008905b294aa00757a418bcd498bc566908b54871085d2786283fa1e7465ffc148ffc083f9037ce94488a318060000e960010000',
    0x119fa: 'c74487101e0000004488a318060000e905010000',
    0x6c549: '803d68794e0000752a8b05c4d74d003d9f8601007d08ffc08905b5d74d008b05b3d74d003d3f420f007d08ffc08905a4d74d00f30f1005a4514e00f3450f58e4f30f58c0c7442420fffffffff3450f58ed41b9010000004c8d442430ba08000000f3450f59e2f3410f59c2f3450f59eaf30f11442438f3440f11642430f3440f116c2434e8aee9fbff8b0560e8a400488105050e4d00f401000083e0bf0fbae807803df37c4e0000890541e8a400753533c0488d0dbe7c4e008b1185d2781783fa1e7421ffc04883c10483f8037ceab802000000eb18488d158a7c4e00c74482101e000000b802000000eb0233c0',
    # 结果页以53d414的真假选择Standard/Challenge指针；配置和回放入口分别写非零及header[6]==1。
    0x75137: '0fb605d6824c004c8d0449ffc149c1e005890dd2bc3b00f6d8488d05a9aef8ff481bc983e108488b940150ed3400',
    0x4f332: '0fb60da2ecc10085c98805ffa94f000fb6058decc1000f9505c5e04e00',
    0x6cfc4: '488b4370807806010f940541044d00',
    # Miss倒计时归零增加53cab0；item5在Challenge减同一有符号计数，在Standard只增加残机库存。
    # 回放用movswl恢复该计数，负值合法，不能以剩余残机或无符号数替代战后门槛。
    0x6a20d: '8b873c77000085c00f842702000083e80189873c7700000f85c5000000ff0580284d00',
    0x4433d: '33ed',
    0x44600: '490fbe463483f8060f87230a00008b8c86685104004803ceffe1',
    0x44cb6: '40382d57874f007408ff0deb7d4f00eb21803d72505000087d188b056261a700fe056450500083e0fe83c80289055061a700',
    0x6d01f: '0fbf4212890587fa4c00',
    # timeline9等待MSG结束/放行，12等待Boss槽真正失活；10只写回保存的战斗子程序，不表示遭遇结束。
    0x38748: '488b05d926a80039b0503b0000775839b0b03600007c508d42ff8987bcc01000eb7d',
    0x3876a: '418b4208488b8cc7e8c01000418b420c8981700f0000',
    0x38790: '490fbf4202488b8cc7e8c010004885c9740d4038b1bc0000007cb4',
    # 出生item低字节保存于+10a8；非负编号在击破时原样符号扩展交给统一道具创建器。
    # Extra尾波的item5不能套旧0…2校验后丢弃；固定掉落不经过-1的三次轮换分支。
    0x37b22: '0fb64424608883a8100000',
    0x395e7: '0fb683a810000084c0783e',
    0x39613: '440fbe83a8100000488d93b0000000440fb6cfe875a90000',
    # MSG2按对手基址0x4c6加表达式选图；Extra预载a/b于0x4c8/0x4c9，非道中MSG将c覆写到0x4c6。
    # 因而真实Flandre表情为0=c、2=a、3=b，不能按三个文件顺序直接映射0/1/2。
    0x40964: '490fbf4e0441b8c6040000410fbf46066685c9450f44c1488d14c94403c0',
    0x3f38f: '41b9c80400004c8d05ec193100ba13000000e89a30fcff',
    0x3f3ae: '41b9c90400004c8d05e5193100ba14000000e87b30fcff',
    0x40769: '83f807753883fe02743383fe0c742e',
    0x4078f: '41b9c60400004c8d05d4063100ba12000000',
    0x350d88: b'data/face12a.anm\0'.hex(),
    0x350da0: b'data/face12b.anm\0'.hex(),
    0x350e70: b'data/face12c.anm\0'.hex(),
    # 65启用+be的0x01边界位，66关闭；与清敌循环读取的+bd字段独立。
    0x272e0: '8b06894728418b461089472c418b4614894730418b4618894734808fbe00000001e927e9ffff80a7be000000fee91be9ffff',
    # ECL返回后主循环才处理模式3的HP1与死亡回调安装；下一次ECL返回-1才清活动位。
    0x39884: '486393e00b000085d27861',
    0x398cd: '48898b8800000044897344c7434019fcffff66899380000000c783e00b0000ffffffff',
    0x390ce: 'e83d8efeff83f8ff757480a3bc0000007f',
    # 124按当前演员位置调用道具池创建器，第三参数为原生道具号、第四参数为0；不进入MSG。
    0x28367: '440fb706488d97b00000004533c9e826bc0100',
    0x43ffd: '81f900040000',
    0x4401a: 'ffc03d000400007ccd',
    0x441bf: 'b8150200004103c066894138490fbfc04d8b84c138410200',
    # 6在bound为零时跳过随机读取；bound为1仍拼接随机32位再做无符号取余。
    0x228f4: '458b084585c9745a83055d85a90002',
    0x22944: '440bc166890d1685a900418bc041f7f1eb03418bd489542440',
    # 82的加速度角<=-999时沿实际发射角(xmm9)，否则使用显式角；时长非正才回退99999。
    0x10ac0: 'f3440f104744f30f10053a233400410f2fc0f30f107f407212410f28c1e80ef52d000f28f0410f28c1eb10410f28c0e8fcf42d000f28f0410f28c0f30f59f7e8fceb2d00f30f59c7b99f860100f30f118308060000f30f11b30c0600008b471c85c00f4fc8894b48',
    # 条件后继：17直接尾调10；19保留化蝠逻辑，但恢复为176，不能复用10的160恢复脚本。
    0x37340: 'e92bf6ffff',
    0x373f6: '83b93402000000488bd90f8e3f010000e875e3ffff',
    0x374e0: '66c74138b0014d8b81103e0200',
    0x37535: '66c7836c0f0000b000808bbd00000001',
    # 18按阶段总钟7200前的HP写f0/i1/i2/i3，所有整数商均向零截断；不是16的环弹参数。
    0x37363: '817904201c0000440f4c813402000066410f6ec80f5bc9f30f590d32b63100f30f5e0de6b93100f30f5cc1f30f114158',
    0x37393: '416bc819f7e9b8f1197605c1fa078bca83c216c1e91f03ca4189494c4169c85e010000f7e9b8f1197605c1fa078bca81c272010000c1e91f03ca418949504169c8fa000000f7e9c1fa078bc2c1e81f03d0b8900100002bc241894154c3',
    # -10100每次读取更新随机流，写+6c临时槽；-10101仅引用+68，不能把两者当同一寄存器。
    0x2fdbc: '83059db0a80002',
    0x2fdfb: '4d8d416c',
    0x2fe24: 'f30f59057c2b3200f30f5905442d3200f30f5c05e82c3200f3410f1100',
    0x2fe43: '4d8d4168',
    # -10021按自机减当前演员求atan2；两点重合时返回pi/2，不读取随机数。
    0x2fc87: 'f30f1005951a52000f57d2f3410f5c81b4000000f30f100d7d1a5200f3410f5c89b00000000f2ec27a2875260f2eca7a21751f',
    0x2fcba: 'f30f1005ee2d32004c8d05efb0a800f30f1105e7b0a800498bc04883c438c3',
    0x2fcd9: 'e8c2f22b004c8d05d3b0a800',
    # 95在执行子演员前按值复制父执行上下文的16个变量槽，不共享其后续写入。
    0x37a8a: '8b41088943488b410c89434c8b41108943508b41148943548b41188943588b411c89435c8b41208943608b41248943648b41288943688b412c89436c8b41308943708b41348943748b41388943788b413c89437c',
    # 54选加速二次模式；58选缓出四次模式。极坐标插值的总位移始终为速度×时长/2。
    0x2719d: '498bd6488bcfe83887000080a7bc000000ef808fbc0000000c',
    0x2720e: '498bd6488bcfe8a787000080a7bc000000eb808fbc00000008',
    0x28777: '0f28c2f30f59c2f30f59c2f30f59c20f28d3f30f5cd0',
    0x2876a: 'f30f5cda0f28d3f30f59d2',
    0x2f90a: 'f30f594314660f6e4b0c0f5bc9f30f59c1f30f590521313200f30f118744020000',
    # Extra战后分支：加载器只记住首个非负帧opcode13；timeline运行分派仅接受0…12。
    # MSG14提前终止当前对白并请求跳转；无13标记时不改时间轴，仍可顺序进入后续演出。
    0x3cd02: '4c89359fe0a70066443b317f1f90668379040d7410480fbf41064803c866443b317eebeb0748890d7ae0a700',
    0x37ce2: '803dc730a800007423488b05b630a8004885c0741748898128c110000fbf008981bcc01000c605a230a80000',
    0x37db2: '490fbf420483f80c0f87ef090000418b8c87348803004903cfffe1',
    0x41308: '833dadb74f00060f8cbbf8ffff803df8c04f0000740d833d8bb74f00020f8fa5f8ffff',
    0x41339: 'c605709aa70001488bcb89b758090000c687150700000166448967344489a7b0360000',
    # 成功取得源编号121…133时增加独立计数，源130成功直接置10；该路径排除53d405模式。
    0x27c4b: '803db3575100007522418d408783f80c7719ff05594e51004181f882000000750ac705464e51000a000000',
    # MSG9进入结算演出后继续脚本；11切换游戏状态且不推进PC，10仅走更新返回，0才正常结束。
    0x408b1: '83f80e0f87160300008b8c82281504004803caffe1',
    0x40bd0: '488b8f683600000fb641034c8d71044c03f04c89b768360000',
    0x40d72: 'ff87503b0000e953feffff',
    0x4118f: '448825ac8b5000',
    0x41196: '803d3fc24f00000f852dfaffff',
    # MSG9把+958置1，独立更新仅在等于1时固定奖励/加分，然后递增；并不等到MSG11才加分。
    # 结算先计算stage/power/graze/point及末关库存，再按difficulty=4乘2并截断10，最后应用初始残机倍率。
    0x40880: '41b801000000',
    0x4110d: '44898758090000833db9b94f00067d72',
    0x41a80: '4c8b4b384183b958090000010f85fe000000',
    0x41a92: '448b053bb04f000fb70538b04f00438d0c808d0c488b056f8250008d14898d0c500fb70550b94f000fafc88d148903d24183f8067c1b0fbe05718250008d0c400fbe056882500003c869c140420f0003d08b0d27b94f0085c9743f83e902742883e901741483f90175468d0c12b867666666f7e9c1fa02eb2b6bca0fb81f85eb51f7e9c1fa05eb1c8d0c52b81f85eb51c1e102f7e9c1fa05eb0ab867666666f7eac1fa038bc2c1e81f03d08d149203d20fb60db7c4c20083e903740a83f901751e8d0c12eb038d0c92b81f85eb51f7e9c1fa058bc2c1e81f03d08d149203d2',
    0x41b71: '4189915c0900004863c248010566b84f00488b4338ff8058090000',
    0x41431: '803dccbf4f0000488b05a9bf4f004889059abf4f00752f',
    # 未到本篇终点的MSG11分支写状态3；初始化器据此保留已有rank，而不是再次查五槽表。
    0x41448: '8b0d86b64f0083f9057c588b15b7bf4f0085d2740583f9057449',
    0x414ab: 'c70507cbc20003000000e953f7ffff',
    0x41477: '48890562bf4f00c70534cbc20007000000e980f7ffff',
    0x414ba: '4489a7b0360000418bc4eb2c',
    # 主循环先执行ECL(21f10)，再做HP死亡检查；死亡模式3回写HP1，模式0当帧清活动位。
    # 因此最终95在60帧写HP0后即结束，不能等到下一个内容帧或源表不可达的3010帧删除。
    0x390c4: '488bd3488d0dc21ca800e83d8efeff83f8ff',
    0x394a6: '83bb34020000000f8f3d0400000fb693bd000000f6c201',
    0x394ff: '80e20f898b340200008893bd000000',
    0x3959a: '80a3bc0000007f',
    # Catadioptric的2048使用当前主体精灵半尺寸判完整离场；X两侧/上缘反射，下缘只计数，先反弹后位移。
    0x10b93: 'b8000c00006685c8741cf30f104740410f2fc57304410f28c2f30f11038b471c894320896b1c',
    0x11028: 'f3440f103d131a3400',
    0x1107a: 'f3440f1005851c340041b9dfff0000f3440f100d821c3400450f57e4',
    0x1162a: '660fbae10b0f8327010000488b8350010000f30f105b30f30f105334',
    0x11649: 'f30f104820f3410f59cff30f58c1440f2fe077320f28c3f30f5cc1410f2fc07725f30f10481c0f28c2f3410f59cff30f58c1440f2fe0770ef30f5cd1410f2fd10f86cd000000',
    0x1168f: '440f2fe37706410f2fd8725ef30f104340418bcd0f57c6f3410f5cc2f3410f58c4',
    0x116f9: '440f2f6334760df30f1043400f57c6f30f114340',
    0x1170d: 'f30f107340f30f103b0f28c6f30f117b24e8dddf2d00f30f59c7f30f1143080f28c6e8bce82d00f30f59c7f30f11430cff431c8b431c3b43207c09b9fff7000066214b14',
    # 彩雨弹模板初始化后立即执行ANM第0帧；显现结束先清运动时钟，再落入运动分支，同步保留末帧显现位移。
    0x127fc: '89abe8000000c783e400000019fcffff4d85c07412488bd1498bc9e80448ffff4c',
    0x12817: 'e80448ffff',
    0x7ac9: '418b83b8000000418983b4000000ffc0418983b800000033c0',
    0x111b4: 'e8675effff85c00f84d10a0000f30f100d',
    0x111cf: '664489634441b9dfff000044896b2cc7432819fcffff0fb74b146685c90f845f0500004184cc',
    0x111f7: '8b432c83f8107f46',
    0x11245: '664133cc66894b14e903010000',
    0x1175c: 'f30f105330f30f585308',
    0x34c90: '0fb6053a7e50004c8d0db2423f000fb6152b7e50004c8bc1488d1450488d0452',
    # 宣告与取得分别拒绝零起算编号>=134；有效记录步长0x180，基址0x53d418。
    # 这只证明战斗记录容量，不等同练习菜单的131张目录。
    0x27929: '8b055928bd003d860000000f83eee2ffff488d0c4048c1e107488db618d453004803f1',
    0x27b51: '4181f8860000000f83e70000004b8d0c4048c1e1074c8d159384fdff4d8d8a18d453004c03c9',
    0x27c25: '418b8489300100003dffc99a3b7311ffc04189848930010000',
    # manager基址+0xd41c是rank；新局非状态3读取难度表，回放恢复入口再次读取。
    # 这里只证明两个具体写入和五槽表，不能据缺少直接交叉引用断言整个运行期间恒定。
    0x3c0c8: '488d3de1095000',
    0x3c167: '488b0d3a095000493bc07507e8d8000000',
    0x3c27c: '833d391dc30003488d35763dfcff488bf941bd0800000041ba040000000f84ee040000',
    0x3c752: '486305b70c5000488d35a038fcff',
    0x3c77e: '8b8486e0e0340089871cd40000',
    0x3c9dd: '4863052c0a50008b8486e0e0340089871cd40000',
    0x2676e: '4489a7a4100000c787a010000019fcffff',
    0x28947: '8b87a41000008987a0100000ff87a4100000',
    0x41ab3: '0fb70550b94f000fafc88d148903d2',
    0x41ac2: '4183f8067c1b0fbe05718250008d0c40',
    0x41ae3: '8b0d27b94f0085c9743f83e9027428',
    0x41b42: '0fb60db7c4c20083e903740a83f901751e',
    0x41448: '8b0d86b64f0083f9057c588b15b7bf4f0085d2740583f9057449',
    # 六面9/11都请求音效12；11在入场及逐弹路径分别更新原生随机流，不能把它当作9的别名。
    0x36535: 'ba0c000000',
    0x36605: 'c743e00ad7233c44897be8c743e419fcffffc7430478000000',
    0x36730: '83052947a80002',
    0x367a5: 'ba0c000000',
    0x36869: '8305f045a80002',
    # 200先解引用半径与角度，分别调用cosf/sinf，写入与81相同的发射偏移，Z强制清零。
    0x268f1: '488d5604488bcfe883920000488bd6488bcff30f1038e874920000f30f10300f28c6e8e88d2c00f30f59c7f30f11875c0c00000f28c6e8c4962c00f30f1035ecc23200f30f59c7f30f103d1cc33200f30f1187600c00004489a7640c0000e9d9f2ffff',
    # 小角路径独立确认三角函数身份：cosf为1-x²/2，sinf为x-x³/6。
    0x2ef760: 'c5d35905383f0600c4e2d1ad051f3f0600c5fb5ac0',
    0x2f0050: 'c5d359cdc5f359c5c4e2d19d05cf370600',
    # Extra扩展13仅在源计数器能被6整除时发射；复制当前发射配置后按圆周修改出生点和角度。
    0x36c22: 'b8abaaaa2a8b4a10f76b54f30f107b608bc2c1e81f03d08d045203c03943540f850e010000',
    0x36c9b: 'f3440f1005d4be3100450f294bb8f3440f100df2bf3100660f6ec10f5bc0450f2953a8f3440f1015e9bf3100f3440f5ec0',
    0x36cd0: '0f28c7e8288a2b00f30f594364f3410f58c1f30f1144242c0f28c7e800932b00f30f594364488d542420488d0def014000f3410f58c2f30f114424300f28c7f30f58435cf30f11442454e88192fdfff3410f58f84883ef0175a6',
    0x36d55: 'ff4354',
    # 95先读无符号短载荷，但出生函数再符号扩展；负HP跳过覆盖，保留初始化模板的1点生命。
    0x283e9: '440fb74da8',
    0x37a04: '4533e4410fbfe9',
    0x37a3e: '488d9130c110004889742440488bcb48897c244841b8b0100000e833a22b0066443be57f0689ab34020000',
    0x37b2d: '66443be57f0689ab34020000',
    0x37952: 'c705b82abc0001000000',
    # 133把当前死亡回调绑定到超时槽并重置阶段时钟；三符各自据此选择18/19/20后继。
    0x28575: '8b87e00b00008987580c000044896704c70719fcffff',
    0x388a6: '8b81540c00003941040f8cc5010000',
    0x388e3: '480fbf91580c000089813c020000488b05a824a800488b0cd0418b81e00b000049898988000000418981580c0000',
    # 辅助ANM按0x120步长从演员+0x2d8选槽；129只中断该槽，本体游标独立。
    0x24f1a: '486306486356044c8b15085da900488d0cc0b80001000048c1e10503c24881c1d80200004803cf',
    0x284a3: '486306488d0cc00fb7460448c1e1056689843962030000',
    # 原型明确初始化八个辅助槽脚本为-1，生产容量据此约束0…7，不按当前只用slot0缩减。
    0x378a5: 'baffffffff',
    0x378b2: '668915372cbc00', 0x378bb: '6689154e2dbc00',
    0x378cf: '6689155a2ebc00', 0x378d6: '668915732fbc00',
    0x378dd: '6689158c30bc00', 0x378e4: '668915a531bc00',
    0x378eb: '668915be32bc00', 0x378f2: '668915d733bc00',
    # 135按参数是否为零设置演员+be的0x10位，不等于开关105；生存符使用非零分支。
    0x28590: '8b06f7d80fb687be0000001ac924ef80e1100ac8888fbe000000',
    # 生存符135(1)只令超时分派绕过标准超时结算/清弹块；回调、生命和时钟交接仍先完成，不能当作105免伤位。
    0x388db: '41f681be00000010480fbf91580c000089813c020000488b05a824a800488b0cd0418b81e00b000049898988000000418981580c00004189594441c7414019fcffff664189918000000041c781540c0000ffffffff4189590441c70119fcffff0f8597000000',
    0x38965: '0fb7086685c9740b6683f905740566c70005004805200600004883ea0175e1',
    # 普通超时把符卡状态从1递增并清可取得标记；94只在状态1且标记保留时登记取得与奖励。
    # 生存位绕过前者后，91开头的94沿用同一成功路径；不能把存活HP解释为伤害击破。
    0x38941: '8b053918bc00881d3f18bc0085c07408ffc089052718bc00',
    0x27a7f: '833dfa26bd00000f84f1010000488b059533a900664489b8c2080000833dde26bd00010f85ce010000',
    0x27ac0: '803dc526bd00000f84a9010000',
    # 追踪使魔57读取的是自机全局X/Y，HP变量读取演员+234；不是当前位置或持续追踪引用。
    0x2fc5a: '4c8d05bf1a5200498bc04883c438c3',
    0x2fc69: '4c8d05b41a5200498bc04883c438c3',
    0x2fd8a: '4d8d8134020000e9b1000000',
    # 最终符原生16：总钟7200前使用当前HP，模式0写速度/等待，模式1按HP缩放矩形并顺序读取X/Y随机数。
    0x3717a: '817904201c00004c8bc10f4c8134020000837a1000660f6ef00f5bf6753f',
    0x37198: 'f30f5e35d0bb3100f30f100528b9310069d0f0000000f30f5cc6b8f1197605f7eac1fa078bc2c1e81f83c02803c2894174f30f114164',
    0x371d7: 'f30f101515bb31000f28c6f30f5905a6ba310041b9309600008305693ca8000241ba53650000f30f100d9fba3100',
    0x37257: '8bc2f30f5935dbb93100f30f5cd0f30f5e3503bb31000f28c2f30f5905ccb73100f30f5cc80f57c0f3480f2ac0f30f59051cb73100f30f59c2f30f1015ecb93100f30f5cd6f30f58c80f28c2f30f590599b73100f3410f114860',
    0x372b1: '66330dac3ba8008305a13ba8000266412bcaf30f100d91b931000fb7c166c1e102f30f5cc866c1e80e',
    0x37304: '66890d593ba8008bc2f3480f2ac0f30f59058eb63100f30f59c2f30f58c8f3410f114864',
    # 原生14只扫描八个活动激光槽，从当前起距到终距按48源像素开区间取点，按激光当前角度发弹。
    0x36d9c: '488db1800f0000',
    0x36db7: 'bd08000000f3440f100d5bbe310044897954',
    0x36dd0: '488b1e4885db0f849f0000004438bb740200000f8492000000',
    0x36de9: 'f30f10b360020000f30f107b10',
    0x36e0a: 'f30f104b140f28f00f2fcf7661',
    0x36e3d: 'f30f588378020000f30f588b7c020000f30f118778020000f30f118f7c020000',
    0x36e5d: '4489bf80020000e83791fdfff30f104314f3410f58f90f2fc777a8ff4754',
    # 88向当前光束角度累加，90直接以演员位置加字面偏移改写基点；92进入淡出并重启该段时钟。
    0x26e2e: 'f30f108160020000f30f5802f30f118160020000',
    0x26edd: 'f3410f104610f30f1097b4000000f30f108fb8000000f30f5887b0000000',
    0x26f07: 'f30f118178020000f30f11917c020000f30f118980020000',
    0x26f62: '80b974020000000f84beecffff80b990020000020f83b1ecffffc6819002000002',
    0x26f8e: '4489610cc7410819fcffff',
    # 淡出计数在当前更新先与时限比较，达到时限即回收；存活路径在末尾才加一，演员逻辑可见末帧。
    0x12055: '8b43043943f80f8cd1fdffff4488ab60020000e9f2fdffff',
    0x11e44: '8b43f8488d530c488b0dde8daa008943f4ffc08943f8e8c151ffff',
    # 原生10先运行外部粒子6，Bomb期间关闭交互并补满60帧；恢复时使用160/165的全局+256脚本。
    0x36976: '83b93402000000488bd90f8e3f010000e8f5edffff',
    0x369b0: '66c74138a5014d8b81b83d0200',
    0x36a05: '66c7836c0f0000ffff80a3bd000000fec783981000003c000000c7839410000019fcffff',
    0x36a2f: '8b839810000085c00f8e8800000083e801898398100000757d',
    0x36a60: '66c74138a0014d8b81903d0200',
    0x36ab5: '66c7836c0f0000a100808bbd00000001',
    # 第四符原生15按ANM宽度区分大小弹，排除失活/淡出状态；只有零加速度小弹能被邻近大弹首次解放。
    # 方向相对Boss径向角按2.2放大后回写，120帧加速；两个640槽循环结束仍调用原生10。
    0x36f0c: '41be80020000', 0x36fb2: 'be80020000',
    0x36f50: '0fb747146685c00f849b0100006683f8050f8491010000',
    0x36f77: 'f30f10401c410f2fc00f8272010000',
    0x36fd6: '488b830c0100004885c00f8401010000440f2f401c0f86f6000000',
    0x36ff1: 'f30f1043e0410f2ec10f8ae70000000f85e1000000',
    0x37044: '440f2fd00f869900000066834bd010c743e00ad7233cc743e800000000c743e419fcffffc7430478000000',
    0x3708d: 'f3410f5cf3f3410f59f5f3410f58f30f28c6f30f1173fc',
    0x370e7: '4881c3200600004883ee010f85c8feffff4881c7200600004983ee010f8547feffff',
    0x37109: '498bd5488bcde85cf8ffff',
    # 第四符边界发射体：132设置特殊演员位；107仅修改死亡模式，离场路径直接释放活动位和粒子引用。
    # 场外检查使用当前ANM精灵半尺寸；必须先设置入场位0x04，不能以碰撞盒或出生中心替代。
    0x275e5: '0fb60e0fb687bd000000c0e105241f0ac8888fbd000000',
    0x28556: '8b06f7d80fb687be0000001ac924f780e1080ac8888fbe000000',
    0x38e9c: '0fb683bd000000a804755e',
    0x38ea7: '488b8b08020000f30f105920f3410f59dd',
    0x38ed1: 'f30f10591cf3410f59dd',
    0x38ef5: '0c048883bd000000',
    0x38f4d: '80e27f8893bc000000',
    0x38fa7: '4489b390100000e9e10a0000',
    # 绘制器以manager+0x89c为游标，-0x7d6正好是首演员(manager+8)的+be，0x08直接跳过整名演员。
    0x39bdc: '4881c39c080000bf0001000033f6',
    0x39bfd: 'f6832af8ffff080f857d030000',
    # manager首演员为+8；清敌游标aee175减首演员aee0b8等于+bd，而非+be。
    # 96及超时清敌跳过+bd的Boss位0x08；其余清HP，非交互者直接安装死亡回调。
    0x37c0b: '488d1da664ab00',
    0x38d3a: '498d5c2408',
    0x270bb: '808fbd00000008',
    0x389df: '488d158f57ab0041ba00010000',
    0x389f5: '0fb602a808753b899a77010000a8017531',
    0x28424: '488d154a5dac0041b900010000',
    0x28437: '0fb602a80875444489a277010000a8017539',
    0x28464: '48894acb44896287c7428319fcffff66448942c3',
    # 122只安装演员+98回调；执行点在移动参数、自动姿态之后，内容时钟递增之前。
    0x28019: '48630685c07814498b8cc2f0dd340048898f98000000',
    0x28b97: '4488bf7c0f0000488b87980000004885c0740733d2488bcfffd0',
    # 加速度在独立运动钟达到120时清除选项，否则先叠加三个速度分量；不清空标量加速度。
    0x11252: 'f6c110745a8b434839432c7c0d664123c866894b14',
    0x1126c: 'f30f108b08060000f30f584b08f30f114b08',
    # 100载荷的低三字节写演员+38/+39/+3a；低两字节分别用于击破效果与掉落效果(后者加4)。
    # 本次分身载荷258为2/1/0；不把它当作本体ANM脚本，也不推断第三字段未追踪的用途。
    0x26715: '0fb606884738410fb6460d884739410fb6460e88473a',
    0x3951f: '0fb65338c7442420ffffffffe8501affff',
    0x395f2: '0fb653394c8d83b000000083c204c7442420ffffffff41b903000000e86d19ffff',
    # 77零周期不重置计数或读取随机流；非零周期拼接32位随机结果后取模，写回独立周期时钟。
    0x267bc: '44898f680f00004585c90f8461f4ffff83058d46a90002',
    0x26811: '0fb7c8440bc166890d4646a900418bc041f7f18997a4100000c787a010000019fcffff',
    # 118向外部效果创建器传递effect/count/color和当前演员位置，不经过普通弹幕入口。
    0x27cf3: '418b46144c8d87b0000000458b4e108b1689442420e873320000',
    # 原生6在隐藏位0x08时释放外部粒子引用，否则向独立512槽效果池发出19号效果。
    # 两次2af80调用与原生子弹入口ffa0分离；该粒子路径的全局随机流仍不在演员局部差分内。
    0x3578d: 'f681be00000008',
    0x359ff: 'e87c55ffff',
    0x35b39: 'ba13000000e83d54ffff',
    0x2afe8: '8bd181f9000200007c09',
}
OPCODE_DISPATCH = {65: 0x272e0, 66: 0x27306, 6: 0x2266e, 124: 0x28367, 54: 0x2719d, 58: 0x2720e, 94: 0x27a7f, 77: 0x26784, 100: 0x26715, 107: 0x275e5, 118: 0x27cf3, 88: 0x26e02, 90: 0x26ec9, 92: 0x26f4e, 95: 0x283a2, 99: 0x24f1a, 127: 0x25c2d, 129: 0x284a3, 132: 0x28556, 133: 0x28575, 135: 0x28590, 200: 0x268f1}
STAGE6_EXTENSIONS = {7: 0x35c70, 8: 0x36270, 9: 0x36470, 10: 0x36970, 11: 0x36710}
# Extra与主线共享分派表，但有独立处理器，不能套六面入口；这里只声明已定位的身份。
EXTRA_EXTENSIONS = {6: 0x35780, 10: 0x36970, 13: 0x36c00, 14: 0x36d80, 15: 0x36ec0, 16: 0x37170, 17: 0x37340, 18: 0x37350, 19: 0x373f0}
EXTRA_ENCORE_VARIABLES = {-10100: 0x2fdbc, -10101: 0x2fe43, -10021: 0x2fc87}
EXTRA_ENCORE_CONSTANTS = {0x3529a8: 2.3283064365386963e-10, 0x352b78: 6.2831854820251465,
                          0x352b24: 3.1415927410125732, 0x352ab0: 1.5707963705062866,
                          0x3529bc: 0.006000000052154064, 0x3529b4: 0.0010000000474974513,
                          0x352e08: -999.0}
# MSG和timeline各有独立分派表；同为14不代表同一功能，不能混用ECL演员opcode。
EXTRA_MSG_DISPATCH = {0: 0x414ba, 2: 0x40964, 6: 0x40d72, 9: 0x4110d, 10: 0x40c0d,
                      11: 0x41431, 12: 0x412c4, 13: 0x412f8, 14: 0x41308}
EXTRA_TIMELINE_DISPATCH = {8: 0x386f2, 9: 0x38748, 10: 0x3876a,
                           11: 0x38782, 12: 0x38790}
EXTRA_CIRCLE_CONSTANTS = {0x352b78: 6.2831854820251465, 0x352ca4: 192.0, 0x352cb0: 224.0}
# 浮点变量读取器独立分派，确认扩展13访问的是哪几个源变量；offset是演员内字段偏移。
EXTRA_CIRCLE_VARIABLES = {-10004: (0x2fbd6, 0x54), -10006: (0x2fbe8, 0x5c),
                          -10007: (0x2fbf1, 0x60), -10008: (0x2fbfa, 0x64)}
EXTRA_FINAL_CONSTANTS = {0x352d70: 6000.0, 0x352cf4: 320.0, 0x352c90: 160.0, 0x352c84: 128.0, 0x352c5c: 96.0, 0x352ad0: 2.0, 0x352a44: 0.5}
EXTRA_PURSUER_VARIABLES = {-10018: 0x2fc5a, -10019: 0x2fc69, -10024: 0x2fd8a}
EXTRA_BEAM_SAMPLE_CONSTANTS = {0x352c20: 48.0}
EXTRA_CAGE_CONSTANTS = {0x352bfc: 30.0, 0x352c3c: 64.0,
                        0x352ad4: 2.200000047683716, 0x3529c8: 0.009999999776482582}
EXTRA_RETIREMENT_CONSTANTS = {0x352a44: 0.5, 0x352d08: 384.0, 0x352d14: 448.0}
EXTRA_RAIN_CONSTANTS = {0x352ad0: 2.0, 0x352b5c: 5.0, 0x3529f8: 0.0625}
MINE_CONSTANTS = {0x352bfc: 30.0, 0x3529fc: 0.10000000149011612,
                  0x3529b8: 1 / 256, 0x3529c8: 0.009999999776482582}
LABELS = (b'Easy Rank       * 0.5\n\0', b'Normal Rank     * 1.0\n\0',
          b'Hard Rank       * 1.2\n\0', b'Lunatic Rank    * 1.5\n\0')
# 指针顺序来自结果页的0/8字节索引，必须同时验证指针和字符串身份。
MODE_LABELS = ((0x351fd8, b'Standard\0'), (0x351100, b'Challenge\0'))


def inspect(pe, require_hash=True):
    """核验 PE 字节并返回源表与标签；任何不匹配或越界均抛 ValueError。"""
    digest = hashlib.sha256(pe.data).hexdigest()
    if require_hash and digest != EXPECTED_SHA256:
        raise ValueError('EXE 哈希与已核对版本不符')
    def read(rva, size):
        offset = pe.offset(pe.base + rva, size)
        if offset is None:
            raise ValueError(f'RVA {rva:#x} 未映射到完整文件范围')
        return pe.data[offset:offset + size]
    for rva, hex_bytes in ANCHORS.items():
        expected = bytes.fromhex(hex_bytes)
        if read(rva, len(expected)) != expected:
            raise ValueError(f'指令锚点不匹配：{rva:#x}')
    # 从真实RIP相对地址和manager首槽偏移算出循环字段，避免仅比较掩码而把相邻字段混为一谈。
    first_actor = 0x37c0b + 7 + struct.unpack('<i', read(0x37c0b + 3, 4))[0]
    cleanup_cursor = 0x28424 + 7 + struct.unpack('<i', read(0x28424 + 3, 4))[0]
    timeout_cursor = 0x389df + 7 + struct.unpack('<i', read(0x389df + 3, 4))[0]
    cleanup_field = cleanup_cursor - first_actor
    if cleanup_field != 0xbd or timeout_cursor != cleanup_cursor:
        raise ValueError('清敌游标与演员字段不匹配')
    for table, entries in ((0x41528, EXTRA_MSG_DISPATCH), (0x38834, EXTRA_TIMELINE_DISPATCH)):
        for opcode, expected in entries.items():
            target, = struct.unpack('<I', read(table + opcode * 4, 4))
            if target != expected:
                raise ValueError(f'Extra流程分派不匹配：{table:#x}/{opcode}')
    rank_values = struct.unpack('<5i', read(0x34e0e0, 20))
    if rank_values != (20, 32, 32, 32, 18):
        raise ValueError('五槽rank写入表不匹配')
    values = rank_values[:4]
    for index, (rva, label) in enumerate(MODE_LABELS):
        pointer, = struct.unpack('<Q', read(0x34ed50 + index * 8, 8))
        if pointer != pe.base + rva or read(rva, len(label)) != label:
            raise ValueError('Standard/Challenge模式标签不匹配')
    life_handler, = struct.unpack('<I', read(0x45168 + 5 * 4, 4))
    if life_handler != 0x44cb6:
        raise ValueError('残机道具分派不匹配')
    for index, label in enumerate(LABELS):
        if read(0x350998 + index * 24, len(label)) != label:
            raise ValueError('结算标签不匹配')
    combinations = struct.unpack('<12i', read(0x428f50, 48))
    if combinations != (0, 3, 1, 2, 3, 4, 1, 4, 0, 4, 2, 3):
        raise ValueError('帕秋莉四机体组合表不匹配')
    # 分派按 opcode-1 查字节索引，再查相对 image base 的 32 位目标。
    for opcode, expected in OPCODE_DISPATCH.items():
        dispatch = read(0x28e0c + opcode - 1, 1)[0]
        target, = struct.unpack('<I', read(0x28c28 + dispatch * 4, 4))
        if target != expected:
            raise ValueError(f'{opcode} 指令分派不匹配')
    for rva, expected in ((0x353690, 1.0), (0x3536a0, 0.5), (0x353830, 1 / 6)):
        value, = struct.unpack('<d', read(rva, 8))
        if value != expected:
            raise ValueError(f'200 三角函数常量不匹配：{rva:#x}')
    for index, rva in STAGE6_EXTENSIONS.items():
        pointer, = struct.unpack('<Q', read(0x34ddf0 + index * 8, 8))
        if pointer != pe.base + rva:
            raise ValueError(f'六面扩展{index}入口不匹配')
    for index, rva in EXTRA_EXTENSIONS.items():
        pointer, = struct.unpack('<Q', read(0x34ddf0 + index * 8, 8))
        if pointer != pe.base + rva:
            raise ValueError(f'Extra扩展{index}入口不匹配')
    for rva, expected in EXTRA_CIRCLE_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError(f'Extra圆周常量不匹配：{rva:#x}')
    for rva, expected in EXTRA_BEAM_SAMPLE_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError('Extra激光采样间距不匹配')
    for rva, expected in EXTRA_CAGE_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError(f'Extra笼目常量不匹配：{rva:#x}')
    for rva, expected in EXTRA_RETIREMENT_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError(f'演员回收常量不匹配：{rva:#x}')
    for rva, expected in EXTRA_RAIN_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError(f'彩雨弹运动常量不匹配：{rva:#x}')
    for variable, (expected_target, field) in EXTRA_CIRCLE_VARIABLES.items():
        index = read(0x2fed0 + variable + 0x2775, 1)[0]
        target, = struct.unpack('<I', read(0x2fe64 + index * 4, 4))
        if target != expected_target or read(target, 4) != bytes((0x4d, 0x8d, 0x41, field)):
            raise ValueError(f'Extra圆周变量映射不匹配：{variable}')
    for variable, expected in EXTRA_PURSUER_VARIABLES.items():
        index = read(0x2fed0 + variable + 0x2775, 1)[0]
        target, = struct.unpack('<I', read(0x2fe64 + index * 4, 4))
        if target != expected:
            raise ValueError(f'Extra追踪变量映射不匹配：{variable}')
    for variable, expected in EXTRA_ENCORE_VARIABLES.items():
        index = read(0x2fed0 + variable + 0x2775, 1)[0]
        target, = struct.unpack('<I', read(0x2fe64 + index * 4, 4))
        if target != expected:
            raise ValueError(f'Extra后继变量映射不匹配：{variable}')
    for rva, expected in EXTRA_ENCORE_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError(f'Extra后继常量不匹配：{rva:#x}')
    for rva, expected in EXTRA_FINAL_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError(f'Extra最终符常量不匹配：{rva:#x}')
    for rva, expected in MINE_CONSTANTS.items():
        value, = struct.unpack('<f', read(rva, 4))
        if value != expected:
            raise ValueError(f'六面地雷常量不匹配：{rva:#x}')
    return {'sha256': digest, 'rank_write_table': values,
            'rank_write_table_with_extra': rank_values,
            'extra_rank_write_value': rank_values[4],
            'rank_manager_rva': '0x53cab0',
            'rank_field_rva': '0x549ecc',
            'rank_new_run_skipped_state': 3,
            'campaign_next_stage_state': 3,
            'rank_replay_reloads_difficulty_table': True,
            'graze_event': {
                'normal_event_rva': '0x118dc', 'laser_event_rva': '0x6c549',
                'score': 500, 'stage_counter_limit': 99999, 'run_counter_limit': 999999,
                'score_still_added_when_counters_saturate': True,
                'rank_write_in_event_block': False, 'external_effect': 8, 'sound': 30,
                'laser_graze': {
                    'factory_rva': '0x10d10', 'update_rva': '0x10fa0', 'contact_rva': '0x6c2b0',
                    'phase_field_from_factory_base': '0x290', 'counter_field_from_factory_base': '0xc',
                    'grow_argument_offset': '0x30', 'sustain_argument_offset': '0x34',
                    'initial_phase': 'sustain_if_grow_zero_otherwise_grow',
                    'grow_to_sustain_resets_counter': True, 'same_update_checks_sustain_zero': True,
                    'period_content_frames': 12, 'first_sustain_frame': 0,
                    'contact_before_counter_increment': True, 'contact_before_sustain_end_check': True,
                    'hit_before_graze_gate': True, 'permanent_grazed_bit': False,
                },
                'scope': 'graze_event_and_laser_cadence_not_global_rank_or_collision_geometry_or_statistics_gate',
            },
            'spell_record_count': 134,
            'spell_record_number_base': 0,
            'spell_record_stride': '0x180',
            'spell_record_base_rva': '0x53d418',
            'timeline_waits_for_dialogue_release': True,
            'timeline_waits_for_boss_deletion': True,
            'actor_fixed_drop_field': '0x10a8',
            'actor_fixed_drop_creator': '0x43fa0',
            'extra_flandre_portraits': {0: 'face12c_0', 2: 'face12a_0', 3: 'face12b_0'},
            'extra_flandre_portrait_reload_excludes_msg': [2, 12],
            'patchouli_combinations': [combinations[i:i + 3] for i in range(0, 12, 3)],
            'opcode_127_noop_target': hex(OPCODE_DISPATCH[127]),
            'opcode_200_polar_offset_target': hex(OPCODE_DISPATCH[200]),
            'negative_spawn_health_preserves_template': 1,
            'opcode_133_arm_death_timeout_target': hex(OPCODE_DISPATCH[133]),
            'auxiliary_animation_slots': 8,
            'opcode_135_actor_flag_mask': '0x10',
            'survival_timeout_flag_skips_standard_cleanup': True,
            'survival_timeout_preserves_capture_eligibility': True,
            'extra_final_zero_health_clock': 7200,
            'extra_final_speed_range': [1, 2],
            'extra_final_wait_range': [40, 280],
            'extra_encore_random_angle_variable': -10100,
            'extra_encore_stored_angle_variable': -10101,
            'extra_encore_player_angle_variable': -10021,
            'extra_encore_coincident_player_angle': EXTRA_ENCORE_CONSTANTS[0x352ab0],
            'extra_encore_native_17_tailcalls_10': True,
            'extra_encore_native_19_restoration_script': 176,
            'extra_encore_spawn_copies_variable_slots': 16,
            'opcode_65_bounds_flag': '0x01',
            'opcode_66_disables_bounds': True,
            'death_mode_3_installs_callback_after_ecl': True,
            'death_mode_3_retains_one_health_until_callback': True,
            'opcode_124_spawns_native_item': True,
            'opcode_124_item_creator': '0x43fa0',
            'native_item_pool_capacity': 1024,
            'integer_random_bound_one_consumes_rng': True,
            'integer_random_bound_zero_consumes_rng': False,
            'opcode_54_interpolation_power': -2,
            'acceleration_angle_sentinel': EXTRA_ENCORE_CONSTANTS[0x352e08],
            'acceleration_sentinel_uses_emission_angle': True,
            'opcode_58_interpolation_power': 4,
            'extra_native_18_zero_health_clock': 7200,
            'extra_native_18_parameters': {'f0': '0.006 - float(hp)*0.001/6000',
                                          'i1': '22 + trunc(hp*25/6000)',
                                          'i2': '370 + trunc(hp*350/6000)',
                                          'i3': '400 - trunc(hp*250/6000)'},
            'extra_postfight_gate': {'minimum_captures': 6, 'mode_counter_maximum': 2,
                                    'capture_counter_rva': '0x53cabc', 'mode_flag_rva': '0x53d414',
                                    'mode_counter_rva': '0x53cab0',
                                    'condition': 'captures >= 6 and (mode_flag == 0 or mode_counter <= 2)',
                                    'source_spell_capture_range': [121, 133],
                                    'source_final_capture_sets_count': 10},
            'run_mode_labels': [label.rstrip(b'\0').decode('ascii') for _, label in MODE_LABELS],
            'challenge_mode_flag_rva': '0x53d414',
            'challenge_net_misses_rva': '0x53cab0',
            'confirmed_miss_counter_delta': 1,
            'challenge_life_pickup_counter_delta': -1,
            'standard_life_pickup_changes_miss_counter': False,
            'replay_miss_counter_is_signed': True,
            'extra_timeline_jump_marker_opcode': 13,
            'extra_timeline_maximum_dispatched_opcode': 12,
            'extra_timeline_null_jump_target_preserves_progress': True,
            'extra_msg_clear_marker_continues_script': True,
            'extra_msg_clear_marker_starts_award_counter': 1,
            'extra_clear_award_requires_counter': 1,
            'extra_clear_award_increments_counter': True,
            'extra_clear_difficulty_multiplier': 2,
            'extra_clear_bonus_truncates_to': 10,
            'extra_msg_state_transition_opcode': 11,
            'actor_death_checked_after_ecl': True,
            'actor_death_mode_three_health': 1,
            'extra_pursuer_variables': {variable: hex(target) for variable, target in EXTRA_PURSUER_VARIABLES.items()},
            'extra_extension_6_effect_pool_capacity': 512,
            'extra_bomb_recovery_frames': 60,
            'extra_beam_sampling_slots': 8,
            'extra_beam_sampling_distance_pixels': 48,
            'laser_force_fade_resets_clock': True,
            'laser_fade_expiration_checks_before_counter_increment': True,
            'random_period_uses_integer_modulo': True,
            'extra_cage_constants': {hex(rva): value for rva, value in EXTRA_CAGE_CONSTANTS.items()},
            'extra_cage_acceleration_frames': 120,
            'extra_cage_source_pool_slots': 640,
            'actor_retirement_constants': {hex(rva): value for rva, value in EXTRA_RETIREMENT_CONSTANTS.items()},
            'rain_spawn_finish_runs_motion': True,
            'rain_launch_boost_last_frame': 16,
            'rain_acceleration_first_frame': 18,
            'actor_retirement_requires_entry': True,
            'actor_outside_retirement_calls_death': False,
            'opcode_132_special_actor_mask': '0x08',
            'opcode_132_hides_actor_rendering': True,
            'opcode_96_flag_field': hex(cleanup_field),
            'actor_first_slot_rva': hex(first_actor),
            'actor_cleanup_cursor_rva': hex(cleanup_cursor),
            'opcode_96_skips_bosses': True,
            'opcode_96_direct_callback_for_noninteractive': True,
            'timeout_cleanup_skips_bosses': True,
            'opcode_122_callback_after_motion_and_poses': True,
            'opcode_107_death_mode_shift': 5,
            'actor_effect_bytes': {'defeat': '0x38', 'drop': '0x39'},
            'opcode_118_external_effect_target': '0x2af80',
            'stage6_extensions': {index: hex(rva) for index, rva in STAGE6_EXTENSIONS.items()},
            'extra_extensions': {index: hex(rva) for index, rva in EXTRA_EXTENSIONS.items()},
            'extra_circle_constants': {hex(rva): value for rva, value in EXTRA_CIRCLE_CONSTANTS.items()},
            'extra_circle_variable_fields': {variable: hex(field) for variable, (_, field) in EXTRA_CIRCLE_VARIABLES.items()},
            'mine_constants': {hex(rva): value for rva, value in MINE_CONSTANTS.items()},
            'settlement_labels': [label.rstrip(b'\n\0').decode('ascii') for label in LABELS],
            'unresolved': ['rank 动态写入与上下界、状态3其他入口用途']}


def main():
    parser = argparse.ArgumentParser(description='只读核验新典 EXE 难度相关证据')
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(PeImage(args.exe)), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()

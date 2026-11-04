#!/usr/bin/env python3
# 本文件只读核验新典外部效果池、ECL入口、附着释放、原生回调及粒子运动语义，不生成生产配方。
import argparse
import hashlib
import json
from pathlib import Path
import struct

from export_native_combat_data import PeImage
from inspect_stage_scripts import read_ecl


EXPECTED_SHA256 = '48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82'
# 静态请求清单必须保留逐关身份；四掩码相同也不代表运行控制流相同。
ECL_IDENTITIES = (
    'be9e49fe6f1c9fcbca858d335773bc16b1a89ae25083b60d1cb5ab33814dd2f3',
    '9a9aa3c3608195f1c814985706189ecf22c2ade1e302780acdf61a270a6267c0',
    '378e9fab04c794fa2573bbb407e6e2cb1c54dbdd372183d6e70c885315e1f171',
    '09744169b7ec3e6525e5cd2ff6e3c1c970cbcde4c8d0d772891aa38fc654bb09',
    '09b6a12c36069ac6caa320ca400ba70897f74ab6a7f04081076442cb5abc5276',
    'bc8013a2461b1e93ecc36acca8f2a81d948e0561e82bcde4176a6268b6985dce',
    'd6eae42d8d8eb669573b8004ba130f818374edc84ec35ccd0eda39eb6627a2eb',
)
# 每项为全局ANM脚本及更新器RVA；0表示无更新器，不是地址base。
# 表步长16字节，中间4字节为零；下一个地址0x428f50已属于Boss组合表。
EFFECT_TABLE = (
    (701, 0), (702, 0), (703, 0), (704, 0x2a6e0),
    (707, 0x2a4d0), (708, 0x2a4d0), (709, 0x2a4d0), (710, 0x2a4d0),
    (711, 0x2a4d0), (712, 0x2a4d0), (713, 0x2a4d0), (714, 0x2a4d0),
    (715, 0), (716, 0x2a990), (716, 0x2a990), (716, 0x2a990),
    (723, 0), (705, 0x2ace0), (706, 0x2ae30), (717, 0x2a900), (719, 0x2a2c0),
)
OPCODE_DISPATCH = {100: (84, 0x26715), 102: (86, 0x270f9), 118: (102, 0x27cf3)}
# 两条装载路径各自有八项跳转表；索引0是非正式入口，1…7才是关卡编号。
# 初始装载六面/Extra复用eff05/eff04；后续初始化另有条件重载，不能据此推断最终绑定。
STAGE_EFFECT_LOAD_SWITCHES = {
    0x2b2fc: (0x2b270, 0x2b270, 0x2b279, 0x2b282, 0x2b28b, 0x2b294, 0x2b2b6, 0x2b2db),
    0x2b56c: (0x2b44d, 0x2b44d, 0x2b46f, 0x2b478, 0x2b481, 0x2b48a, 0x2b48a, 0x2b481),
}
STAGE_EFFECT_FILES = ('eff01', 'eff02', 'eff03', 'eff04', 'eff05', 'eff05', 'eff04')
# 102颜色索引属于28行调色表，与21种效果身份分开；按EXE的小端ARGB整数保存。
ATTACHED_COLORS = (
    0xfff0f0f0, 0xfff0f0f0, 0xffffffff,
    0xffffe0e0, 0xffffe0e0, 0xffffe0e0,
    0xffffe0ff, 0xffffe0ff, 0xffffe0ff,
    0xffe0e0ff, 0xffe0e0ff, 0xffe0e0ff,
    0xffe0ffff, 0xffe0ffff, 0xffe0ffff,
    0xffe0ffe0, 0xffe0ffe0, 0xffe0ffe0,
    0xffe0ffe0, 0xffe0ffe0, 0xffe0ffe0,
    0xffffffe0, 0xffffffe0, 0xffffffe0,
    0xffffe0e0, 0xffffe0e0, 0xffffe0e0, 0xffffffff,
)
ANCHORS = {
    # 擦弹8固定完整更新器；两种请求分别取普通弹中点与自机位置，包含寄存器来源、半倍常量和创建调用。
    0x2a4d0: '837928004c8bc10f855e010000837924000f845401000083057209a900020f57c0f30f1015e789320041b930960000f30f100ded84320041ba536500000f57db418bd16633154a09a90066412bd20fb7c266c1e20266c1e80e6603c20fb7d00fb7cac1e210664133c966412bca0fb7c166c1e10266c1e80e6603c10fb7c866890d0f09a9000bd18bc2418bc9f3480f2ac0f30f59053f843200f30f59055f873200f30f5c050b873200f30f5e0523863200f3410f11808801000066330dd308a9008305c808a9000266412bca0fb7c166c1e10266c1e80e6603c10fb7d00fb7cac1e210664133c966412bca0fb7c166c1e10266c1e80e6603c10fb7c866890d9108a9000bd141c78090010000000000008bc2f3480f2ad8f30f591db9833200f30f591dd9863200f30f5c1d85863200f30f5e1d9d853200f3410f11988c0100000f57daf3410f1080880100000f57c2f30f59d9f30f59c1f3410f114008f3410f11580c41c7401000000080f3410f108088010000b801000000f3410f588074010000f3410f108888010000f3410f118074010000f3410f108078010000f3410f58808c010000f3410f118078010000f3410f10807c010000f3410f588090010000f3410f11807c010000f3410f584808f3410f118888010000f3410f10400cf3410f58808c010000f3410f11808c010000f3410f104810f3410f588890010000f3410f118890010000c3',
    0x11028: 'f3440f103d131a3400',
    0x11065: '41bc01000000',
    0x1188a: 'f30f10158efe5300f3410f5df1f30f101d85fe5300',
    0x1190f: 'f30f100511fe53004c8d442430f30f584338f30f585330f30f585b34458bccc7442420ffffffffba08000000f3410f59c7f3410f59d7f3410f59dff30f11442438f30f11542430f30f115c2434e81f960100',
    0x6c2fd: 'f3440f10251a544e00440f296c2440450f28d4f3440f102d0b544e00',
    0x6c36a: 'f3440f1015d1662e00',
    0x6c57c: 'f30f1005a4514e00f3450f58e4f30f58c0c7442420fffffffff3450f58ed41b9010000004c8d442430ba08000000f3450f59e2f3410f59c2f3450f59eaf30f11442438f3440f11642430f3440f116c2434e8aee9fbff',

    # 后续初始化在+955非零且未被匹配配置+6排除时，六面/Extra以相同基址723重载eff06/eff07。
    # 保留整个判定块；未证明条件字段含义，不将初始别名误报为全程唯一素材。
    0x3cd5f: '80bf5509000000747c8b2d66fd4f000fb63593065000e8b6b503008b152c15c300418bc685d27e304c8b055216c300498bc8403831740cffc04883c1283bc27cf1eb154898488d0c80498d04c84885c0740680780600752883fd0675094c8d058d403100eb0c83fd0775154c8d058f40310041b9d3020000418bd4e86156fcff',
    0x350e50: b'data/eff06.anm\0'.hex(),
    0x350e60: b'data/eff07.anm\0'.hex(),
    # 100将原始整数的低三个字节分别写入演员+38/+39/+3a，不经浮点寄存器或动态值解析。
    0x26715: '0fb606884738410fb6460d884739410fb6460e88473ae9fdf4ffff',
    # 独立装载与注册时内联装载均按关卡选择同一基址723；初始六面复用eff05，Extra复用eff04。
    0x2b240: '4883ec2833d241b8a0310300e87f662c004863057c18510083f8077752488d159c4dfdff8b8c82fcb202004803caffe14c8d05314e3200eb224c8d05384e3200eb194c8d053f4e3200eb104c8d05464e3200eb074c8d054d4e320041b9d3020000ba0b000000e89571fdff85c0752233c04883c428c341b9d30200004c8d05254e3200ba0b000000e87371fdff85c074deb8ffffffff4883c428c341b9d30200004c8d05f04d3200ba0b000000e84e71fdfff7d81bc04883c428c390',
    0x2b42e: '4863059f16510083f8077759488d15bf4bfdff8b8c826cb502004803caffe14c8d05544c320041b9d3020000ba0b000000e8dc6ffdff85c0742bb8ffffffffeb2a4c8d05424c3200ebdc4c8d05494c3200ebd34c8d05504c3200ebca4c8d05574c3200ebc1',
    0x2b320: '4883ec28488b0d05f9a800ba0b000000e8ab78fdff33c04883c428c3',
    0x3500a8: b'data/eff01.anm\0'.hex(),
    0x3500b8: b'data/eff02.anm\0'.hex(),
    0x3500c8: b'data/eff03.anm\0'.hex(),
    0x3500d8: b'data/eff04.anm\0'.hex(),
    0x3500e8: b'data/eff05.anm\0'.hex(),
    # 原生2完整函数只调用三角函数与弹幕工厂，末尾排队音效22；拒绝任何改成效果工厂的版本。
    0x34de00: '1046034001000000',
    0x34610: '4053574881ecc80000008b415033db488bf93b41547c11488999980000004881c4c80000005f5bc3f30f101d38e53100f30f100ddce43100f30f102d9ce43100f30f102560e73100f30f10154ce731004c89a424f000000041bc536500004c89bc24c000000041bf30960000440f29642450440f296c2440440f297c2420f3440f103d11e3310085c00f850d010000f20f1081b0000000f3440f102570d05100f3440f102d6bd05100f20f1105cf99ab008b81b8000000418bcf66330d9367a800f30f10054fd0510066412bcc83057c67a800028905ae99ab000fb7c166c1e10266c1e80e6603c1f30f1105a899ab000fb7d00f57c00fb7caf3440f11258e99ab00664133cfc1e21066412bccf3440f112d7e99ab000fb7c166c1e10266c1e80e6603c10fb7c80bd166890d2467a8008bc28bcbf3480f2ac0f3410f59c7f30f59c3f30f5cc1f30f11056a66a800f30f58c50f2fc1761b660f1f8400000000008bc1f30f58c4ffc183f8107f050f2fc177ee0f2fd0761b660f1f8400000000008bc1f30f58c3ffc183f8107f050f2fd077eef30f11052266a800eb1af3440f102def98ab00f3440f1025e298ab00f30f10050666a800448b4750b88988888841f7e84103d0c1fa048bc2c1e81f03d06bca1e443bc10f858f010000f30f1105d565a8008bcbf30f58c50f2fc1761466908bc1f30f58c4ffc183f8107f050f2fc177ee0f2fd0761b660f1f8400000000008bc1f30f58c3ffc183f8107f050f2fd077eef30f11059265a8008bcbf30f58c50f2fc1761d6666660f1f8400000000008bc1f30f58c4ffc183f8107f050f2fc177ee0f2fd0761b660f1f8400000000008bc1f30f58c3ffc183f8107f050f2fd077eef30f11054665a8008bcbf30f58c50f2fc1761d6666660f1f8400000000008bc1f30f58c4ffc183f8107f050f2fc177ee0f2fd0761b660f1f8400000000008bc1f30f58c3ffc183f8107f050f2fd077eef30f1105fa64a8008bcbf30f58c50f2fc1761d6666660f1f8400000000008bc1f30f58c4ffc183f8107f050f2fc177ee0f2fd0761b660f1f8400000000008bc1f30f58c3ffc183f8107f050f2fd077eef30f1105ae64a8008bcbf30f58c50f2fc1761d6666660f1f8400000000008bc1f30f58c4ffc183f8107f050f2fc177ee0f2fd0761b660f1f8400000000008bc1f30f58c3ffc183f8107f050f2fd077eef30f11056264a800b8abaaaa2a41f7e88bc2c1e81f03d08d0c5203c9443bc10f85ca020000f3440f5c25f096ab00f3440f5c2deb96ab004889ac24e00000000f57c0f30f2a47544889b424e8000000488d350064a8004c89b424f80000004c8d350564a8000f29b424b00000000f29bc24a0000000440f29842490000000440f298c2480000000440f29542470440f295c2460450f57dbf3450f2ad8440f29742430f3440f1035dfe13100f3440f5ed8410f28cbf3440f581d15e03100f30f590dc5df3100410f28c3f3440f59e1f30f59054ce03100f3440f58253f96ab00f3440f591deedf3100f3440f59e9f3440f582d2c96ab00f30f1187a00200008b4f50b889888888f30f1036f3440f104f640f28c6f7e903d1c1fa048bc2c1e81f03d06bc21e2bc866440f6ed1450f5bd2f3450f5ed6e855ac2b000f28f80f28c6f3410f59f9e835b52b00f30f107604440f28c04883c604f3450f59c10f28c6e82bac2b00f3410f59c1f30f5cc7f3410f59c2f30f58c7f3410f58c4f30f1187780200000f28c6e8f4b42b00f3410f59c1418bcff3410f5cc0f3410f59c2f3410f58c0f3410f58c5f30f11877c0200000f57c0899f8002000066330d3463a800f30f108f7402000066412bccf30f10b7700200000fb7c183051263a8000266c1e10266c1e80e6603c10fb7d00fb7cac1e210664133cf66412bcc0fb7c166c1e10266c1e80e6603c10fb7c80bd166890de062a8008bc2488d0d63234000488d976c020000f3480f2ac0f3410f59c7f30f59c1f30f588770020000f30f118770020000e8e9b3fdfff30f1087a0020000f30f11b770020000f3410f5cc3f30f1187a0020000493bf60f8c93feffff381d01f75100440f28742430440f285c2460440f28542470440f288c2480000000440f288424900000000f28bc24a00000000f28b424b00000004c8bb424f8000000488bb424e8000000488bac24e00000007530488d1588f65100488bc20f1f4400008b0885c9781283f9167416ffc34883c00483fb037ceaeb098bc3c7048216000000ff4750440f287c2420440f286c2440440f286424504c8bbc24c00000004c8ba424f00000004881c4c80000005f5bc3',
    # 漂散20的两次随机读取、固定初速/加速度和逐步积分顺序由完整更新器固定。
    0x2a2c0: '837928004c8bc10f855e010000837924000f84540100008305820ba900020f57c0f30f1015f78b320041b930960000f30f100dd586320041ba536500000f57db418bd16633155a0ba90066412bd20fb7c266c1e20266c1e80e6603c20fb7d00fb7cac1e210664133c966412bca0fb7c166c1e10266c1e80e6603c10fb7c866890d1f0ba9000bd18bc2418bc9f3480f2ac0f30f59054f863200f30f59056f893200f30f5c051b893200f30f5e05c3883200f3410f11808801000066330de30aa9008305d80aa9000266412bca0fb7c166c1e10266c1e80e6603c10fb7d00fb7cac1e210664133c966412bca0fb7c166c1e10266c1e80e6603c10fb7c866890da10aa9000bd141c78090010000000000008bc2f3480f2ad8f30f591dc9853200f30f591de9883200f30f5c1d95883200f30f5e1d3d883200f3410f11988c0100000f57daf3410f1080880100000f57c2f30f59d9f30f59c1f3410f114008f3410f11580c41c7401000000080f3410f108088010000b801000000f3410f588074010000f3410f108888010000f3410f118074010000f3410f108078010000f3410f58808c010000f3410f118078010000f3410f10807c010000f3410f588090010000f3410f11807c010000f3410f584808f3410f118888010000f3410f10400cf3410f58808c010000f3410f11808c010000f3410f104810f3410f588890010000f3410f118890010000c3',
    # 原生5的完整函数与两条返回路径一并固定，保证无粒子创建的审阅结论不只覆盖几个正向调用点。
    # 该函数只经三角/开方辅助、弹幕创建器及栈检查调用，末尾直接排队音效7；不能误套原生6的环境效果。
    0x34de18: '5053034001000000',
    0x35350: '4c8bdc56574881ec28010000488b051d0f3f004833c44889442470b8398ee338488bf9f769508bf2d1fe8bc6c1e81f03f08d04f63941500f85e803000049895b10b80800000049896b1833db8b2d6e8050004d8973204d897be841bf03000000410f2973d8410f297bc8450f2943b8450f294ba8450f295398450f295b88450f29a378ffffff450f29ab68ffffff668944244cb801000000450f29b358ffffff450f29bb48ffffff895c242848895c243c48895c2444895c245048895c245c48895c2464895c246c66894424383be87e066644897c2438f30f103515d631000f57c9f3440f101de5c25100f3440f1025e0c25100f3440f5c99b0000000f3440f5ca1b4000000f3440f102dc9c25100f3440f5ca9b8000000f30f2ace668944243ac7442458920a063f895c2420410f28d4410f28c3f30f59cef3410f59d4f30f5e0dfad63100f3410f59c3f30f5cf1410f28cdf3410f59cdf30f58d00f57c0f30f58d10f5ada660f2ec37706660f51c3eb080f28c3e8a6f729000f57d2450f57d2f20f5ad0f30f1005cfd431000f28ca0f540de9d931000f2fc1720a450f57c0450f57f6eb25f3440f101591d53100f3440f5ed2450f28f2450f28c2f3450f59f3f3450f59c4f3450f59d583e601740af30f1005d0d83100eb08f30f10059ed73100f3440f59f0f3440f59c0f3440f59d0f30f100597d93100f3440f59def3440f59e6f3440f59eef3450f58def30f10350fd53100f3450f58e0440f57f0440f57c0f3450f58ea440f57d00f28c6e87da12b000f28f80f28c6e862aa2b00f30f1035f2d73100450f28c8410f28cef3440f59c8f3440f59f00f28c6f30f59cff3440f59c7f3440f58c9f3450f5cc6e83da12b000f28f80f28c6e822aa2b00f30f1035b6d73100440f28f0f3440f103d31d4310041be09000000895c24540f1f00c744242400000040410f28cef3410f59c8410f28d10f28c7f3410f59c1440f28c90f28cff3410f59c8f3440f58c8410f28c6f30f59c2440f28c1f3440f5cc0410f28c1f30f5887b0000000410f28c8f3410f58c3f30f588fb4000000f30f1144242c410f28c2f3410f58ccf30f5887b8000000f30f114c2430f3410f58c5f30f1144243485f6740b83fd017f06f30f11742454488d5424206644897c244e488d0d5b184000e806a9fdfff3410f58f74983ee01740b8b2d657d5000e940ffffff381d2eec5100440f28bc2480000000440f28b42490000000440f28ac24a0000000440f28a424b0000000440f289c24c0000000440f289424d0000000440f288c24e0000000440f288424f00000000f28bc24000100000f28b424100100004c8bbc24200100004c8bb42458010000488bac24500100007520488d1591eb5100488bc28b0885c9783283f907740bffc34883c00483fb037ceaff4750488b9c2448010000488b4c24704833cce840aa28004881c4280100005f5ec38bc3c7048207000000ebd3ff4150ebd9',
    # etama4装入全局ANM基址698；装载器把每个局部script id加到r9d保存的基址后登记。
    0x126f8: '41b9ba0200004c8d0523c83300ba07000000e831fdfeff',
    0x34ef28: b'data/etama4.anm\0'.hex(),
    0x2495: '458bf94d8be0',
    0x2ab3: '418b50044803d7418b084103cf488994cb90300200',
    # 118直接转交种类/数量/颜色和演员当前位置，不登记演员持有的效果句柄。
    0x27cf3: '418b46144c8d87b0000000458b4e108b1689442420e873320000',
    # 102固定创建13号，并按颜色表转换；创建后保存句柄、三个字面参数及演员的额外标量。
    0x270f9: '4863064c8d87b0000000458bcfba0d000000418b848210dd340089442420e8643e0000',
    0x2711f: '4863879010000048898cc7e80b000048638790100000f20f1046048b560c488b8cc7e80b0000f20f1141188951208b4610898768020000ff8790100000',
    # 工厂从循环游标起扫描；游标先递增/环回再检查旧槽，不覆盖活动粒子。
    0x2af97: '48630dee30ac004c8d154bffa800',
    0x2afb7: '4869d9980100004c63e24c8d0538de3f00',
    0x2afe0: 'ffc1890da430ac008bd181f9000200007c09418bcd890d9130ac0044386b14',
    0x2b014: 'c6431401488d4b404488a370010000498bf4f2410f10074803f6f20f118374010000418b470889837c010000496304f066894138',
    # ANM先启动并立即执行，再覆盖请求的32位颜色；运动回调到后续池更新才调用。
    0x2b085: '4d85c07419488bd1498bc9e88bbffdff',
    0x2b0a3: '4489b32c0100004c8d054fdd3f00498b44f00848890344896b28c7432419fcffff4488ab8001000044886b3c83ed01',
    0x2b0dc: '7505498bdaeb074881c398010000ffc781ff000200000f8ce8feffff488d05f12dac00',
    # 更新顺序为自定义回调、ANM、内容时钟+1；二者任一要求删除均清活动位。
    0x2b150: 'bf00020000803b00743eff8698310300488d4bec488b014885c0740affd083f8017403c60300',
    0x2b176: '488b0db3faa800488d532ce89abefdff',
    0x2b186: '85c07403c603008b4314894310ffc08943144881c3980100004883ef0175b0',
    # 初始化/关卡装载清整池0x331a0；绘制逐槽只看活动位并复制位置至ANM。
    0x2b142: 'c7819831030000000000488d5914',
    0x2b244: '33d241b8a0310300e87f662c00',
    0x2b1f3: '80bba0feffff007422f20f1003488d93ccfeffff8b4308488b0d1ffaa800f20f11439489439ce822a2fdff',
    # 17/18只在时钟0且前帧非0初始化：保存出生点，并用一次32位随机角选择单位方向。
    0x2ace6: '83792800488bd90f85c7000000837924000f84bd000000',
    0x2acfd: '8b817c01000041b830960000f20f10817401000041b953650000f20f114130894138',
    0x2ad29: '83053001a90002',
    0x2ad75: '8bc2f3480f2af0f30f5935247c3200f30f5935ec7d3200f30f5c35907d3200',
    0x2ad94: '0f28c6e864492c00f30f1143180f28c6e847522c000f28742420f30f11431cc7432000000000',
    # position=origin+direction*(256-frame*256/denominator)，不是逐帧累加位置。
    0x2adba: 'f30f10150e7f3200b801000000660f6e43280f5bc0f30f59c2f30f5e05597e3200f30f5cd0',
    0x2addf: '0f28ca0f28c2f30f595318f30f594b1cf30f594320f30f585330f30f584b34f30f584338f30f119374010000f30f118b78010000f30f11837c010000',
    0x2ae36: '83792800488bd90f85c7000000837924000f84bd000000',
    0x2ae4d: '8b817c01000041b830960000f20f10817401000041b953650000f20f114130894138',
    0x2ae79: '8305e0ffa80002',
    0x2aec5: '8bc2f3480f2af0f30f5935d47a3200f30f59359c7c3200f30f5c35407c3200',
    0x2aee4: '0f28c6e814482c00f30f1143180f28c6e8f7502c000f28742420f30f11431cc7432000000000',
    0x2af0a: 'f30f1015be7d3200b801000000660f6e43280f5bc0f30f59c2f30f5e058d7d3200f30f5cd0',
    0x2af2f: '0f28ca0f28c2f30f595318f30f594b1cf30f594320f30f585330f30f584b34f30f584338f30f119374010000f30f118b78010000f30f11837c010000',
    # 3的X/Y初速各用一次32位随机值，随后固定加速度为初速的-0.05；每帧先移动再加速。
    0x2a6e0: '837928004c8bc10f856e010000837924000f846401000083056207a90002',
    0x2a70f: 'f30f100dd9823200',
    0x2a76c: 'f3480f2ac0f30f59052f823200f30f59054f853200f30f5c05fb843200f30f5905b3833200f30f5e056f843200f3410f118088010000',
    0x2a7a9: '8305b006a90002',
    0x2a7ed: '41c78090010000000000008bc2f3480f2ad8f30f591da1813200f30f591dc1843200f30f5c1d6d843200f30f591d25833200f30f5e1de1833200f3410f11988c010000',
    0x2a830: '0f57daf3410f1080880100000f57c2f30f59d9f30f59c1f3410f114008f3410f11580c41c7401000000080',
    0x2a85b: 'f3410f108074010000b801000000f3410f588088010000f3410f118074010000f3410f108078010000f3410f58808c010000f3410f118078010000f3410f108090010000f3410f58807c010000f3410f11807c010000',
    0x2a8b1: 'f3410f104808f3410f588888010000f3410f118888010000f3410f10400cf3410f58808c010000f3410f11808c010000f3410f104810f3410f588890010000f3410f118890010000c3',
    # 19没有随机初始化，消费创建者提供的速度/加速度，仍是先移动、后加速。
    0x2a900: 'f30f108174010000b801000000f30f588188010000f30f118174010000f30f10818c010000f30f588178010000f30f118178010000f30f10817c010000f30f588190010000f30f11817c010000',
    0x2a94d: 'f30f104908f30f588988010000f30f118988010000f30f10410cf30f58818c010000f30f11818c010000f30f104910f30f588990010000f30f118990010000c3',
    # 原生6隐藏时只将持有的102效果标为淡出，不推进环境粒子时钟；普通分支每步增加1度并折返至[-pi/4,pi/4)。
    0x3578d: 'f681be00000008488bd9744e',
    0x357a5: '488d83e80b00000f1f4000488b104885d2740ac6828001000001488938',
    0x357d0: '89bb90100000',
    0x357e7: 'f30f108128020000f30f5805e1d13100f30f100db1d231000f2f056ed23100f30f118128020000720cf30f5cc1f30f118128020000',
    # 环境时钟0…30每8帧、31…60每4帧、61…120每2帧，之后每帧；一组创建两个19。
    0x3581c: '8b81d00200003b81cc0200000f841004000083f8787f3e83f83c7e06a8017435eb0583f81e7e15',
    0x35843: '8bc881e1030000807d07ffc983c9fcffc185c974198bc881e1070000807d07ffc983c9f8ffc185c90f85cd030000',
    0x358cd: '250f0000807d07ffc883c8f0ffc09933ff2bc2be30960000d1f8bd53650000448bc06685c07434',
    0x35914: '33d20fb7c1410fb7c86689054055a800f7f1eb030fb7d7',
    0x3592b: 'f3440f103de0d23100f3440f1083280200000fb7c24103c066440f6ef0450f5bf6410f28c6410f28fef30f5905c8d13100f30f593d2cd33100f3410f5ec7f30f593d87d03100f3440f5cc0f30f10050ed43100f30f583d7ed23100',
    0x359dc: 'c7442420ff3030fff3440f59e741b901000000ba13000000',
    0x359ff: 'e87c55ffff',
    0x35a96: '4189b8900100008bc2f3450f5ef1f3480f2ac0f3440f5c3576cf3100f3410f59c3f3450f11b08c010000450f28c6f3410f59c7440f57c7f3440f59c6f3410f5cc2f3410f5ec1f3410f1180880100000f57c7f30f59c6f3410f114008f3450f11400c41c7401000000080',
    0x35b39: 'ba13000000e83d54ffff',
    0x35b93: 'f3450f11b08c0100008bc24189b890010000f3480f2ac0f3410f59c3f3410f59c7f3410f5cc2f3410f5ec1f3410f1180880100000f57c7f30f59c6f3410f114008f3450f11400c41c7401000000080',
    0x35c3e: '8b83d00200008983cc020000ffc08983d0020000',
    # 演员构造器将独立环境时钟current设为0、previous设为-1；不是猜测通用timer的初始化值。
    0x37550: 'baffffffff4533c0448941040f57c0891133c0',
    0x375f7: '448981d00200008991cc020000',
    # 启动后模板整段memset清零，再按0x10b0字节复制到出生槽；实际current/previous/phase均为0。
    0x37888: '33d2488d0d4f29bc0041b8b0100000e834a02b00',
    0x37a3e: '488d9130c110004889742440488bcb48897c244841b8b0100000e833a22b00',
    0x37c45: '41b8b0100000488d158e25bc00488bcbe836a02b00',
    0x2f18d0: '488bc14c8bc94c8d1523e7d0ff0fb6d249bb01010101010101014c0fafda66490f6ec3',
    # 13以三个字面量为归一化轴，sin/cos(phase)构造四元数；不是平面匀速旋转或源像素位移参数。
    0x2a9a3: '488d51180f2978d8488bf9',
    0x2a9cc: 'e86fd20200f30f10772c0f28c6e812562c00',
    0x2aa21: 'e8da4c2c00488d9760010000f30f11876c010000488d4c2430e8d1d50200',
    0x2aa54: '440f28dff3450f59c2f3410f59faf3450f5cd8f3450f5cc1f3450f59caf3440f5ccf',
    # 输出Z乘6后加独立中心；淡出先递增计数，达到16立即返回0，否则写alpha和scale。
    0x2ac00: 'f30f5905687f3200f30f584f30f30f585734f30f584738f30f118f74010000f30f119778010000f30f11877c01000080bf80010000007465',
    0x2ac38: '0fb6473cfec088473c3c107c0433c0eb59',
    0x2ac49: '81a72c010000ffffff000fbec0660f6ec00f5bc0f30f5905937d3200f30f5cf00f28c6f30f590554803200f30f2cc0f30f1005507e3200f30f5cc6c1e01809872c010000f30f118724010000f30f118728010000',
    # 演员末尾跟随：更新中心，半径小于共享目标时每帧加0.3；相位每帧加pi/100，再归一化。
    0x399dc: 'f20f1083b0000000f30f108a84010000f20f1142308b83b8000000894238f30f1083680200000f2fc1760df3410f58cbf30f118a84010000',
    0x39a14: 'f30f10422c418bcef3410f58c40f2fc6',
    0x39a30: '8bc1f3410f58c0ffc183f8107f050f2fc677ed0f2ff8761b0f1f8400000000008bc1f3410f58c1ffc183f8107f050f2ff877edf30f11422c',
    # 回收、超时和击破路径均清演员句柄并设置淡出，不立即删除效果槽。
    0x38f87: '488b084885c9740ac68180010000014c8930ffc24883c0083b93901000007ce04489b390100000',
    0x39120: '488b084885c9740ac68180010000014c8930ffc24883c0083b93901000007ce04489b390100000',
    0x395c0: '488b084885c9740ac68180010000014c8930ffc24883c0083b93901000007ce04489b390100000',
}
CONSTANTS = {0x3529f4: 0.05263157933950424, 0x352ba4: 12.0, 0x352a44: 0.5, 0x3529cc: 0.010526316240429878, 0x3529a8: 2.3283064365386963e-10, 0x352b24: 3.1415927410125732,
             0x352b78: 6.2831854820251465, 0x352cd0: 256.0, 0x352c34: 60.0, 0x352cb8: 240.0,
             0x3529f0: 0.05000000074505806, 0x352c84: 128.0, 0x352b44: 4.0,
             0x352c08: 33.0, 0x352ee0: -0.0,
             0x3529d8: 0.01745329238474369, 0x352ab0: 1.5707963705062866,
             0x352a74: 0.7853981852531433, 0x352c14: 40.0, 0x352c90: 160.0,
             0x3529f8: 0.0625, 0x352c04: 32.0, 0x352b88: 8.0,
             0x352a28: 0.2666666805744171, 0x352bc8: 20.0, 0x3529c4: 0.008333333767950535,
             0x352a2c: 0.30000001192092896, 0x3529ec: 0.03141592815518379,
             0x352b70: 6.0, 0x352ad0: 2.0, 0x352cc8: 255.0,
             0x352db8: -6.2831854820251465, 0x352dac: -3.1415927410125732}


def read(pe, rva, size):
    """按RVA读取完整范围；不可映射即拒绝，避免截断被误当为已核验机器码。"""
    offset = pe.offset(pe.base + rva, size)
    if offset is None:
        raise ValueError(f'外部效果读取越界：{rva:#x} + {size:#x}')
    result = pe.data[offset:offset + size]
    if len(result) != size:
        raise ValueError(f'外部效果读取截断：{rva:#x}')
    return result


def inspect(pe, *, require_hash=True):
    """返回这份EXE中已人工解读且通过锚点核验的效果语义；关闭哈希仅供合成测试。

    报告不宣称全部效果更新器、全局随机流、ANM删除帧或生产效果渲染已完成。
    """
    digest = hashlib.sha256(pe.data).hexdigest()
    if require_hash and digest != EXPECTED_SHA256:
        raise ValueError('外部效果EXE版本未知')
    for rva, encoded in ANCHORS.items():
        expected = bytes.fromhex(encoded)
        if read(pe, rva, len(expected)) != expected:
            raise ValueError(f'外部效果机器码锚点不符：{rva:#x}')
    for opcode, (index, target) in OPCODE_DISPATCH.items():
        if read(pe, 0x28e0c + opcode - 1, 1)[0] != index:
            raise ValueError(f'外部效果操作码分派索引不符：{opcode}')
        actual, = struct.unpack('<I', read(pe, 0x28c28 + index * 4, 4))
        if actual != target:
            raise ValueError(f'外部效果操作码目标不符：{opcode}')
    effects = []
    for index, (expected_script, expected_target) in enumerate(EFFECT_TABLE):
        script, padding, target = struct.unpack('<iiQ', read(pe, 0x428e00 + 16 * index, 16))
        relative = target - pe.base if target else 0
        if (script, padding, relative) != (expected_script, 0, expected_target):
            raise ValueError(f'外部效果表不符：{index}')
        effects.append({'effect': index, 'global_anm': script, 'callback_rva': hex(relative) if relative else None})
    for rva, expected in CONSTANTS.items():
        if read(pe, rva, 4) != struct.pack('<f', expected):
            raise ValueError(f'外部效果常量不符：{rva:#x}')
    if tuple(struct.unpack('<28I', read(pe, 0x34dd10, 28 * 4))) != ATTACHED_COLORS:
        raise ValueError('附着效果调色表不符')
    for rva, expected in STAGE_EFFECT_LOAD_SWITCHES.items():
        if struct.unpack('<8I', read(pe, rva, 32)) != expected:
            raise ValueError(f'关卡效果装载分派不符：{rva:#x}')
    return {
        'sha256': digest, 'pool_capacity': 512, 'slot_stride': '0x198',
        'pool_rva': '0xabaef0', 'cursor_rva': '0xaee08c', 'pool_bytes': '0x331a0',
        'factory_rva': '0x2af80', 'update_rva': '0x2b130', 'draw_rva': '0x2b1c0',
        'table': effects, 'opcode_118_retains_actor_handle': False,
        'common_anm_file': 'data/etama4.anm', 'common_anm_base': 698,
        'common_animation_scripts': {str(i): script - 698 for i, (script, _) in enumerate(EFFECT_TABLE) if i != 16},
        'stage_effect_16': {
            'global_anm': 723, 'animation_script': 0, 'custom_callback': None,
            'archive_slot': 11, 'loader_rva': '0x2b240', 'inlined_loader_rva': '0x2b42e',
            'unloader_rva': '0x2b320',
            'files_by_stage': {str(stage): f'data/{name}.anm'
                               for stage, name in enumerate(STAGE_EFFECT_FILES, 1)},
            'files_by_stage_scope': 'initial_loader_only',
            'conditional_reload': {
                'branch_rva': '0x3cd5f', 'load_call_rva': '0x3cdda', 'global_anm': 723,
                'files_by_stage': {'6': 'data/eff06.anm', '7': 'data/eff07.anm'},
                'condition': 'owner+0x955非零，且匹配配置不存在或配置+6为零；字段业务含义另验',
                'dynamic_reachability_proven': False,
            },
            'unresolved': ['实际创建入口及动态可达性', '源ANM逐帧语义', '世界坐标生产接线与图形生命周期'],
        },
        'opcode_102_effect': 13, 'opcode_102_retains_actor_handle': True,
        'opcode_102_color_argb': [hex(color) for color in ATTACHED_COLORS],
        'attached_13': {
            'callback_rva': '0x2a990', 'animation_script': 18,
            'axis_fields': ['0x18', '0x1c', '0x20'], 'axis_is_dimensionless': True,
            'center_field': '0x30', 'radius_field': '0x184', 'phase_field': '0x2c',
            'actor_shared_target_radius_field': '0x268',
            'radius_step_source_pixels': CONSTANTS[0x352a2c], 'radius_clamps_to_target': False,
            'phase_step_radians': CONSTANTS[0x3529ec],
            'output_z_factor': CONSTANTS[0x352b70],
            'quaternion': '(normalize(axis) * sin(phase), cos(phase))',
            'fade_flag_field': '0x180', 'fade_counter_field': '0x3c',
            'fade_expiry_frames': 16, 'fade_increments_before_expiry_check': True,
            'fade_alpha': 'trunc((1 - fade_counter / 16) * 255)',
            'fade_scale': '1 + fade_counter / 16',
            'actor_release_sets_fade_and_clears_handle': True,
            # 工厂只重置通用时钟/淡出状态，不清每槽半径和相位，不能从新池默认值推断复用。
            'factory_resets_radius_and_phase': False,
            'unresolved': ['完整绕轴矩阵的独立数值对照', '槽位复用及哨兵后的调用方语义',
                           'ANM与外部alpha/scale覆盖的逐步组合', '世界坐标生产接线与图形生命周期'],
        },
        'request_color_written_after_anm_start': True,
        'update_order': ['callback', 'anm', 'increment_content_frame'],
        'callback_keep_result': 1, 'content_hz': 60, 'spatial_units': 'source_pixels',
        'converging_effects': {
            '17': {'callback_rva': '0x2ace0', 'radius': 256.0, 'denominator_frames': 60.0},
            '18': {'callback_rva': '0x2ae30', 'radius': 256.0, 'denominator_frames': 240.0},
        },
        'converging_position': 'origin + unit_direction * (radius - frame * radius / denominator_frames)',
        'converging_random_reads_16bit': 2,
        'graze_requests': {
            'effect': 8, 'count': 1, 'color_argb': '0xffffffff',
            'ordinary_position': '(player_xyz + bullet_xyz) * 0.5',
            'beam_position': 'player_xyz',
            'ordinary_factory_call_rva': '0x1195c', 'beam_factory_call_rva': '0x6c5cd',
            'scope': 'creation_position_count_color_only',
            'unresolved': ['统计门控字段553eb8含义', '光束重复擦弹资格与时钟', '原生全局随机流交错'],
        },
        'ballistic_effects': {
            '8': {'callback_rva': '0x2a4d0', 'random_reads_16bit': 4,
                  'initial_xy_component': '(uniform_0_1 * 256 - 128) / 12',
                  'initial_z_velocity': 0, 'acceleration_initial_velocity_factor': -CONSTANTS[0x3529f4]},
            '3': {'callback_rva': '0x2a6e0', 'random_reads_16bit': 4,
                  'initial_xy_component': '(uniform_0_1 * 256 - 128) * 4 / 33',
                  'initial_z_velocity': 0, 'acceleration_initial_velocity_factor': -0.05000000074505806},
            '20': {'callback_rva': '0x2a2c0', 'random_reads_16bit': 4,
                   'initial_xy_component': '(uniform_0_1 * 256 - 128) / 60',
                   'initial_z_velocity': 0, 'acceleration_initial_velocity_factor': -CONSTANTS[0x3529cc]},
            '19': {'callback_rva': '0x2a900', 'random_reads_16bit': 0, 'parameters_from_creator': True},
        },
        'ballistic_update_order': ['position += velocity', 'velocity += acceleration'],
        'native_2': {
            'callback_rva': '0x34610', 'audited_end_rva_exclusive': '0x34c90',
            'whole_function_guarded': True, 'counter_field': '0x50', 'vertex_period_frames': 30,
            'period_frames': 6, 'edges_per_emission': 5, 'sound': 22,
            'bullet_factory_rva': '0xffa0', 'external_particles_per_emission': 0, 'calls_effect_factory': False,
        },
        'native_5': {
            'callback_rva': '0x35350', 'audited_end_rva_exclusive': '0x3577a',
            'whole_function_guarded': True, 'counter_field': '0x50', 'period_frames': 9,
            'bullet_factory_rva': '0xffa0', 'bullets_per_emission': 9, 'sound': 7,
            'external_particles_per_emission': 0, 'calls_effect_factory': False,
        },
        'ambient_6': {
            'callback_rva': '0x35780', 'hidden_actor_mask': '0x08', 'held_effect_fade_flag': '0x180',
            'clock_field': '0x2d0', 'phase_field': '0x228', 'phase_step_radians': CONSTANTS[0x3529d8],
            'constructor_current_frame': 0, 'constructor_previous_frame': -1,
            'spawn_current_frame': 0, 'spawn_previous_frame': 0, 'spawn_phase': 0,
            'cadence_inclusive_bounds_and_period': [[30, 8], [60, 4], [120, 2]], 'period_after_120': 1,
            'effect': 19, 'particles_per_emission': 2, 'color_argb': '0xff3030ff',
            'random_bound': 'floor((clock % 16) / 2)', 'radial_index': 'bound + random_16bit % bound (zero when bound is zero)',
            'radius': 'radial_index * 10 + 32', 'angle': 'phase - radial_index * pi / 40',
            'origins': ['actor + (cos(angle) * radius, sin(angle) * radius, 0)',
                        'actor + (-cos(angle) * radius, sin(angle) * radius, 0)'],
            'velocity_x': '(uniform_0_1 * 40 - 20) / 60',
            'velocity_y': 'radial_index * 8 / 60 - 0.2666666805744171',
            'velocity_z': 0, 'acceleration_initial_velocity_factor': -CONSTANTS[0x3529c4],
        },
        'unresolved': ['其他效果更新器完整语义', 'ANM与效果回调的完整组合寿命',
                       '原生全局随机流及满池哨兵的调用方写入', '生产差分、图形与关卡生命周期需由独立验收证明'],
    }


def inspect_requests(ecl):
    """汇总read_ecl结果中的102/118静态请求及100击破配置，保留四掩码与源偏移；未知源或参数布局拒绝。

    102首参数是颜色表索引，不是效果号；118颜色按EXE读取的小端ARGB整数报告。
    清单包含未证明可达的子程序，不展开循环，不能据此宣称运行时覆盖或粒子数量。
    """
    if ecl['sha256'] not in ECL_IDENTITIES:
        raise ValueError('外部效果ECL版本未知')
    rows = []
    defeat = []
    difficulties = {name: {} for name in 'ENHL'}
    for sub, program in enumerate(ecl['programs']):
        for ins in program:
            opcode = ins['opcode']
            if opcode not in (100, 102, 118):
                continue
            raw = bytes.fromhex(ins['payload'])
            if len(raw) != {100: 4, 102: 20, 118: 12}[opcode]:
                raise ValueError(f'外部效果请求参数布局不符：Sub{sub}@{ins["offset"]:#x}')
            value, = struct.unpack_from('<i', raw)
            row = {'sub': sub, 'frame': ins['frame'], 'offset': hex(ins['offset']),
                   'rank_mask': hex(ins['rank_mask']), 'opcode': opcode}
            if opcode == 100:
                # 这里只记录字面量配置；不能把它当成一次真实击破请求或证明其他原生写入不可达。
                row.update(raw_value=hex(value & 0xffffffff), primary_effect=value & 255,
                           secondary_effect_base=(value >> 8) & 255, third_byte=(value >> 16) & 255)
                defeat.append(row)
                continue
            if opcode == 102:
                row.update(effect=13, color_table_index=value,
                           parameters=list(struct.unpack_from('<4f', raw, 4)))
                key = f'102:color:{value}'
            else:
                count, color = struct.unpack_from('<II', raw, 4)
                row.update(effect=value, count=count, color_argb=hex(color))
                key = f'118:effect:{value}'
            rows.append(row)
            for index, name in enumerate('ENHL'):
                if ins['rank_mask'] & (0x100 << index):
                    counts = difficulties[name]
                    counts[key] = counts.get(key, 0) + 1
    return {'stage': ECL_IDENTITIES.index(ecl['sha256']) + 1, 'sha256': ecl['sha256'],
            'scope': 'static_instructions_not_runtime_emissions',
            'difficulties': difficulties, 'requests': rows, 'defeat_configurations': defeat,
            'explicit_effect_16_instructions': [row for row in rows if row['effect'] == 16],
            'primary_defeat_effect_16_configurations': [row for row in defeat if row['primary_effect'] == 16]}


def main():
    parser = argparse.ArgumentParser(description='只读核验新典独立外部效果池及蓄力粒子证据')
    parser.add_argument('exe', type=Path)
    parser.add_argument('--ecl', type=Path, action='append', default=[], help='追加指定关卡的静态请求清单，可重复')
    args = parser.parse_args()
    result = inspect(PeImage(args.exe))
    if args.ecl:
        result['ecl_requests'] = [inspect_requests(read_ecl(path)) for path in args.ecl]
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
# 本文件只读核对新典四 Bomb 的回调、伤害/尺寸/节奏指令、保护期和 ANM 层，不修改运行时代码。
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from export_native_combat_data import PeImage


def inspect(exe, assets):
    """读取本项目已核对的 PE64 布局；结构不符或动画缺失即拒绝，不猜测其他版本的偏移。"""
    image = PeImage(exe)
    digest = hashlib.sha256(image.data).hexdigest()
    if digest != '48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82':
        raise ValueError('EXE 版本未核对，拒绝把其他版本的地址当作新典攻击参数')
    # 直接验证产生伤害常量、判定宽度及非整除帧分支的机器指令，而非只从反编译工程抄常量。
    witnesses = {
        0xde6b: 'c743fc00004042', 0xde9a: 'c7870064ffff08000000',
        0xdec7: '83bf8064ffff64', 0xdf3b: 'c743fc00008043', 0xdf48: 'c7870064ffffc8000000',
        0xe56a: 'c7875477000000007842', 0xe5cd: 'a801', 0xe642: 'c7870003000008000000',
        0xecc7: 'c746f400000043', 0xecb7: '8d04523bc87448', 0xecff: '41c70608000000',
        0xf22d: '2503000080', 0xf293: 'c787000300000c000000', 0xf1d9: 'c787147700009a99993e',
        0x393dd: '83f846', 0x39446: '4183f803', 0x3944c: 'b856555555', 0x6b561: 'b856555555',
        # 帧首分别清空攻击宽度和清弹宽度；回调未写入的通道不会沿用上一帧。
        0x69f5b: '89b180010000', 0x6a01b: '89b154770000',
        # 爆破状态跳过几何写入；梦札仅跳过伤害写入，魔/恋札同时跳过伤害及清弹写入。
        0xdd70: '0f8537020000', 0xe5cf: '747b', 0xecbc: '7448', 0xf23d: '745e',
        # 同一次接触先累加原始 HP，再反馈对应通道；不是按追踪/视觉距离引爆。
        0x6b7bb: '4403f0', 0x6b7be: '0107',
        # 清弹查询遍历 16 个矩形；敌弹传入的矩形边界额外扩展 20 像素。
        0x6c064: '83f810', 0x11849: 'f30f5cd1', 0x352bc8: '0000a041',
    }
    for rva, expected_hex in witnesses.items():
        expected = bytes.fromhex(expected_hex)
        offset = image.offset(image.base + rva, len(expected))
        if offset is None or image.data[offset:offset + len(expected)] != expected:
            raise ValueError(f'Bomb 攻击指令 {rva:#x} 不符')
    attacks = (
        {'damage': 8, 'size_px': [48, 48], 'burst_damage': 200, 'burst_size_px': [256, 256],
         'burst_after_raw_contacts': 100, 'burst_force_frame': 270, 'burst_damage_frames': 1,
         'cancel': 'initial frame 0; flying orb every frame; burst trigger frame only',
         'initial_cancel_size_px': [256, 256], 'spawn_frames': list(range(80, 177, 16))},
        {'damage': 8, 'damage_frame_predicate': 'frame > 0 and frame % 2 != 0',
         'size_px': [[62, 448], [384, 62]], 'channels': 4, 'cancel_each_frame': True},
        {'damage': 8, 'damage_frame_predicate': 'frame > 0 and frame % 3 != 0',
         'size_px': [128, 128], 'channels': 8, 'radial_speed_px_per_frame': 2,
         'cancel_frame_predicate': 'frame > 0 and frame % 3 != 0'},
        {'damage': 12, 'damage_frame_predicate': 'frame > 0 and frame % 4 != 0',
         'channels': 1, 'size_px': [384, 'player_y'], 'movement_multiplier': 0.3,
         'cancel_frame_predicate': 'frame > 0 and frame % 4 != 0'},
    )
    table = image.offset(image.base + 0x426eb0, 64)
    if table is None:
        raise ValueError('无法定位新典 Bomb 回调表')
    names = ('灵符「梦想封印」', '梦符「封魔阵」', '魔符「Stardust Reverie」', '恋符「Master Spark」')
    records = []
    for identity in range(4):
        calc, draw = struct.unpack_from('<QQ', image.data, table + identity * 16)
        start, end = image.offset(calc, 1), image.offset(draw, 1)
        if start is None or end is None or not 0 < end - start < 0x2000:
            raise ValueError('Bomb 回调边界不符')
        code = image.data[start:end]

        def setting(displacement):
            # mov dword ptr [rsi/rdi+disp32],imm32；只认已核对的 x64 存储形态，重复赋值拒绝。
            pattern = b'\xc7[\x80-\x87]' + re.escape(struct.pack('<I', displacement)) + b'(.{4})'
            values = [struct.unpack('<I', match)[0] for match in re.findall(pattern, code, re.DOTALL)]
            if len(values) != 1:
                raise ValueError(f'Bomb 参数 {displacement:#x} 无唯一常量')
            return values[0]

        atlas = assets / 'battle/native' / ('player00.anm' if identity < 2 else 'player01.anm')
        raw = atlas.read_bytes()
        sprites, scripts = struct.unpack_from('<II', raw)
        available = {struct.unpack_from('<II', raw, 64 + sprites * 4 + i * 8)[0] for i in range(scripts)}
        first, count = (133, 4) if identity == 0 else (137, 4) if identity == 1 else (5, 3) if identity == 2 else (8, 4)
        layers = list(range(first, first + count))
        if not all(layer in available for layer in layers):
            raise ValueError('Bomb 原生 ANM 层不完整')
        records.append({'shot': identity, 'name': names[identity], 'calc_rva': hex(calc - image.base),
                        'draw_rva': hex(draw - image.base), 'duration_frames': setting(0x9f20),
                        'invulnerability_frames': setting(0x7858), 'anm': atlas.name,
                        'anm_sha256': hashlib.sha256(raw).hexdigest(), 'scripts': layers,
                        'attack': attacks[identity]})
    return {'exe_sha256': digest, 'table_rva': '0x426eb0', 'frames_per_second': 60,
            'damage_frame_cap': 70, 'spell_bomb_divisor': 3, 'normal_shot_divisor_during_bomb': 3,
            'cancel_margin_px': 20,
            'runtime_boundary': {'geometry': '二维矩形由项目适配为三维体积，不是原生三维判定',
                                 'provider': '仅替换有效接触后的 HP，不产生接触或清弹',
                                 'unimplemented': ['普通弹与 Bomb 联合上限', '普通弹完整 Boss 结算']},
            'instruction_witnesses': {hex(rva): value for rva, value in witnesses.items()}, 'bombs': records}


def main():
    """输出只读审阅 JSON；assets 为运行时素材根目录，输出可省略并打印到标准输出。"""
    parser = argparse.ArgumentParser(description='只读核对新典四 Bomb 的回调和时序')
    parser.add_argument('exe', type=Path)
    parser.add_argument('--assets', type=Path, default=Path('assets/runtime'))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = json.dumps(inspect(args.exe, args.assets), ensure_ascii=False, indent=2) + '\n'
    if args.output:
        args.output.write_text(result, encoding='utf-8')
    else:
        print(result, end='')


if __name__ == '__main__':
    main()

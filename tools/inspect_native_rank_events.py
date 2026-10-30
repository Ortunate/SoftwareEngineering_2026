#!/usr/bin/env python3
# 本文件只读保护新典事件、辅助代码与逐帧管理回调，限定局部rank证据，不生成运行时规则。
import argparse
import hashlib
import json
from pathlib import Path
import struct

from export_native_combat_data import PeImage

EXPECTED_SHA256 = '48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82'
# 范围为[start,end)，经完整控制流审阅后固定摘要；.pdata可能只覆盖一个展开片段，不能把首个ret当整函数结束。
# Bomb以已核对的calc/draw相邻入口界定全部计算片段，拾取保留七分派及两条返回路径。
CODE_REGIONS = {
    'manager_frame': (0x3baa0, 0x3c07f, '388e2fefdca20275768748ea38060c93d850606a93a93d08ddd8ef9bcc97d844'),
    'manager_registration': (0x3c0a0, 0x3c24d, 'e627a35cba983e3c76a9ded4b7a8f4c044049855a6293ae4231a73f2da4392f8'),
    'pickup_update': (0x44250, 0x45168, 'a2a21b4835098045beb1c8c81948091dfab5c9fbff75acac1aa00a260df1f855'),
    'score_popup': (0xa300, 0xa409, '857afeb748f3098e5a44b0e3fea7a2863e40ab544d6b01535b68581b5ecd7299'),
    'enemy_cancel': (0xf630, 0xfa24, 'cc7190a52203d8d7d5933ef351c6b2351a5a15317708f1fd98e6e41316a9c19c'),
    'item_factory': (0x43fa0, 0x44250, 'd91f879af429af170c658aac7475094f11531c71c75c90648ddbf4e0a634f458'),
    'player_event_update': (0x69f30, 0x6a761, 'c08039f7c503b1deda7b6c65031e1a26ebdcb822cdcf690dc74d6d4d8feaef43'),
    'bomb_binding': (0x69e51, 0x69e9c, 'eb38b1823546cffe9b3388562dc55a94b6c61ada5316b49c01ac4ca0420d55d6'),
    'homing_bomb': (0xd8d0, 0xe0f0, 'c536fbee4e789da7632d9eb98d16f3b4a610349148e171be66d10b4c658bed6d'),
    'needle_bomb': (0xe280, 0xe860, 'b16e84ac216b2536a8117ec5a043afb0b6ae78e792c1df82c496bb23c3ac311c'),
    'missile_bomb': (0xe9a0, 0xed80, 'ba6045d4778f009c32876650311aedd299fef7da9388356dc47758974d18b6c3'),
    'laser_bomb': (0xef20, 0xf310, '74e4558592beb50daf6b4278a70ca34569e6c419a90ea80045d32e147c57b704'),
}
BOMB_CALLBACKS = ((0xd8d0, 0xe0f0), (0xe280, 0xe860), (0xe9a0, 0xed80), (0xef20, 0xf310))
PICKUP_DISPATCH = (0x4461a, 0x4492b, 0x44a4e, 0x44c87, 0x44d3d, 0x44cb6, 0x44f36)


def read(pe, rva, size):
    """按完整RVA区间借用只读字节；截断或未映射区间抛ValueError，不以零填充代替源输入。"""
    offset = pe.offset(pe.base + rva, size)
    if offset is None or len(pe.data[offset:offset + size]) != size:
        raise ValueError(f'rank事件代码区间不可读取：{rva:#x}+{size:#x}')
    return pe.data[offset:offset + size]


def inspect(pe, *, require_hash=True):
    """返回已人工审阅且字节完整匹配的局部事件结论；关闭整文件哈希仅供合成测试。

    摘要证明代码身份，不自动证明语义；本工具不执行原生代码，也不证明整个运行期间rank恒定。
    """
    digest = hashlib.sha256(pe.data).hexdigest()
    if require_hash and digest != EXPECTED_SHA256:
        raise ValueError('rank事件EXE版本未知')
    for name, (start, end, expected) in CODE_REGIONS.items():
        if hashlib.sha256(read(pe, start, end - start)).hexdigest() != expected:
            raise ValueError(f'rank事件代码摘要不符：{name}')
    for shot, (calc, draw) in enumerate(BOMB_CALLBACKS):
        actual = struct.unpack('<QQ', read(pe, 0x426eb0 + shot * 16, 16))
        if actual != (pe.base + calc, pe.base + draw):
            raise ValueError(f'Bomb事件回调表不符：{shot}')
    if struct.unpack('<7I', read(pe, 0x45168, 28)) != PICKUP_DISPATCH:
        raise ValueError('拾取事件分派不符')
    return {
        'sha256': digest,
        'rank_field_rva': '0x549ecc',
        'code_regions': {name: {'start': hex(start), 'end_exclusive': hex(end), 'sha256': expected}
                         for name, (start, end, expected) in CODE_REGIONS.items()},
        'pickup_dispatch': [hex(value) for value in PICKUP_DISPATCH],
        'bomb_callbacks': [{'calc': hex(calc), 'draw': hex(draw)} for calc, draw in BOMB_CALLBACKS],
        'event_rank_delta': {'pickup': 0, 'miss_confirmation': 0, 'bomb_acceptance': 0},
        'manager_frame': {'callback_rva': '0x3baa0', 'owner_rva': '0x53cab0',
                          'frame_counter_offset': '0xd418', 'rank_offset': '0xd41c',
                          'top_level_rank_write': False, 'lives_based_periodic_rank_rule': False},
        'scope': 'audited_events_builtin_bombs_and_top_level_manager_not_global_rank_invariance',
        'unresolved': ['其他帧管理器、被调函数及间接写入', '动态rank及上下界',
                       '外部可替换回调及完整原生RNG交错', '拾取与Miss其他三维适配差异'],
    }


def main():
    """核验显式指定的本机EXE并输出审阅JSON；不修改原游戏或项目生产配置。"""
    parser = argparse.ArgumentParser(description='核验新典事件及逐帧管理回调的局部rank证据')
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(PeImage(args.exe)), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()

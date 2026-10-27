#!/usr/bin/env python3
# 本文件只读审阅新典全部战斗精灵显示尺寸及七关 ECL 判定设置，不生成生产时间轴或修改原游戏。
import argparse
import hashlib
import json
from pathlib import Path
import struct

from export_native_combat_data import PeImage, unpack


EXECUTABLE_HASH = '48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82'


def read_archive(path):
    """返回 ANM 图集裁切、显式尺寸与加载后逻辑尺寸；损坏/非有限/负尺寸拒绝导出。"""
    import math
    data = path.read_bytes()
    count, = unpack(data, '<I', 0)
    width, height = unpack(data, '<II', 12)
    if not 0 < count <= 4096 or width <= 0 or height <= 0:
        raise ValueError('ANM 精灵表无效')
    sprites = []
    for index in range(count):
        offset, = unpack(data, '<I', 64 + index * 4)
        number, x, y, w, h, display_w, display_h = unpack(data, '<I6f', offset)
        if (not all(math.isfinite(v) for v in (x,y,w,h,display_w,display_h)) or
                w <= 0 or h <= 0 or display_w < 0 or display_h < 0):
            raise ValueError('ANM 精灵尺寸无效')
        sprites.append({'id': number, 'offset': hex(offset), 'source': [x,y,w,h],
                        'explicit_size': [display_w,display_h],
                        'logical_size': [display_w or w * 480 / 1080,display_h or h * 480 / 1080]})
    return {'file': path.name, 'sha256': hashlib.sha256(data).hexdigest(),
            'atlas_size': [width,height], 'sprites': sprites}


def read_hitboxes(path):
    """列出各难度 ECL 103 的原始全宽 XYZ 与偏移；源 Z 不等同于项目三维球直径。"""
    data = path.read_bytes()
    count, = unpack(data, '<H', 0)
    starts = unpack(data, '<' + 'I' * count, 16)
    records = []
    for sub, start in enumerate(starts):
        offset = start
        while offset < len(data):
            frame, opcode, size, mask, parameters = unpack(data, '<iHHHH', offset)
            if frame == -1 and opcode == 65535:
                break
            if size < 12 or offset + size > len(data):
                raise ValueError('ECL 指令长度无效')
            if opcode == 103:
                if size != 24:
                    raise ValueError('ECL 103 长度无效')
                records.append({'sub': sub, 'frame': frame, 'offset': hex(offset),
                                'rank_mask': hex(mask), 'parameter_mask': hex(parameters),
                                'size': list(unpack(data, '<3f', offset + 12))})
            offset += size
        else:
            raise ValueError('ECL 子程序缺少终止标记')
    return {'file': path.name, 'sha256': hashlib.sha256(data).hexdigest(), 'hitboxes': records}


def read_executable(path):
    """核对已审阅 EXE 的加载比例、自机半径和敌弹判定写入；不推测其他版本地址。"""
    image = PeImage(path)
    if hashlib.sha256(image.data).hexdigest() != EXECUTABLE_HASH:
        raise ValueError('新典 EXE 版本未经尺寸核对')

    def real(rva):
        offset = image.offset(image.base + rva, 4)
        if offset is None:
            raise ValueError('尺寸常量超出 PE 原始节')
        return unpack(image.data, '<f', offset)[0]

    locations = (0x12979,0x129ab,0x12979,0x129ab,0x129c4,0x12979,0x129ee,0x12a1a,0x12a04,0x12a30)
    for rva in (*locations,0x69a24):
        length = 7 if rva == 0x69a24 else 6
        offset = image.offset(image.base + rva, length)
        expected = bytes.fromhex('48c7834c770000' if rva == 0x69a24 else 'c783b4040000')
        if offset is None or image.data[offset:offset+length] != expected:
            raise ValueError(f'尺寸写入指令不符：{rva:#x}')
    return {'sha256': EXECUTABLE_HASH, 'default_scale': [real(0x352d20),real(0x352d60)],
            'player_radius': real(0x69a24+7),
            'enemy_bullet_diameters': [real(rva+6) for rva in locations],
            'bullet_size_rvas': [hex(rva) for rva in locations]}


def main():
    """使用仓库素材和显式指定的本机输入写审阅 JSON；输出目录由调用方选择。"""
    parser = argparse.ArgumentParser(description='只读审阅新典 ANM 显示尺寸、ECL 判定与 EXE 尺寸常量')
    parser.add_argument('exe', type=Path)
    parser.add_argument('ecl_directory', type=Path)
    parser.add_argument('--assets', type=Path, default=Path('assets/runtime/battle/native'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = {'units': '原作逻辑像素；判定 size/diameter 为全宽，radius 为半径',
              'executable': read_executable(args.exe),
              'archives': [read_archive(path) for path in sorted(args.assets.glob('*.anm'))],
              'stages': [read_hitboxes(args.ecl_directory / f'ecldata{stage}.ecl') for stage in range(1,8)]}
    if len(result['archives']) != 25:
        raise ValueError('须提供项目完整的 25 套 ANM 素材')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(f"已核对 {len(result['archives'])} 套 ANM、七关 ECL 和新典 EXE：{args.output}")


if __name__ == '__main__':
    main()

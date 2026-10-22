#!/usr/bin/env python3
# 本文件只读解析本机新典梦之御札的侧针表，生成运行时通道参数，不导出主弹或游戏素材。

import argparse
import hashlib
from pathlib import Path
import struct


def read_levels(source: Path):
    """返回输入哈希、唯一梦之御札表 RVA 和九档侧针；PE、针束结构不匹配时抛 ValueError。"""
    data = source.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[:2] != b'MZ' or data[pe:pe+4] != b'PE\0\0' or struct.unpack_from('<H', data, pe+24)[0] != 0x20b:
        raise ValueError('输入须为新典 PE64 可执行文件')
    count = struct.unpack_from('<H', data, pe+6)[0]
    optional = struct.unpack_from('<H', data, pe+20)[0]
    image_base = struct.unpack_from('<Q', data, pe+48)[0]
    sections = []
    for index in range(count):
        _, rva, size, raw = struct.unpack_from('<IIII', data, pe+24+optional+40*index+8)
        sections.append((rva, raw, size))

    def file_offset(address, length):
        """映射完整记录的虚拟地址；不得越过节的原始数据或文件边界。"""
        for rva, raw, size in sections:
            relative = address-image_base-rva
            if 0 <= relative and relative+length <= size and raw+relative+length <= len(data):
                return raw+relative
        return None

    candidates = []
    for rva, raw, size in sections:
        for offset in range(raw, min(raw+size, len(data))-144+1, 8):
            entries = [struct.unpack_from('<iiQ', data, offset+index*16) for index in range(9)]
            if [entry[1] for entry in entries] != [8,16,32,48,64,80,96,128,999]:
                continue
            levels = []
            for count, _, address in entries:
                location = file_offset(address, count*36) if 0 < count <= 32 else None
                if location is None:
                    break
                records = [struct.unpack_from('<hh6fHBBhh', data, location+index*36) for index in range(count)]
                sides = [record for record in records if record[9] != 0]
                if len(sides) % 2 or any(record[10] != 0 or record[11] != 1122 for record in sides):
                    break
                pairs = []
                for left, right in zip(sides[::2], sides[1::2]):
                    if not (left[:9] == right[:9] and left[9] == 1 and right[9] == 2 and
                            0 <= left[1] < left[0] <= 30 and left[3:6] == (-16.0,12.0,40.0) and
                            abs(left[6]+1.57079632679) < 0.00001 and left[7] == 22.0 and left[8] > 0):
                        raise ValueError('侧针须为同速同相位的左右直射针束')
                    pairs.append((left[0], left[1], left[2], left[8]))
                levels.append((0 if not levels else entries[len(levels)-1][1], pairs))
            if len(levels) == 9 and not levels[0][1] and all(pairs for _, pairs in levels[1:]):
                candidates.append((rva+offset-raw, levels))
    if len(candidates) != 1:
        raise ValueError(f'预期唯一梦之御札表，实际找到 {len(candidates)} 个')
    return hashlib.sha256(data).hexdigest(), *candidates[0]


def write_header(destination: Path, sha256: str, rva: int, levels):
    """覆盖指定 C++ 参数头；只保存侧针的时序、横向偏移、伤害及共同的原始单位常量。"""
    lines = [
        '// 本文件保存新典梦之御札侧针通道；由 tools/import_reimu_needle.py 只读导入，主弹不在此表。',
        f'// 输入 SHA-256：{sha256}；火力表 RVA：0x{rva:x}。',
        '#pragma once', '#include <array>', '#include <span>', '',
        'namespace koumakyou::reimu_needle {',
        '// 时序沿用 60 Hz、30 帧窗口；偏移采用原始逻辑像素，X 向屏幕右，Y 向屏幕下。',
        'inline constexpr float kOffsetYPixels = -16.0F;',
        'inline constexpr float kSpeedPixelsPerFrame = 22.0F;',
        '/// @brief 左右模块各一颗的直射针束；周期/相位为 60 Hz 帧，offsetXPixels 为两侧共用的横向偏移。',
        '/// nativeDamage 为新典每颗针的原始伤害；渲染与三维数值的换算由 Player 负责。',
        'struct Pair { int periodFrames; int phaseFrame; float offsetXPixels; float nativeDamage; };',
        '/// @brief Power 达到 minimumPower 后使用该档；pairs 借用下方静态数组。',
        'struct Level { int minimumPower; std::span<const Pair> pairs; };', '',
    ]
    for minimum, pairs in levels:
        lines.append(f'inline constexpr std::array<Pair, {len(pairs)}> kPower{minimum}{{{{')
        lines.extend(f'    {{{period}, {phase}, {offset:.1f}F, {damage:.1f}F}},' for period, phase, offset, damage in pairs)
        lines.append('}};')
    lines.append('inline constexpr std::array<Level, 9> kLevels{{')
    lines.extend(f'    {{{minimum}, kPower{minimum}}},' for minimum, _ in levels)
    lines.extend(['}};', '', '/// @brief 返回 Power 对应的侧针档；越界值沿用首档或末档，不改变玩家状态。',
                  'inline const Level& ForPower(int power) {', '    const Level* level = &kLevels.front();',
                  '    for (const auto& candidate : kLevels) {', '        if (power < candidate.minimumPower) break;',
                  '        level = &candidate;', '    }', '    return *level;', '}',
                  '}  // namespace koumakyou::reimu_needle', ''])
    destination.write_text('\n'.join(lines), encoding='utf-8')


def main():
    """只读指定的新典文件并写入参数头；失败以异常退出，不启动原游戏。"""
    parser = argparse.ArgumentParser(description='只读导入新典梦之御札侧针')
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, default=Path('src/gameplay/actors/reimu_needle.hpp'))
    args = parser.parse_args()
    sha256, rva, levels = read_levels(args.source)
    write_header(args.output, sha256, rva, levels)
    print(f'SHA-256 {sha256}; Reimu B table RVA 0x{rva:x}')
    for minimum, pairs in levels:
        print(f'Power {minimum}: {pairs}')


if __name__ == '__main__':
    main()

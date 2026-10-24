#!/usr/bin/env python3
# 本文件只读解析本机新典 PE64 的灵梦追踪弹火力表，生成运行时使用的发射通道参数。

"""不启动、修改或复制游戏；仅导出周期、相位、左右模块和出射角，不导出可执行代码或素材。"""

import argparse
import hashlib
import math
from pathlib import Path
import struct


def read_levels(source: Path):
    """读取 PE64，返回唯一灵梦 A 火力表的 RVA 与九档数据；结构不匹配或候选不唯一时报错。"""
    data = source.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[:2] != b'MZ' or data[pe:pe+4] != b'PE\0\0' or struct.unpack_from('<H', data, pe+24)[0] != 0x20b:
        raise ValueError('输入须为新典 PE64 可执行文件')
    section_count = struct.unpack_from('<H', data, pe+6)[0]
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    image_base = struct.unpack_from('<Q', data, pe+48)[0]
    sections = []
    for index in range(section_count):
        offset = pe+24+optional_size+40*index
        _, rva, size, raw = struct.unpack_from('<IIII', data, offset+8)
        sections.append((rva, raw, size))

    def file_offset(va, length):
        """将原始虚拟地址映射到文件偏移；整个记录须位于同一节的原始数据内。"""
        for rva, raw, size in sections:
            relative = va-image_base-rva
            if 0 <= relative and relative+length <= size and raw+relative+length <= len(data):
                return raw+relative
        return None

    candidates = []
    for rva, raw, size in sections:
        for offset in range(raw, min(raw+size, len(data))-9*16+1, 8):
            entries = [struct.unpack_from('<iiQ', data, offset+i*16) for i in range(9)]
            if [entry[1] for entry in entries] != [8, 16, 32, 48, 64, 80, 96, 128, 999]:
                continue
            levels = []
            for count, limit, address in entries:
                location = file_offset(address, count*36) if 0 < count <= 32 else None
                if location is None:
                    break
                records = [struct.unpack_from('<hh6fHBBhh', data, location+i*36) for i in range(count)]
                if any(record[10] not in (0, 1) for record in records):
                    break
                homing = [record for record in records if record[10] == 1]
                if any(record[11] != 1121 for record in homing) or len(homing) % 2:
                    break
                pairs = []
                for left, right in zip(homing[::2], homing[1::2]):
                    angle = round(math.degrees(right[6])+90, 4)
                    if not (left[0:2] == right[0:2] and 0 <= left[1] < left[0] <= 30 and
                            left[9] == 1 and right[9] == 2 and 0 < angle < 90 and
                            abs(math.degrees(left[6])+90+angle) < 0.001):
                        raise ValueError('追踪通道不满足左右成对的周期/相位/角度约定')
                    pairs.append((left[0], left[1], angle))
                levels.append((0 if not levels else entries[len(levels)-1][1], pairs))
            if len(levels) == 9 and not levels[0][1] and all(pairs for _, pairs in levels[1:]):
                candidates.append((rva+offset-raw, levels))
    if len(candidates) != 1:
        raise ValueError(f'预期唯一灵梦 A 火力表，实际找到 {len(candidates)} 个；请重新核对版本')
    return hashlib.sha256(data).hexdigest(), *candidates[0]


def write_header(destination: Path, sha256: str, rva: int, levels):
    """将已验证的九档参数写入 C++ 头文件；destination 为允许覆盖的源码输出路径。"""
    lines = [
        '// 本文件保存从本机新典解析的灵梦追踪弹发射通道；由 tools/import_reimu_homing.py 导入。',
        f'// 输入 SHA-256：{sha256}；火力表 RVA：0x{rva:x}。',
        '#pragma once', '#include <array>', '#include <span>', '',
        'namespace koumakyou::reimu_homing {',
        '// 原作按 60 Hz 推进，射击计时在 30 帧后归零；满火力的 16 帧周期也在该窗口内取模。',
        'inline constexpr int kFramesPerSecond = 60;',
        'inline constexpr int kCycleFrames = 30;',
        '/// @brief 左右模块同时各发一颗的通道；cycleFrame % periodFrames == phaseFrame 时发射。',
        '/// halfAngleDegrees 为相对准星的单侧横向角（度），左负右正；周期和相位单位为 60 Hz 帧。',
        'struct Pair { int periodFrames; int phaseFrame; float halfAngleDegrees; };',
        '/// @brief Power 达到 minimumPower 时使用该档；pairs 借用下方静态数组，不随玩家状态变化。',
        'struct Level { int minimumPower; std::span<const Pair> pairs; };', '',
    ]
    for minimum, pairs in levels:
        lines.append(f'inline constexpr std::array<Pair, {len(pairs)}> kPower{minimum}{{{{')
        for period, phase, angle in pairs:
            lines.append(f'    {{{period}, {phase}, {angle:.1f}F}},')
        lines.append('}};')
    lines.append('inline constexpr std::array<Level, 9> kLevels{{')
    lines.extend(f'    {{{minimum}, kPower{minimum}}},' for minimum, _ in levels)
    lines.extend(['}};', '',
        '/// @brief 返回 Power 对应的追踪通道；Power 单位为点，低于 0 或高于 128 时沿用首档/末档。',
        'inline const Level& ForPower(int power) {',
        '    const Level* level = &kLevels.front();',
        '    for (const auto& candidate : kLevels) {',
        '        if (power < candidate.minimumPower) break;',
        '        level = &candidate;',
        '    }',
        '    return *level;',
        '}',
        '}  // namespace koumakyou::reimu_homing', ''])
    destination.write_text('\n'.join(lines), encoding='utf-8')


def main():
    """解析明确指定的本机文件并打印导入摘要；输入或输出失败时以异常退出。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='本机 th06nc.exe；只读')
    parser.add_argument('--output', type=Path, default=Path('src/gameplay/actors/reimu_homing.hpp'))
    args = parser.parse_args()
    sha256, rva, levels = read_levels(args.source)
    write_header(args.output, sha256, rva, levels)
    print(f'SHA-256 {sha256}; Reimu A table RVA 0x{rva:x}')
    for minimum, pairs in levels:
        print(f'Power {minimum}: {pairs}')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
# 本文件解析新典 ECL 的独立测试参考指令并生成关卡专用夹具，不参与生产行为。
"""关卡调用方显式声明源身份、子程序数量和受支持格式，未知指令拒绝导入。"""
import argparse
import math
from pathlib import Path

# 参数类型来自 https://github.com/thpatch/thtk/blob/master/thecl/thecl06.c 的 th06_fmts；仅支持本源实际出现的固定宽度类型。
# z 为以零填充的末尾字符串，旧参考解释器不读取文本，保留为零；原始字节仍留在 ECL。
FORMATS = {0: '', 1: 'S', 2: 'to', 3: 'toS', 4: 'SS', 5: 'Sf', 6: 'SU', 7: 'SUS', 8: 'Sf', 9: 'Sff', 10: 'S', 11: 'S', 12: 'S', 13: 'SSS', 14: 'SSS', 15: 'SSS', 16: 'SSS', 17: 'SSS', 18: 'S', 19: 'S', 20: 'Sff', 21: 'Sff', 22: 'Sff', 23: 'Sff', 24: 'Sff', 25: 'Sffff', 26: 'S', 27: 'SS', 28: 'ff', 29: 'to', 30: 'to', 31: 'to', 32: 'to', 33: 'to', 34: 'to', 35: 'NSf', 36: '', 37: 'NSfSS', 38: 'NSfSS', 39: 'NSfSS', 40: 'NSfSS', 41: 'NSfSS', 42: 'NSfSS', 43: 'fff', 44: 'fff', 45: 'ff', 46: 'f', 47: 'f', 48: 'f', 49: 'ff', 50: 'ff', 51: 'ff', 52: 'Sff', 53: 'Sff', 54: 'Sff', 55: 'Sff', 56: 'Sfff', 57: 'Sfff', 58: 'Sfff', 59: 'Sfff', 60: 'Sfff', 61: 'S', 62: 'S', 63: 'S', 64: 'S', 65: 'ffff', 66: '', 67: 'ssSSffffS', 68: 'ssSSffffS', 69: 'ssSSffffS', 70: 'ssSSffffS', 71: 'ssSSffffS', 72: 'ssSSffffS', 73: 'ssSSffffS', 74: 'ssSSffffS', 75: 'ssSSffffS', 76: 'S', 77: 'S', 78: '', 79: '', 80: '', 81: 'fff', 82: 'SSSSffff', 83: '', 84: 'S', 85: 'ssffffffSSSSSS', 86: 'ssffffffSSSSSS', 87: 'S', 88: 'Sf', 89: 'Sf', 90: 'Sfff', 91: 'S', 92: 'S', 93: 'ssz', 94: '', 95: 'NfffssS', 96: '', 97: 'S', 98: 'ssssS', 99: 'SS', 100: 'S', 101: 'S', 102: 'Sffff', 103: 'fff', 104: 'S', 105: 'S', 106: 'S', 107: 'S', 108: 'N', 109: 'NS', 110: 'S', 111: 'S', 112: 'S', 113: 'S', 114: 'N', 115: 'S', 116: 'N', 117: 'S', 118: 'SUC', 119: 'S', 120: 'S', 121: 'SS', 122: 'S', 123: 'S', 124: 'S', 125: '', 126: 'S', 127: 'S', 128: 'S', 129: 'SS', 130: 'S', 131: 'ffSSSS', 132: 'S', 133: '', 134: '', 135: 'S'}


# 新典201的布局与普通发射一致；其模式8的原生语义另由六面测试验证，导入不把它改写为75。
FORMATS[201] = 'ssSSffffS'
# 新典200按角度（弧度）和半径（源像素）设置发射偏移，第三个float保留但原生处理器不读取。
# 与43的设置位置不同；分派、cosf/sinf及偏移字段由 inspect_native_difficulty 独立核验。
FORMATS[200] = 'fff'


def parse_binary_stage(path, difficulty, expected_subs):
    """按关卡声明的子程序数量校验 ECL，返回筛选指令和源位置；未知格式立即失败。"""
    import struct
    import inspect_stage_scripts

    ecl = inspect_stage_scripts.read_ecl(path)
    if len(ecl['programs']) != expected_subs:
        raise ValueError(f'输入不是预期的关卡脚本（{expected_subs} 个子程序）')
    if difficulty not in 'ENHL' or len(difficulty) != 1:
        raise ValueError('难度须为 E/N/H/L 之一')
    mask = 0x100 << 'ENHL'.index(difficulty)
    programs = []
    for sub, raw in enumerate(ecl['programs']):
        enabled = [i for i in raw if i['rank_mask'] & mask]
        # 原始标签可以落在被筛除的指令，目标须重定位到该位置后的第一条有效指令。
        offsets = [i['offset'] for i in enabled]
        output = []
        for ins in enabled:
            location = f"{path}:Sub{sub}@{ins['offset']:#x}"
            fmt = FORMATS.get(ins['opcode'])
            if fmt is None:
                raise ValueError(f'{location}: 未知操作码')
            data = bytes.fromhex(ins['payload'])
            args, position = [], 0
            for kind in fmt:
                if kind == 'z':
                    if position >= len(data) or b'\0' not in data[position:]:
                        raise ValueError(f'{location}: 字符串缺少终止符')
                    args.append(0)
                    position = len(data)
                    continue
                code = {'s': 'h', 'S': 'i', 'N': 'i', 't': 'i', 'o': 'i',
                        'U': 'I', 'C': 'I', 'f': 'f'}.get(kind)
                if code is None:
                    raise ValueError(f'{location}: 未支持的参数类型 {kind}')
                size = struct.calcsize('<' + code)
                if position + size > len(data):
                    raise ValueError(f'{location}: 参数截断')
                value, = struct.unpack_from('<' + code, data, position)
                position += size
                if kind == 'C':
                    # thtk 的颜色值按字节顺序输出，而普通整数按小端解释。
                    value = int.from_bytes(data[position - size:position], 'big')
                if kind == 'o':
                    target = ins['offset'] + value
                    value = sum(offset < target for offset in offsets)
                    if value == len(offsets):
                        raise ValueError(f'{location}: 筛选后的跳转目标超出子程序')
                if not math.isfinite(value):
                    raise ValueError(f'{location}: 参数必须有限')
                args.append(value)
            if position != len(data) or len(args) > 16:
                raise ValueError(f'{location}: 参数长度与格式不符')
            output.append((ins['frame'], ins['opcode'], args, ins['offset'],
                           ins['rank_mask'], ins['parameter_mask'], ins.get('target_offset', 0)))
        programs.append(output)
    return ecl['sha256'], programs


def write_stage_programs(all_programs, output, sha256, stage, count):
    """输出指定关卡的独立指令及源偏移；只供测试，不链接进生产引擎。"""
    with output.open('w', encoding='utf-8') as f:
        extra = stage == 7
        identity = 'Extra' if extra else f'第 {stage} 面'
        scope = '单套' if extra else '四难度'
        f.write(f'// 本文件保存{identity} ECL {scope}测试指令、掩码和文件偏移，不供生产解释器使用。\n')
        f.write(f'// ecldata{stage}.ecl SHA-256: ' + sha256 + '\n')
        f.write('#pragma once\n#include <array>\n#include <cstdint>\n#include <span>\n')
        namespace = 'stage_extra' if extra else f'stage{stage}'
        f.write(f'namespace koumakyou::{namespace}::reference {{\n')
        f.write('/// @brief 帧为 60 Hz 局部帧；参数保留源单位，跳转已重定位至筛选后的指令索引。\n')
        f.write('/// @brief effectColor独立保存118的小端ARGB原始整数，避免args的float舍入破坏颜色。\n')
        color_field = ' std::uint32_t effectColor=0;'
        f.write('struct Instruction { int frame; int opcode; std::array<float,16> args; std::uint32_t offset; std::uint16_t rankMask; std::uint16_t parameterMask; std::uint32_t targetOffset;'+color_field+' };\n')
        for name, programs in all_programs.items():
            for n, program in enumerate(programs):
                f.write(f'inline constexpr Instruction k{name}Sub{n}[]{{\n')
                for frame, op, args, offset, rank, parameter, target in program:
                    a = ', '.join(str(float(v)) + 'F' for v in args)
                    # C参数在解析结果中按thtk显示字节序保存；重新按原字节组成EXE读取的小端ARGB。
                    color = f', {int.from_bytes(int(args[2]).to_bytes(4, "big"), "little")}U' if op == 118 else ''
                    f.write(f'    {{{frame}, {op}, {{{a}}}, {offset}, {rank}, {parameter}, {target}{color}}},\n')
                f.write('};\n')
            f.write(f'inline constexpr std::array<std::span<const Instruction>,{count}> k{name}Programs{{\n')
            for n in range(count):
                f.write(f'    k{name}Sub{n},\n')
            f.write('};\n')
        if extra:
            f.write('/// @brief Extra只保留一套源程序；四个掩码已逐指令核对相同，不提供虚构的难度选择。\n')
            f.write('inline std::span<const std::span<const Instruction>> Programs() { return kExtraPrograms; }\n}\n')
        else:
            f.write('/// @brief index 为 E/N/H/L 的 0…3，越界时抛出 std::out_of_range。\n')
            f.write('inline std::span<const std::span<const Instruction>> ProgramsForDifficulty(unsigned index) {\n')
            f.write('    return std::array{std::span{kEPrograms}, std::span{kNPrograms}, std::span{kHPrograms}, std::span{kLPrograms}}.at(index);\n}\n}\n')



def main():
    """按关卡校验源 SHA-256 和子程序数；输出只用于测试。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--stage', choices=('2', '3', '4', '5', '6', '7'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    identities = {'2': (49, '9a9aa3c3608195f1c814985706189ecf22c2ade1e302780acdf61a270a6267c0'),
                  '3': (61, '378e9fab04c794fa2573bbb407e6e2cb1c54dbdd372183d6e70c885315e1f171'),
                  '4': (119, '09744169b7ec3e6525e5cd2ff6e3c1c970cbcde4c8d0d772891aa38fc654bb09'),
                  '5': (75, '09b6a12c36069ac6caa320ca400ba70897f74ab6a7f04081076442cb5abc5276'),
                  '6': (87, 'bc8013a2461b1e93ecc36acca8f2a81d948e0561e82bcde4176a6268b6985dce'),
                  '7': (145, 'd6eae42d8d8eb669573b8004ba130f818374edc84ec35ccd0eda39eb6627a2eb')}
    count, expected_hash = identities[args.stage]
    programs = {}
    for difficulty in 'ENHL':
        sha256, programs[difficulty] = parse_binary_stage(args.source, difficulty, count)
    if sha256 != expected_hash:
        raise ValueError('关卡输入 SHA-256 与已审阅新典版本不符')
    if args.stage == '7':
        if any(programs[d] != programs['N'] for d in 'EHL'):
            raise ValueError('Extra四个掩码的源程序不同，须先重新审阅分支')
        programs = {'Extra': programs['N']}
    filename = 'stage_extra_reference_program.hpp' if args.stage == '7' else f'stage{args.stage}_reference_program.hpp'
    destination = args.output / filename
    if destination.resolve() == args.source.resolve():
        parser.error('输出路径不能覆盖输入文件')
    args.output.mkdir(parents=True, exist_ok=True)
    write_stage_programs(programs, destination, sha256, int(args.stage), count)
    print(' '.join(f'{d}: {sum(map(len, p))}' for d, p in programs.items()))


if __name__ == '__main__':
    main()

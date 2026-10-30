#!/usr/bin/env python3
# 本文件导入一面反编译文本或原始ECL，核对旧Normal并保留四难度来源/整数颜色，不读取游戏安装目录。
"""默认导入新典一面文本；--binary-all导入同版ECL并独立保存四难度精确来源记录。"""
import argparse
import math
import re
from pathlib import Path
from stage_reference import parse_binary_stage


SUB = re.compile(r'^sub Sub(\d+)\(\)\s*(\{)?\s*$')
INSTRUCTION = re.compile(r'^ins_(\d+)\((.*)\);$')
MARKER = re.compile(r'^!([ENHL*]+)\s*(.*)$')
LABEL = re.compile(r'^[A-Za-z_][A-Za-z_0-9]*:$')


def parse_programs(text, difficulty='N', source='<input>'):
    """筛选四难度之一，返回 38 个子程序的 (帧, 操作码, 数值参数)；错误含源行号。

    难度标记持续至下一标记或子程序结束；标签在筛选后的当前位置解析，因此跳转到
    已过滤的指令时落在其后第一条保留指令。时间为子程序局部的 60 Hz 内容帧。
    """
    if difficulty not in 'ENHL' or len(difficulty) != 1:
        raise ValueError('难度须为 E/N/H/L 之一')
    subs, body, labels, enabled, frame = [], None, None, True, 0
    awaiting_brace = False
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        where = f'{source}:{lineno}'
        match = SUB.fullmatch(line)
        if match:
            if body is not None or int(match[1]) != len(subs):
                raise ValueError(f'{where}: 子程序必须从 Sub0 连续编号且不能嵌套')
            body, labels, enabled, frame = [], {}, True, 0
            awaiting_brace = match[2] is None
            continue
        if awaiting_brace:
            if line != '{':
                raise ValueError(f'{where}: 子程序缺少开始括号')
            awaiting_brace = False
            continue
        if body is None:
            continue
        if line == '}':
            for index, (at, op, args, argline) in enumerate(body):
                values = []
                for arg in args:
                    if arg in labels:
                        value = labels[arg]
                    elif re.fullmatch(r'"Sub\d+"', arg):
                        value = int(arg[4:-1])
                    elif arg.startswith('"') and arg.endswith('"'):
                        value = 0
                    elif re.fullmatch(r'#[0-9a-fA-F]+', arg):
                        value = int(arg[1:], 16)
                    else:
                        try:
                            value = float(arg.removesuffix('f'))
                        except ValueError as exc:
                            raise ValueError(f'{source}:{argline}: 未知参数或标签 {arg}') from exc
                    if not math.isfinite(value):
                        raise ValueError(f'{source}:{argline}: 参数必须有限')
                    values.append(value)
                body[index] = (at, op, values)
            subs.append(body)
            body = None
            continue
        if not line or line.startswith('//'):
            continue
        if line.startswith('+'):
            match = re.fullmatch(r'\+\d+:\s*//\s*(\d+)\s*', line)
            if not match:
                raise ValueError(f'{where}: 无效的帧标记')
            frame = int(match[1])
            continue
        if LABEL.fullmatch(line):
            name = line[:-1]
            if name in labels:
                raise ValueError(f'{where}: 标签重复 {name}')
            labels[name] = len(body)
            continue
        if line.startswith('!'):
            match = MARKER.fullmatch(line)
            if not match:
                raise ValueError(f'{where}: 无效的难度标记')
            enabled = difficulty in match[1] or '*' in match[1]
            line = match[2]
            if not line:
                continue
        match = INSTRUCTION.fullmatch(line)
        if not match:
            raise ValueError(f'{where}: 无法识别的子程序内容 {line}')
        args = [arg.strip() for arg in match[2].split(',')] if match[2] else []
        if len(args) > 16 or any(not arg for arg in args):
            raise ValueError(f'{where}: 指令参数无效或超过 16 项')
        if enabled:
            body.append((frame, int(match[1]), args, lineno))
    if body is not None:
        raise ValueError(f'{source}: 子程序缺少结束括号')
    if len(subs) != 38:
        raise ValueError(f'{source}: 输入不是预期的新典一面脚本（38 个子程序）')
    return subs


def write_programs(subs, output, difficulty='N'):
    """写入单难度 C++ 基准；调用方负责避免与其他难度输出目录冲突。"""
    output.mkdir(parents=True, exist_ok=True)
    name = {'E': 'Easy', 'N': 'Normal', 'H': 'Hard', 'L': 'Lunatic'}[difficulty]
    with (output / 'reference_program.hpp').open('w', encoding='utf-8') as f:
        f.write(f'// 本文件保存一面 {name} 差分测试指令；仅供旧解释器基准使用，时间为 60 Hz 帧，角度为弧度。\n// 跳转目标为同一子程序的指令索引，负数 -10001…-10012 为上下文寄存器。\n#pragma once\n#include <array>\n#include <span>\nnamespace koumakyou::stage1::reference {{\n/// @brief 一条借用的关卡指令；参数保留原作像素、帧及弧度单位，最多 16 项。\nstruct Instruction {{ int frame; int opcode; std::array<float,16> args; }};\n')
        for n, instructions in enumerate(subs):
            f.write('inline constexpr Instruction kSub%d[]{\n' % n)
            for frame, op, args in instructions:
                a = ', '.join(str(float(v)) + 'F' for v in args)
                f.write('    {%d, %d, {%s}},\n' % (frame, op, a))
            f.write('};\n')
        f.write('inline constexpr std::array<std::span<const Instruction>,38> kPrograms{\n')
        for n in range(38):
            f.write('    kSub%d,\n' % n)
        f.write('};\n}\n')



def parse_binary_programs(path, difficulty):
    """一面专用入口保留 38 子程序身份校验。"""
    return parse_binary_stage(path, difficulty, 38)


def verify_normal_baseline(programs, baseline):
    """逐项比较现存 Normal 基准的帧、操作码及 float32 位模式，不依赖生产配方。"""
    import struct
    text = baseline.read_text(encoding='utf-8')
    matches = re.findall(r'inline constexpr Instruction kSub(\d+)\[\]\{(.*?)\n\};', text, re.S)
    if [int(number) for number, _ in matches] != list(range(38)):
        raise ValueError(f'{baseline}: Normal 基准子程序编号不连续')
    for number, body in matches:
        sub = int(number)
        rows = re.findall(r'\{(\d+), (\d+), \{(.*?)\}\}', body)
        if sub >= 38 or len(rows) != len(programs[sub]):
            raise ValueError(f'{baseline}: Sub{sub} 指令数量不一致')
        for index, ((frame, op, args), actual) in enumerate(zip(rows, programs[sub])):
            expected = [float(v.removesuffix('F')) for v in args.split(', ') if v]
            if (int(frame), int(op)) != actual[:2] or len(expected) != len(actual[2]) or any(
                    struct.pack('<f', left) != struct.pack('<f', right)
                    for left, right in zip(expected, actual[2])):
                raise ValueError(f'{baseline}: Sub{sub} 指令 {index} 与 ECL 不一致')


def write_binary_programs(all_programs, output, sha256):
    """写四难度并行指令与来源记录；Normal 继续复用现存参考表以保持兼容。"""
    with output.open('w', encoding='utf-8') as f:
        f.write('// 本文件保存原始一面 ECL 的四难度差分指令及文件偏移，仅供测试。\n')
        f.write('// 输入 ecldata1.ecl SHA-256: ' + sha256 + '\n')
        f.write('#pragma once\n#include "reference_program.hpp"\n#include <cstdint>\n')
        f.write('namespace koumakyou::stage1::reference {\n')
        f.write('/// @brief 源偏移、难度/参数掩码与跳转目标；effectColor独立保存118的小端ARGB，不能从float参数还原。\n')
        f.write('struct SourceLocation { std::uint32_t offset; std::uint16_t rankMask; std::uint16_t parameterMask; std::uint32_t targetOffset; std::uint32_t effectColor=0; };\n')
        for name, programs in all_programs.items():
            for n, program in enumerate(programs):
                if name != 'N':
                    f.write(f'inline constexpr Instruction k{name}Sub{n}[]{{\n')
                    for frame, op, args, *_ in program:
                        a = ', '.join(str(float(v)) + 'F' for v in args)
                        f.write(f'    {{{frame}, {op}, {{{a}}}}},\n')
                    f.write('};\n')
                f.write(f'inline constexpr SourceLocation k{name}Source{n}[]{{\n')
                for _, op, args, offset, rank, parameter, target in program:
                    color = f', {int.from_bytes(int(args[2]).to_bytes(4, "big"), "little")}U' if op == 118 else ''
                    f.write(f'    {{{offset}, {rank}, {parameter}, {target}{color}}},\n')
                f.write('};\n')
            if name != 'N':
                f.write(f'inline constexpr std::array<std::span<const Instruction>,38> k{name}Programs{{\n')
                for n in range(38):
                    f.write(f'    k{name}Sub{n},\n')
                f.write('};\n')
            f.write(f'inline constexpr std::array<std::span<const SourceLocation>,38> k{name}Sources{{\n')
            for n in range(38):
                f.write(f'    k{name}Source{n},\n')
            f.write('};\n')
        f.write('/// @brief difficultyIndex 为 E/N/H/L 的 0…3；越界时抛出 std::out_of_range。\n')
        f.write('inline std::span<const std::span<const Instruction>> ProgramsForDifficulty(unsigned difficultyIndex) {\n')
        f.write('    return std::array{std::span{kEPrograms}, std::span{kPrograms}, std::span{kHPrograms}, std::span{kLPrograms}}.at(difficultyIndex);\n}\n')
        f.write('/// @brief 来源记录与 ProgramsForDifficulty 的子程序和指令索引一一对应。\n')
        f.write('inline std::span<const std::span<const SourceLocation>> SourcesForDifficulty(unsigned difficultyIndex) {\n')
        f.write('    return std::array{std::span{kESources}, std::span{kNSources}, std::span{kHSources}, std::span{kLSources}}.at(difficultyIndex);\n}\n}\n')


def main():
    """先完整校验输入，再写单个难度夹具；默认路径与 Normal 行为保持兼容。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='thecl -d 6 输出的文本')
    parser.add_argument('--output', type=Path, default=Path('tests'), help='参考指令输出目录，默认 tests')
    parser.add_argument('--difficulty', choices='ENHL', default='N')
    parser.add_argument('--binary-all', action='store_true', help='读取一面 ECL，生成四难度参考头')
    args = parser.parse_args()
    if args.binary_all:
        all_programs = {}
        for difficulty in 'ENHL':
            sha256, all_programs[difficulty] = parse_binary_programs(args.source, difficulty)
        verify_normal_baseline(all_programs['N'], Path(__file__).resolve().parents[1] / 'tests/reference_program.hpp')
        args.output.mkdir(parents=True, exist_ok=True)
        destination = args.output / 'reference_program_difficulties.hpp'
        if destination.resolve() == args.source.resolve():
            parser.error('输出路径不能覆盖输入文件')
        write_binary_programs(all_programs, destination, sha256)
        print(' '.join(f'{d}: {sum(map(len, p))}' for d, p in all_programs.items()))
        return
    subs = parse_programs(args.source.read_text(encoding='utf-8'), args.difficulty, str(args.source))
    if (args.output / 'reference_program.hpp').resolve() == args.source.resolve():
        parser.error('输出路径不能覆盖输入文件')
    write_programs(subs, args.output, args.difficulty)
    print(f'{args.difficulty} instructions {sum(map(len, subs))}')


if __name__ == '__main__':
    main()

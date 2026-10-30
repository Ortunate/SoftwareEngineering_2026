#!/usr/bin/env python3
# 本文件只读审阅新典 ECL 的结构、难度入口与保守可达关系，不生成生产时间轴或执行关卡脚本。
"""输出移植前的逐关事实；静态可达集合不能代替实际执行覆盖或可玩性验收。

格式依据：https://github.com/thpatch/thtk/blob/master/thecl/thecl06.c
只提取已知的结构字段；不从经典版指令格式推断新典扩展的执行语义。
"""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct


# 链接参数均为子程序编号；普通调用、条件调用、使魔与回调分别保留，不能混算出生次数。
LINKS = {35: 'call', **dict.fromkeys(range(37, 43), 'conditional_call'),
         95: 'child_spawn', 108: 'death_callback', 109: 'interrupt_callback',
         114: 'health_callback', 116: 'timeout_callback'}
SPAWN_FORMATS = {**dict.fromkeys((0, 2, 4, 6), '<3fhhI'),
                 **dict.fromkeys((1, 3, 5, 7), '<3f')}
JUMPS = {2, 3, 29, 30, 31, 32, 33, 34}
LINK_SIZES = {35: 12, **dict.fromkeys(range(37, 43), 20),
              95: 24, 108: 4, 109: 8, 114: 4, 116: 4}


def unpack(data, fmt, offset, end):
    """读取限定区段内的小端结构；拒绝负偏移、截断和跨子程序读取。"""
    if not 0 <= offset <= end <= len(data) or offset + struct.calcsize(fmt) > end:
        raise ValueError(f'结构越界：{offset:#x} {fmt}')
    return struct.unpack_from(fmt, data, offset)


def read_ecl(path):
    """读取 TH06 新典布局，返回保留原始偏移/掩码的记录；未知指令只保留字节，不推测参数语义。

    子程序时间与时间轴时间均为 60 Hz 帧，但基准不同。所有出生数量都是静态记录数，
    不执行难度分支、循环、回调或引擎扩展；timeline 的出生参数仍使用原始坐标及 HP。
    """
    data = path.read_bytes()
    count, variant = unpack(data, '<HH', 0, len(data))
    if not 0 < count <= 4096 or variant != 0:
        raise ValueError('输入不是受支持的 TH06 新典 ECL 布局')
    timelines = unpack(data, '<3I', 4, len(data))
    subs = unpack(data, f'<{count}I', 16, len(data))
    header_end = 16 + 4 * count
    starts = [*subs, *(offset for offset in timelines if offset)]
    if len(set(starts)) != len(starts) or any(not header_end <= p < len(data) for p in starts):
        raise ValueError('区段偏移重复、越界或落入文件头')
    ordered = sorted(starts)
    ends = dict(zip(ordered, [*ordered[1:], len(data)]))
    programs = []
    for sub, start in enumerate(subs):
        instructions = []
        at, end = start, ends[start]
        while at < end:
            frame, opcode, size, rank, mask = unpack(data, '<iHHHH', at, end)
            if size < 12 or at + size > end:
                raise ValueError(f'Sub{sub} 指令长度无效：{at:#x}')
            if frame == -1:
                if opcode != 65535 or size != 12:
                    raise ValueError(f'Sub{sub} 终止标记无效')
                break
            if frame < 0:
                raise ValueError(f'Sub{sub} 帧号为负')
            instruction = {'index': len(instructions), 'offset': at, 'frame': frame,
                           'opcode': opcode, 'rank_mask': rank, 'parameter_mask': mask,
                           'payload': data[at + 12:at + size].hex()}
            if opcode in LINKS:
                if size != 12 + LINK_SIZES[opcode]:
                    raise ValueError(f'Sub{sub} 链接参数长度无效')
                target, = unpack(data, '<i', at + 12, at + size)
                if not -1 <= target < count:
                    raise ValueError(f'Sub{sub} 引用不存在的子程序 {target}')
                instruction.update(link_kind=LINKS[opcode], target_sub=target)
            if opcode in JUMPS:
                if size != (24 if opcode == 3 else 20):
                    raise ValueError(f'Sub{sub} 跳转参数长度无效')
                target_frame, relative = unpack(data, '<ii', at + 12, at + size)
                instruction.update(target_frame=target_frame, target_offset=at + relative)
            instructions.append(instruction)
            at += size
        else:
            raise ValueError(f'Sub{sub} 缺少终止标记')
        # 跳转偏移相对当前指令，不是子程序起点；即使目标难度被过滤也必须先验证原始边界。
        offsets = {instruction['offset'] for instruction in instructions}
        if any(i.get('target_offset', i['offset']) not in offsets for i in instructions):
            raise ValueError(f'Sub{sub} 跳转未指向指令起点')
        programs.append(instructions)
    tracks = []
    for track, start in enumerate(timelines):
        if not start:
            continue
        records = []
        at, end = start, ends[start]
        while at < end:
            frame, arg = unpack(data, '<hh', at, end)
            if frame == -1:
                break
            opcode, size = unpack(data, '<HH', at + 4, end)
            if frame < 0 or size < 8 or at + size > end:
                raise ValueError(f'Timeline{track} 指令长度或帧号无效：{at:#x}')
            record = {'offset': at, 'frame': frame, 'opcode': opcode, 'argument': arg,
                      'payload': data[at + 8:at + size].hex()}
            if opcode in SPAWN_FORMATS:
                fmt = SPAWN_FORMATS[opcode]
                if size != 8 + struct.calcsize(fmt) or not 0 <= arg < count:
                    raise ValueError('出生结构或子程序编号无效')
                values = unpack(data, fmt, at + 8, at + size)
                if not all(math.isfinite(value) for value in values[:3]):
                    raise ValueError('出生位置含非有限数')
                record.update(target_sub=arg, position=values[:3],
                              health=values[3] if len(values) > 3 else None,
                              item=values[4] if len(values) > 3 else None,
                              score=values[5] if len(values) > 3 else None)
            records.append(record)
            at += size
        else:
            raise ValueError(f'Timeline{track} 缺少终止标记')
        tracks.append({'index': track, 'records': records})
    return {'file': path.name, 'sha256': hashlib.sha256(data).hexdigest(),
            'programs': programs, 'timelines': tracks}


def inspect(ecl, difficulty=0x0200):
    """从全部时间轴出生入口求调用/回调/使魔的保守闭包；difficulty 必须是单个已知难度位。

    不做条件求值，因此集合可能包含指定角色不会进入的分支，也可能包含被早期返回遮住的指令。
    不给未知操作码、原生扩展、动态出生或运行时完成状态赋予默认语义。
    """
    if difficulty not in (0x0100, 0x0200, 0x0400, 0x0800):
        raise ValueError('难度须为单个 E/N/H/L 位')
    programs = [[i for i in p if i['rank_mask'] & difficulty] for p in ecl['programs']]
    roots = {r['target_sub'] for t in ecl['timelines'] for r in t['records'] if 'target_sub' in r}
    reached, pending = set(), list(roots)
    while pending:
        sub = pending.pop()
        if sub in reached:
            continue
        reached.add(sub)
        pending.extend(i['target_sub'] for i in programs[sub] if i.get('target_sub', -1) >= 0)
    operations = Counter(i['opcode'] for sub in reached for i in programs[sub])
    extensions = []
    for sub in sorted(reached):
        for instruction in programs[sub]:
            if instruction['opcode'] in (121, 122) or instruction['opcode'] > 135:
                extensions.append({'sub': sub, **instruction})
    return {'file': ecl['file'], 'sha256': ecl['sha256'], 'difficulty_mask': difficulty,
            'sub_count': len(programs), 'timeline_spawn_records': sum(
                'target_sub' in r for t in ecl['timelines'] for r in t['records']),
            'roots': sorted(roots), 'conservative_reachable_subs': sorted(reached),
            'reachable_instruction_records': sum(len(programs[sub]) for sub in reached),
            'opcode_counts': dict(sorted(operations.items())), 'native_extensions': extensions}


def main():
    """审阅显式指定的解包文件；输出只写入 --output，不修改输入或任何运行时源码。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--difficulty', choices=('E', 'N', 'H', 'L', 'all'), default='N',
                        help='选择单一难度或 all；默认 N，all 在同一清单输出四项')
    args = parser.parse_args()
    # 全部输入先通过校验，避免后一个损坏文件使本次报告混入上次遗留的完整性假象。
    sources = [read_ecl(path) for path in args.inputs]
    if len({path.stem for path in args.inputs}) != len(args.inputs):
        parser.error('输入文件名重复，会覆盖审阅结果')
    inputs = {path.resolve() for path in args.inputs}
    outputs = [*(args.output / f'{path.stem}.json' for path in args.inputs),
               args.output / 'coverage.json']
    if any(path.resolve() in inputs for path in outputs):
        parser.error('输出路径不能覆盖输入文件')
    args.output.mkdir(parents=True, exist_ok=True)
    summaries = []
    for path, ecl in zip(args.inputs, sources):
        difficulties = 'ENHL' if args.difficulty == 'all' else args.difficulty
        results = [inspect(ecl, 0x100 << 'ENHL'.index(difficulty)) for difficulty in difficulties]
        (args.output / f'{path.stem}.json').write_text(
            json.dumps(ecl, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        summaries.extend(results)
        for difficulty, summary in zip(difficulties, results):
            print(f"{path.name} {difficulty}: {summary['timeline_spawn_records']} 条出生记录，"
                  f"{len(summary['conservative_reachable_subs'])} 个保守可达子程序")
    (args.output / 'coverage.json').write_text(
        json.dumps(summaries, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()

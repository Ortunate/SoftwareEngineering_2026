#!/usr/bin/env python3
# 本文件独立审阅Extra时间轴与MSG的战后条件、结算出口和后继演出，不生成生产编排。
"""输入须为已核对的新典ECL/MSG/EXE；模式身份及有符号净Miss规则由EXE证据工具另行核验。"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from export_native_combat_data import PeImage
from inspect_native_difficulty import inspect as inspect_native
from inspect_stage_scripts import read_ecl

ECL_SHA256 = 'd6eae42d8d8eb669573b8004ba130f818374edc84ec35ccd0eda39eb6627a2eb'
MSG_SHA256 = '5232a8c0bd6db8ca8346640fbbffeee710830014887221fe8354d218ee3c6ab9'
FIXED_SIZES = {0: 0, 1: 4, 2: 4, 4: 4, 5: 4, 6: 0, 7: 4,
               9: 4, 10: 0, 11: 0, 12: 0, 13: 4, 14: 0}


def read_msg_branch(data, block):
    """只读一个MSG分支至首个0；9不是结束，保留10/11之后不可达的静态记录供审阅。"""
    if len(data) < 4:
        raise ValueError('MSG分支表截断')
    count, = struct.unpack_from('<I', data)
    if not 0 < count <= 128 or len(data) < 4 + 4 * count or not 0 <= block < count:
        raise ValueError('MSG分支表无效')
    offsets = struct.unpack_from(f'<{count}I', data, 4)
    if any(at < 4 + 4 * count or at >= len(data) for at in offsets):
        raise ValueError('MSG分支偏移越界')
    at = offsets[block]
    end = min([offset for offset in offsets if offset > at] + [len(data)])
    records = []
    previous_frame = -1
    while at + 4 <= end:
        frame, opcode, size = struct.unpack_from('<HBB', data, at)
        if at + 4 + size > end or frame < previous_frame:
            raise ValueError('MSG参数截断或帧号倒退')
        payload = data[at + 4:at + 4 + size]
        record = {'offset': at, 'frame': frame, 'opcode': opcode}
        if opcode in (3, 8):
            if size < 5 or b'\0' not in payload[4:]:
                raise ValueError('MSG文本载荷无效')
            slot, row = struct.unpack_from('<hh', payload)
            try:
                key = payload[4:].split(b'\0', 1)[0].decode('ascii')
            except UnicodeDecodeError as error:
                raise ValueError('MSG文本键不是ASCII') from error
            record.update(slot=slot, row=row, key=key)
        elif opcode not in FIXED_SIZES:
            raise ValueError(f'未核对的MSG操作码{opcode}')
        elif size != FIXED_SIZES[opcode]:
            raise ValueError(f'MSG操作码{opcode}参数长度无效')
        elif size:
            if opcode in (1, 2, 5):
                slot, value = struct.unpack('<hh', payload)
                record.update(slot=slot, value=value)
            else:
                value, = struct.unpack('<i', payload)
                if opcode == 4 and value < 0:
                    raise ValueError('MSG等待帧数不能为负')
                record['value'] = value
        records.append(record)
        if opcode == 0:
            return records
        previous_frame = frame
        at += 4 + size
    raise ValueError('MSG分支缺少结束指令')


def passes_postfight_gate(captures, mode_flag, mode_counter):
    """复述MSG14原生谓词；captures为53cabc，mode_flag为53d414的Challenge模式。

    mode_counter为53cab0有符号净Miss：确认受击加一，Challenge拾取残机减一；不代替运行时规则。
    """
    return captures >= 6 and (not mode_flag or mode_counter <= 2)


def trace_branch(records, captures, mode_flag, mode_counter):
    """按60Hz源内容钟展开无输入路径，返回实际出口与已执行记录；wait冻结内容钟。

    MSG11会切换游戏状态，10不推进PC；二者之后的静态记录不能当作实际执行。
    本模型只用于流程审阅，不模拟ANM、音乐、输入或其他管理器的后续帧。
    """
    pc = clock = wait = 0
    events = []
    seen_waits = set()
    for wall in range(60 * 300):
        for _ in range(1024):
            if pc >= len(records):
                raise ValueError('MSG执行超出分支')
            record = records[pc]
            if record['frame'] > clock:
                clock += 1
                break
            opcode = record['opcode']
            if opcode == 4:
                if pc not in seen_waits:
                    events.append(dict(record, wall_frame=wall, initial_wait=wait))
                    seen_waits.add(pc)
                if wait < record['value']:
                    wait += 1
                    break
            else:
                events.append(dict(record, wall_frame=wall))
            reason = None
            if opcode == 0:
                reason = 'dialogue_end'
            elif opcode == 10:
                reason = 'nonadvancing_barrier'
            elif opcode == 11:
                reason = 'game_state_transition'
            elif opcode == 14 and passes_postfight_gate(captures, mode_flag, mode_counter):
                reason = 'postfight_gate'
            if reason:
                return {'exit': reason, 'wall_frame': wall, 'events': events}
            if opcode == 3:
                wait = 0
            pc += 1
        else:
            raise ValueError('MSG同帧无进展循环')
    raise ValueError('MSG执行超过五分钟')


def inspect_timeline(ecl):
    """保留ECL记录顺序并区分加载标记、运行分派和静态后继；不将出生闭包当可达性证明。"""
    if len(ecl['timelines']) != 1:
        raise ValueError('Extra应只有一条时间轴')
    records = ecl['timelines'][0]['records']
    markers = [record for record in records if record['opcode'] == 13]
    ignored = [record for record in records if record['opcode'] > 12]
    return {'record_count': len(records), 'jump_marker': markers[0] if markers else None,
            'ignored_runtime_records': ignored,
            'postfight_records': [record for record in records if record['frame'] >= 8493]}


def inspect_sources(ecl, msg, require_hash=True):
    """返回静态与六类门槛路径；默认严格核对输入身份，测试可关闭哈希但仍校验所有结构。"""
    digest = hashlib.sha256(msg).hexdigest()
    if require_hash and (ecl['sha256'] != ECL_SHA256 or digest != MSG_SHA256):
        raise ValueError('Extra源文件哈希与已核对版本不符')
    count, = struct.unpack_from('<I', msg) if len(msg) >= 4 else (0,)
    if count != 16:
        raise ValueError('Extra应有16个MSG分支')
    branches = []
    for block in range(count):
        records = read_msg_branch(msg, block)
        cases = []
        for captures, mode_flag, counter in ((5, False, 0), (6, False, 99),
                                             (6, True, 2), (6, True, 3),
                                             (10, True, 2), (10, True, 3)):
            cases.append({'captures': captures, 'mode_flag': mode_flag, 'mode_counter': counter,
                          **trace_branch(records, captures, mode_flag, counter)})
        branches.append({'block': block, 'static_records': records, 'cases': cases})
    return {'ecl_sha256': ecl['sha256'], 'msg_sha256': digest,
            'timeline': inspect_timeline(ecl), 'branches': branches,
            'unresolved': ['Challenge净Miss与取得计数的正式运行时接入', '正式条件后继、结算与入口']}


def main():
    """先核验EXE身份和控制流锚点，再只读审阅ECL/MSG；报告不进入运行包。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ecl', type=Path)
    parser.add_argument('msg', type=Path)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    native = inspect_native(PeImage(args.exe))
    report = inspect_sources(read_ecl(args.ecl), args.msg.read_bytes())
    report['native_gate'] = native['extra_postfight_gate']
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    for branch in report['branches']:
        if branch['block'] in (1, 11):
            print(f"MSG{branch['block']}: " + ', '.join(case['exit'] for case in branch['cases']))


if __name__ == '__main__':
    main()

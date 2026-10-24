#!/usr/bin/env python3
# 本文件只读展开新典 msg1 的四个自机对话分支，输出中文原文、差分和实际播放帧的审阅 JSON。
import argparse
import hashlib
import json
from pathlib import Path
import struct


def inspect_branch(data: bytes, translation: dict, block: int):
    """展开一个无输入的 60 Hz MSG 分支；wait 冻结脚本时钟，直到计数满足，不重置相邻 wait。"""
    count = struct.unpack_from('<I', data)[0]
    if count > 128 or not 0 <= block < count:
        raise ValueError('无效 MSG 分支表')
    offsets = struct.unpack_from('<' + 'I' * count, data, 4)
    pc = offsets[block]
    end = min([offset for offset in offsets if offset > pc] + [len(data)])
    clock = wait = 0
    events = []
    recorded_waits = set()
    for wall in range(60 * 300):
        budget = 1024
        while True:
            budget -= 1
            if budget <= 0 or pc < 4 + 4 * count or pc + 4 > end:
                raise ValueError('MSG 指令越界或无进展循环')
            frame, opcode, size = struct.unpack_from('<HBB', data, pc)
            if pc + 4 + size > end:
                raise ValueError('截断的 MSG 参数')
            if frame > clock:
                clock += 1
                break
            args = data[pc + 4:pc + 4 + size]
            event = {'frame': wall, 'script_frame': frame, 'opcode': opcode}
            if opcode == 4:
                maximum, = struct.unpack('<i', args)
                if pc not in recorded_waits:
                    event.update(value=maximum, initial_counter=wait)
                    events.append(event)
                    recorded_waits.add(pc)
                if wait < maximum:
                    wait += 1
                    break
            elif opcode in (3, 8):
                slot, row = struct.unpack_from('<hh', args)
                key = args[4:].split(b'\0')[0].decode('ascii')
                text = translation[key]
                event.update(slot=slot, row=row, key=key,
                             text=text['zh-CN'] if isinstance(text, dict) else text)
                events.append(event)
                if opcode == 3:
                    wait = 0
            elif opcode in (1, 2, 5):
                slot, value = struct.unpack('<hh', args)
                event.update(slot=slot, value=value)
                events.append(event)
            elif opcode in (0, 6, 7, 9, 13):
                if args:
                    event['value'], = struct.unpack('<i', args)
                events.append(event)
                if opcode in (0, 9):
                    return {'block': block, 'end_frame': wall, 'events': events}
            else:
                raise ValueError(f'未核对的 MSG 操作码 {opcode}')
            pc += 4 + size
    raise ValueError('MSG 对话超过五分钟')


def main():
    """读取已解包 MSG 与中文翻译 JSON；输出到指定文件，输入文件及游戏安装目录不发生写入。"""
    parser = argparse.ArgumentParser(description='只读审阅新典一面对话')
    parser.add_argument('msg', type=Path)
    parser.add_argument('localization', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    data = args.msg.read_bytes()
    translation = json.loads(args.localization.read_text(encoding='utf-8'))
    result = {
        'msg_sha256': hashlib.sha256(data).hexdigest(),
        'frames_per_second': 60,
        'branches': [inspect_branch(data, translation, block) for block in (0, 1, 10, 11)],
        'portrait_indices': {
            'reimu': ['face00a_0', 'face00a_1', 'face00b_0', 'face00b_1', 'face00c_0', 'face00c_1'],
            'marisa': ['face01a_0', 'face01a_1', 'face01b_0', 'face01b_1', 'face01c_0', 'face01c_1'],
            'rumia': ['face03a_0', 'face03a_1', 'face03b_0', 'face03b_1'],
        },
    }
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'一面对话四个分支已审阅，SHA-256 {result["msg_sha256"]}')


if __name__ == '__main__':
    main()

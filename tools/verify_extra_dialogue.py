#!/usr/bin/env python3
# 本文件独立核对Extra静态对白的逐行写入、中文原文、演出顺序与结算/退出边界，不生成生产表。
"""读取已核对MSG、官方本地化JSON与静态dialogue.cpp；不执行生产C++或进入运行包。"""
import argparse
import hashlib
import json
from pathlib import Path
import re

from inspect_extra_flow import MSG_SHA256, read_msg_branch, trace_branch

SECTIONS = ('MainBefore', 'MainAfter', 'Midboss', 'EncoreBefore', 'EncoreAfter', 'EncoreInterlude')
CUE_KINDS = {1: 'PortraitStart', 2: 'Expression', 5: 'Portrait', 6: 'StageSignal', 7: 'StageMusic', 13: 'InputEnabled'}


def fields(row):
    """仅识别本静态表使用的字符串/布尔/整数/整数帧除60字面量，拒绝表达式或可执行代码。"""
    result = []
    for match in re.finditer(r'"(?:\\.|[^"\\])*"|[^,]+', row):
        token = match.group().strip()
        if token.startswith('"'):
            result.append(json.loads(token))
        elif token in ('true', 'false'):
            result.append(token == 'true')
        else:
            value = re.fullmatch(r'(-?\d+)(?:\.0F/60)?', token)
            if value is None:
                raise ValueError(f'非静态对白字面量：{token}')
            result.append(int(value[1]))
    return result


def read_table(source, name):
    """读取一段命名静态表；返回的时间数值均为内容帧，未调用生产接口反推期望。"""
    def array(kind, suffix):
        pattern = rf'constexpr std::array<{kind},(\d+)> {name}{suffix}\{{\{{(.*?)\}}\}};'
        matches = list(re.finditer(pattern, source, re.S))
        if len(matches) != 1:
            raise ValueError(f'对白表缺失或重复：{name}{suffix}')
        match = matches[0]
        rows = re.findall(r'^\s*\{(.*?)\},\s*$', match[2], re.M)
        if len(rows) != int(match[1]):
            raise ValueError('对白静态数组长度不匹配')
        return rows
    lines = [fields(row) for row in array('DialogueLine', 'Lines')]
    if any(len(line) != 15 for line in lines):
        raise ValueError('对白字段数量不匹配')
    cues = []
    for row in array('DialogueCue', 'Cues'):
        match = re.fullmatch(r'(\d+),DialogueCueKind::(\w+),(-?\d+),(-?\d+)', row)
        if not match:
            raise ValueError('演出字段无效')
        cues.append((int(match[1]), match[2], int(match[3]), int(match[4])))
    script = re.search(rf'constexpr DialogueScript {name}\{{{name}Lines,{name}Cues,(\d+),(-?\d+)\}};', source)
    if script is None:
        raise ValueError('对白结束/结算边界缺失')
    return {'lines': lines, 'cues': cues, 'end': int(script[1]), 'clear': int(script[2])}


def verify_table(table, report, translations, empty_interlude=False):
    """逐条比较写入事件，包含延迟第二行/空字清除；期望来自独立MSG时钟而非生产分组过程。"""
    expected_text = []
    expected_cues = []
    introductions = []
    signal = 0
    clear = -1
    for event in report['events']:
        opcode = event['opcode']
        frame = event['wall_frame']
        if opcode == 3:
            text = translations[event['key']]['zh-CN'] if event['key'] else ''
            expected_text.append((frame, event['slot'], event['row'], text))
        elif opcode == 8 and event['row'] == 0:
            introductions.append(frame)
        elif opcode == 9:
            clear = frame
        if opcode in CUE_KINDS:
            value = event.get('value', 0)
            if opcode == 6:
                signal += 1
                value = signal
            expected_cues.append((frame, CUE_KINDS[opcode], event.get('slot', 0), value))
    actual_text = []
    actual_introductions = []
    start = 0
    for line in table['lines']:
        if line[3] <= 0 or line[9] < 0 or line[4] < 0 or line[11] < line[9] + line[4] + 8:
            raise ValueError('台词时长/写入/确认边界无效')
        actual_text.extend(((start + line[9], int(line[2]), 0, line[1]),
                            (start + line[9] + line[4], int(line[2]), 1, line[7])))
        if line[6]:
            actual_introductions.append(start + line[14])
        if line[12] >= 0 and not 0 <= line[12] <= line[3]:
            raise ValueError('末句确认跳转越过真实结束')
        start += line[3]
    if empty_interlude:
        if any(event[-1] for event in expected_text) or actual_text:
            raise ValueError('无台词演出含虚构台词')
    elif actual_text != expected_text or start != report['wall_frame']:
        raise ValueError('台词原文/槽位/逐行写入帧/时长与MSG不一致')
    if actual_introductions != introductions or table['cues'] != expected_cues:
        raise ValueError('演出顺序/时刻与MSG不一致')
    if (table['end'], table['clear']) != (report['wall_frame'], clear):
        raise ValueError('结算/退出边界与MSG不一致')
    return {'text_writes': len(expected_text), 'cues': len(expected_cues),
            'end_frame': table['end'], 'clear_frame': table['clear'], 'exit': report['exit']}


def verify(msg, translations, source):
    """严格核对源MSG身份，分别验证两角色六段对白；战后表明确验证未触发条件后继的路径。"""
    if hashlib.sha256(msg).hexdigest() != MSG_SHA256:
        raise ValueError('Extra MSG身份不匹配')
    result = {}
    for character, base in (('Reimu', 0), ('Marisa', 10)):
        for index, section in enumerate(SECTIONS):
            name = 'k' + character + section
            records = read_msg_branch(msg, base + index)
            report = trace_branch(records, 0, False, 0)
            result[name] = verify_table(read_table(source, name), report, translations, section == 'EncoreInterlude')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('msg', type=Path)
    parser.add_argument('localization', type=Path)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    result = verify(args.msg.read_bytes(), json.loads(args.localization.read_text()), args.source.read_text())
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()

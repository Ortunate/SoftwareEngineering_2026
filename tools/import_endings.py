#!/usr/bin/env python3
# 本文件只读核对新典结局分支并导入六条中文演出；在 staff00.end 入口结束，不导入制作人员演出。
import argparse
import hashlib
import json
from pathlib import Path
import re

from export_native_combat_data import PeImage


def read_localization(path):
    """读取本机只含字符串与映射的 MessagePack；截断或其他类型拒绝，避免静默丢失译文。"""
    data = path.read_bytes()
    offset = 0

    def take(size):
        nonlocal offset
        if size < 0 or offset + size > len(data):
            raise ValueError('翻译 MessagePack 截断')
        value = data[offset:offset + size]
        offset += size
        return value

    def read():
        tag = take(1)[0]
        if tag & 0xe0 == 0xa0:
            return take(tag & 31).decode('utf-8')
        if tag & 0xf0 == 0x80:
            return {read(): read() for _ in range(tag & 15)}
        if tag in (0xde, 0xdf):
            size = int.from_bytes(take(2 if tag == 0xde else 4), 'big')
            return {read(): read() for _ in range(size)}
        if tag in (0xd9, 0xda, 0xdb):
            size = int.from_bytes(take({0xd9: 1, 0xda: 2, 0xdb: 4}[tag]), 'big')
            return take(size).decode('utf-8')
        raise ValueError(f'未核对的 MessagePack 类型 {tag:#x}')

    result = read()
    if offset != len(data):
        raise ValueError('翻译 MessagePack 有多余内容')
    return result


def inspect_rules(exe):
    """在已核对 PE64 上验证难度、续关及挑战 Miss 5/6 边界，返回可审阅的来源证据。"""
    image = PeImage(exe)
    digest = hashlib.sha256(image.data).hexdigest()
    if digest != '48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82':
        raise ValueError('新典 EXE 版本未核对')
    witnesses = {
        0x2d3b7: '803d6ec95100007757',       # 续关次数 >0，走坏结局分支。
        0x2d3c0: '803d4d005100007409',       # 非挑战模式绕过 Miss 上限。
        0x2d3c9: '833de0f65000057f45',       # 挑战模式 Miss >5，5 次仍属于好结局。
        0x2d456: '833db3ff5000000f84f7010000',  # Easy 使用坏结局。
        0x6a22a: 'ff0580284d00',             # 玩家死亡结算递增该 Miss 计数。
    }
    for rva, raw in witnesses.items():
        expected = bytes.fromhex(raw)
        offset = image.offset(image.base + rva, len(expected))
        if offset is None or image.data[offset:offset + len(expected)] != expected:
            raise ValueError(f'结局分支证据 {rva:#x} 不符')
    return {'exe_sha256': digest, 'witnesses': {hex(k): v for k, v in witnesses.items()},
            'bad_if': 'difficulty == Easy || continues > 0 || (challenge && misses > 5)'}


def parse_script(data, localization):
    """展开原始 END 的有序操作；等待以 60 Hz 帧计，音乐淡出以秒计，文本保留官方 zh-CN 换行。"""
    events = []
    for raw in data.splitlines():
        if not raw or raw == b'\0':
            continue
        args = raw.rstrip(b'\0').split(b'\0')
        if raw.startswith(b'@'):
            opcode = chr(raw[1])
            value = args[0][2:].decode('ascii')
            if opcode == 'F':
                if value != 'data/staff00.end':
                    raise ValueError('未核对的结局跳转')
                events.append(('Finish', 0, 0, ''))
                return events
            if opcode == 'm':
                if value != 'data/bgm/th06_17.opus':
                    raise ValueError('未核对的结局音乐')
                continue
            if opcode == 'b':
                name = Path(value).stem
                if not re.fullmatch(r'end(?:0[0-9]|1[0-3]|00b)', name):
                    raise ValueError('未知结局背景')
                events.append(('Background', 14 if name == 'end00b' else int(name[3:]), 0, ''))
                continue
            names = {'s': 'TextDelay', 'w': 'Wait', 'r': 'WaitClear', 'c': 'Color',
                     '2': 'FadeIn', '3': 'FadeOut', 'M': 'MusicFade'}
            if opcode not in names:
                raise ValueError(f'未知 END 操作码 {opcode}')
            number = int(value)
            second = int(args[1]) if len(args) > 1 else 0
            if number < 0 or second < 0:
                raise ValueError('负数 END 参数')
            events.append((names[opcode], number, second, ''))
        else:
            key = args[0]
            if key.startswith(b'ED_'):
                text = localization[key.decode('ascii')]['zh-CN']
            else:
                text = key.decode('cp932')
                if text.strip():
                    raise ValueError('END 出现未知文本格式')
            events.append(('Text', 0, 0, text))
    raise ValueError('END 没有制作人员入口')


def main():
    """参数为只读 EXE、解包 END 目录和翻译 MessagePack；输出为显式指定的 C++ 内容及来源报告。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('source', type=Path)
    parser.add_argument('localization', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--runtime-root', type=Path, help='可选：核对已复制的背景和已转换的结局音乐')
    parser.add_argument('--source-music', type=Path, help='与 --runtime-root 一起提供原生 th06_17.opus')
    args = parser.parse_args()
    if bool(args.runtime_root) != bool(args.source_music):
        parser.error('--runtime-root 与 --source-music 必须一起提供')
    report = inspect_rules(args.exe)
    report['localization_sha256'] = hashlib.sha256(args.localization.read_bytes()).hexdigest()
    report['scripts'] = {}
    localization = read_localization(args.localization)
    source = ['// 本文件保存新典 END 的中文内容、背景、颜色与等待顺序；由 tools/import_endings.py 导入。',
              '// 等待为 60 Hz 内容帧；音乐淡出为秒；Finish 代替 staff00.end 跳转，不包含制作人员演出。']
    for name in ('end00', 'end01', 'end10', 'end11', 'end00b', 'end10b'):
        data = (args.source / (name + '.end')).read_bytes()
        events = parse_script(data, localization)
        source.append(f'static constexpr std::array<Event,{len(events)}> k{name.capitalize()}{{{{')
        source.extend('    {Op::' + op + f', {a}, {b}, ' + json.dumps(text, ensure_ascii=False) + '},'
                      for op, a, b, text in events)
        source.append('}};')
        report['scripts'][name] = {'sha256': hashlib.sha256(data).hexdigest(), 'events': len(events),
                                   'text_blocks': sum(op == 'Text' for op, _, _, _ in events)}
    if args.runtime_root:
        report['backgrounds'] = []
        for name in ['end00b'] + [f'end{i:02d}' for i in range(14)]:
            original = args.source / (name + '_4k.dds')
            runtime = 'endings/native/' + name + '.dds'
            data = original.read_bytes()
            if data != (args.runtime_root / runtime).read_bytes():
                raise ValueError(f'运行时背景与原生来源不符：{name}')
            report['backgrounds'].append({'source': original.name, 'runtime': runtime,
                                          'sha256': hashlib.sha256(data).hexdigest()})
        report['music'] = {'source': 'bgm/th06_17.opus',
                           'source_sha256': hashlib.sha256(args.source_music.read_bytes()).hexdigest(),
                           'runtime': 'endings/ending.mp3',
                           'runtime_sha256': hashlib.sha256((args.runtime_root / 'endings/ending.mp3').read_bytes()).hexdigest()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n'.join(source) + '\n')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print('已核对新典结局条件并导入六条中文结局，制作人员入口替换为完成事件。')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
# 本文件只读导出新典 ECL 中的血量相关指令和 PE64 四机体火力表，供数值审阅及显式导入只读运行时目录。
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import struct


HEALTH_OPS = {
    105: 'damage_enabled', 108: 'death_callback', 111: 'set_health',
    112: 'reset_phase_timer', 113: 'health_threshold', 114: 'health_callback',
    115: 'timeout_frames', 116: 'timeout_callback',
}
SHOT_NAMES = {0: '灵之御札', 1: '梦之御札', 2: '魔之御札', 3: '恋之御札'}


def unpack(data, fmt, offset):
    """从完整文件内读取小端结构；负偏移或越界抛 ValueError，不容许切片静默截断。"""
    if offset < 0 or offset + struct.calcsize(fmt) > len(data):
        raise ValueError(f'结构越界：{offset:#x} {fmt}')
    return struct.unpack_from(fmt, data, offset)


def ranks(mask):
    """解读 TH06 已知的四个难度位；保留的原始 mask 仍是权威值，不推测其余位的用途。"""
    return [name for index, name in enumerate(('E', 'N', 'H', 'L')) if mask & (0x100 << index)]


def read_health(path):
    """返回 ECL 血量相关记录；保留子程序局部时间、难度掩码与调用位置，不把静态指令当实际执行次数。"""
    data = path.read_bytes()
    count, timeline_count = unpack(data, '<HH', 0)
    if not 0 < count <= 4096 or timeline_count != 0:
        raise ValueError('输入须为 TH06 格式的新典 ECL')
    timelines = list(unpack(data, '<3I', 4))
    subs = list(unpack(data, '<' + 'I' * count, 16))
    header_end = 16 + 4 * count
    starts = sorted(set(subs + [offset for offset in timelines if offset]))
    if len(set(subs)) != count or any(offset < header_end or offset >= len(data) for offset in starts):
        raise ValueError('ECL 子程序或时间轴偏移无效')
    boundaries = {start: next((value for value in starts if value > start), len(data)) for start in starts}
    records = []
    instruction_count = 0
    for sub, start in enumerate(subs):
        at, end = start, boundaries[start]
        normal_index = 0
        while at < end:
            frame, opcode, size, mask, param_mask = unpack(data, '<iHHHH', at)
            if size < 12 or at + size > end:
                raise ValueError(f'ECL Sub{sub} 指令长度无效：{at:#x}')
            if frame == -1 and opcode == 65535:
                break
            base = {'scope': 'sub_instruction', 'sub': sub, 'frame': frame,
                    'opcode': opcode, 'offset': hex(at), 'rank_mask': hex(mask),
                    'difficulties': ranks(mask), 'parameter_mask': hex(param_mask),
                    'normal_index': normal_index if mask & 0x0200 else -1}
            if opcode in HEALTH_OPS:
                if size != 16:
                    raise ValueError(f'ECL 血量指令 {opcode} 结构不符')
                value, = unpack(data, '<i', at + 12)
                records.append(dict(base, kind=HEALTH_OPS[opcode], value=value,
                                    value_kind='variable_reference' if -10024 <= value <= -10001 else 'literal'))
            elif opcode == 95:
                if size != 36:
                    raise ValueError('ECL 子程序出生指令结构不符')
                target, x, y, z, health, item, score = unpack(data, '<ifffhhI', at + 12)
                records.append(dict(base, scope='sub_spawn', kind='spawn_health', value=health,
                                    target_sub=target, position=[x, y, z], item=item, score=score,
                                    value_kind='variable_reference' if -10024 <= health <= -10001 else 'literal'))
            instruction_count += 1
            if mask & 0x0200:
                normal_index += 1
            at += size
    for timeline, start in enumerate(timelines):
        if not start:
            continue
        at, end = start, boundaries[start]
        while at < end:
            frame, arg0 = unpack(data, '<hh', at)
            if frame == -1:
                break
            opcode, size = unpack(data, '<HH', at + 4)
            if size < 8 or at + size > end:
                raise ValueError(f'ECL 时间轴指令长度无效：{at:#x}')
            if opcode in (0, 2, 4, 6):
                if size != 28:
                    raise ValueError('ECL 时间轴显式生命出生结构不符')
                x, y, z, health, item, score = unpack(data, '<3fhhI', at + 8)
                records.append({'scope': 'timeline_spawn', 'timeline': timeline, 'frame': frame,
                                'opcode': opcode, 'offset': hex(at), 'kind': 'spawn_health',
                                'target_sub': arg0, 'value': health, 'value_kind': 'literal',
                                'position': [x, y, z], 'item': item, 'score': score})
            elif opcode in (1, 3, 5, 7):
                if size != 20:
                    raise ValueError('ECL 时间轴默认生命出生结构不符')
                records.append({'scope': 'timeline_spawn', 'timeline': timeline, 'frame': frame,
                                'opcode': opcode, 'offset': hex(at), 'kind': 'spawn_default_health',
                                'target_sub': arg0, 'value': None, 'value_kind': 'engine_default',
                                'position': list(unpack(data, '<3f', at + 8))})
            at += size
        else:
            raise ValueError('ECL 时间轴缺少终止标记')
    return {'file': path.name, 'sha256': hashlib.sha256(data).hexdigest(),
            'sub_count': count, 'instruction_count': instruction_count, 'records': records}


class PeImage:
    """只读 PE64 地址映射；读取范围须完整落在某节原始数据内，不依赖硬编码绝对地址。"""
    def __init__(self, path):
        self.data = path.read_bytes()
        data = self.data
        pe, = unpack(data, '<I', 0x3c)
        if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0' or unpack(data, '<H', pe + 24)[0] != 0x20b:
            raise ValueError('自机火力表输入须为新典 PE64')
        count, = unpack(data, '<H', pe + 6)
        optional, = unpack(data, '<H', pe + 20)
        self.base, = unpack(data, '<Q', pe + 48)
        self.sections = []
        for index in range(count):
            _, rva, size, raw = unpack(data, '<IIII', pe + 24 + optional + 40 * index + 8)
            if raw + size > len(data):
                raise ValueError('PE 节超出文件')
            self.sections.append((rva, raw, size))

    def offset(self, address, length):
        """映射完整虚拟地址范围；不可映射时返回 None，输入 address 是绝对虚拟地址。"""
        for rva, raw, size in self.sections:
            delta = address - self.base - rva
            if 0 <= delta and delta + length <= size:
                return raw + delta
        return None


def read_shots(path):
    """扫描并验证四张唯一九档火力表，导出主副弹原始伤害；激光记录明确使用按帧伤害单位。"""
    pe = PeImage(path)
    tables = {}
    for rva, raw, size in pe.sections:
        for at in range(raw, raw + size - 144 + 1, 8):
            entries = [unpack(pe.data, '<iiQ', at + index * 16) for index in range(9)]
            if [entry[1] for entry in entries] != [8, 16, 32, 48, 64, 80, 96, 128, 999]:
                continue
            levels, all_records = [], []
            for index, (count, upper, address) in enumerate(entries):
                location = pe.offset(address, count * 36) if 0 < count <= 32 else None
                if location is None:
                    break
                shots = []
                for channel in range(count):
                    record = unpack(pe.data, '<hh6fHBBhh', location + channel * 36)
                    period, phase, x, y, width, height, angle, speed, damage, module, kind, script, sound = record
                    if (period <= 0 or module > 2 or kind > 3 or not 1120 <= script <= 1127 or damage <= 0 or
                            not all(math.isfinite(value) for value in record[2:8])):
                        break
                    shots.append({'channel': channel, 'record_rva': hex(address - pe.base + channel * 36),
                                  'period_or_cooldown_frames': period, 'phase_or_slot': phase,
                                  'module': module, 'kind': kind, 'x_pixels': x, 'y_pixels': y,
                                  'hitbox_width_pixels': width, 'hitbox_height_pixels': height,
                                  'angle_radians': angle, 'speed_pixels_per_frame': speed,
                                  'damage': damage, 'damage_basis': 'per_contact_frame_60hz' if kind == 3 else 'per_hit',
                                  'animation_global': script, 'sound': sound,
                                  'shots_per_30_frame_window': None if kind == 3 else sum(frame % period == phase for frame in range(30))})
                if len(shots) != count:
                    break
                levels.append({'power_min': 0 if index == 0 else entries[index - 1][1],
                               'power_max': min(128, upper - 1), 'raw_upper_exclusive': upper, 'shots': shots})
                all_records.extend(shots)
            if len(levels) != 9:
                continue
            side_kinds = {shot['kind'] for shot in all_records if shot['module']}
            identity = {frozenset({1}): 0, frozenset({0}): 1, frozenset({2}): 2, frozenset({3}): 3}.get(frozenset(side_kinds))
            if identity is None or identity in tables:
                raise ValueError('火力表身份不明确或重复')
            tables[identity] = {'id': identity, 'name': SHOT_NAMES[identity],
                                'table_rva': hex(rva + at - raw), 'levels': levels}
    if set(tables) != {0, 1, 2, 3}:
        raise ValueError(f'未找到唯一的四机体火力表：{list(tables)}')
    return {'sha256': hashlib.sha256(pe.data).hexdigest(), 'frames_per_second': 60,
            'damage_values_are_native': True, 'tables': [tables[index] for index in range(4)]}


def write_json(path, value):
    """写入 UTF-8 审阅数据；只写调用者选择的输出目录，不覆盖游戏或运行时参数。"""
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def write_csv(path, rows):
    """按记录键的并集输出 CSV；嵌套字段使用 JSON，文件带 UTF-8 BOM 以便表格软件直接打开。"""
    columns = list(dict.fromkeys(key for row in rows for key in row))
    with path.open('w', encoding='utf-8-sig', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        for row in rows:
            writer.writerow({key: json.dumps(value, ensure_ascii=False) if isinstance(value, (dict, list)) else value
                             for key, value in row.items()})


def write_summary(path, stages, shots):
    """写入可直接阅读的索引及关键数值；不把通道伤害相加冒充保证命中的实际 DPS。"""
    lines = ['# 新典血量与自机伤害导出', '',
             '全部数值采用新典原始单位，项目已统一使用原始 HP 与伤害，不再应用旧的 HP ÷25、伤害 ÷12 换算。', '',
             '- [敌人生命 CSV](enemy_health.csv)／[完整 JSON](enemy_health.json)',
             '- [主副弹伤害 CSV](player_shots.csv)／[完整 JSON](player_shots.json)', '',
             '## 关卡覆盖', '', '| 关卡 | 子程序数 | 生命相关静态记录 |', '|---|---:|---:|']
    for number, stage in enumerate(stages, 1):
        lines.append(f'| {number} | {stage["sub_count"]} | {len(stage["records"])} |')
    lines += ['', '记录包含全部难度掩码。子程序帧是局部脚本时间，出生时间轴帧是关卡时间；循环、分支、继承与动态变量没有被静态展开。', '',
              '## Power 8 与 128 的原始伤害', '',
              '主弹及副弹按原表记录顺序列出，副弹按模块 1／2 分开；逗号表示多个通道，不能当成一颗弹。', '',
              '| 御札 | Power | 主弹各通道 | 左副机各通道 | 右副机各通道 | 副弹计量 |',
              '|---|---:|---|---|---|---|']
    for table in shots['tables']:
        for level in table['levels']:
            if level['power_min'] not in (8, 128):
                continue
            damage = [', '.join(str(shot['damage']) for shot in level['shots'] if shot['module'] == module) for module in range(3)]
            basis = '每束每 60 Hz 接触帧' if table['id'] == 3 else '每颗命中'
            lines.append(f'| {table["name"]} | {level["power_min"]} | {damage[0]} | {damage[1]} | {damage[2]} | {basis} |')
    lines += ['', '## 读取边界', '',
              '- 恋札激光的 3～6 是每束每个有效接触帧的原始伤害，生效期间相当于每束 180～360 点／秒；还需考虑冷却、寿命和目标减伤。',
              '- 魔札导弹命中后爆破伤害为 max(1, trunc(原始单发伤害 / 4))，持续判定 20 帧；这不是把单发总伤害统一除以 4。',
              '- kind=3 的前两项是副机冷却和槽号；其他弹是周期和相位。无法命中的原始相位不归一化。',
              '- 血量阈值与重设血量不是新的独立 Boss；需要结合回调、难度及子程序调用解释。',
              '- Bomb、受保护期、目标减伤和同帧结算上限等可执行代码规则未被完整导出；本目录不代表最终伤害模拟器。',
              '- 敌弹发射指令没有逐弹伤害字段；此处没有伪造一张“敌弹种类到玩家扣血量”的表。', '',
              f'EXE SHA-256：`{shots["sha256"]}`。', '']
    path.write_text('\n'.join(lines), encoding='utf-8')


def write_runtime(path, stages, shots):
    """导入可直接编译的只读数值目录；保留全部 3598 记录，不在构建或运行时调用解析器。"""
    def real(value):
        text = format(value, '.9g')
        return text + ('F' if '.' in text or 'e' in text else '.0F')
    kinds = dict(zip(HEALTH_OPS.values(), ('DamageEnabled', 'DeathCallback', 'SetHealth', 'ResetTimer', 'Threshold', 'HealthCallback', 'Timeout', 'TimeoutCallback')))
    kinds.update(spawn_health='Spawn', spawn_default_health='DefaultSpawn')
    scopes = {'sub_instruction': 'Instruction', 'timeline_spawn': 'TimelineSpawn', 'sub_spawn': 'SubSpawn'}
    lines = ['// 本文件是新典原始战斗数值的已导入源码；由 tools/export_native_combat_data.py --runtime-output 重建。',
             '// 生命及伤害为原始点数，时间为 60 Hz 帧；静态指令不是执行轨迹，动态/默认生命不猜测。',
             f'constexpr std::string_view kExecutableHash="{shots["sha256"]}";']
    for number, stage in enumerate(stages, 1):
        lines.append(f'constexpr std::array<HealthRecord,{len(stage["records"])}> kHealth{number}{{{{')
        for row in stage['records']:
            value = 'std::nullopt' if row['value'] is None else str(row['value'])
            position = '{' + ','.join(real(value) for value in row.get('position', [0, 0, 0])) + '}'
            fields = [f'HealthScope::{scopes[row["scope"]]}', str(row.get('sub', -1)), str(row.get('timeline', -1)),
                      str(row['frame']), str(row['opcode']), str(row.get('normal_index', -1)), row['offset'],
                      row.get('rank_mask', '0'), row.get('parameter_mask', '0'), f'HealthKind::{kinds[row["kind"]]}', value,
                      'true' if row['value_kind'] == 'variable_reference' else 'false', str(row.get('target_sub', -1)),
                      position, str(row.get('item', 0)), str(row.get('score', 0))]
            lines.append('    {' + ','.join(fields) + '},')
        lines.append('}};')
    lines.append('constexpr std::array<StageData,7> kStages{{')
    for number, stage in enumerate(stages, 1):
        state = 'Playable' if number == 1 else 'DataOnly'
        lines.append(f'    {{{number},Integration::{state},"{stage["sha256"]}",kHealth{number}}},')
    lines.append('}};')
    for table in shots['tables']:
        identity = table['id']
        for level in table['levels']:
            name = f'kShot{identity}Power{level["power_min"]}'
            lines.append(f'constexpr std::array<ShotChannel,{len(level["shots"])}> {name}{{{{')
            for row in level['shots']:
                values = [str(row[key]) for key in ('period_or_cooldown_frames', 'phase_or_slot', 'module', 'kind', 'animation_global', 'sound')]
                values += [real(row[key]) for key in ('x_pixels', 'y_pixels', 'hitbox_width_pixels', 'hitbox_height_pixels', 'angle_radians', 'speed_pixels_per_frame', 'damage')]
                values.append(row['record_rva'])
                lines.append('    {' + ','.join(values) + '},')
            lines.append('}};')
        lines.append(f'constexpr std::array<ShotLevel,9> kShot{identity}Levels{{{{')
        lines += [f'    {{{level["power_min"]},{level["power_max"]},kShot{identity}Power{level["power_min"]}}},' for level in table['levels']]
        lines.append('}};')
    lines.append('constexpr std::array<ShotDefinition,4> kShots{{')
    for table in shots['tables']:
        lines.append(f'    {{static_cast<ShotType>({table["id"]}),"{table["name"]}",{table["table_rva"]},kShot{table["id"]}Levels}},')
    lines.append('}};')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main():
    """导出七关 ECL 与四机体伤害表；输出 JSON/CSV，原始 ECL 和可执行文件不改写；仅显式指定 --runtime-output 时导入源码目录。"""
    parser = argparse.ArgumentParser(description='只读导出新典血量及自机弹伤害')
    parser.add_argument('exe', type=Path)
    parser.add_argument('ecl_directory', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime-output', type=Path, help='显式导入 C++ 静态目录，不设置时仅导出审阅文件')
    args = parser.parse_args()
    stages = [read_health(args.ecl_directory / f'ecldata{number}.ecl') for number in range(1, 8)]
    shots = read_shots(args.exe)
    args.output.mkdir(parents=True, exist_ok=True)
    write_json(args.output / 'enemy_health.json', {'units': 'native_hp_and_60hz_frames', 'stages': stages})
    write_json(args.output / 'player_shots.json', shots)
    health_rows = [dict(stage=index, **record) for index, stage in enumerate(stages, 1) for record in stage['records']]
    shot_rows = [dict(shot=table['id'], name=table['name'], table_rva=table['table_rva'],
                      power_min=level['power_min'], power_max=level['power_max'], **shot)
                 for table in shots['tables'] for level in table['levels'] for shot in level['shots']]
    write_csv(args.output / 'enemy_health.csv', health_rows)
    write_csv(args.output / 'player_shots.csv', shot_rows)
    write_summary(args.output / 'README.md', stages, shots)
    if args.runtime_output:
        write_runtime(args.runtime_output, stages, shots)
    print(f'已导出七关 {len(health_rows)} 条生命相关记录、四机体 {len(shot_rows)} 条逐通道火力记录')
    for index, stage in enumerate(stages, 1):
        print(f'第 {index} 关：{stage["sub_count"]} 个子程序，{len(stage["records"])} 条生命相关记录')
    print(f'EXE SHA-256 {shots["sha256"]}')


if __name__ == '__main__':
    main()

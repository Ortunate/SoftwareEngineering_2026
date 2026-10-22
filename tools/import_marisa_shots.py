#!/usr/bin/env python3
# 本文件只读解析新典魔理沙魔/恋两张火力表，导出主弹、导弹和激光的参数，不复制游戏代码或素材。
import argparse
import hashlib
import math
from pathlib import Path
import struct


def read_tables(source: Path):
    """返回哈希及导弹/激光两表；输入须为 PE64，候选或弹种结构不唯一时抛 ValueError。"""
    data=source.read_bytes()
    pe=struct.unpack_from('<I',data,0x3c)[0]
    if data[:2]!=b'MZ' or data[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',data,pe+24)[0]!=0x20b:
        raise ValueError('输入须为新典 PE64')
    count=struct.unpack_from('<H',data,pe+6)[0]
    optional=struct.unpack_from('<H',data,pe+20)[0]
    image=struct.unpack_from('<Q',data,pe+48)[0]
    sections=[]
    for index in range(count):
        _,rva,size,raw=struct.unpack_from('<IIII',data,pe+24+optional+40*index+8)
        sections.append((rva,size,raw))

    def offset(address,length):
        """将完整记录映射到节内文件数据；跨节或越界返回 None。"""
        for rva,size,raw in sections:
            delta=address-image-rva
            if 0<=delta and delta+length<=size and raw+delta+length<=len(data):return raw+delta
        return None

    found={2:[],3:[]}
    for rva,size,raw in sections:
        for at in range(raw,min(raw+size,len(data))-144+1,8):
            entries=[struct.unpack_from('<iiQ',data,at+16*i) for i in range(9)]
            if [entry[1] for entry in entries]!=[8,16,32,48,64,80,96,128,999]:continue
            levels=[]
            kinds=set()
            for count,_,address in entries:
                loc=offset(address,count*36) if 0<count<=32 else None
                if loc is None:break
                records=[struct.unpack_from('<hh6fHBBhh',data,loc+36*i) for i in range(count)]
                if any(rec[9] not in (0,1,2) or rec[10] not in (0,2,3) or not 1120<=rec[11]<=1127 for rec in records):break
                kinds.update(rec[10] for rec in records if rec[9])
                levels.append((0 if not levels else entries[len(levels)-1][1],records))
            if len(levels)==9 and len(kinds)==1 and next(iter(kinds)) in found:
                kind=next(iter(kinds))
                if any(rec[0]<=0 or (rec[9]==0 and (rec[10]!=0 or rec[11]!=1120)) or
                       (rec[9]!=0 and rec[10]!=kind) for _,records in levels for rec in records):
                    raise ValueError('魔理沙模块与弹种结构不一致')
                found[kind].append((rva+at-raw,levels))
    if any(len(tables)!=1 for tables in found.values()):raise ValueError('未找到唯一的魔/恋火力表')
    return hashlib.sha256(data).hexdigest(),found[2][0],found[3][0]


def write_header(path: Path, sha256: str, missile, laser):
    """覆盖运行时参数头；原始像素/帧/伤害均保留，三维换算由 Player 负责。"""
    lines=[
        '// 本文件保存从新典只读解析的魔理沙魔/恋两张火力表；由 tools/import_marisa_shots.py 导入。',
        f'// 输入 SHA-256：{sha256}；魔表 RVA 0x{missile[0]:x}，恋表 RVA 0x{laser[0]:x}。',
        '#pragma once','#include <array>','#include <span>','',
        'namespace koumakyou::marisa_shots {',
        '// 周期/相位为 60 Hz 帧；kind=3 时前两项改为副机槽冷却帧数和槽号，不能做取模调度。',
        '// X/Y 与宽高为逻辑像素，Y 向下；angleDegrees 相对正前方，右正左负；speed 为像素/帧。',
        '// module=0 为主机、1/2 为左右副机；kind=0 主弹、2 加速导弹、3 随副机移动的持续激光。',
        'struct Shot { int period, phase, module, kind, script; float x, y, width, height, angleDegrees, speed, damage; };',
        '/// @brief Power 达到 minimumPower 后使用该档；shots 借用下方静态数组。',
        'struct Level { int minimumPower; std::span<const Shot> shots; };',
        'inline constexpr float kMissileAccelerationPixelsPerFrameSquared = 0.3F;',
        'inline constexpr int kExplosionFrames = 20;', '',
    ]
    for name,(_,levels) in [('Missile',missile),('Laser',laser)]:
        for minimum,records in levels:
            lines.append(f'inline constexpr std::array<Shot,{len(records)}> k{name}{minimum}{{{{')
            for rec in records:
                angle=round(math.degrees(rec[6])+90,4)
                floats=[rec[2],rec[3],rec[4],rec[5],angle,rec[7],rec[8]]
                values=', '.join(f'{value:.4f}F' for value in floats)
                lines.append(f'    {{{rec[0]}, {rec[1]}, {rec[9]}, {rec[10]}, {rec[11]-1056}, {values}}},')
            lines.append('}};')
        lines.append(f'inline constexpr std::array<Level,9> k{name}Levels{{{{')
        lines.extend(f'    {{{minimum}, k{name}{minimum}}},' for minimum,_ in levels)
        lines.append('}};')
    lines.extend(['','/// @brief laser 选择恋札；Power 越界时沿用首档/末档，不修改玩家。',
                  'inline const Level& ForPower(bool laser,int power) {',
                  '    const auto& levels=laser ? kLaserLevels : kMissileLevels;',
                  '    const Level* result=&levels.front();',
                  '    for(const auto& level:levels) { if(power<level.minimumPower)break;result=&level; }',
                  '    return *result;', '}', '} // namespace koumakyou::marisa_shots',''])
    path.write_text('\n'.join(lines),encoding='utf-8')


def main():
    """只读指定的 th06nc.exe，验证并写出表；错误以异常退出，不启动原游戏。"""
    parser=argparse.ArgumentParser(description='只读导入新典魔理沙火力表')
    parser.add_argument('source',type=Path)
    parser.add_argument('--output',type=Path,default=Path('src/gameplay/actors/marisa_shots.hpp'))
    args=parser.parse_args()
    sha256,missile,laser=read_tables(args.source)
    write_header(args.output,sha256,missile,laser)
    print(f'SHA-256 {sha256}; Magic Missile RVA 0x{missile[0]:x}; Illusion Laser RVA 0x{laser[0]:x}')


if __name__=='__main__':main()

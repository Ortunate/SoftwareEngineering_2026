#!/usr/bin/env python3
# 本文件将 thecl -d 6 输出的一面脚本转换为 Normal 帧指令表和逐只出生表。
"""输入须为本机新典 ecldata1.ecl 的反编译文本；不负责解包，也不读取游戏安装目录。"""
import re, argparse
from pathlib import Path
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path, help='thecl -d 6 输出的文本')
parser.add_argument('--output', type=Path, default=Path('src/stages/stage1'))
args = parser.parse_args()
root = args.output
root.mkdir(parents=True, exist_ok=True)
s = args.source.read_text()
subs=[]
for number,body in re.findall(r'sub Sub(\d+)\(\)\s*\{(.*?)\n\}',s,re.S):
 if int(number)!=len(subs):raise ValueError("子程序必须从 Sub0 连续编号")
 frame=0;enabled=True;inst=[];labels={}
 for line in body.splitlines():
  line=line.strip()
  if line.startswith('+'):frame=int(line.split('//')[1]);continue
  if line.endswith(':'):labels[line[:-1]]=len(inst);continue
  if line.startswith('!'):
   m=re.match(r'!([A-Z*]+)\s*(.*)',line);enabled='N' in m[1] or '*' in m[1];line=m[2]
  if not enabled:continue
  m=re.match(r'ins_(\d+)\((.*)\);',line)
  if not m:continue
  args=m[2].split(', ') if m[2] else []
  if len(args)>16:raise ValueError("指令参数超过 16 项")
  inst.append((frame,int(m[1]),args))
 for i,(frame,op,args) in enumerate(inst):
  for n,a in enumerate(args):
   if a in labels:args[n]=str(labels[a])
   elif a.startswith('"Sub'):args[n]=re.search(r'\d+',a)[0]
   elif a.startswith('"'):args[n]='0'
   elif a.startswith('#'):args[n]=str(int(a[1:],16))
   elif a.endswith('f'):args[n]=a[:-1]
  inst[i]=(frame,op,args)
 subs.append(inst)
if len(subs)!=38:raise ValueError('输入不是预期的新典一面脚本（38 个子程序）')
with (root/'reference_program.hpp').open('w') as f:
 f.write('// 本文件保存本机新典 ecldata1 的 Normal 帧指令；时间为 60 Hz 帧，角度为弧度。\n// 跳转目标为同一子程序的指令索引，负数 -10001…-10012 为上下文寄存器。\n#pragma once\n#include <array>\n#include <span>\nnamespace redmist::stage1::reference {\n/// @brief 一条借用的关卡指令；参数保留原作像素、帧及弧度单位，最多 16 项。\nstruct Instruction { int frame; int opcode; std::array<float,16> args; };\n')
 for n,inst in enumerate(subs):
  f.write('inline constexpr Instruction kSub%d[]{\n'%n)
  for frame,op,args in inst:
   a=', '.join(str(float(v))+'F' for v in args);f.write('    {%d, %d, {%s}},\n'%(frame,op,a))
  f.write('};\n')
 f.write('inline constexpr std::array<std::span<const Instruction>,38> kPrograms{\n')
 for n in range(38):f.write('    kSub%d,\n'%n)
 f.write('};\n}\n')
frame=0;births=[]
body=s.split('timeline Timeline0()')[1]
for line in body.splitlines():
 if line.startswith('+'):frame=int(line.split('//')[1]);continue
 m=re.search(r'ins_(\d+)\("Sub(\d+)", (.*)\);',line)
 if m and int(m[2])<=4:
  args=[float(a.rstrip('f')) for a in m[3].split(', ')]
  births.append((frame,int(m[2]),int(m[1]) in [2,6],args[0],args[3],args[5],args[4]))
with (root/'reference_births.hpp').open('w') as f:
 f.write('// 本文件保存新典一面逐只出生数据；frame 为开局起算的 60 Hz 帧，x 为 384 像素场地的横坐标。\n#pragma once\n#include <array>\nnamespace redmist::stage1::reference {\n/// @brief 出生子程序编号、镜像标记及原作生命/击破分数；item=-1 为轮换掉落，0 为小 P 点；x=-999 表示出生时均匀随机横坐标。\nstruct Birth { int frame; int sub; bool mirrored; float x; float health; int score; int item; };\ninline constexpr std::array<Birth,%d> kBirths{{\n'%len(births))
 for frame,sub,mirror,x,hp,score,item in births:f.write('    {%d, %d, %s, %.1fF, %.1fF, %d, %d},\n'%(frame,sub,str(mirror).lower(),x,hp,score,item))
 f.write('}};\n}\n')
print('Normal instructions',sum(map(len,subs)),'births',len(births))

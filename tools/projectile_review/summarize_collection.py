"""统一汇总全组完整压测，区分短时诊断、正常 8192 目标和极端场景。"""
import csv
from collections import defaultdict
from pathlib import Path
import sys

folder=Path(sys.argv[1]).resolve()/'output/projectile-review/collection/performance'
rows=list(csv.DictReader((folder/'results.csv').open(encoding='utf-8',newline='')))
groups=defaultdict(list)
for row in rows:groups[row['case']].append(row)
types=[5,8,2,0,3,1,6,9,7]
expected={f'diagnostic-type-{k}-{n}' for k in types for n in (1024,4096)}
expected|={f'type-{k}-8192-{s}' for k in types for s in ('static-off','moving-glow')}
expected|={f'extreme-type-{k}-near' for k in (1,3,6,9,7)}
expected|={'mixed-solid','mixed-shell','mixed-fire-bubble','culled-75-percent','collision-heavy','sprite-1024','sprite-4096','sprite-8192'}
assert set(groups)==expected,'Incomplete scenario coverage'
assert len(rows)==111,'Incomplete repeats'
lines=['# 全组弹体性能汇总','', '各完整场景取三次重复中最差的 P95，单位 ms；诊断不参与验收结论。','',
       '| 场景 | 整帧 | GPU | 更新+碰撞 CPU* | 可见数 | 绘制数 | 结果 |','| --- | ---: | ---: | ---: | ---: | ---: | --- |']
passed=True
for name,group in groups.items():
    diagnostic=name.startswith('diagnostic-')
    assert len(group)==(1 if diagnostic else 3)
    assert all(int(r['frames'])>0 for r in group)
    if diagnostic:continue
    frame=max(float(r['frame_p95_ms']) for r in group)
    available=all(r['gpu_p95_ms']!='NA' and int(r['gpu_samples'])>0 for r in group)
    gpu=max(float(r['gpu_p95_ms']) for r in group) if available else None
    cpu=max(float(r['update_p95_ms'])+float(r['collision_p95_ms']) for r in group)
    normal=name.startswith('type-') or name in ('mixed-solid','mixed-shell')
    ok=frame<=16.7 and gpu is not None and gpu<=6 and cpu<=2
    if normal:passed&=ok
    label=('PASS' if ok else 'FAIL / GPU unavailable') if normal else ('极端整帧超过 16.7 ms' if frame>16.7 else '单列压力场景 / 基线')
    value=f'{gpu:.3f}' if gpu is not None else 'NA'
    lines.append(f'| {name} | {frame:.3f} | {value} | {cpu:.3f} | {group[0]["visible"]} | {group[0]["draw_calls"]} | {label} |')
lines+=['','*CPU 为独立更新阶段和碰撞阶段 P95 相加的保守指标。',
        '',f'正常 8192 场景目标：{"PASS" if passed else "FAIL"}。',
        '球壳和尖丝采用加色，泡内球乘法衰减背景；按固定角色顺序绘制，不进行实例深度排序，极端覆盖须人工评估。',
        '本结果只代表 hardware.txt 所列机器和独立合成场景，不包含真实关卡与技能脚本。',
        'sprite 基线为不同提交方式的简化圆点图，不作为三维普遍快于二维的结论。',
        '人工验收状态以各资产 asset.json 为准；自动检查不代签。']
report='\n'.join(lines)+'\n';(folder/'summary.md').write_text(report,encoding='utf-8')
print('Collection performance targets: '+('PASS' if passed else 'FAIL')+'. Full report: output/projectile-review/collection/performance/summary.md')
sys.exit(0 if passed else 1)

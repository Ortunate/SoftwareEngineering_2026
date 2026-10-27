"""检查完整压力实验的覆盖与帧预算，汇总各场景三次重复中的最差 P95。"""
import csv
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
folder = root/'output/projectile-review/performance'
acceptance = json.loads((root/'assets/projectiles/kunai_classic/asset.json').read_text(encoding='utf-8'))['humanAcceptance']
rows = list(csv.DictReader((folder/'results.csv').open(newline='',encoding='utf-8')))
names = ['normal-1024-static','normal-4096-moving','normal-8192-static','normal-8192-moving',
         'normal-8192-glow','normal-8192-one-color','dense-8192-glow','near-8192-glow',
         'culled-8192','collision-heavy-8192','sprite-1024','sprite-4096','sprite-8192']
assert len(rows) == 39 and {r['case'] for r in rows} == set(names), 'Incomplete experiment'
lines = ['# Independent Kunai stress results', '', 'Worst P95 across three repeats; times in ms.', '',
         '| Case | Frame | GPU | Update+collision CPU* | Visible |', '| --- | ---: | ---: | ---: | ---: |']
passed = True
for name in names:
    group = [r for r in rows if r['case'] == name]
    assert {r['repeat'] for r in group} == {'1','2','3'}
    assert all(int(r['frames'])>0 for r in group)
    frame = max(float(r['frame_p95_ms']) for r in group)
    cpu = max(float(r['update_p95_ms'])+float(r['collision_p95_ms']) for r in group)
    available = all(r['gpu_p95_ms'] != 'NA' and int(r['gpu_samples'])>0 for r in group)
    gpu = max(float(r['gpu_p95_ms']) for r in group) if available else None
    if name.startswith('normal-8192'):
        passed &= frame <= 16.7 and cpu <= 2 and gpu is not None and gpu <= 6
    value = f'{gpu:.3f}' if gpu is not None else 'unavailable'
    lines.append(f'| {name} | {frame:.3f} | {value} | {cpu:.3f} | {group[0]["visible"]} |')
lines += ['', '*Conservative sum of the separately measured stage P95s, not a percentile of paired samples.',
          '', f'Normal 8192 budget targets: {"PASS" if passed else "FAIL / GPU unavailable"}.',
          'Hardware and sampling conditions: hardware.txt. Full distributions: results.csv.',
          'Synthetic standalone channel only. Full game and lower hardware require separate measurements.',
          f'Art acceptance: {acceptance.upper()}.']
report = '\n'.join(lines)+'\n'
(folder/'summary.md').write_text(report,encoding='utf-8')
print(report)
sys.exit(0 if passed else 1)

"""汇总激光完整实验，普通数量与极端覆盖分列，拒绝用短诊断代替完整数据。"""
import csv
from pathlib import Path
from collections import defaultdict
import sys
folder=Path(sys.argv[1])/'output/projectile-review/lasers'
rows=list(csv.DictReader((folder/'performance.csv').open(newline='')));assert len(rows)==33
groups=defaultdict(list)
for r in rows:groups[int(r['case'])].append(r)
assert set(groups)==set(range(11)) and all(len(g)==3 for g in groups.values())
lines=['# 敌方激光完整性能报告','', '各场景三次重复取最差 P95，单位 ms；普通目标 frame≤16.7、GPU≤6、更新/碰撞 CPU≤2。','',
       '| 场景 | 激光/普通弹 | 整帧 | CPU | GPU | 调用 | 结论 |','| --- | --- | ---: | ---: | ---: | ---: | --- |']
passed=True
for case,g in sorted(groups.items()):
    assert all(r['gpu_p95']!='NA' for r in g)
    f,c,gpu=[max(float(r[s+'_p95']) for r in g) for s in ('frame','cpu','gpu')]
    normal=case<6 or case==9;ok=f<=16.7 and c<=2 and gpu<=6
    if normal:passed&=ok
    name=f'套组 {case//3} / {g[0]["beams"]} 束' if case<6 else {6:'默认套组极近',7:'圆润套组极近',8:'交叉',9:'普通弹混合',10:'8192 束极限'}[case]
    lines.append(f'| {name} | {g[0]["beams"]}/{g[0]["ordinary"]} | {f:.3f} | {c:.3f} | {gpu:.3f} | {g[0]["draws"]} | '+(('PASS' if ok else 'FAIL') if normal else '单列压力')+' |')
lines+=['',f'普通场景目标：{"PASS" if passed else "FAIL"}。',
        'CPU 列仅为合成运动及胶囊探针循环，打包、提交和窗口呈现计入整帧。GPU 异步查询仅覆盖绘制及混合场景 Prepare；不代表完整游戏。',
        '硬件见 hardware.txt；无剔除，全部提交。预分配实例缓冲，两套最多六次调用；混合普通弹按其类型增加调用。',
        '两套激光均待人工验收；套组 1 为圆形光斑的三维适配，并非已确认的原作独立关卡激光。']
(folder/'performance.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Laser normal targets: '+('PASS' if passed else 'FAIL'))
sys.exit(0 if passed else 1)

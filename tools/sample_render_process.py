"""在指定压测控制进程存活期间，每分钟记录本任务持续负载进程的 CPU 与 RSS；不控制渲染。"""
import argparse
import csv
import os
import subprocess
import time
from datetime import datetime, timezone


def sample(controller, folder):
    """controller 是本任务控制进程 PID；RSS 单位 KiB，CPU 为 ps 的单核等效百分比。"""
    path = folder + "/soak-process-samples.csv"
    needs_header = not os.path.exists(path) or os.path.getsize(path) == 0
    with open(path, "a", newline="") as stream:
        writer = csv.writer(stream)
        if needs_header:
            writer.writerow(["utc", "variant", "pid", "ps_cpu_percent", "rss_kib", "process_elapsed"])
            stream.flush()
        deadline = time.monotonic()
        while True:
            try:
                os.kill(controller, 0)
            except ProcessLookupError:
                break
            if time.monotonic() >= deadline:
                # C locale 会把中文路径转义，导致本任务进程无法匹配；保持 UTF-8 且使用点号小数。
                output = subprocess.check_output(
                    ["ps", "-ww", "-axo", "pid=,ppid=,%cpu=,rss=,etime=,args="], text=True,
                    env=dict(os.environ, LC_ALL="en_US.UTF-8"))
                for line in output.splitlines():
                    fields = line.strip().split(None, 5)
                    if len(fields) != 6:
                        continue
                    pid, parent, cpu, rss, elapsed, command = fields
                    if not command.split(None, 1)[0].endswith("/render_benchmark") or folder not in command:
                        continue
                    for variant in ("baseline", "optimized"):
                        if "/soak-" + variant in command:
                            writer.writerow([datetime.now(timezone.utc).isoformat(), variant, pid, cpu, rss, elapsed])
                            stream.flush()
                deadline = time.monotonic() + 60
            time.sleep(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("controller", type=int)
    parser.add_argument("folder")
    arguments = parser.parse_args()
    sample(arguments.controller, os.path.abspath(arguments.folder))

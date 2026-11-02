"""顺序交替运行两个正式渲染压测二进制，保存环境、逐帧原始数据和可审阅的汇总。"""
import argparse
import csv
import hashlib
import json
import platform
import statistics
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def digest(path):
    """返回文件 SHA-256，用于固定实际参与测量的二进制及工具版本。"""
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def capture(command):
    """记录只读环境命令；不可用时保留错误，不把缺失遥测伪造成零。"""
    result = subprocess.run(command, capture_output=True, text=True)
    return {"exit": result.returncode, "stdout": result.stdout, "stderr": result.stderr}


def run(args):
    """每个场景三次重复采用 AB/BA/AB 顺序；两个 GPU 负载绝不并行。"""
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    previous = json.loads((output / "metadata.json").read_text()) if args.resume else None
    binaries = {"baseline": args.baseline.resolve(), "optimized": args.optimized.resolve()}
    assets = {"baseline": args.baseline_assets.resolve(), "optimized": args.assets.resolve()}
    metadata = {
        "utc_start": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(),
        "command": vars(args) | {key: str(value) for key, value in vars(args).items() if isinstance(value, Path)},
        "binaries": {key: {"path": str(value), "sha256": digest(value)} for key, value in binaries.items()},
        "harness_sha256": digest(Path(__file__).with_name("render_benchmark.cpp")),
        "git_head": capture(["git", "rev-parse", "HEAD"]),
        "git_diff": capture(["git", "diff", "--stat"]),
        "hardware": capture(["system_profiler", "SPHardwareDataType", "SPDisplaysDataType"]),
        "power": capture(["pmset", "-g", "batt"]),
        "thermal": capture(["pmset", "-g", "therm"]),
        "power_watts": None,
        "notes": "固定快照、原生窗口；game 包含完整 Game::Draw，未测战斗 Update。GPU 数值来自异步 GL_TIME_ELAPSED。功率瓦数未采集。",
    }
    # 两份素材按内容校验，避免 shader 或模型在测量前后悄然发生变化。
    for label, root in assets.items():
        signatures = []
        for folder in ("shaders", "projectiles", "items"):
            directory = root / folder
            if not directory.exists():
                directory = root.parent / folder
            for path in sorted(directory.rglob("*")):
                if path.is_file() and path.suffix in (".vs", ".fs", ".gltf", ".bin", ".json"):
                    signatures.append((folder + "/" + str(path.relative_to(directory)), digest(path)))
        metadata[label + "_asset_sha256"] = hashlib.sha256(json.dumps(signatures).encode()).hexdigest()
    if metadata["baseline_asset_sha256"] != metadata["optimized_asset_sha256"]:
        raise RuntimeError("A/B model and shader contents differ")
    rows = []
    completed = set()
    if previous:
        # 只保留暂停前完成整组 AB/BA/AB 的场景，不能把暂停两侧的半组样本当作公平配对。
        for label in binaries:
            if previous["binaries"][label]["sha256"] != metadata["binaries"][label]["sha256"]:
                raise RuntimeError("Resume binary differs: " + label)
        for key in ("harness_sha256", "baseline_asset_sha256", "optimized_asset_sha256"):
            if previous[key] != metadata[key]:
                raise RuntimeError("Resume content differs: " + key)
        for key in ("cases", "warm", "measure", "repeats", "msaa", "fps", "soak", "soak_fps"):
            if previous["command"][key] != metadata["command"][key]:
                raise RuntimeError("Resume sampling configuration differs: " + key)
        old_rows = list(csv.DictReader((output / "results.csv").open()))
        expected = {(label, str(repeat)) for label in binaries for repeat in range(1, args.repeats + 1)}
        for case in args.cases:
            group = [row for row in old_rows if row["case"] == case]
            actual = {(row["variant"], row["repeat"]) for row in group}
            images_complete = all((output / label / case / repeat / (case + ".png")).is_file()
                                  for label, repeat in expected)
            if actual == expected and len(group) == len(expected) and images_complete:
                completed.add(case)
                rows.extend(group)
        history = output / ("interrupted-" + str(len(previous.get("sessions", [])) + 1))
        for label in binaries:
            for case in args.cases:
                source = output / label / case
                if case not in completed and source.exists():
                    destination = history / label / case
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.move(str(source), str(destination))
        metadata["sessions"] = previous.get("sessions", [previous]) + [{
            "utc_resume": metadata["utc_start"], "power": metadata["power"],
            "thermal": metadata["thermal"], "retained_cases": sorted(completed)}]
        metadata["utc_start"] = previous["utc_start"]
        if (output / "PAUSED.json").exists():
            history.mkdir(parents=True, exist_ok=True)
            shutil.move(str(output / "PAUSED.json"), str(history / "PAUSED.json"))
        with (output / "results.csv").open("w", newline="") as stream:
            if rows:
                writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)
    metadata["status"] = "running"
    (output / "metadata.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    failures = []
    for case in args.cases:
        if case in completed:
            print(f"KEEP completed case {case}", flush=True)
            continue
        for repeat in range(1, args.repeats + 1):
            order = ("baseline", "optimized") if repeat % 2 else ("optimized", "baseline")
            for label in order:
                destination = output / label / case / str(repeat)
                destination.mkdir(parents=True, exist_ok=True)
                command = [str(binaries[label]), str(assets[label]), str(destination), case,
                           str(args.warm), str(args.measure), "1", str(args.msaa), str(args.fps)]
                print(f"START {case} repeat={repeat} {label}", flush=True)
                result = subprocess.run(["/usr/bin/time", "-l", *command], capture_output=True, text=True)
                (destination / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
                if result.returncode:
                    failures.append({"case": case, "repeat": repeat, "variant": label, "exit": result.returncode})
                    print(f"FAIL {case} {label}: {result.stderr[:200]}", flush=True)
                    continue
                row = next(csv.DictReader((destination / (case + ".csv")).open()))
                row.update(variant=label, repeat=str(repeat))
                rows.append(row)
                print(f"DONE {case} {label} frame_p95={row['frame_p95_ms']} cpu_p95={row['cpu_draw_p95_ms']} gpu_p95={row['gpu_p95_ms']}", flush=True)
                with (output / "results.csv").open("w", newline="") as stream:
                    writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
                    writer.writeheader()
                    writer.writerows(rows)
        # 除完整游戏的实时 FPS 文本外，固定快照要求 PNG 字节完全相同。
        for repeat in range(1, args.repeats + 1):
            left = output / "baseline" / case / str(repeat) / (case + ".png")
            right = output / "optimized" / case / str(repeat) / (case + ".png")
            failed = any(item["case"] == case and item["repeat"] == repeat for item in failures)
            if not failed and (not left.is_file() or not right.is_file()):
                raise RuntimeError(f"Image missing: {case} repeat={repeat}")
            if left.exists() and right.exists() and left.read_bytes() != right.read_bytes() and case != "game":
                raise RuntimeError(f"Image changed: {case} repeat={repeat}")
    (output / "failures.json").write_text(json.dumps(failures, indent=2), encoding="utf-8")
    if args.soak:
        for label in ("baseline", "optimized"):
            destination = output / ("soak-" + label)
            destination.mkdir(exist_ok=True)
            command = [str(binaries[label]), str(assets[label]), str(destination), "lasers",
                       str(args.warm), str(args.soak), "1", str(args.msaa), str(args.soak_fps)]
            print(f"START SOAK {label} seconds={args.soak}", flush=True)
            result = subprocess.run(["/usr/bin/time", "-l", *command], capture_output=True, text=True)
            (destination / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
            if result.returncode:
                raise RuntimeError(f"Soak failed: {label}")
            print(f"DONE SOAK {label}", flush=True)
    lines = ["# 正式渲染链 A/B 压测", "", f"{args.repeats} 次重复的 P95 中位数；毫秒。固定快照，同像素、MSAA 和画质。", "",
             "| 场景 | 基线帧 P95 | 优化帧 P95 | 变化 | 基线 CPU P95 | 优化 CPU P95 | 基线 GPU P95 | 优化 GPU P95 |", "|---|---:|---:|---:|---:|---:|---:|---:|"]
    for case in args.cases:
        groups = {label: [row for row in rows if row["case"] == case and row["variant"] == label] for label in binaries}
        if not all(groups.values()):
            continue
        values = []
        for metric in ("frame_p95_ms", "cpu_draw_p95_ms", "gpu_p95_ms"):
            for label in binaries:
                numbers = [float(row[metric]) for row in groups[label] if row[metric] != "NA"]
                values.append(statistics.median(numbers) if numbers else None)
        frame_a, frame_b, cpu_a, cpu_b, gpu_a, gpu_b = values
        fmt = lambda value: "NA" if value is None else f"{value:.3f}"
        lines.append(f"| {case} | {fmt(frame_a)} | {fmt(frame_b)} | {(frame_b / frame_a - 1) * 100:+.1f}% | {fmt(cpu_a)} | {fmt(cpu_b)} | {fmt(gpu_a)} | {fmt(gpu_b)} |")
    (output / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    metadata["status"] = "complete" if not failures else "failed"
    metadata["utc_end"] = datetime.now(timezone.utc).isoformat()
    metadata["end_power"] = capture(["pmset", "-g", "batt"])
    metadata["end_thermal"] = capture(["pmset", "-g", "therm"])
    (output / "metadata.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    if failures:
        raise RuntimeError(f"Incomplete experiment: {len(failures)} failed runs; see failures.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resume", action="store_true", help="仅保留已完成整组的场景，重测被暂停的场景")
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--optimized", type=Path, required=True)
    parser.add_argument("--baseline-assets", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cases", nargs="+", default=["empty", "mixed1024", "mixed8192", "interleaved", "lasers", "shots", "drops", "bomb", "game", "menu"])
    parser.add_argument("--warm", type=float, default=5)
    parser.add_argument("--measure", type=float, default=30)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--msaa", type=int, choices=[0, 4], default=4)
    parser.add_argument("--fps", type=int, default=0)
    parser.add_argument("--soak", type=float, default=0)
    parser.add_argument("--soak-fps", type=int, default=60)
    run(parser.parse_args())

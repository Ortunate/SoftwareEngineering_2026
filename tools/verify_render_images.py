"""核对 macOS 压测 A/B 截图，仅允许完整游戏右下角实时 FPS 文字发生差异。"""
import argparse
import csv
import hashlib
import json
import struct
import subprocess
import tempfile
from pathlib import Path


def bitmap(path, temporary):
    """用系统 sips 解码为 BMP；返回自上而下的原始像素，不引入 Python 图像依赖。"""
    destination = temporary / (hashlib.sha256(str(path).encode()).hexdigest() + ".bmp")
    subprocess.run(["sips", "-s", "format", "bmp", str(path), "--out", str(destination)],
                   check=True, capture_output=True)
    data = destination.read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if data[:2] != b"BM" or bits not in (24, 32) or compression not in (0, 3):
        raise ValueError("Unsupported BMP conversion")
    channels = bits // 8
    stride = ((width * bits + 31) // 32) * 4
    rows = [data[offset + row * stride:offset + row * stride + width * channels]
            for row in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), channels, rows


def compare(left, right, temporary):
    """报告差异边界及像素数；FPS 放行框按固定 1100×760 逻辑窗口映射到实际像素。"""
    if left.read_bytes() == right.read_bytes():
        return {"identical": True, "changed_pixels": 0, "fps_only": False}
    width, height, channels, first = bitmap(left, temporary)
    other_width, other_height, other_channels, second = bitmap(right, temporary)
    if (width, height, channels) != (other_width, other_height, other_channels):
        raise ValueError("A/B image sizes differ")
    x0, y0, x1, y1 = width, height, -1, -1
    changed = 0
    maximum = 0
    outside_fps = 0
    # GameplayLayout 在 1100×760 下 scale=1，FPS 右对齐于 x=1084，文字顶部 y=697。
    rect = (int(995 * width / 1100), int(692 * height / 760),
            int(1088 * width / 1100), int(720 * height / 760))
    for y, (a, b) in enumerate(zip(first, second)):
        if a == b:
            continue
        for x in range(width):
            start = x * channels
            one, two = a[start:start + 3], b[start:start + 3]
            if one == two:
                continue
            changed += 1
            maximum = max(maximum, *(abs(v - w) for v, w in zip(one, two)))
            x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
            outside_fps += not (rect[0] <= x < rect[2] and rect[1] <= y < rect[3])
    return {"identical": not changed, "changed_pixels": changed, "max_channel_delta": maximum,
            "bbox": [x0, y0, x1, y1] if changed else None,
            "fps_only": changed > 0 and outside_fps == 0, "fps_allow_rect": rect}


def verify(folder):
    """检查配对截图并输出可审阅的 JSON；任何 FPS 框外的差异都导致失败。"""
    results = []
    with tempfile.TemporaryDirectory(prefix="render-image-check-") as temporary:
        rows = list(csv.DictReader((folder / "results.csv").open()))
        expected = sorted({(row["case"], row["repeat"]) for row in rows})
        for case, repeat in expected:
            left = folder / "baseline" / case / repeat / (case + ".png")
            right = folder / "optimized" / case / repeat / (case + ".png")
            if not left.is_file() or not right.is_file():
                raise RuntimeError(f"Missing paired image: {case} repeat={repeat}")
            result = compare(left, right, Path(temporary))
            result.update(case=left.stem, repeat=left.parent.name)
            result["passed"] = result["identical"] or (left.stem == "game" and result["fps_only"])
            results.append(result)
    (folder / "image-verification.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    if not results or not all(row["passed"] for row in results):
        raise RuntimeError("A/B image regression; inspect image-verification.json")
    print(f"PASS: {len(results)} paired images; only Game FPS text may differ")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    verify(parser.parse_args().folder.resolve())

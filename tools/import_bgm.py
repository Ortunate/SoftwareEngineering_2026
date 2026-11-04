#!/usr/bin/env python3
# 本文件负责离线校验、无损转码和批量发布 BGM；不解包 MD、不修改原包或删除旧 MP3。
"""从已解包的 .pos 和 Nintendo Opus 生成运行时 FLAC、.loop 及可复核报告。"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import tempfile

from rewrap_native_opus import packet_samples, rewrap


EXPECTED_EXE_SHA256 = '48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82'
CATALOG = Path(__file__).resolve().parents[1] / 'src/resources/music_catalog.hpp'
# .pos 为 44100 Hz 起点和绝对排他终点；原生 movsxd 将字段符号扩展，因此只接受非负 int32。
POSITION_RATE = 44100
OUTPUT_RATE = 48000
CHANNELS = 2
SAMPLE_BYTES = 2
PRE_SKIP = 120


def sha256(path):
    """流式计算文件 SHA-256；不把本机路径写入报告。"""
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def catalog_paths(text):
    """按 kTracks 表顺序解析恰好 17 个相对 FLAC 路径；拒绝逃逸、重复及不支持的表结构。"""
    table = re.search(r'\bkTracks\s*\{\{(.*?)\}\};', text, re.S)
    if table is None:
        raise ValueError('缺少 kTracks 静态目录表')
    entries = re.findall(r'\{\s*"[^"\n]*"\s*,\s*"[^"\n]*"\s*,\s*"([^"\n]+)"\s*\}', table[1])
    if len(entries) != 17 or len(set(entries)) != 17:
        raise ValueError('目录必须包含 17 个互异曲目')
    for name in entries:
        path = PurePosixPath(name)
        if (path.is_absolute() or '..' in path.parts or '\\' in name
                or path.suffix != '.flac' or str(path) != name or ':' in name):
            raise ValueError('目录路径必须是规范的相对 .flac 路径')
    return entries


def read_positions(data):
    """读取恰好两枚 little-endian uint32；字段是 44100 Hz 起点/绝对排他终点，只接受非负 int32。"""
    if len(data) != 8:
        raise ValueError('.pos 必须恰好为 8 字节')
    start, end = struct.unpack('<II', data)
    if not 0 <= start < end <= 0x7fffffff:
        raise ValueError('.pos 必须满足 0 <= start < end <= INT32_MAX')
    return start, end


def convert_positions(values, pcm_frames):
    """44100 Hz 坐标两端向下取整到 48000 Hz 帧；原生 EOF 短读回起点，因此终点截到 PCM 长度。"""
    start, end = values
    if not (0 <= start < end <= 0x7fffffff) or pcm_frames <= 0:
        raise ValueError('非法原始坐标或 PCM 帧数')
    # 不通过浮点秒数中转，避免长曲边界舍入；pre-skip 已由 Opus 解码器消费，不重复扣除。
    converted = tuple(value * 480 // 441 for value in values)
    effective = (converted[0], min(converted[1], pcm_frames))
    if not 0 <= effective[0] < effective[1] <= pcm_frames:
        raise ValueError('循环范围必须落在解码 PCM 内且非空')
    return effective


def loop_bytes(start, end):
    """输出 UTF-8/ASCII 旁路协议；start/end 为 48000 Hz 帧，end 排他。"""
    return f'KOUMAKYOU_BGM_LOOP 1\n48000 {start} {end}\n'.encode('ascii')


def validate_loop(data, pcm_frames):
    """严格校验两行旁路文件及范围，返回循环起止帧；不接受多余字节。"""
    match = re.fullmatch(rb'KOUMAKYOU_BGM_LOOP 1\n48000 (0|[1-9][0-9]*) (0|[1-9][0-9]*)\n', data)
    if not match:
        raise ValueError('非法 .loop 格式或长度')
    start, end = map(int, match.groups())
    if not 0 <= start < end <= pcm_frames:
        raise ValueError('非法 .loop 帧范围')
    return start, end


def prepare_opus(data):
    """校验 Nintendo 容器并返回 Ogg 与预期解码帧数；此来源必须双声道、48 kHz、pre-skip=120。"""
    ogg = rewrap(data)
    if data[9] != CHANNELS or struct.unpack_from('<H', data, 28)[0] != PRE_SKIP:
        raise ValueError('源 Opus 必须双声道且 pre-skip=120')
    at, frames = 40, 0
    while at < len(data):
        size = struct.unpack_from('>I', data, at)[0]
        frames += packet_samples(data[at + 8:at + 8 + size])
        at += 8 + size
    frames -= PRE_SKIP
    if frames <= 0:
        raise ValueError('pre-skip 后没有 PCM')
    return ogg, frames


def flac_frames(path):
    """读取 FLAC STREAMINFO，要求 48 kHz 双声道 16 bit 和明确的正帧数。"""
    with path.open('rb') as stream:
        header = stream.read(42)
    if (len(header) != 42 or header[:4] != b'fLaC' or header[4] & 127 != 0
            or int.from_bytes(header[5:8], 'big') != 34):
        raise ValueError('FLAC STREAMINFO 缺失或截断')
    packed = int.from_bytes(header[18:26], 'big')
    rate, channels, bits, frames = packed >> 44, ((packed >> 41) & 7) + 1, ((packed >> 36) & 31) + 1, packed & ((1 << 36) - 1)
    if (rate, channels, bits) != (OUTPUT_RATE, CHANNELS, 16) or frames <= 0:
        raise ValueError('FLAC 必须为 48 kHz 双声道 s16 且包含帧数')
    return frames


def run_ffmpeg(arguments):
    """执行 PATH 中的 ffmpeg；失败时中止导入，stderr 仅供本地诊断，不进入报告。"""
    result = subprocess.run(['ffmpeg', '-hide_banner', '-nostdin', '-v', 'error', '-xerror', '-y', *map(str, arguments)],
                            capture_output=True)
    if result.returncode:
        raise ValueError('ffmpeg 转码/校验失败：' + result.stderr.decode('utf-8', errors='replace').strip())


def transcode(ogg, flac, scratch, expected_frames):
    """独立解码 Ogg 和 FLAC 到 s16le，比对完整 PCM 长度和 SHA-256，证明无损且 skip 只消费一次。"""
    run_ffmpeg(['-i', ogg, '-map', '0:a:0', '-vn', '-map_metadata', '-1', '-ar', '48000', '-ac', '2',
                '-sample_fmt', 's16', '-c:a', 'flac', flac])
    if flac_frames(flac) != expected_frames:
        raise ValueError('FLAC 帧数与源 Opus 时长/pre-skip 不一致')
    run_ffmpeg(['-i', flac, '-map', '0:a:0', '-c:a', 'pcm_s16le', '-f', 's16le', scratch])
    size = scratch.stat().st_size
    if size != expected_frames * CHANNELS * SAMPLE_BYTES:
        raise ValueError('完整解码 PCM 长度不一致')
    pcm_hash = sha256(scratch)
    independent = scratch.with_suffix('.ogg.pcm')
    run_ffmpeg(['-i', ogg, '-map', '0:a:0', '-ar', '48000', '-ac', '2',
                '-c:a', 'pcm_s16le', '-f', 's16le', independent])
    if independent.stat().st_size != size or sha256(independent) != pcm_hash:
        raise ValueError('Ogg 与 FLAC 独立解码 PCM 不一致')
    return {'frames': expected_frames, 'bytes': size, 'sha256': pcm_hash,
            'ogg_decode_sha256': pcm_hash, 'flac_decode_sha256': pcm_hash}


def publish(outputs):
    """所有内容校验后才调用；逐文件原子替换，新文件保留源权限，普通 I/O 失败回滚旧内容和权限；不承诺断电时跨文件原子性。"""
    prepared, committed = [], []
    try:
        # 先在各目标同一文件系统准备新副本和旧内容备份，准备失败不替换任何目标。
        for source, destination in outputs:
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists() and not destination.is_file():
                raise ValueError('输出目标不是普通文件')
            with tempfile.NamedTemporaryFile(dir=destination.parent, prefix='.bgm-', delete=False) as stream:
                new = Path(stream.name)
            backup = None
            prepared.append((destination, new, backup))
            shutil.copyfile(source, new)
            shutil.copymode(source, new)
            if destination.exists():
                with tempfile.NamedTemporaryFile(dir=destination.parent, prefix='.bgm-old-', delete=False) as stream:
                    backup = Path(stream.name)
                prepared[-1] = (destination, new, backup)
                shutil.copyfile(destination, backup)
                shutil.copymode(destination, backup)
        for destination, new, backup in prepared:
            os.replace(new, destination)
            committed.append((destination, backup))
    except BaseException:
        for destination, backup in reversed(committed):
            if backup is None:
                destination.unlink()
            else:
                os.replace(backup, destination)
        raise
    finally:
        for _, new, backup in prepared:
            new.unlink(missing_ok=True)
            if backup is not None:
                backup.unlink(missing_ok=True)


def import_bgm(native_root, positions, runtime_root, report):
    """导入 1..17，排除 18；四参数均为 Path。所有曲目校验成功后发布素材和 JSON 报告，返回报告对象。"""
    native_root, positions, runtime_root, report = (p.resolve() for p in (native_root, positions, runtime_root, report))
    paths = catalog_paths(CATALOG.read_text(encoding='utf-8'))
    destinations = [runtime_root / name for name in paths]
    targets = [p for flac in destinations for p in (flac, flac.with_suffix('.loop'))] + [report]
    resolved = [p.resolve() for p in targets]
    if len(set(resolved)) != len(resolved):
        raise ValueError('报告和素材输出路径冲突')
    for target in resolved:
        if any(target.is_relative_to(root) for root in (native_root, positions)) or target == CATALOG.resolve():
            raise ValueError('输出不能覆盖源目录或目录表')
    if any(not p.resolve().is_relative_to(runtime_root) for p in targets[:-1]):
        raise ValueError('素材输出不能通过符号链接逃出 runtime-root')
    exe = native_root / 'th06nc.exe'
    exe_hash = sha256(exe)
    if exe_hash != EXPECTED_EXE_SHA256:
        raise ValueError('th06nc.exe SHA-256 与已核对版本不一致')
    md = native_root / 'data/th06MD.dat'
    version = subprocess.run(['ffmpeg', '-version'], check=True, capture_output=True, text=True).stdout.splitlines()[0]
    result = {
        'schema_version': 1, 'hash_algorithm': 'sha256',
        'inputs': {'th06nc.exe': exe_hash, 'data/th06MD.dat': sha256(md)},
        'catalog_sha256': sha256(CATALOG), 'ffmpeg_version': version,
        'conversion': {'status': 'source_verified',
                       'pos_layout': 'nonnegative little-endian int32 start, exclusive_end',
                       'position_rate': POSITION_RATE, 'output_rate': OUTPUT_RATE,
                       'rounding': 'floor(value * 480 / 441)',
                       'pre_skip': PRE_SKIP, 'pre_skip_handling': 'decoder_consumes_once; no_coordinate_subtraction',
                       'channels': CHANNELS, 'sample_format': 's16le', 'loop_end': 'exclusive',
                       'position_variant': 'no-o suffix -> data/bgm; o suffix belongs to data/bgm2 and is excluded',
                       'eof_rule': 'effective_end = min(configured_end, pcm_frames); no_silence_padding'},
        'tracks': [],
    }
    with tempfile.TemporaryDirectory(prefix='bgm-import-') as directory:
        work = Path(directory)
        outputs = []
        for source_id, (name, destination) in enumerate(zip(paths, destinations), 1):
            stem = f'th06_{source_id:02d}'
            print(f'[{source_id:02d}/17] {stem}: 校验并转码', flush=True)
            source_name, pos_name = f'data/bgm/{stem}.opus', f'{stem}.pos'
            source_data = (native_root / source_name).read_bytes()
            pos_data = (positions / pos_name).read_bytes()
            values = read_positions(pos_data)
            ogg, expected = prepare_opus(source_data)
            start, end = convert_positions(values, expected)
            ogg_path, flac_path = work / f'{stem}.ogg', work / f'{stem}.flac'
            ogg_path.write_bytes(ogg)
            pcm = transcode(ogg_path, flac_path, work / 'verify.pcm', expected)
            loop_path = work / f'{stem}.loop'
            loop_path.write_bytes(loop_bytes(start, end))
            validate_loop(loop_path.read_bytes(), pcm['frames'])
            result['tracks'].append({
                'source_id': source_id, 'source': source_name, 'source_sha256': hashlib.sha256(source_data).hexdigest(),
                'position': pos_name, 'position_sha256': hashlib.sha256(pos_data).hexdigest(),
                'position_raw': list(values), 'position_raw_hex': pos_data.hex(), 'runtime': name, 'runtime_sha256': sha256(flac_path),
                'loop': str(PurePosixPath(name).with_suffix('.loop')), 'loop_sha256': sha256(loop_path),
                'configured_loop_frames': [value * 480 // 441 for value in values],
                'effective_loop_frames': [start, end], 'pcm': pcm,
            })
            outputs.extend(((flac_path, destination), (loop_path, destination.with_suffix('.loop'))))
            print(f'[{source_id:02d}/17] PCM={expected} frames; loop={start}..{end}; 双路 PCM 一致', flush=True)
        staged_report = work / 'report.json'
        staged_report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        print('17 曲全部校验通过，开始发布 FLAC、loop 和报告', flush=True)
        publish(outputs + [(staged_report, report)])
        print('发布完成', flush=True)
    return result


def main():
    """CLI 仅接收原包根、已解包 .pos 目录、运行素材根和报告路径；不会自动解包或删除旧文件。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('native-root', 'positions', 'runtime-root', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        import_bgm(args.native_root, args.positions, args.runtime_root, args.report)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'BGM 导入失败：{error}\n')


if __name__ == '__main__':
    main()

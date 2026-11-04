#!/usr/bin/env python3
# 本文件用合成 Nintendo Opus、pos 和临时目录验证导入、边界、发布失败回滚及独立 Ogg CRC。
from pathlib import Path
import hashlib
import json
import random
import shutil
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import import_bgm as subject
import rewrap_native_opus as opus


def native_opus(packets=(b'\xfc\xff\xfe',), channels=2, skip=120):
    """合成 Nintendo 头及有效的 20 ms 静音包；不读取原游戏素材。"""
    payload = b''.join(struct.pack('>II', len(p), 0) + p for p in packets)
    return (struct.pack('<IIBB2xIIIIH2x', 0x80000001, 24, 0, channels, 48000, 32, 0, 0, skip)
            + struct.pack('<II', 0x80000004, len(payload)) + payload)


def bitwise_crc(data):
    """独立逐位基准；不读取生产查表及其表生成函数。"""
    remainder = 0
    for byte in data:
        remainder ^= byte << 24
        for bit in range(8):
            top = remainder & 0x80000000
            remainder = (remainder << 1) & 0xffffffff
            if top:
                remainder ^= 0x04c11db7
    return remainder


class ImportBgmTests(unittest.TestCase):
    """格式和生命周期测试独立于安装目录，真实解码测试仅依赖 PATH 中 ffmpeg。"""

    def test_position_layout_signed_limit_and_length(self):
        self.assertEqual(subject.read_positions(struct.pack('<II', 1, 0x7fffffff)), (1, 0x7fffffff))
        for data in (b'', bytes(7), bytes(9), struct.pack('<II', 1, 1),
                     struct.pack('<II', 2, 1), struct.pack('<II', 0, 0x80000000),
                     struct.pack('<II', 0xffffffff, 0xffffffff)):
            with self.subTest(data=data), self.assertRaises(ValueError):
                subject.read_positions(data)

    def test_floor_boundaries_and_no_second_preskip(self):
        for start, end, expected in ((0, 1, (0, 1)), (5, 6, (5, 6)),
                                     (440, 441, (478, 480)), (441, 442, (480, 481)),
                                     (882, 883, (960, 961))):
            self.assertEqual(subject.convert_positions((start, end), 10000), expected)
        self.assertEqual(subject.convert_positions((0, 0x7fffffff), 3000000000), (0, 2337397166))
        for values, frames in (((1, 1), 100), ((-1, 2), 100), ((0, 0x80000000), 10),
                               ((441, 882), 480), ((0, 1), 0)):
            with self.subTest(values=values), self.assertRaises(ValueError):
                subject.convert_positions(values, frames)

    def test_eof_short_read_uses_pcm_end_without_padding(self):
        # 原生 07 的 configuredEnd 超 EOF；原始两字段不能被有效终点覆盖。
        raw = (926100, 4552468)
        self.assertEqual(raw[1] * 480 // 441, 4955067)
        self.assertEqual(subject.convert_positions(raw, 4954440), (1008000, 4954440))

    def test_loop_exact_protocol_and_ranges(self):
        data = subject.loop_bytes(120, 840)
        self.assertEqual(data, b'KOUMAKYOU_BGM_LOOP 1\n48000 120 840\n')
        self.assertEqual(subject.validate_loop(data, 840), (120, 840))
        for bad in (data + b'\n', data[:-1], data.replace(b'48000', b'44100'),
                    data.replace(b'120', b'-1'), data.replace(b'840', b'841'),
                    data.replace(b'120', b'0120'), data.replace(b'120', b'840')):
            with self.subTest(data=bad), self.assertRaises(ValueError):
                subject.validate_loop(bad, 840)

    def test_catalog_order_and_path_safety(self):
        paths = subject.catalog_paths(subject.CATALOG.read_text())
        self.assertEqual(len(paths), 17)
        self.assertEqual(paths[15:], ['stages/stage_extra/event_theme.flac', 'endings/ending.flac'])
        text = subject.CATALOG.read_text()
        for bad in ('../bad.flac', '/bad.flac', 'common/../bad.flac', 'bad.mp3'):
            with self.subTest(path=bad), self.assertRaises(ValueError):
                subject.catalog_paths(text.replace(paths[0], bad))
        with self.assertRaises(ValueError):
            subject.catalog_paths(text.replace(paths[0], paths[1]))

    def test_opus_duration_preskip_and_truncation(self):
        data = native_opus()
        ogg, frames = subject.prepare_opus(data)
        self.assertTrue(ogg.startswith(b'OggS'))
        self.assertEqual(frames, 840)
        self.assertEqual(subject.prepare_opus(native_opus((b'\xfc\xff\xfe',) * 2))[1], 1800)
        for bad in (data[:-1], data + b'x', data[:39], native_opus(channels=1),
                    native_opus(skip=0), native_opus(packets=()), native_opus((b'\xfb',)),
                    native_opus((b'\xfb\x3f',))):
            with self.subTest(data=bad), self.assertRaises(ValueError):
                subject.prepare_opus(bad)

    def test_ogg_crc_against_independent_bitwise_for_lacing_boundaries(self):
        rng = random.Random(123)
        for size in (0, 1, 254, 255, 256, 510, 4096, 65024):
            packet = bytes(rng.randrange(256) for _ in range(size))
            page = opus.page(packet, 2, 960, 4)
            actual = struct.unpack_from('<I', page, 22)[0]
            page[22:26] = bytes(4)
            self.assertEqual(actual, bitwise_crc(page))
            self.assertEqual(page[27 + page[26]:], packet)
        with self.assertRaises(ValueError):
            opus.page(bytes(65025), 2, 960, 4)

    def test_every_rewrapped_page_crc_and_final_granule(self):
        ogg, frames = subject.prepare_opus(native_opus((b'\xfc\xff\xfe',) * 2))
        at, sequence = 0, 0
        while at < len(ogg):
            segments = ogg[at + 26]
            end = at + 27 + segments + sum(ogg[at + 27:at + 27 + segments])
            page = bytearray(ogg[at:end])
            checksum = struct.unpack_from('<I', page, 22)[0]
            page[22:26] = bytes(4)
            self.assertEqual(checksum, bitwise_crc(page))
            self.assertEqual(struct.unpack_from('<I', page, 18)[0], sequence)
            sequence += 1
            at = end
        self.assertEqual(struct.unpack_from('<Q', page, 6)[0] - 120, frames)
        self.assertEqual(page[5], 4)

    @unittest.skipUnless(shutil.which('ffmpeg'), '需要 ffmpeg 执行合成音频完整转码')
    def test_real_ffmpeg_synthetic_opus_to_flac(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            ogg, frames = subject.prepare_opus(native_opus((b'\xfc\xff\xfe',) * 3))
            source = root / 'test.ogg'
            source.write_bytes(ogg)
            pcm = subject.transcode(source, root / 'test.flac', root / 'test.pcm', frames)
            self.assertEqual(pcm['frames'], 2760)
            self.assertEqual(pcm['bytes'], 11040)
            self.assertEqual(pcm['ogg_decode_sha256'], pcm['flac_decode_sha256'])

    def test_flac_metadata_rejects_wrong_format_and_truncation(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'test.flac'
            for rate, channels, bits, frames in ((48000, 2, 16, 840), (44100, 2, 16, 840),
                                                 (48000, 1, 16, 840), (48000, 2, 24, 840),
                                                 (48000, 2, 16, 0)):
                packed = (rate << 44) | ((channels - 1) << 41) | ((bits - 1) << 36) | frames
                data = b'fLaC' + bytes([128, 0, 0, 34]) + bytes(10) + packed.to_bytes(8, 'big') + bytes(16)
                path.write_bytes(data)
                if (rate, channels, bits, frames) == (48000, 2, 16, 840):
                    self.assertEqual(subject.flac_frames(path), 840)
                else:
                    with self.assertRaises(ValueError):
                        subject.flac_frames(path)
            for bad in (b'', data[:41], b'xxxx' + data[4:], data[:5] + bytes(3) + data[8:]):
                path.write_bytes(bad)
                with self.assertRaises(ValueError):
                    subject.flac_frames(path)

    def test_independent_pcm_mismatch_or_short_read_aborts(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for bad in (b'wrong-size', b'xxxx'):
                def fake_ffmpeg(arguments):
                    """将差异注入独立 Ogg 解码输出，模拟等长数据损坏或短读。"""
                    output = Path(arguments[-1])
                    output.write_bytes(bad if output.name.endswith('.ogg.pcm') else b'good')

                with patch.object(subject, 'run_ffmpeg', side_effect=fake_ffmpeg), \
                     patch.object(subject, 'flac_frames', return_value=1), self.assertRaisesRegex(ValueError, 'PCM 不一致'):
                    subject.transcode(root / 'input.ogg', root / 'output.flac', root / 'pcm', 1)
            with patch.object(subject, 'run_ffmpeg'), \
                 patch.object(subject, 'flac_frames', return_value=2), self.assertRaisesRegex(ValueError, '帧数'):
                subject.transcode(root / 'input.ogg', root / 'output.flac', root / 'pcm', 1)

    def test_publish_rolls_back_and_preserves_unrelated_mp3(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source, first, second, mp3 = (root / name for name in ('source', 'first', 'second', 'old.mp3'))
            for path, data in ((source, b'new'), (first, b'old'), (mp3, b'music')):
                path.write_bytes(data)
            source.chmod(0o644)
            first.chmod(0o640)
            original = subject.os.replace
            calls = 0

            def fail_second(src, dst):
                """模拟发布到第二个文件时发生普通 I/O 失败。"""
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError('injected failure')
                return original(src, dst)

            with patch.object(subject.os, 'replace', side_effect=fail_second), self.assertRaises(OSError):
                subject.publish([(source, first), (source, second)])
            self.assertEqual(first.read_bytes(), b'old')
            self.assertEqual(first.stat().st_mode & 0o777, 0o640)
            self.assertFalse(second.exists())
            self.assertEqual(mp3.read_bytes(), b'music')
            self.assertEqual(sorted(p.name for p in root.iterdir()), ['first', 'old.mp3', 'source'])
            subject.publish([(source, first), (source, second)])
            self.assertEqual(first.stat().st_mode & 0o777, 0o644)
            self.assertEqual(second.stat().st_mode & 0o777, 0o644)

    def test_batch_report_and_validation_before_publish(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            native, positions, runtime = (root / name for name in ('native', 'positions', 'runtime'))
            (native / 'data/bgm').mkdir(parents=True)
            positions.mkdir()
            (native / 'th06nc.exe').write_bytes(b'synthetic-exe')
            (native / 'data/th06MD.dat').write_bytes(b'synthetic-md')
            for number in range(1, 18):
                (native / f'data/bgm/th06_{number:02d}.opus').write_bytes(native_opus())
                (positions / f'th06_{number:02d}.pos').write_bytes(struct.pack('<II', 5, 1000))
            report = runtime / 'report.json'

            def fake_transcode(ogg, flac, scratch, expected):
                """隔离批量发布测试和编解码器；格式真实性另由真实 ffmpeg 用例检验。"""
                flac.write_bytes(b'synthetic-flac')
                return {'frames': expected, 'bytes': expected * 4, 'sha256': 'synthetic-pcm'}

            with patch.object(subject, 'EXPECTED_EXE_SHA256', subject.sha256(native / 'th06nc.exe')), \
                 patch.object(subject.subprocess, 'run') as run, \
                 patch.object(subject, 'transcode', side_effect=fake_transcode):
                run.return_value.stdout = 'ffmpeg version synthetic\n'
                result = subject.import_bgm(native, positions, runtime, report)
                self.assertEqual(len(result['tracks']), 17)
                self.assertEqual(result['conversion']['pos_layout'], 'nonnegative little-endian int32 start, exclusive_end')
                track = result['tracks'][6]
                self.assertEqual(track['position_raw'], [5, 1000])
                self.assertEqual(track['configured_loop_frames'], [5, 1088])
                self.assertEqual(track['effective_loop_frames'], [5, 840])
                encoded = report.read_text()
                self.assertNotIn(str(root), encoded)
                self.assertEqual(json.loads(encoded), result)
                self.assertEqual(result['inputs']['data/th06MD.dat'], hashlib.sha256(b'synthetic-md').hexdigest())
                self.assertEqual(subject.sha256(runtime / track['runtime']), track['runtime_sha256'])
                self.assertEqual(subject.sha256(runtime / track['loop']), track['loop_sha256'])
                before = {p: p.read_bytes() for p in runtime.rglob('*') if p.is_file()}
                (positions / 'th06_17.pos').write_bytes(bytes(7))
                with self.assertRaises(ValueError):
                    subject.import_bgm(native, positions, runtime, report)
                self.assertEqual(before, {p: p.read_bytes() for p in runtime.rglob('*') if p.is_file()})
                with self.assertRaises(ValueError):
                    subject.import_bgm(native, positions, native, report)


if __name__ == '__main__':
    unittest.main()

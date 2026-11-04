# 本文件用独立构造的 ECL 检查逐关审阅工具的边界、难度闭包与未知指令保留，不依赖原游戏安装。
import importlib.util
from pathlib import Path
import struct
import json
import subprocess
import sys
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    'stage_script_inspection', Path(__file__).parents[1] / 'tools/inspect_stage_scripts.py')
INSPECTOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INSPECTOR)


def instruction(opcode, payload=b'', mask=0xff00, frame=0):
    """构造保留原始参数字节的指令；mask 属于源文件，不从待测解析器导出。"""
    return struct.pack('<iHHHH', frame, opcode, 12 + len(payload), mask, 0xff) + payload


def fixture(programs, timeline=None):
    """打包互不重叠的子程序与单条时间轴；每个区段单独写源格式终止标记。"""
    header_end = 16 + 4 * len(programs)
    offsets, contents = [], b''
    for program in programs:
        offsets.append(header_end + len(contents))
        contents += program + struct.pack('<iHHHH', -1, 65535, 12, 0xff00, 0xff)
    if timeline is None:
        timeline = struct.pack('<hhHH3fhhI', 240, 0, 4, 28, -999, -32, 0, 70, 0, 700)
    header = struct.pack('<HH3I', len(programs), 0, header_end + len(contents), 0, 0)
    header += struct.pack(f'<{len(offsets)}I', *offsets)
    return header + contents + timeline + struct.pack('<hh', -1, 0)


class StageScriptInspectionTests(unittest.TestCase):
    """静态报告只证明记录结构与保守闭包；测试不把它当作运行时行为解释器。"""

    def read(self, data):
        """在临时文件中调用公开读取入口；调用结束即释放所有输入文件。"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.ecl'
            path.write_bytes(data)
            return INSPECTOR.read_ecl(path)

    def test_masks_links_and_distinct_spawn_counts(self):
        # Normal 从 Sub0 调用 Sub1；Sub1 的使魔出生和死亡回调都指向 Sub2。
        # Easy 独有的 Sub3 不属于 Normal 闭包，但审阅器必须保留其原始记录。
        root = instruction(35, struct.pack('<iif', 1, 0, 0))
        root += instruction(39, struct.pack('<iifii', 3, 0, 0, -10001, 0), 0x0100)
        child = instruction(95, struct.pack('<ifffhhI', 2, 10, 20, 0, 50, -2, 100))
        child += instruction(108, struct.pack('<i', 2))
        data = self.read(fixture([root, child, instruction(1, struct.pack('<i', 0)), b'']))
        normal = INSPECTOR.inspect(data)
        self.assertEqual(normal['timeline_spawn_records'], 1)
        self.assertEqual(normal['roots'], [0])
        self.assertEqual(normal['conservative_reachable_subs'], [0, 1, 2])
        self.assertEqual(normal['reachable_instruction_records'], 4)
        self.assertEqual(INSPECTOR.inspect(data, 0x0100)['conservative_reachable_subs'], [0, 1, 2, 3])
        spawn = data['timelines'][0]['records'][0]
        self.assertEqual(spawn['position'], (-999, -32, 0))
        self.assertEqual((spawn['health'], spawn['item'], spawn['score']), (70, 0, 700))
        self.assertEqual(data['programs'][0][1]['rank_mask'], 0x0100)
        self.assertEqual(len(data['sha256']), 64)

    def test_cycles_and_disabled_callbacks_do_not_expand_forever(self):
        first = instruction(35, struct.pack('<iif', 1, 0, 0))
        second = instruction(35, struct.pack('<iif', 0, 0, 0))
        second += instruction(116, struct.pack('<i', -1))
        self.assertEqual(INSPECTOR.inspect(self.read(fixture([first, second])))['conservative_reachable_subs'], [0, 1])

    def test_jump_offsets_are_relative_to_current_instruction(self):
        prefix = instruction(0)
        jump = instruction(2, struct.pack('<ii', 0, -len(prefix)))
        data = self.read(fixture([prefix + jump]))
        self.assertEqual(data['programs'][0][1]['target_offset'], data['programs'][0][0]['offset'])
        with self.assertRaisesRegex(ValueError, '指令起点'):
            self.read(fixture([prefix + instruction(2, struct.pack('<ii', 0, -1))]))

    def test_unknown_extensions_are_preserved_without_guessed_semantics(self):
        raw = bytes.fromhex('0102030405060708')
        data = self.read(fixture([instruction(201, raw) + instruction(121, struct.pack('<ii', 0, 1))]))
        result = INSPECTOR.inspect(data)
        self.assertEqual(result['opcode_counts'], {121: 1, 201: 1})
        self.assertEqual(result['native_extensions'][0]['payload'], raw.hex())
        self.assertNotIn('target_sub', data['programs'][0][0])

    def test_default_health_is_not_invented(self):
        timeline = struct.pack('<hhHH3f', 20, 0, 1, 20, 1, 2, 3)
        spawn = self.read(fixture([b''], timeline))['timelines'][0]['records'][0]
        self.assertIsNone(spawn['health'])
        self.assertIsNone(spawn['item'])
        self.assertIsNone(spawn['score'])

    def test_cli_all_preserves_single_difficulty_format(self):
        # 同一源文件只生成一份原始记录；覆盖清单按 E/N/H/L 顺序分别保存。
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'fixture.ecl'
            path.write_bytes(fixture([instruction(0)]))
            script = Path(__file__).parents[1] / 'tools/inspect_stage_scripts.py'
            for difficulty, masks in [('all', [0x100, 0x200, 0x400, 0x800]), ('H', [0x400])]:
                output = root / difficulty
                subprocess.run([sys.executable, str(script), str(path), '--output', str(output),
                                '--difficulty', difficulty], check=True, capture_output=True)
                report = json.loads((output / 'coverage.json').read_text())
                self.assertEqual([entry['difficulty_mask'] for entry in report], masks)
                self.assertTrue((output / 'fixture.json').exists())

    def test_invalid_inputs_fail_closed(self):
        valid = fixture([instruction(0)])
        for data in (b'', valid[:16], valid[:-4]):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                self.read(data)
        for target in (-2, 1):
            with self.subTest(target=target), self.assertRaisesRegex(ValueError, '不存在'):
                self.read(fixture([instruction(35, struct.pack('<iif', target, 0, 0))]))
        with self.assertRaisesRegex(ValueError, '参数长度'):
            self.read(fixture([instruction(35, struct.pack('<i', 0))]))
        for mask in (0, 0x300, 0xffff):
            with self.subTest(mask=mask), self.assertRaises(ValueError):
                INSPECTOR.inspect(self.read(valid), mask)
        nonfinite = struct.pack('<hhHH3fhhI', 20, 0, 0, 28, float('nan'), 0, 0, 1, 0, 0)
        with self.assertRaisesRegex(ValueError, '非有限'):
            self.read(fixture([b''], nonfinite))
        aliased = bytearray(fixture([b'', b'']))
        struct.pack_into('<I', aliased, 20, struct.unpack_from('<I', aliased, 16)[0])
        with self.assertRaisesRegex(ValueError, '区段偏移'):
            self.read(aliased)


if __name__ == '__main__':
    unittest.main()

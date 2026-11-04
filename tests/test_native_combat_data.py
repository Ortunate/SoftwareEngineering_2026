# 本文件用人工构造的 ECL/PE64 检查数值导出器的边界、难度位、原始伤害宽度和四机体识别。
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('native_combat_export', Path(__file__).parents[1] / 'tools/export_native_combat_data.py')
EXPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORT)


class NativeCombatDataTests(unittest.TestCase):
    """不依赖原游戏文件，以不含版权数据的最小二进制样例核对单位和失败条件。"""
    def test_ecl_health_and_difficulty(self):
        def instruction(opcode, payload, mask=0xff00):
            return struct.pack('<iHHHH', 0, opcode, 12 + len(payload), mask, 0xff) + payload
        program = instruction(0, b'', 0x0100) + instruction(0, b'')
        program += instruction(111, struct.pack('<i', 7000))
        program += instruction(113, struct.pack('<i', 900), 0x0200)
        program += instruction(95, struct.pack('<ifffhhI', 0, 1, 2, 0, 100, -1, 300))
        program += struct.pack('<iHHHH', -1, 65535, 12, 0xff00, 0xff)
        timeline = struct.pack('<hhHH3fhhI', 100, 0, 0, 28, 1, 2, 0, 32, -1, 300)
        timeline += struct.pack('<hhHH3f', 200, 0, 1, 20, 1, 2, 0) + struct.pack('<hh', -1, 0)
        data = struct.pack('<HH4I', 1, 0, 20 + len(program), 0, 0, 20) + program + timeline
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.ecl'
            path.write_bytes(data)
            result = EXPORT.read_health(path)
            self.assertEqual(result['sub_count'], 1)
            records = result['records']
            self.assertEqual(records[0]['value'], 7000)
            self.assertEqual(records[0]['normal_index'], 1)
            self.assertEqual(records[1]['normal_index'], 2)
            self.assertEqual(records[1]['difficulties'], ['N'])
            self.assertEqual(records[2]['scope'], 'sub_spawn')
            self.assertEqual(records[2]['value'], 100)
            self.assertEqual(records[3]['frame'], 100)
            self.assertEqual(records[3]['value'], 32)
            self.assertIsNone(records[4]['value'])
            self.assertEqual(records[4]['value_kind'], 'engine_default')
            path.write_bytes(data[:-4])
            with self.assertRaises(ValueError):
                EXPORT.read_health(path)

    def test_four_shot_tables_and_unsigned_damage(self):
        data = bytearray(0x2200)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 0x3c, 0x80)
        data[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<H', data, 0x86, 1)
        struct.pack_into('<H', data, 0x94, 0xf0)
        struct.pack_into('<H', data, 0x98, 0x20b)
        base = 0x140000000
        struct.pack_into('<Q', data, 0xb0, base)
        struct.pack_into('<IIII', data, 0x190, 0x2000, 0x1000, 0x2000, 0x200)
        for identity, kind in enumerate((1, 0, 2, 3)):
            start = 0x500 + identity * 72
            primary = (5, 0, 0, 0, 12, 24, -1.57, 12, 513, 0, 0, 1120, 0)
            secondary = (120 if kind == 3 else 30, 0, 0, 0, 12, 40, -1.57, 3, 6, 1, kind, 1125 if kind == 3 else 1122 if kind == 0 else 1121, -1)
            struct.pack_into('<hh6fHBBhh', data, start, *primary)
            struct.pack_into('<hh6fHBBhh', data, start + 36, *secondary)
            address = base + 0x1000 + start - 0x200
            for level, upper in enumerate((8, 16, 32, 48, 64, 80, 96, 128, 999)):
                struct.pack_into('<iiQ', data, 0x200 + identity * 144 + level * 16, 2, upper, address)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.exe'
            path.write_bytes(data)
            result = EXPORT.read_shots(path)
            self.assertEqual([table['id'] for table in result['tables']], [0, 1, 2, 3])
            self.assertEqual(result['tables'][0]['levels'][0]['shots'][0]['damage'], 513)
            self.assertEqual(result['tables'][0]['levels'][1]['power_min'], 8)
            self.assertEqual(result['tables'][0]['levels'][-1]['power_max'], 128)
            laser = result['tables'][3]['levels'][1]['shots'][1]
            self.assertEqual(laser['damage_basis'], 'per_contact_frame_60hz')
            self.assertIsNone(laser['shots_per_30_frame_window'])
            runtime = Path(directory) / 'runtime.inc'
            stages = [{'records': [], 'sha256': '0' * 64}] * 7
            EXPORT.write_runtime(runtime, stages, result)
            source = runtime.read_text(encoding='utf-8')
            self.assertIn('std::array<StageData,7>', source)
            self.assertEqual(source.count('Integration::DataOnly'), 0)
            self.assertEqual(source.count('Integration::Playable'), 7)
            self.assertIn('513.0F', source)
            self.assertIn('kShot3Power128', source)
            path.write_bytes(data[:0x100])
            with self.assertRaises(ValueError):
                EXPORT.read_shots(path)


if __name__ == '__main__':
    unittest.main()

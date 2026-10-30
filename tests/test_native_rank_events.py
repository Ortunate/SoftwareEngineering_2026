#!/usr/bin/env python3
# 本文件以独立合成PE字节验证rank事件审阅的整段摘要、分派与截断守卫；语义结论仍须核对真实EXE。
import hashlib
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import inspect_native_rank_events as subject


class FakePe:
    """合成RVA直映像；保留真实区间形状，内容是测试标记，不能作为原生机器码语义基准。"""
    base = 0x140000000

    def __init__(self):
        self.data = bytearray(0x427000)
        for index, (start, end, _) in enumerate(subject.CODE_REGIONS.values()):
            self.data[start:end] = bytes([index + 1]) * (end - start)
        for shot, (calc, draw) in enumerate(subject.BOMB_CALLBACKS):
            struct.pack_into('<QQ', self.data, 0x426eb0 + shot * 16, self.base + calc, self.base + draw)
        struct.pack_into('<7I', self.data, 0x45168, *subject.PICKUP_DISPATCH)

    def offset(self, address, size):
        offset = address - self.base
        return offset if 0 <= offset and offset + size <= len(self.data) else None


class NativeRankEventTests(unittest.TestCase):
    def setUp(self):
        # 只在测试内替换摘要为合成标记的独立预期；被测工具的生产CLI始终核验真实EXE摘要。
        pristine = FakePe()
        regions = {name: (start, end, hashlib.sha256(pristine.data[start:end]).hexdigest())
                   for name, (start, end, _) in subject.CODE_REGIONS.items()}
        self.regions = regions
        self.guard = patch.object(subject, 'CODE_REGIONS', regions)
        self.guard.start()
        self.addCleanup(self.guard.stop)

    def test_report_is_limited_to_audited_events_and_builtin_bombs(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['event_rank_delta'], {'pickup': 0, 'miss_confirmation': 0, 'bomb_acceptance': 0})
        self.assertEqual(result['rank_field_rva'], '0x549ecc')
        self.assertEqual(len(result['bomb_callbacks']), 4)
        self.assertEqual(len(result['pickup_dispatch']), 7)
        self.assertIn('not_global_rank_invariance', result['scope'])
        self.assertTrue(result['unresolved'])

    def test_manager_counter_and_rank_are_distinct_with_limited_scope(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['manager_frame'], {
            'callback_rva': '0x3baa0', 'owner_rva': '0x53cab0',
            'frame_counter_offset': '0xd418', 'rank_offset': '0xd41c',
            'top_level_rank_write': False, 'lives_based_periodic_rank_rule': False})
        self.assertIn('not_global_rank_invariance', result['scope'])
        self.assertIn('其他帧管理器、被调函数及间接写入', result['unresolved'])

    def test_every_region_rejects_changes_at_start_middle_and_end(self):
        for name, (start, end, _) in self.regions.items():
            for address in (start, (start + end) // 2, end - 1):
                pe = FakePe();pe.data[address] ^= 1
                with self.subTest(region=name, address=hex(address)), self.assertRaisesRegex(ValueError, '代码摘要'):
                    subject.inspect(pe, require_hash=False)

    def test_late_homing_bomb_fragment_is_not_omitted_at_first_pdata_end(self):
        pe = FakePe();pe.data[0xdd70] ^= 1
        with self.assertRaisesRegex(ValueError, 'homing_bomb'):
            subject.inspect(pe, require_hash=False)

    def test_every_bomb_callback_pointer_is_guarded(self):
        for shot in range(4):
            for field in (0, 8):
                pe = FakePe();pe.data[0x426eb0 + shot * 16 + field] ^= 1
                with self.subTest(shot=shot, field=field), self.assertRaisesRegex(ValueError, '回调表'):
                    subject.inspect(pe, require_hash=False)

    def test_every_pickup_dispatch_target_is_guarded(self):
        for item in range(7):
            pe = FakePe();pe.data[0x45168 + item * 4] ^= 1
            with self.subTest(item=item), self.assertRaisesRegex(ValueError, '分派'):
                subject.inspect(pe, require_hash=False)

    def test_truncated_code_region_is_rejected(self):
        pe = FakePe();del pe.data[0x6a760:]
        with self.assertRaisesRegex(ValueError, '不可读取'):
            subject.inspect(pe, require_hash=False)

    def test_unknown_executable_is_rejected_before_semantic_report(self):
        with self.assertRaisesRegex(ValueError, 'EXE版本未知'):
            subject.inspect(FakePe())

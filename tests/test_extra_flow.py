# 本文件以独立合成MSG验证Extra战后条件、结算后续和静态/动态控制流边界。
import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import inspect_extra_flow as subject


def instruction(frame, opcode, payload=b''):
    """构造原始MSG记录；测试期望不从被测解析器反向生成。"""
    return struct.pack('<HBB', frame, opcode, len(payload)) + payload


def message(records):
    """16分支共享同一只读程序，验证合法别名偏移；只保留源格式的首个结束指令。"""
    return struct.pack('<I16I', 16, *([68] * 16)) + records


class ExtraFlowTests(unittest.TestCase):
    def test_capture_gate_boundaries_and_signed_mode_counter(self):
        for captures in (0, 5, 6, 10, 13):
            for flag in (False, True):
                for counter in (-1, 0, 2, 3, 99):
                    expected = captures >= 6 and (not flag or counter < 3)
                    with self.subTest(captures=captures, flag=flag, counter=counter):
                        self.assertEqual(subject.passes_postfight_gate(captures, flag, counter), expected)

    def test_qualified_gate_ends_before_normal_clear(self):
        data = message(instruction(0, 14) + instruction(0, 9, struct.pack('<i', 0)) +
                       instruction(0, 12) + instruction(60, 11) + instruction(90, 0))
        records = subject.read_msg_branch(data, 1)
        qualified = subject.trace_branch(records, 6, False, 99)
        self.assertEqual((qualified['exit'], qualified['wall_frame']), ('postfight_gate', 0))
        self.assertEqual([event['opcode'] for event in qualified['events']], [14])
        ordinary = subject.trace_branch(records, 5, False, 0)
        self.assertEqual((ordinary['exit'], ordinary['wall_frame']), ('game_state_transition', 60))
        self.assertEqual([event['opcode'] for event in ordinary['events']], [14, 9, 12, 11])

    def test_clear_marker_continues_waits_and_second_transition_is_unreachable(self):
        data = message(instruction(0, 9, struct.pack('<i', 0)) + instruction(0, 12) +
                       instruction(1, 4, struct.pack('<i', 3)) + instruction(1, 11) +
                       instruction(1, 10) + instruction(60, 9, struct.pack('<i', 0)) +
                       instruction(60, 11) + instruction(90, 0))
        records = subject.read_msg_branch(data, 11)
        self.assertEqual(sum(record['opcode'] == 9 for record in records), 2)
        result = subject.trace_branch(records, 0, False, 0)
        self.assertEqual(result['wall_frame'], 4)
        self.assertEqual([event['opcode'] for event in result['events']], [9, 12, 4, 11])

    def test_wait_counter_only_resets_on_text_and_preserves_clock(self):
        text = struct.pack('<hh', 1, 0) + b'TEST\0'
        data = message(instruction(0, 4, struct.pack('<i', 3)) +
                       instruction(0, 4, struct.pack('<i', 5)) + instruction(0, 3, text) +
                       instruction(0, 4, struct.pack('<i', 2)) + instruction(1, 0))
        result = subject.trace_branch(subject.read_msg_branch(data, 0), 0, False, 0)
        waits = [event for event in result['events'] if event['opcode'] == 4]
        self.assertEqual([event['initial_wait'] for event in waits], [0, 3, 0])
        self.assertEqual(result['wall_frame'], 8)

    def test_ten_is_nonadvancing_barrier_zero_is_end(self):
        for opcode, reason in ((10, 'nonadvancing_barrier'), (0, 'dialogue_end')):
            data = message(instruction(0, opcode) + instruction(10, 0))
            result = subject.trace_branch(subject.read_msg_branch(data, 0), 10, False, 0)
            self.assertEqual(result['exit'], reason)
            self.assertEqual(len(result['events']), 1)

    def test_timeline_fourteen_is_ignored_and_does_not_become_jump_target(self):
        records = [{'opcode': 8, 'frame': 8497}, {'opcode': 14, 'frame': 8499},
                   {'opcode': 0, 'frame': 8500, 'target_sub': 97}]
        result = subject.inspect_timeline({'timelines': [{'records': records}]})
        self.assertIsNone(result['jump_marker'])
        self.assertEqual(result['ignored_runtime_records'], [records[1]])
        self.assertEqual(result['postfight_records'], records)
        records.insert(1, {'opcode': 13, 'frame': 8498})
        self.assertIs(subject.inspect_timeline({'timelines': [{'records': records}]})['jump_marker'], records[1])
        with self.assertRaisesRegex(ValueError, '一条时间轴'):
            subject.inspect_timeline({'timelines': []})

    def test_parser_rejects_truncated_unknown_and_backward_records(self):
        valid = message(instruction(0, 0))
        for data in (b'', valid[:67], valid[:-1]):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                subject.read_msg_branch(data, 0)
        for body in (instruction(0, 15), instruction(0, 14, b'\0'),
                     instruction(0, 4, struct.pack('<i', -1)), instruction(1, 6) + instruction(0, 0),
                     instruction(0, 3, b'\0\0\0\0abc'), instruction(0, 6)):
            with self.subTest(body=body), self.assertRaises(ValueError):
                subject.read_msg_branch(message(body), 0)
        bad_offset = bytearray(valid)
        struct.pack_into('<I', bad_offset, 4, 0)
        with self.assertRaisesRegex(ValueError, '偏移'):
            subject.read_msg_branch(bad_offset, 1)

    def test_report_requires_source_identity_and_audits_every_branch(self):
        data = message(instruction(0, 14) + instruction(0, 0))
        ecl = {'sha256': 'fixture', 'timelines': [{'records': []}]}
        with self.assertRaisesRegex(ValueError, '哈希'):
            subject.inspect_sources(ecl, data)
        report = subject.inspect_sources(ecl, data, require_hash=False)
        self.assertEqual([branch['block'] for branch in report['branches']], list(range(16)))
        self.assertEqual([case['exit'] for case in report['branches'][1]['cases']],
                         ['dialogue_end', 'postfight_gate', 'postfight_gate', 'dialogue_end',
                          'postfight_gate', 'dialogue_end'])


if __name__ == '__main__':
    unittest.main()

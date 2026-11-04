#!/usr/bin/env python3
# 本文件以合成地址空间验证难度证据工具拒绝错误锚点、截断和未知版本。
import sys
from pathlib import Path
import struct
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import inspect_native_difficulty as subject


class FakePe:
    base = 0x140000000

    def __init__(self):
        self.data = bytearray(0x428f80)
        for rva, data in subject.ANCHORS.items():
            data = bytes.fromhex(data)
            self.data[rva:rva + len(data)] = data
        for table, entries in ((0x41528, subject.EXTRA_MSG_DISPATCH), (0x38834, subject.EXTRA_TIMELINE_DISPATCH)):
            for opcode, target in entries.items():
                struct.pack_into('<I', self.data, table + opcode * 4, target)
        struct.pack_into('<12i', self.data, 0x428f50, 0, 3, 1, 2, 3, 4, 1, 4, 0, 4, 2, 3)
        for opcode, index in ((65, 58), (66, 59), (6, 4), (124, 108), (54, 47), (58, 51), (94, 78), (77, 103), (100, 104), (107, 91), (118, 105), (88, 100), (90, 101), (92, 102), (95, 79), (99, 83), (127, 120), (129, 112), (132, 115), (133, 116), (135, 118), (200, 119)):
            self.data[0x28e0c + opcode - 1] = index
            struct.pack_into('<I', self.data, 0x28c28 + index * 4, subject.OPCODE_DISPATCH[opcode])
        for rva, value in ((0x353690, 1.0), (0x3536a0, 0.5), (0x353830, 1 / 6)):
            struct.pack_into('<d', self.data, rva, value)
        struct.pack_into('<5i', self.data, 0x34e0e0, 20, 32, 32, 32, 18)
        for index, (rva, label) in enumerate(subject.MODE_LABELS):
            struct.pack_into('<Q', self.data, 0x34ed50 + index * 8, self.base + rva)
            self.data[rva:rva + len(label)] = label
        struct.pack_into('<I', self.data, 0x45168 + 5 * 4, 0x44cb6)
        for index, rva in subject.STAGE6_EXTENSIONS.items():
            struct.pack_into('<Q', self.data, 0x34ddf0 + index * 8, self.base + rva)
        for index, rva in subject.EXTRA_EXTENSIONS.items():
            struct.pack_into('<Q', self.data, 0x34ddf0 + index * 8, self.base + rva)
        for rva, value in subject.EXTRA_CIRCLE_CONSTANTS.items():
            struct.pack_into('<f', self.data, rva, value)
        for index, (variable, (target, field)) in enumerate(subject.EXTRA_CIRCLE_VARIABLES.items()):
            self.data[0x2fed0 + variable + 0x2775] = index
            struct.pack_into('<I', self.data, 0x2fe64 + index * 4, target)
            self.data[target:target + 4] = bytes((0x4d, 0x8d, 0x41, field))
        for index, (variable, target) in enumerate(subject.EXTRA_PURSUER_VARIABLES.items(), start=10):
            self.data[0x2fed0 + variable + 0x2775] = index
            struct.pack_into('<I', self.data, 0x2fe64 + index * 4, target)
        for index, (variable, target) in enumerate(subject.EXTRA_ENCORE_VARIABLES.items(), start=13):
            self.data[0x2fed0 + variable + 0x2775] = index
            struct.pack_into('<I', self.data, 0x2fe64 + index * 4, target)
        for rva, value in subject.EXTRA_ENCORE_CONSTANTS.items():
            struct.pack_into('<f', self.data, rva, value)
        for rva, value in subject.EXTRA_BEAM_SAMPLE_CONSTANTS.items():
            struct.pack_into('<f', self.data, rva, value)
        for rva, value in {**subject.MINE_CONSTANTS, **subject.EXTRA_CAGE_CONSTANTS, **subject.EXTRA_RAIN_CONSTANTS, **subject.EXTRA_FINAL_CONSTANTS}.items():
            struct.pack_into('<f', self.data, rva, value)
        for rva, value in subject.EXTRA_RETIREMENT_CONSTANTS.items():
            struct.pack_into('<f', self.data, rva, value)
        for index, label in enumerate(subject.LABELS):
            at = 0x350998 + 24 * index
            self.data[at:at + len(label)] = label

    def offset(self, address, size):
        offset = address - self.base
        return offset if 0 <= offset and offset + size <= len(self.data) else None


class InspectTests(unittest.TestCase):
    def test_practice_entry_resources_and_tamper_rejection(self):
        result = subject.inspect(FakePe(), require_hash=False)['practice_entry']
        self.assertEqual(result['stage_power'], [0, 64, 128, 128, 128, 128])
        self.assertEqual(result['spell_power_overrides'], {str(i): 32 if i < 2 else 64 for i in range(9)})
        self.assertEqual(result['spell_default_power'], 128)
        self.assertEqual((result['spell_extra_lives'], result['spell_bombs']), (0, 0))
        for rva in (0x178b, 0x3c860, 0x3c932):
            pe = FakePe()
            pe.data[rva + len(bytes.fromhex(subject.ANCHORS[rva])) - 1] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_encounter_records_accept_three_encore_cards_without_expanding_practice_catalog(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['spell_record_count'], 134)
        self.assertEqual(result['spell_record_number_base'], 0)
        self.assertEqual(result['spell_record_stride'], '0x180')
        self.assertEqual(result['spell_record_base_rva'], '0x53d418')
        for at in (0x27929 + 7, 0x27929 + 21, 0x27b51 + 3, 0x27b51 + 20,
                   0x27b51 + 32, 0x27c25 + 19):
            pe = FakePe();pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_extra_award_precedes_exit_and_consumes_marker_once(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_msg_clear_marker_starts_award_counter'], 1)
        self.assertEqual(result['extra_clear_award_requires_counter'], 1)
        self.assertTrue(result['extra_clear_award_increments_counter'])
        self.assertEqual(result['extra_clear_difficulty_multiplier'], 2)
        self.assertEqual(result['extra_clear_bonus_truncates_to'], 10)
        for at in (0x40880 + 2, 0x4110d + 3, 0x41a80 + 7, 0x41a92 + 48,
                   0x41af7 + 2, 0x41b3d + 2, 0x41b71 + 3, 0x41b7b + 3, 0x41b86 + 3):
            pe = FakePe();pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_challenge_gate_counter_has_mode_identity_and_signed_life_credits(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['run_mode_labels'], ['Standard', 'Challenge'])
        self.assertEqual(result['challenge_mode_flag_rva'], '0x53d414')
        self.assertEqual(result['challenge_net_misses_rva'], '0x53cab0')
        self.assertEqual(result['confirmed_miss_counter_delta'], 1)
        self.assertEqual(result['challenge_life_pickup_counter_delta'], -1)
        self.assertFalse(result['standard_life_pickup_changes_miss_counter'])
        self.assertTrue(result['replay_miss_counter_is_signed'])
        for at in (0x75137 + 35, 0x4f332 + 25, 0x6cfc4 + 7, 0x6a20d + 28,
                   0x4433d, 0x44600 + 3, 0x44cb6 + 10, 0x44cb6 + 21, 0x6d01f + 1):
            pe = FakePe();pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for at in (0x34ed50, 0x34ed58, 0x351fd8, 0x351100):
            pe = FakePe();pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '模式标签'):
                subject.inspect(pe, require_hash=False)
        pe = FakePe();struct.pack_into('<I', pe.data, 0x45168 + 5 * 4, 0x44cc7)
        with self.assertRaisesRegex(ValueError, '道具分派'):
            subject.inspect(pe, require_hash=False)

    def test_timeline_waits_and_fixed_enemy_drop_use_actual_native_fields(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertTrue(result['timeline_waits_for_dialogue_release'])
        self.assertTrue(result['timeline_waits_for_boss_deletion'])
        self.assertEqual(result['actor_fixed_drop_field'], '0x10a8')
        self.assertEqual(result['actor_fixed_drop_creator'], '0x43fa0')
        for at in (0x38748 + 9, 0x3876a + 21, 0x38790 + 21,
                   0x37b22 + 7, 0x395e7 + 10, 0x39613 + 3):
            pe = FakePe();pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_extra_portraits_preserve_sparse_native_expression_ids(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_flandre_portraits'], {0: 'face12c_0', 2: 'face12a_0', 3: 'face12b_0'})
        self.assertEqual(result['extra_flandre_portrait_reload_excludes_msg'], [2, 12])
        for at in (0x40969 + 2, 0x3f38f + 2, 0x3f3ae + 2, 0x40769 + 8, 0x4078f + 2,
                   0x350d88 + 11, 0x350da0 + 11, 0x350e70 + 11):
            pe = FakePe();pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_bounds_and_death_mode_three_are_distinct(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['opcode_65_bounds_flag'], '0x01')
        self.assertTrue(result['opcode_66_disables_bounds'])
        self.assertTrue(result['death_mode_3_installs_callback_after_ecl'])
        self.assertTrue(result['death_mode_3_retains_one_health_until_callback'])
        for at in (0x272e0 + 29, 0x272e0 + 44, 0x39884 + 10,
                   0x398cd + 6, 0x398cd + 10, 0x398cd + 19, 0x390ce + 9):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for opcode in (65, 66):
            pe = FakePe()
            pe.data[0x28e0c + opcode - 1] = 0
            with self.subTest(opcode=opcode), self.assertRaisesRegex(ValueError, '指令分派'):
                subject.inspect(pe, require_hash=False)

    def test_item_request_and_integer_random_boundaries(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertTrue(result['opcode_124_spawns_native_item'])
        self.assertEqual(result['opcode_124_item_creator'], '0x43fa0')
        self.assertEqual(result['native_item_pool_capacity'], 1024)
        self.assertTrue(result['integer_random_bound_one_consumes_rng'])
        self.assertFalse(result['integer_random_bound_zero_consumes_rng'])
        for at in (0x28367 + 3, 0x28367 + 14, 0x43ffd + 3, 0x4401a + 4,
                   0x441bf + 2, 0x228f4 + 5, 0x22944 + 1, 0x22944 + 13):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for opcode in (6, 124):
            pe = FakePe()
            pe.data[0x28e0c + opcode - 1] = 0
            with self.subTest(opcode=opcode), self.assertRaisesRegex(ValueError, '指令分派'):
                subject.inspect(pe, require_hash=False)

    def test_acceleration_angle_sentinel(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['acceleration_angle_sentinel'], -999.0)
        self.assertTrue(result['acceleration_sentinel_uses_emission_angle'])
        for at in (0x10ace, 0x10ad7, 0x10ad9, 0x10aeb, 0x10b04, 0x10b22):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_encore_variables_interpolation_and_extensions(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['opcode_54_interpolation_power'], -2)
        self.assertEqual(result['opcode_58_interpolation_power'], 4)
        self.assertEqual(result['extra_encore_spawn_copies_variable_slots'], 16)
        self.assertEqual(result['extra_encore_native_19_restoration_script'], 176)
        self.assertTrue(result['extra_encore_native_17_tailcalls_10'])
        self.assertEqual(result['extra_native_18_zero_health_clock'], 7200)
        for at in (0x37340 + 1, 0x37363 + 5, 0x37393 + 18, 0x374e0 + 4, 0x37535 + 7,
                   0x2fdbc + 6, 0x2fdfb + 3, 0x2fe24 + 15, 0x2fe43 + 3,
                   0x2fc87 + 32, 0x2fcba + 4, 0x2fcd9 + 1, 0x37a8a + 63,
                   0x2719d + 24, 0x2720e + 24, 0x28777 + 10, 0x2876a + 8, 0x2f90a + 22):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_encore_variable_table_constants_and_native_targets_fail_closed(self):
        for opcode in (54, 58):
            pe = FakePe()
            pe.data[0x28e0c + opcode - 1] = 0
            with self.subTest(opcode=opcode), self.assertRaisesRegex(ValueError, '指令分派'):
                subject.inspect(pe, require_hash=False)
        for variable in subject.EXTRA_ENCORE_VARIABLES:
            pe = FakePe()
            pe.data[0x2fed0 + variable + 0x2775] = 0
            with self.subTest(variable=variable), self.assertRaisesRegex(ValueError, '后继变量'):
                subject.inspect(pe, require_hash=False)
        for rva in subject.EXTRA_ENCORE_CONSTANTS:
            pe = FakePe()
            struct.pack_into('<f', pe.data, rva, 0)
            with self.subTest(rva=hex(rva)), self.assertRaisesRegex(ValueError, '常量'):
                subject.inspect(pe, require_hash=False)
        for native in (17, 18, 19):
            pe = FakePe()
            struct.pack_into('<Q', pe.data, 0x34ddf0 + native * 8, pe.base)
            with self.subTest(native=native), self.assertRaisesRegex(ValueError, '扩展'):
                subject.inspect(pe, require_hash=False)

    def test_extra_flow_distinguishes_markers_dispatch_and_msg_exit(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_postfight_gate']['minimum_captures'], 6)
        self.assertEqual(result['extra_postfight_gate']['mode_counter_maximum'], 2)
        self.assertEqual(result['extra_timeline_jump_marker_opcode'], 13)
        self.assertEqual(result['extra_timeline_maximum_dispatched_opcode'], 12)
        self.assertTrue(result['extra_timeline_null_jump_target_preserves_progress'])
        self.assertTrue(result['extra_msg_clear_marker_continues_script'])
        for at in (0x3cd02 + 20, 0x37ce2 + 12, 0x37db2 + 7, 0x41308 + 6,
                   0x41308 + 28, 0x41339 + 1, 0x27c4b + 14, 0x27c4b + 39,
                   0x408b1 + 2, 0x40bd0 + 16, 0x41196 + 9, 0x41477 + 11, 0x414ba + 2):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.subTest(at=hex(at)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_extra_msg_and_timeline_dispatch_fail_closed(self):
        for table, entries in ((0x41528, subject.EXTRA_MSG_DISPATCH), (0x38834, subject.EXTRA_TIMELINE_DISPATCH)):
            for opcode in entries:
                pe = FakePe()
                struct.pack_into('<I', pe.data, table + opcode * 4, 0)
                with self.subTest(table=hex(table), opcode=opcode), self.assertRaisesRegex(ValueError, '流程分派'):
                    subject.inspect(pe, require_hash=False)

    def test_survival_timeout_flag_preserves_callback_before_cleanup_branch(self):
        self.assertTrue(subject.inspect(FakePe(), require_hash=False)['survival_timeout_flag_skips_standard_cleanup'])
        for at in (0x388db + 7, 0x388db + 45, 0x3893b + 1, 0x38965 + 15):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_survival_capture_and_pursuer_variable_boundaries(self):
        self.assertTrue(subject.inspect(FakePe(), require_hash=False)['survival_timeout_preserves_capture_eligibility'])
        for at in (0x38941 + 7, 0x27a7f + 28, 0x27ac0 + 7, 0x2fc5a + 3, 0x2fc69 + 3, 0x2fd8a + 3):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for variable in subject.EXTRA_PURSUER_VARIABLES:
            pe = FakePe()
            pe.data[0x2fed0 + variable + 0x2775] = 0
            with self.assertRaisesRegex(ValueError, '追踪变量'):
                subject.inspect(pe, require_hash=False)

    def test_final_exit_death_order(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertTrue(result['actor_death_checked_after_ecl'])
        self.assertEqual(result['actor_death_mode_three_health'], 1)
        for at in (0x390c4 + 10, 0x394a6, 0x394ff + 5, 0x3959a + 6):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_final_spell_health_clock_and_random_rectangle(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_final_zero_health_clock'], 7200)
        self.assertEqual(result['extra_final_wait_range'], [40, 280])
        for at in (0x3717a + 3, 0x37198 + 46, 0x371d7 + 29, 0x37257 + 21, 0x372b1 + 10, 0x37304 + 31):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<f', pe.data, 0x352cf4, 0)
        with self.assertRaisesRegex(ValueError, '最终符'):
            subject.inspect(pe, require_hash=False)

    def test_catadioptric_bounce_boundaries_and_count(self):
        subject.inspect(FakePe(), require_hash=False)
        for at in (0x10b93 + 24, 0x11028 + 5, 0x1107a + 16,
                   0x1162a + 4, 0x11649 + 13, 0x1168f + 6, 0x116f9 + 5, 0x1170d + 61):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_rain_spawn_finish_and_boost_handoff(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertTrue(result['rain_spawn_finish_runs_motion'])
        self.assertEqual(result['rain_launch_boost_last_frame'], 16)
        self.assertEqual(result['rain_acceleration_first_frame'], 18)
        for at in (0x127fc + 5, 0x12817 + 1, 0x7ac9 + 17, 0x111b4 + 8,
                   0x111cf + 11, 0x111f7 + 4, 0x11245 + 9, 0x1175c + 7):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for at in subject.EXTRA_RAIN_CONSTANTS:
            pe = FakePe()
            struct.pack_into('<f', pe.data, at, 0)
            with self.assertRaisesRegex(ValueError, '彩雨弹运动常量'):
                subject.inspect(pe, require_hash=False)

    def test_cage_emitter_entry_retirement_and_death_mode(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertTrue(result['actor_retirement_requires_entry'])
        self.assertFalse(result['actor_outside_retirement_calls_death'])
        self.assertEqual(result['opcode_132_special_actor_mask'], '0x08')
        self.assertTrue(result['opcode_132_hides_actor_rendering'])
        self.assertEqual(result['opcode_96_flag_field'], '0xbd')
        self.assertEqual(result['actor_first_slot_rva'], '0xaee0b8')
        self.assertEqual(result['actor_cleanup_cursor_rva'], '0xaee175')
        self.assertTrue(result['opcode_96_skips_bosses'])
        self.assertTrue(result['opcode_96_direct_callback_for_noninteractive'])
        self.assertTrue(result['timeout_cleanup_skips_bosses'])
        self.assertTrue(result['opcode_122_callback_after_motion_and_poses'])
        self.assertEqual(result['opcode_107_death_mode_shift'], 5)
        for at in (0x275e5 + 12, 0x28556 + 15, 0x38e9c + 8, 0x38ea7 + 11, 0x38ed1 + 4,
                   0x38ef5 + 1, 0x38f4d + 2, 0x38fa7 + 8, 0x11252 + 10, 0x1126c + 8,
                   0x37c0b + 3, 0x38d3a + 4, 0x270bb + 2, 0x389df + 3, 0x389f5 + 4,
                   0x39bdc + 3, 0x39bfd + 6, 0x28424 + 9, 0x28437 + 7, 0x28464 + 15,
                   0x28019 + 17, 0x28b97 + 9, 0x28b97 + 23):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for at in subject.EXTRA_RETIREMENT_CONSTANTS:
            pe = FakePe()
            struct.pack_into('<f', pe.data, at, 0)
            with self.assertRaisesRegex(ValueError, '演员回收'):
                subject.inspect(pe, require_hash=False)
        for opcode, index in ((107, 91), (132, 115)):
            pe = FakePe()
            struct.pack_into('<I', pe.data, 0x28c28 + index * 4, subject.OPCODE_DISPATCH[127])
            with self.assertRaisesRegex(ValueError, str(opcode)):
                subject.inspect(pe, require_hash=False)

    def test_cage_proximity_acceleration_and_turn(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_extensions'][15], '0x36ec0')
        self.assertEqual(result['extra_cage_acceleration_frames'], 120)
        self.assertEqual(result['extra_cage_source_pool_slots'], 640)
        self.assertEqual(result['extra_cage_constants']['0x352c3c'], 64)
        for at in (0x36f0c + 3, 0x36fb2 + 2, 0x36f50 + 16, 0x36f77 + 11, 0x36fd6 + 21, 0x36ff1 + 7,
                   0x37044 + 39, 0x3708d + 8, 0x370e7 + 3, 0x37109 + 7):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for at in subject.EXTRA_CAGE_CONSTANTS:
            pe = FakePe()
            struct.pack_into('<f', pe.data, at, 0)
            with self.assertRaisesRegex(ValueError, '笼目常量'):
                subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<Q', pe.data, 0x34ddf0 + 15 * 8, pe.base + subject.EXTRA_EXTENSIONS[14])
        with self.assertRaisesRegex(ValueError, 'Extra扩展15'):
            subject.inspect(pe, require_hash=False)

    def test_clone_effects_and_random_period(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertTrue(result['random_period_uses_integer_modulo'])
        self.assertEqual(result['actor_effect_bytes'], {'defeat': '0x38', 'drop': '0x39'})
        self.assertEqual(result['opcode_118_external_effect_target'], '0x2af80')
        for at in (0x26715 + 5, 0x26715 + 13, 0x3951f + 3, 0x395f2 + 13,
                   0x267bc + 9, 0x26811 + 21, 0x27cf3 + 22):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for opcode, index in ((94, 78), (77, 103), (100, 104), (118, 105)):
            pe = FakePe()
            struct.pack_into('<I', pe.data, 0x28c28 + index * 4, subject.OPCODE_DISPATCH[127])
            with self.assertRaisesRegex(ValueError, str(opcode)):
                subject.inspect(pe, require_hash=False)

    def test_controlled_laser_rotation_anchor_and_forced_fade(self):
        self.assertTrue(subject.inspect(FakePe(), require_hash=False)['laser_force_fade_resets_clock'])
        self.assertTrue(subject.inspect(FakePe(), require_hash=False)['laser_fade_expiration_checks_before_counter_increment'])
        for at in (0x26e2e + 11, 0x26edd + 27, 0x26f07 + 4, 0x26f62 + 18, 0x26f8e + 3,
                   0x12055 + 7, 0x12055 + 17, 0x11e44 + 18):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<I', pe.data, 0x28c28 + 102 * 4, subject.OPCODE_DISPATCH[127])
        with self.assertRaisesRegex(ValueError, '92'):
            subject.inspect(pe, require_hash=False)

    def test_extra_beam_sampling_rejects_changed_slots_step_and_endpoint(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_beam_sampling_slots'], 8)
        self.assertEqual(result['extra_beam_sampling_distance_pixels'], 48)
        pe = FakePe()
        struct.pack_into('<f', pe.data, 0x352c20, 64)
        with self.assertRaisesRegex(ValueError, '采样间距'):
            subject.inspect(pe, require_hash=False)
        for at in (0x36db7 + 1, 0x36dd0 + 16, 0x36de9 + 12, 0x36e0a + 11, 0x36e5d + 12, 0x36e5d + 22):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_extra_bomb_recovery_rejects_changed_counter_and_animation(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_bomb_recovery_frames'], 60)
        self.assertEqual(result['extra_extensions'][10], '0x36970')
        for at in (0x369b0 + 4, 0x36a05 + 21, 0x36a2f + 15, 0x36a60 + 4, 0x36ab5 + 13):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_auxiliary_slots_and_independent_interrupt(self):
        self.assertEqual(subject.inspect(FakePe(), require_hash=False)['auxiliary_animation_slots'], 8)
        for at in (0x24f1a + 23, 0x284a3 + 18, 0x378f2 + 3):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_timeout_callback_arm_and_boundary(self):
        self.assertEqual(subject.inspect(FakePe(), require_hash=False)['opcode_133_arm_death_timeout_target'], '0x28575')
        pe = FakePe()
        struct.pack_into('<I', pe.data, 0x28c28 + 116 * 4, subject.OPCODE_DISPATCH[127])
        with self.assertRaisesRegex(ValueError, '133'):
            subject.inspect(pe, require_hash=False)
        for at in (0x28575 + 8, 0x388a6 + 10, 0x388e3 + 4):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_negative_spawn_health_preserves_template(self):
        self.assertEqual(subject.inspect(FakePe(), require_hash=False)['negative_spawn_health_preserves_template'], 1)
        for at in (0x37a07, 0x37a61, 0x37b31, 0x37952 + 6):
            pe = FakePe()
            pe.data[at] ^= 1
            with self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_extra_extension_identity_and_circle(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_extensions'], {6: '0x35780', 10: '0x36970', 13: '0x36c00', 14: '0x36d80', 15: '0x36ec0', 16: '0x37170', 17: '0x37340', 18: '0x37350', 19: '0x373f0'})
        self.assertEqual(result['extra_circle_variable_fields'][-10004], '0x54')
        pe = FakePe()
        struct.pack_into('<Q', pe.data, 0x34ddf0 + 13 * 8, pe.base + subject.STAGE6_EXTENSIONS[7])
        with self.assertRaisesRegex(ValueError, 'Extra扩展13'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<f', pe.data, 0x352cb0, 120.0)
        with self.assertRaisesRegex(ValueError, 'Extra圆周常量'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        pe.data[0x36c22 + 1] ^= 1  # 篡改源计数器除6的乘法常量。
        with self.assertRaisesRegex(ValueError, '锚点'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        pe.data[0x2fed0 - 10004 + 0x2775] = 3
        with self.assertRaisesRegex(ValueError, '变量映射'):
            subject.inspect(pe, require_hash=False)

    def test_flandre_visual_extension_and_flag_reset(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['extra_extension_6_effect_pool_capacity'], 512)
        self.assertEqual(result['opcode_135_actor_flag_mask'], '0x10')
        pe = FakePe()
        pe.data[0x35b39 + 5] ^= 1  # 把效果调用改写后，旧的纯粒子边界证据失效。
        with self.assertRaisesRegex(ValueError, '锚点'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<I', pe.data, 0x28c28 + 118 * 4, subject.OPCODE_DISPATCH[127])
        with self.assertRaisesRegex(ValueError, '135'):
            subject.inspect(pe, require_hash=False)

    def test_extra_polar_offset_rejects_wrong_handler_and_math(self):
        self.assertEqual(subject.inspect(FakePe(), require_hash=False)['opcode_200_polar_offset_target'], '0x268f1')
        pe = FakePe()
        # 同样12字节载荷不能证明等价：重指向43的位置处理器必须失败。
        struct.pack_into('<I', pe.data, 0x28c28 + 119 * 4, 0x24f9b)
        with self.assertRaisesRegex(ValueError, '200'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        pe.data[0x268f1 + 3] = 8  # 将半径改读第三个参数也须拒绝。
        with self.assertRaisesRegex(ValueError, '锚点'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<d', pe.data, 0x3536a0, 1 / 3)
        with self.assertRaisesRegex(ValueError, '三角函数常量'):
            subject.inspect(pe, require_hash=False)

    def test_stage6_extension_identity_and_constants(self):
        pe = FakePe()
        result = subject.inspect(pe, require_hash=False)
        self.assertEqual(result['stage6_extensions'][11], '0x36710')
        struct.pack_into('<Q', pe.data, 0x34ddf0 + 11 * 8, pe.base + 0x36470)
        with self.assertRaisesRegex(ValueError, '扩展11'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<f', pe.data, 0x3529b8, 1 / 255)
        with self.assertRaisesRegex(ValueError, '地雷常量'):
            subject.inspect(pe, require_hash=False)

    def test_native_graze_events_do_not_write_rank_and_add_score_after_saturated_counters(self):
        result = subject.inspect(FakePe(), require_hash=False)['graze_event']
        self.assertEqual(result['score'], 500)
        self.assertEqual(result['stage_counter_limit'], 99999)
        self.assertEqual(result['run_counter_limit'], 999999)
        self.assertTrue(result['score_still_added_when_counters_saturate'])
        self.assertFalse(result['rank_write_in_event_block'])
        self.assertEqual((result['external_effect'], result['sound']), (8, 30))
        # 改动计数上限、事件写入目标、工厂调用、加分立即数或音效都必须使证据失效。
        for rva in (0x118eb+1, 0x11900+1, 0x118f4+2, 0x1195c+1, 0x11967+7, 0x119fa+4,
                    0x6c558+1, 0x6c56d+1, 0x6c561+2, 0x6c5cd+1, 0x6c5d8+7, 0x6c626+4):
            pe = FakePe();pe.data[rva] ^= 1
            with self.subTest(rva=hex(rva)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_laser_graze_uses_sustain_counter_before_increment_and_hit_has_priority(self):
        result = subject.inspect(FakePe(), require_hash=False)['graze_event']['laser_graze']
        self.assertEqual((result['period_content_frames'], result['first_sustain_frame']), (12, 0))
        self.assertEqual((result['grow_argument_offset'], result['sustain_argument_offset']), ('0x30', '0x34'))
        self.assertEqual(result['initial_phase'], 'sustain_if_grow_zero_otherwise_grow')
        for key in ('grow_to_sustain_resets_counter', 'same_update_checks_sustain_zero',
                    'contact_before_counter_increment', 'contact_before_sustain_end_check', 'hit_before_graze_gate'):
            self.assertTrue(result[key])
        self.assertFalse(result['permanent_grazed_bit'])
        # 保护构造、阶段分派/归零、除12乘回比较、栈参数、调用顺序及helper门控，不能只固定周期立即数。
        for rva in (0x10f21+3, 0x10f56+2, 0x10f61+2, 0x10f7d+1, 0x1104d,
                    0x11da3+3, 0x11e23+2, 0x11e52+2, 0x11e55,
                    0x11ee8+3, 0x11f5f+2, 0x11f68+3, 0x11f75+2, 0x11f7b+1,
                    0x11f87+2, 0x11f9b+2, 0x11fa3+2, 0x11fa9+3, 0x11fb3+1,
                    0x11fbe+2, 0x11fcd+3, 0x6c3f4+2, 0x6c4d9+3, 0x6c4e1+2):
            pe = FakePe(); pe.data[rva] ^= 1
            with self.subTest(rva=hex(rva)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_five_rank_slots_keep_extra_and_entry_condition_separate(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['rank_write_table_with_extra'], (20, 32, 32, 32, 18))
        self.assertEqual(result['extra_rank_write_value'], 18)
        self.assertEqual(result['rank_manager_rva'], '0x53cab0')
        self.assertEqual(result['rank_field_rva'], '0x549ecc')
        self.assertEqual(result['rank_new_run_skipped_state'], 3)
        self.assertEqual(result['campaign_next_stage_state'], 3)
        self.assertTrue(result['rank_replay_reloads_difficulty_table'])
        for rva in (0x3c0c8, 0x3c167, 0x3c27c, 0x3c752, 0x3c77e, 0x3c9dd,
                    0x41448 + 10, 0x41448 + 25, 0x414ab + 6):
            pe = FakePe(); pe.data[rva] ^= 1
            with self.subTest(rva=hex(rva)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)
        for index in range(5):
            pe = FakePe(); pe.data[0x34e0e0 + index * 4] ^= 1
            with self.subTest(index=index), self.assertRaisesRegex(ValueError, 'rank写入表'):
                subject.inspect(pe, require_hash=False)

    def test_valid_and_rejections(self):
        pe = FakePe()
        self.assertEqual(subject.inspect(pe, require_hash=False)['rank_write_table'], (20, 32, 32, 32))
        with self.assertRaises(ValueError):
            subject.inspect(pe)
        pe.data[0x2676e] ^= 1
        with self.assertRaises(ValueError):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        del pe.data[0x350998:]
        with self.assertRaises(ValueError):
            subject.inspect(pe, require_hash=False)

    def test_combination_table_and_noop_dispatch(self):
        pe = FakePe()
        result = subject.inspect(pe, require_hash=False)
        self.assertEqual(result['patchouli_combinations'], [(0, 3, 1), (2, 3, 4), (1, 4, 0), (4, 2, 3)])
        self.assertEqual(result['opcode_127_noop_target'], '0x25c2d')
        pe.data[0x428f50] ^= 1
        with self.assertRaisesRegex(ValueError, '组合表'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        struct.pack_into('<I', pe.data, 0x28c28 + 120 * 4, 0x25c2e)
        with self.assertRaisesRegex(ValueError, '127'):
            subject.inspect(pe, require_hash=False)
        pe = FakePe()
        del pe.data[0x428f50 + 47:]
        with self.assertRaisesRegex(ValueError, '未映射'):
            subject.inspect(pe, require_hash=False)


if __name__ == '__main__':
    unittest.main()

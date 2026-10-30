#!/usr/bin/env python3
# 本文件独立构造PE地址空间，验证外部效果源证据拒绝错分派、错槽结构、错动画/更新器及运动常量。
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import inspect_native_effects as subject


class FakePe:
    """合成RVA直映像，仅供工具边界检查；不依赖本机原包或生产效果配方。"""
    base = 0x140000000

    def __init__(self):
        self.data = bytearray(0x428f50)
        for rva, encoded in subject.ANCHORS.items():
            data = bytes.fromhex(encoded)
            self.data[rva:rva + len(data)] = data
        for opcode, (index, target) in subject.OPCODE_DISPATCH.items():
            self.data[0x28e0c + opcode - 1] = index
            struct.pack_into('<I', self.data, 0x28c28 + index * 4, target)
        for index, (script, target) in enumerate(subject.EFFECT_TABLE):
            struct.pack_into('<iiQ', self.data, 0x428e00 + 16 * index,
                             script, 0, self.base + target if target else 0)
        for rva, value in subject.CONSTANTS.items():
            struct.pack_into('<f', self.data, rva, value)
        struct.pack_into('<28I', self.data, 0x34dd10, *subject.ATTACHED_COLORS)
        for rva, targets in subject.STAGE_EFFECT_LOAD_SWITCHES.items():
            struct.pack_into('<8I', self.data, rva, *targets)

    def offset(self, address, size):
        offset = address - self.base
        return offset if 0 <= offset and offset + size <= len(self.data) else None


class InspectEffectsTests(unittest.TestCase):
    def test_graze_motion_and_creation_positions_are_independent_of_drift(self):
        result = subject.inspect(FakePe(), require_hash=False)
        graze = result['ballistic_effects']['8']
        self.assertEqual(graze['callback_rva'], '0x2a4d0')
        self.assertEqual(graze['random_reads_16bit'], 4)
        self.assertEqual(graze['initial_xy_component'], '(uniform_0_1 * 256 - 128) / 12')
        self.assertEqual(graze['initial_z_velocity'], 0)
        self.assertAlmostEqual(graze['acceleration_initial_velocity_factor'], -1 / 19)
        self.assertEqual(result['common_animation_scripts']['8'], 13)
        requests = result['graze_requests']
        self.assertEqual((requests['effect'], requests['count'], requests['color_argb']), (8, 1, '0xffffffff'))
        self.assertEqual(requests['ordinary_position'], '(player_xyz + bullet_xyz) * 0.5')
        self.assertEqual(requests['beam_position'], 'player_xyz')
        self.assertEqual(requests['scope'], 'creation_position_count_color_only')
        self.assertTrue(requests['unresolved'])

    def test_stage_effect_sixteen_uses_native_aliases_for_stage_six_and_extra(self):
        result = subject.inspect(FakePe(), require_hash=False)['stage_effect_16']
        self.assertEqual(result['global_anm'], 723)
        self.assertEqual(result['animation_script'], 0)
        self.assertIsNone(result['custom_callback'])
        self.assertEqual(result['archive_slot'], 11)
        self.assertEqual(result['files_by_stage'], {
            '1': 'data/eff01.anm', '2': 'data/eff02.anm', '3': 'data/eff03.anm',
            '4': 'data/eff04.anm', '5': 'data/eff05.anm',
            '6': 'data/eff05.anm', '7': 'data/eff04.anm',
        })
        self.assertTrue(result['unresolved'])
        self.assertEqual(result['files_by_stage_scope'], 'initial_loader_only')
        reload = result['conditional_reload']
        self.assertEqual(reload['files_by_stage'], {'6': 'data/eff06.anm', '7': 'data/eff07.anm'})
        self.assertEqual(reload['global_anm'], 723)
        self.assertFalse(reload['dynamic_reachability_proven'])

    def test_conditional_stage_effect_reload_is_guarded_including_its_gates(self):
        for rva in (0x3cd5f, 0x3cdb1, 0x3cdb7, 0x3cdc5, 0x3cdd1, 0x3cdda, 0x350e50, 0x350e60):
            pe = FakePe()
            pe.data[rva] ^= 1
            with self.subTest(rva=hex(rva)), self.assertRaisesRegex(ValueError, '锚点'):
                subject.inspect(pe, require_hash=False)

    def test_both_stage_effect_loader_switches_reject_every_changed_target(self):
        for rva in subject.STAGE_EFFECT_LOAD_SWITCHES:
            for stage in range(8):
                pe = FakePe()
                pe.data[rva + stage * 4] ^= 1
                with self.subTest(rva=hex(rva), stage=stage), self.assertRaisesRegex(ValueError, '装载分派'):
                    subject.inspect(pe, require_hash=False)

    def test_meiling_native_two_has_no_external_particle_side_effect(self):
        result = subject.inspect(FakePe(), require_hash=False)['native_2']
        self.assertEqual(result['audited_end_rva_exclusive'], '0x34c90')
        self.assertEqual(len(bytes.fromhex(subject.ANCHORS[0x34610])), 0x680)
        self.assertTrue(result['whole_function_guarded'])
        self.assertEqual(result['period_frames'], 6)
        self.assertEqual(result['vertex_period_frames'], 30)
        self.assertEqual(result['edges_per_emission'], 5)
        self.assertEqual(result['sound'], 22)
        self.assertFalse(result['calls_effect_factory'])
        self.assertEqual(result['external_particles_per_emission'], 0)
        pe = FakePe()
        struct.pack_into('<i', pe.data, 0x34bb2 + 1, 0x2af80 - (0x34bb2 + 5))
        with self.assertRaisesRegex(ValueError, '锚点'):
            subject.inspect(pe, require_hash=False)

    def test_rotating_drift_uses_its_own_velocity_and_deceleration(self):
        result = subject.inspect(FakePe(), require_hash=False)['ballistic_effects']['20']
        self.assertEqual(result['callback_rva'], '0x2a2c0')
        self.assertEqual(result['random_reads_16bit'], 4)
        self.assertEqual(result['initial_xy_component'], '(uniform_0_1 * 256 - 128) / 60')
        self.assertAlmostEqual(result['acceleration_initial_velocity_factor'], -1 / 95)
        for rva in (0x2a381, 0x2a369, 0x2a42b, 0x3529cc):
            pe = FakePe()
            pe.data[rva] ^= 1
            with self.subTest(rva=rva), self.assertRaises(ValueError):
                subject.inspect(pe, require_hash=False)

    def test_sakuya_native_five_has_no_external_particle_side_effect(self):
        result = subject.inspect(FakePe(), require_hash=False)['native_5']
        self.assertEqual(result['callback_rva'], '0x35350')
        self.assertEqual(result['audited_end_rva_exclusive'], '0x3577a')
        self.assertTrue(result['whole_function_guarded'])
        self.assertEqual(len(bytes.fromhex(subject.ANCHORS[0x35350])), 0x42a)
        self.assertEqual(result['period_frames'], 9)
        self.assertEqual(result['bullets_per_emission'], 9)
        self.assertEqual(result['sound'], 7)
        self.assertEqual(result['external_particles_per_emission'], 0)
        self.assertFalse(result['calls_effect_factory'])
        # 把真实弹幕调用替换成外部粒子创建器，必须拒绝，不能仍输出无粒子结论。
        pe = FakePe()
        struct.pack_into('<i', pe.data, 0x35695 + 1, 0x2af80 - (0x35695 + 5))
        with self.assertRaisesRegex(ValueError, '锚点'):
            subject.inspect(pe, require_hash=False)

    def test_attachment_color_table_is_separate_from_effect_identity_and_guarded(self):
        colors = subject.inspect(FakePe(), require_hash=False)['opcode_102_color_argb']
        self.assertEqual(len(colors), 28)
        self.assertEqual(colors[11], '0xffe0e0ff')
        self.assertEqual(colors[25], '0xffffe0e0')
        for index in range(28):
            pe = FakePe()
            pe.data[0x34dd10 + index * 4] ^= 1
            with self.subTest(index=index), self.assertRaisesRegex(ValueError, '调色表'):
                subject.inspect(pe, require_hash=False)

    def test_attachment_follows_actor_without_clamping_radius_and_uses_quaternion_phase(self):
        result = subject.inspect(FakePe(), require_hash=False)['attached_13']
        self.assertEqual(result['animation_script'], 18)
        self.assertEqual(result['axis_fields'], ['0x18', '0x1c', '0x20'])
        self.assertTrue(result['axis_is_dimensionless'])
        self.assertEqual(result['actor_shared_target_radius_field'], '0x268')
        self.assertAlmostEqual(result['radius_step_source_pixels'], .3)
        self.assertFalse(result['radius_clamps_to_target'])
        self.assertAlmostEqual(result['phase_step_radians'], .03141592815518379)
        self.assertEqual(result['output_z_factor'], 6)
        self.assertEqual(result['quaternion'], '(normalize(axis) * sin(phase), cos(phase))')

    def test_attachment_release_fades_for_sixteen_frames_without_resetting_reused_orbit(self):
        result = subject.inspect(FakePe(), require_hash=False)['attached_13']
        self.assertTrue(result['actor_release_sets_fade_and_clears_handle'])
        self.assertEqual(result['fade_expiry_frames'], 16)
        self.assertTrue(result['fade_increments_before_expiry_check'])
        self.assertEqual(result['fade_alpha'], 'trunc((1 - fade_counter / 16) * 255)')
        self.assertEqual(result['fade_scale'], '1 + fade_counter / 16')
        self.assertFalse(result['factory_resets_radius_and_phase'])
        self.assertTrue(result['unresolved'])

    def test_ambient_timer_initialization_is_independent_of_animation_clock(self):
        result = subject.inspect(FakePe(), require_hash=False)['ambient_6']
        self.assertEqual(result['clock_field'], '0x2d0')
        self.assertEqual(result['constructor_current_frame'], 0)
        self.assertEqual(result['constructor_previous_frame'], -1)
        self.assertEqual(result['spawn_current_frame'], 0)
        self.assertEqual(result['spawn_previous_frame'], 0)
        self.assertEqual(result['spawn_phase'], 0)

    def test_request_inventory_keeps_masks_source_locations_and_argb_byte_order(self):
        ecl = {'sha256': subject.ECL_IDENTITIES[6], 'programs': [[
            {'opcode': 118, 'payload': struct.pack('<iII', 17, 6, 0xffff0000).hex(),
             'frame': 120, 'offset': 0x100, 'rank_mask': 0x500},
            {'opcode': 102, 'payload': struct.pack('<i4f', 25, -.3, -.7, -.3, 48).hex(),
             'frame': 30, 'offset': 0x118, 'rank_mask': 0xa00},
        ]]}
        result = subject.inspect_requests(ecl)
        self.assertEqual(result['stage'], 7)
        self.assertEqual(result['scope'], 'static_instructions_not_runtime_emissions')
        self.assertEqual(result['difficulties']['E'], {'118:effect:17': 1})
        self.assertEqual(result['difficulties']['H'], {'118:effect:17': 1})
        self.assertEqual(result['difficulties']['N'], {'102:color:25': 1})
        self.assertEqual(result['difficulties']['L'], {'102:color:25': 1})
        self.assertEqual(result['requests'][0]['offset'], '0x100')
        self.assertEqual(result['requests'][0]['color_argb'], '0xffff0000')
        self.assertEqual(result['requests'][0]['count'], 6)
        self.assertEqual(result['requests'][1]['effect'], 13)
        self.assertEqual(result['requests'][1]['color_table_index'], 25)
        self.assertAlmostEqual(result['requests'][1]['parameters'][1], -.7)
        self.assertEqual(result['requests'][1]['parameters'][3], 48)

    def test_request_inventory_rejects_unknown_source_and_bad_payload(self):
        with self.assertRaisesRegex(ValueError, 'ECL版本未知'):
            subject.inspect_requests({'sha256': 'unknown', 'programs': []})
        for opcode, size in ((100, 3), (100, 5), (102, 19), (102, 21), (118, 11), (118, 13)):
            ecl = {'sha256': subject.ECL_IDENTITIES[0], 'programs': [[
                {'opcode': opcode, 'payload': bytes(size).hex(), 'frame': 0, 'offset': 0x100, 'rank_mask': 0xf00},
            ]]}
            with self.subTest(opcode=opcode, size=size), self.assertRaisesRegex(ValueError, '参数布局'):
                subject.inspect_requests(ecl)

    def test_effect_sixteen_inventory_distinguishes_requests_and_defeat_configuration(self):
        program = [
            {'opcode': 100, 'payload': struct.pack('<I', value).hex(), 'frame': frame,
             'offset': 0x100 + frame * 16, 'rank_mask': 0xf00}
            for frame, value in enumerate((258, 768, 0x030410, 0xffffffff))
        ]
        program.append({'opcode': 118, 'payload': struct.pack('<iII', 16, 1, 0xffffffff).hex(),
                        'frame': 10, 'offset': 0x200, 'rank_mask': 0x400})
        result = subject.inspect_requests({'sha256': subject.ECL_IDENTITIES[2], 'programs': [program]})
        self.assertEqual(len(result['requests']), 1)
        self.assertEqual(len(result['defeat_configurations']), 4)
        self.assertEqual(result['defeat_configurations'][0]['primary_effect'], 2)
        self.assertEqual(result['defeat_configurations'][0]['secondary_effect_base'], 1)
        self.assertEqual(result['defeat_configurations'][1]['secondary_effect_base'], 3)
        row, = result['primary_defeat_effect_16_configurations']
        self.assertEqual((row['sub'], row['frame'], row['offset']), (0, 2, '0x120'))
        self.assertEqual((row['primary_effect'], row['secondary_effect_base'], row['third_byte']), (16, 4, 3))
        self.assertEqual(result['defeat_configurations'][3]['third_byte'], 255)
        self.assertEqual(result['explicit_effect_16_instructions'], result['requests'])
        self.assertEqual(result['difficulties'], {'E': {}, 'N': {}, 'H': {'118:effect:16': 1}, 'L': {}})

    def test_external_pool_identity_and_update_order(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['pool_capacity'], 512)
        self.assertEqual(result['slot_stride'], '0x198')
        self.assertEqual(result['pool_bytes'], hex(512 * 0x198 + 0x1a0))
        self.assertEqual(result['pool_rva'], '0xabaef0')
        self.assertEqual(result['cursor_rva'], '0xaee08c')
        self.assertEqual(result['update_order'], ['callback', 'anm', 'increment_content_frame'])
        self.assertEqual(result['content_hz'], 60)
        self.assertEqual(result['callback_keep_result'], 1)
        self.assertTrue(result['request_color_written_after_anm_start'])
        self.assertEqual(result['common_anm_file'], 'data/etama4.anm')
        self.assertEqual(result['common_anm_base'], 698)
        self.assertEqual(result['common_animation_scripts']['17'], 7)
        self.assertEqual(result['common_animation_scripts']['18'], 8)
        self.assertEqual(result['common_animation_scripts']['13'], 18)

    def test_distinct_actor_attachment_and_fire_and_forget_requests(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['opcode_102_effect'], 13)
        self.assertTrue(result['opcode_102_retains_actor_handle'])
        self.assertFalse(result['opcode_118_retains_actor_handle'])
        self.assertEqual(result['table'][13], {'effect': 13, 'global_anm': 716, 'callback_rva': '0x2a990'})

    def test_charge_particles_recompute_position_and_use_different_convergence(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['table'][17], {'effect': 17, 'global_anm': 705, 'callback_rva': '0x2ace0'})
        self.assertEqual(result['table'][18], {'effect': 18, 'global_anm': 706, 'callback_rva': '0x2ae30'})
        self.assertEqual(result['converging_effects']['17']['denominator_frames'], 60)
        self.assertEqual(result['converging_effects']['18']['denominator_frames'], 240)
        self.assertEqual(result['converging_effects']['17']['radius'], 256)
        self.assertEqual(result['converging_position'],
                         'origin + unit_direction * (radius - frame * radius / denominator_frames)')
        self.assertEqual(result['spatial_units'], 'source_pixels')
        self.assertEqual(result['converging_random_reads_16bit'], 2)
        self.assertTrue(result['unresolved'])

    def test_all_twenty_one_effect_table_rows_are_guarded(self):
        self.assertEqual(len(subject.EFFECT_TABLE), 21)
        self.assertEqual(subject.EFFECT_TABLE[19], (717, 0x2a900))
        self.assertEqual(subject.EFFECT_TABLE[20], (719, 0x2a2c0))
        for index in range(21):
            for field in (0, 4, 8, 15):
                pe = FakePe()
                pe.data[0x428e00 + 16 * index + field] ^= 1
                with self.subTest(index=index, field=field), self.assertRaisesRegex(ValueError, '效果表'):
                    subject.inspect(pe, require_hash=False)

    def test_burst_and_caller_configured_particles_move_before_accelerating(self):
        result = subject.inspect(FakePe(), require_hash=False)
        self.assertEqual(result['ballistic_update_order'], ['position += velocity', 'velocity += acceleration'])
        burst = result['ballistic_effects']['3']
        self.assertEqual(burst['random_reads_16bit'], 4)
        self.assertEqual(burst['initial_xy_component'], '(uniform_0_1 * 256 - 128) * 4 / 33')
        self.assertEqual(burst['initial_z_velocity'], 0)
        self.assertEqual(burst['acceleration_initial_velocity_factor'], -0.05000000074505806)
        configured = result['ballistic_effects']['19']
        self.assertEqual(configured['random_reads_16bit'], 0)
        self.assertTrue(configured['parameters_from_creator'])

    def test_ambient_sparkles_keep_independent_clock_cadence_and_two_origins(self):
        result = subject.inspect(FakePe(), require_hash=False)['ambient_6']
        self.assertEqual(result['clock_field'], '0x2d0')
        self.assertEqual(result['cadence_inclusive_bounds_and_period'], [[30, 8], [60, 4], [120, 2]])
        self.assertEqual(result['period_after_120'], 1)
        self.assertEqual(result['effect'], 19)
        self.assertEqual(result['particles_per_emission'], 2)
        self.assertEqual(result['color_argb'], '0xff3030ff')
        self.assertEqual(result['random_bound'], 'floor((clock % 16) / 2)')
        self.assertEqual(result['radius'], 'radial_index * 10 + 32')
        self.assertEqual(result['velocity_x'], '(uniform_0_1 * 40 - 20) / 60')
        self.assertEqual(result['velocity_z'], 0)
        self.assertEqual(result['acceleration_initial_velocity_factor'], -0.008333333767950535)

    def test_each_semantic_anchor_rejects_mutation(self):
        for rva, encoded in subject.ANCHORS.items():
            size = len(bytes.fromhex(encoded))
            for delta in sorted({0, size // 2, size - 1}):
                pe = FakePe()
                pe.data[rva + delta] ^= 1
                with self.subTest(rva=hex(rva), delta=delta), self.assertRaisesRegex(ValueError, '锚点'):
                    subject.inspect(pe, require_hash=False)

    def test_effect_opcode_indices_and_targets_are_guarded(self):
        for opcode, (index, _) in subject.OPCODE_DISPATCH.items():
            for rva in (0x28e0c + opcode - 1, 0x28c28 + index * 4):
                pe = FakePe()
                pe.data[rva] ^= 1
                with self.subTest(opcode=opcode, rva=hex(rva)), self.assertRaisesRegex(ValueError, '分派|目标'):
                    subject.inspect(pe, require_hash=False)

    def test_float_constants_reject_even_one_bit_changes(self):
        for rva in subject.CONSTANTS:
            pe = FakePe()
            pe.data[rva] ^= 1
            with self.subTest(rva=hex(rva)), self.assertRaisesRegex(ValueError, '常量'):
                subject.inspect(pe, require_hash=False)

    def test_unknown_exe_is_rejected_before_interpretation(self):
        with self.assertRaisesRegex(ValueError, '版本未知'):
            subject.inspect(FakePe())

    def test_unmapped_and_truncated_input_never_yields_partial_evidence(self):
        pe = FakePe()
        pe.data = pe.data[:0x428f4f]
        with self.assertRaisesRegex(ValueError, '越界'):
            subject.inspect(pe, require_hash=False)
        with self.assertRaisesRegex(ValueError, '越界'):
            subject.read(pe, -1, 4)
        # 即使错误的映射器声称地址可用，也不能接受不足长度的切片。
        pe.offset = lambda _address, _size: len(pe.data) - 1
        with self.assertRaisesRegex(ValueError, '截断'):
            subject.read(pe, 0x428f40, 4)


if __name__ == '__main__':
    unittest.main()

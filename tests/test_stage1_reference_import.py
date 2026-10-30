# 本文件用独立合成的文本和二进制验证参考导入的难度、标签重定位、扩展参数及坏输入拒绝。
import importlib.util
from pathlib import Path
import tempfile
import struct
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
SPEC = importlib.util.spec_from_file_location(
    'stage1_reference_import', Path(__file__).parents[1] / 'tools/import_stage1_reference.py')
IMPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(IMPORTER)
import stage_reference
from tests.test_stage_script_inspection import fixture, instruction


def source(first):
    """补齐一面身份要求的 38 个连续子程序，首个子程序由测试独立构造。"""
    return '\n'.join(f'sub Sub{i}()\n{{\n{first if i == 0 else ""}\n}}' for i in range(38))


class ReferenceImportTests(unittest.TestCase):
    """检验持久掩码与过滤后的同子程序索引，不依赖生产行为。"""

    def test_masks_and_relocated_labels(self):
        text = source('''+0: // 0
ins_2(0, target);
!E ins_10(1);
ins_11(2);
target:
!N ins_12(3);
!HL ins_13(4);
!* ins_14(#10, "Sub3", "text", -1.5f);''')
        expected = {'E': ([2, 10, 11, 14], 3), 'N': ([2, 12, 14], 1),
                    'H': ([2, 13, 14], 1), 'L': ([2, 13, 14], 1)}
        for difficulty, (ops, target) in expected.items():
            with self.subTest(difficulty=difficulty):
                program = IMPORTER.parse_programs(text, difficulty, 'fixture')[0]
                self.assertEqual([row[1] for row in program], ops)
                self.assertEqual(program[0][2], [0.0, target])
                self.assertEqual(program[-1][2], [16, 3, 0, -1.5])
        self.assertEqual(IMPORTER.parse_programs(text), IMPORTER.parse_programs(text, 'N'))

    def test_output_normal_compatibility(self):
        programs = IMPORTER.parse_programs(source('ins_1(2);'))
        with tempfile.TemporaryDirectory() as directory:
            IMPORTER.write_programs(programs, Path(directory))
            result = (Path(directory) / 'reference_program.hpp').read_text()
            self.assertIn('一面 Normal 差分测试指令', result)
            self.assertIn('    {0, 1, {2.0F}},', result)
            self.assertIn('std::array<std::span<const Instruction>,38>', result)

    def test_binary_masks_jump_source_and_color(self):
        # 跳转目标是 Easy 独有指令的起点；Normal 应定位到后续保留指令。
        jump = instruction(2, struct.pack('<ii', 8, 20))
        easy = instruction(1, struct.pack('<i', 7), 0x100)
        color = instruction(118, struct.pack('<II', 3, 2) + bytes.fromhex('8080ffff'))
        programs = [jump + easy + color] + [instruction(0)] * 37
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'sample.ecl'
            path.write_bytes(fixture(programs))
            digest, normal = IMPORTER.parse_binary_programs(path, 'N')
            _, easy_programs = IMPORTER.parse_binary_programs(path, 'E')
            self.assertEqual(len(digest), 64)
            self.assertEqual(normal[0][0][2], [8, 1])
            self.assertEqual(easy_programs[0][0][2], [8, 1])
            self.assertEqual(normal[0][1][2], [3, 2, 0x8080ffff])
            self.assertEqual(normal[0][1][3], normal[0][0][3] + len(jump) + len(easy))
            self.assertEqual(normal[0][1][4:], (0xff00, 0xff, 0))
            self.assertEqual(normal[0][0][6], normal[0][0][3] + 20)

    def test_binary_unknown_and_bad_format_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'sample.ecl'
            for first in (instruction(202, b'\0' * 12), instruction(200, b'\0' * 4), instruction(1, b'\0' * 2)):
                path.write_bytes(fixture([first] + [instruction(0)] * 37))
                with self.assertRaisesRegex(ValueError, 'sample.ecl:Sub0@'):
                    IMPORTER.parse_binary_programs(path, 'N')
            path.write_bytes(fixture([instruction(0)]))
            with self.assertRaisesRegex(ValueError, '38 个子程序'):
                IMPORTER.parse_binary_programs(path, 'N')

    def test_stage1_raw_effect_color_is_kept_in_source_records(self):
        # 旧Normal指令表不变；四难度来源记录独立携带原始颜色，包括float不能精确表示的低字节。
        for color in (0xffffffff, 0xffff8080, 0xff8080ff, 0xff80ff80):
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / 'stage1.ecl'
                path.write_bytes(fixture([instruction(118, struct.pack('<iII', 3, 2, color))] + [instruction(0)] * 37))
                programs = {name: IMPORTER.parse_binary_programs(path, name)[1] for name in 'ENHL'}
                output = Path(directory) / 'four.hpp'
                IMPORTER.write_binary_programs(programs, output, 'synthetic')
                text = output.read_text()
                self.assertIn('std::uint32_t effectColor=0;', text)
                self.assertEqual(text.count(f', {color}U}}'), 4)

    def test_bad_input_reports_source(self):
        for body in ('!Q ins_1();', 'ins_2(missing);', 'same:\nsame:',
                     'ins_1(nan);', 'ins_1(' + ', '.join(['1'] * 17) + ');',
                     '+oops', 'ins_1(1,);'):
            with self.subTest(body=body), self.assertRaisesRegex(ValueError, 'fixture:'):
                IMPORTER.parse_programs(source(body), source='fixture')
        with self.assertRaisesRegex(ValueError, '38 个子程序'):
            IMPORTER.parse_programs('sub Sub0()\n{\n}', source='fixture')
        with self.assertRaises(ValueError):
            IMPORTER.parse_programs(source(''), 'X')

    def test_stage4_cli_rejects_other_structure_or_revision(self):
        # 相同子程序数也不能绕过版本身份；失败时不生成可被误认为已审阅的夹具。
        script = Path(__file__).parents[1] / 'tools/stage_reference.py'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path, output = root / 'fixture.ecl', root / 'output'
            for count, message in ((61, '119 个子程序'), (119, 'SHA-256')):
                path.write_bytes(fixture([instruction(0)] * count))
                before = path.read_bytes()
                result = subprocess.run([sys.executable, str(script), str(path), '--stage', '4',
                                         '--output', str(output)], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)
                self.assertEqual(path.read_bytes(), before)
                self.assertFalse(output.exists())

    def test_stage6_native_shot_preserves_opcode_and_parameters(self):
        # 新典201不能在导入阶段降级为普通随机弹；原生模式语义由独立六面测试验证。
        arguments = (2, 6, 17, 3, 4.5, 1.25, -0.5, 0.125, 8)
        raw = instruction(201, struct.pack('<hhii4fi', *arguments), mask=0x400, frame=37)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'stage6.ecl'
            path.write_bytes(fixture([raw] + [instruction(0)] * 86))
            _, hard = stage_reference.parse_binary_stage(path, 'H', 87)
            _, normal = stage_reference.parse_binary_stage(path, 'N', 87)
            self.assertEqual(hard[0][0][:3], (37, 201, list(arguments)))
            self.assertEqual(hard[0][0][4:], (0x400, 0xff, 0))
            self.assertEqual(normal[0], [])
            for payload in (b'\0' * 30, b'\0' * 38):
                path.write_bytes(fixture([instruction(201, payload)] + [instruction(0)] * 86))
                with self.assertRaisesRegex(ValueError, 'stage6.ecl:Sub0@'):
                    stage_reference.parse_binary_stage(path, 'N', 87)

    def test_stage6_cli_rejects_other_structure_or_revision(self):
        script = Path(__file__).parents[1] / 'tools/stage_reference.py'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path, output = root / 'fixture.ecl', root / 'output'
            for count, message in ((75, '87 个子程序'), (87, 'SHA-256')):
                path.write_bytes(fixture([instruction(0)] * count))
                result = subprocess.run([sys.executable, str(script), str(path), '--stage', '6',
                                         '--output', str(output)], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)
                self.assertFalse(output.exists())

    def test_extra_polar_offset_preserves_variables_and_unused_payload(self):
        # 第三个float虽未被原生200读取，导入仍保留；禁止改写成43或丢弃源掩码/参数。
        arguments = (-10006.0, -10005.0, 27.0)
        raw = instruction(200, struct.pack('<3f', *arguments), mask=0xff00, frame=1)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'extra.ecl'
            path.write_bytes(fixture([raw] + [instruction(0)] * 144))
            digest, programs = stage_reference.parse_binary_stage(path, 'N', 145)
            self.assertEqual(programs[0][0][:3], (1, 200, list(arguments)))
            self.assertEqual(programs[0][0][4:], (0xff00, 0xff, 0))
            output = Path(directory) / 'extra.hpp'
            stage_reference.write_stage_programs({'Extra': programs}, output, digest, 7, 145)
            text = output.read_text()
            self.assertIn('namespace koumakyou::stage_extra::reference', text)
            self.assertIn('return kExtraPrograms;', text)
            self.assertNotIn('ProgramsForDifficulty', text)
            for payload in (b'\0' * 8, b'\0' * 16):
                path.write_bytes(fixture([instruction(200, payload)] + [instruction(0)] * 144))
                with self.assertRaisesRegex(ValueError, 'extra.ecl:Sub0@'):
                    stage_reference.parse_binary_stage(path, 'N', 145)

    def test_extra_effect_colors_keep_raw_argb_outside_float_parameters(self):
        # 白色与非对称红色不能先转float再转回；Extra参考侧须保存EXE实际读取的32位字。
        for color in (0xffffffff, 0xffff0000, 0xffff8080):
            raw = instruction(118, struct.pack('<iII', 3, 2, color), mask=0xff00, frame=2)
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / 'extra.ecl'
                path.write_bytes(fixture([raw] + [instruction(0)] * 144))
                digest, programs = stage_reference.parse_binary_stage(path, 'N', 145)
                output = Path(directory) / 'extra.hpp'
                stage_reference.write_stage_programs({'Extra': programs}, output, digest, 7, 145)
                text = output.read_text()
                self.assertIn('std::uint32_t effectColor=0;', text)
                self.assertIn(f', {color}U}}', text)

    def test_mainline_effect_colors_keep_raw_argb_in_each_difficulty(self):
        # 主线与Extra采用相同118载荷；每个难度都须保留低字节，不能只修复Extra导出。
        for color in (0xffffffff, 0xffff8080, 0xff8080ff, 0xff80ff80):
            raw = instruction(118, struct.pack('<iII', 18, 2, color), mask=0xff00, frame=40)
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / 'stage6.ecl'
                path.write_bytes(fixture([raw] + [instruction(0)] * 86))
                programs = {name: stage_reference.parse_binary_stage(path, name, 87)[1] for name in 'ENHL'}
                output = Path(directory) / 'stage6.hpp'
                stage_reference.write_stage_programs(programs, output, 'synthetic', 6, 87)
                text = output.read_text()
                self.assertIn('std::uint32_t effectColor=0;', text)
                self.assertEqual(text.count(f', {color}U}}'), 4)

    def test_extra_cli_rejects_other_structure_or_revision(self):
        script = Path(__file__).parents[1] / 'tools/stage_reference.py'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path, output = root / 'fixture.ecl', root / 'output'
            for count, message in ((87, '145 个子程序'), (145, 'SHA-256')):
                path.write_bytes(fixture([instruction(0)] * count))
                result = subprocess.run([sys.executable, str(script), str(path), '--stage', '7',
                                         '--output', str(output)], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)
                self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()

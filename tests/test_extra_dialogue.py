# 本文件以独立MSG写入序列核对白对话验证器，防止原文、半帧边界、同帧演出与结算等待被静态表掩盖。
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import verify_extra_dialogue as subject


class ExtraDialogueTests(unittest.TestCase):
    def setUp(self):
        self.translation = {'first': {'zh-CN': '前行，含逗号'}, 'second': {'zh-CN': '后行'}}
        self.report = {'wall_frame': 120, 'exit': 'game_state_transition', 'events': [
            {'wall_frame': 0, 'opcode': 7, 'value': -1},
            {'wall_frame': 0, 'opcode': 7, 'value': 2},
            {'wall_frame': 60, 'opcode': 3, 'slot': 1, 'row': 0, 'key': 'first'},
            {'wall_frame': 61, 'opcode': 3, 'slot': 1, 'row': 1, 'key': 'second'},
            {'wall_frame': 90, 'opcode': 9}, {'wall_frame': 120, 'opcode': 11}]}
        self.table = {'lines': [['芙兰朵露', '前行，含逗号', True, 120, 1, False, False,
                                 '后行', 2, 60, 0, 69, 90, False, 0]],
                      'cues': [(0, 'StageMusic', 0, -1), (0, 'StageMusic', 0, 2)], 'end': 120, 'clear': 90}

    def test_exact_writes_and_two_distinct_exit_boundaries(self):
        result = subject.verify_table(self.table, self.report, self.translation)
        self.assertEqual((result['text_writes'], result['clear_frame'], result['end_frame']), (2, 90, 120))

    def test_parser_handles_commas_quotes_and_rejects_expressions(self):
        self.assertEqual(subject.fields('"a,b\\\"c",true,61.0F/60,-1'), ['a,b"c', True, 61, -1])
        for row in ('1+2', 'SomeFunction()', '2.5F/60', 'nan'):
            with self.subTest(row=row), self.assertRaises(ValueError):
                subject.fields(row)

    def test_read_table_requires_exact_storage_and_counts(self):
        source = '''constexpr std::array<DialogueLine,0> kFixtureLines{{
}};
constexpr std::array<DialogueCue,2> kFixtureCues{{
    {0,DialogueCueKind::StageMusic,0,-1},
    {0,DialogueCueKind::StageMusic,0,2},
}};
constexpr DialogueScript kFixture{kFixtureLines,kFixtureCues,86,-1};'''
        table = subject.read_table(source, 'kFixture')
        self.assertEqual(table['lines'], [])
        self.assertEqual(table['cues'], self.table['cues'])
        for bad in (source.replace('DialogueCue,2', 'DialogueCue,3'), source + source,
                    source.replace('kFixtureLines,kFixtureCues,86,-1', 'kFixtureLines,kFixtureCues,86,Unknown')):
            with self.subTest(source=bad), self.assertRaises(ValueError):
                subject.read_table(bad, 'kFixture')

    def test_rejects_changed_text_slot_delay_and_early_confirmation(self):
        for index, value in ((1, '别的文字'), (2, False), (4, 0), (9, 59), (11, 68)):
            table = copy.deepcopy(self.table);table['lines'][0][index] = value
            with self.subTest(index=index), self.assertRaises(ValueError):
                subject.verify_table(table, self.report, self.translation)

    def test_same_frame_stop_and_restart_order_cannot_be_coalesced(self):
        for cues in ([self.table['cues'][1]], self.table['cues'][::-1], [(1, *cue[1:]) for cue in self.table['cues']]):
            table = dict(self.table, cues=cues)
            with self.subTest(cues=cues), self.assertRaises(ValueError):
                subject.verify_table(table, self.report, self.translation)

    def test_portrait_creation_cannot_be_lost_without_an_interrupt(self):
        report = copy.deepcopy(self.report)
        report['events'].insert(0, {'wall_frame': 0, 'opcode': 1, 'slot': 1, 'value': 0})
        table = dict(self.table, cues=[(0, 'PortraitStart', 1, 0), *self.table['cues']])
        subject.verify_table(table, report, self.translation)
        with self.assertRaisesRegex(ValueError, '演出顺序'):
            subject.verify_table(self.table, report, self.translation)

    def test_clear_is_not_dialogue_end_and_post_transition_is_not_reachable(self):
        for table in (dict(self.table, end=90), dict(self.table, clear=120)):
            with self.subTest(table=table), self.assertRaises(ValueError):
                subject.verify_table(table, self.report, self.translation)

    def test_empty_interlude_has_real_duration_without_placeholder_lines(self):
        report = dict(self.report, events=[{'wall_frame': 0, 'opcode': 3, 'slot': 1, 'row': row, 'key': ''}
                                          for row in (0, 1)], wall_frame=86, exit='dialogue_end')
        table = {'lines': [], 'cues': [], 'end': 86, 'clear': -1}
        subject.verify_table(table, report, self.translation, empty_interlude=True)
        with self.assertRaises(ValueError):
            subject.verify_table(self.table, report, self.translation, empty_interlude=True)

    def test_unknown_msg_identity_is_rejected(self):
        with self.assertRaisesRegex(ValueError, '身份'):
            subject.verify(b'invalid', {}, '')


if __name__ == '__main__':
    unittest.main()

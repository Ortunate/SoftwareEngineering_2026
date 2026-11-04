# 本文件通过临时 HOME 和替身程序验证 app 启动的数据隔离、参数传递与目录失败，不加载图形或真实存档。
import importlib.util
import os
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('package_game_macos', ROOT / 'tools/package_game_macos.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


@unittest.skipUnless(os.name == 'posix', 'app 启动器需要 POSIX shell')
class MacosPackageTests(unittest.TestCase):
    """实际执行生成的启动器；所有读写限定在临时目录，兼容含中文和空格的路径。"""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.build = self.root / '构建 目录'
        (self.build / 'assets').mkdir(parents=True)
        (self.build / 'assets/marker').write_text('asset', encoding='utf-8')
        executable = self.build / 'koumakyou'
        executable.write_text(
            '#!/bin/sh\n'
            '# 本替身模拟游戏以相对路径读写三类玩家数据，并输出工作目录和参数。\n'
            'set -eu\n'
            'pwd\n'
            'printf "%s\\n" "$@"\n'
            'test -f koumakyou_settings.cfg || printf default > koumakyou_settings.cfg\n'
            'printf score > koumakyou_scores.dat\n'
            'mkdir -p replays\n'
            'printf replay > replays/slot.krep\n', encoding='utf-8')
        executable.chmod(0o755)
        self.home = self.root / '用户 主目录'
        self.home.mkdir()
        self.data = self.home / 'Library/Application Support/koumakyou'
        self.app = PACKAGE.package_game(self.build, self.root / '游戏 包.app')
        with (self.app / 'Contents/Info.plist').open('rb') as stream:
            info = plistlib.load(stream)
        self.launcher = self.app / 'Contents/MacOS' / info['CFBundleExecutable']
        self.environment = dict(os.environ, HOME=str(self.home))

    def launch(self, *arguments):
        """从 app 外部目录启动，确保保存位置不依赖调用方的工作目录。"""
        return subprocess.run([str(self.launcher), *arguments], cwd=self.root,
                              env=self.environment, capture_output=True, text=True)

    def test_app_data_and_arguments_survive_relaunch(self):
        result = self.launch('--replay', '/录像 目录/测试.krep')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), [str(self.data), '--replay', '/录像 目录/测试.krep'])
        for name in ('koumakyou_settings.cfg', 'koumakyou_scores.dat', 'replays/slot.krep'):
            self.assertTrue((self.data / name).is_file())
            self.assertFalse((self.app / 'Contents/MacOS' / name).exists())
            self.assertFalse((self.root / name).exists())
        self.assertEqual((self.app / 'Contents/MacOS/assets/marker').read_text(), 'asset')
        settings = self.data / 'koumakyou_settings.cfg'
        settings.write_text('remembered', encoding='utf-8')
        self.assertEqual(self.launch().returncode, 0)
        self.assertEqual(settings.read_text(), 'remembered')

    def test_direct_program_keeps_working_directory(self):
        result = subprocess.run([str(self.build / 'koumakyou')], cwd=self.build,
                                env=self.environment, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines()[0], str(self.build))
        self.assertTrue((self.build / 'koumakyou_settings.cfg').is_file())
        self.assertFalse(self.data.exists())

    def test_unusable_data_directory_stops_before_game(self):
        self.data.parent.mkdir(parents=True)
        self.data.write_text('blocked', encoding='utf-8')
        result = self.launch()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')
        self.assertFalse((self.app / 'Contents/MacOS/koumakyou_settings.cfg').exists())

    def test_missing_home_does_not_fall_back(self):
        self.environment.pop('HOME')
        result = self.launch()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')
        self.assertFalse(self.data.exists())


if __name__ == '__main__':
    unittest.main()

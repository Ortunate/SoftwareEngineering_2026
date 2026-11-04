# 本文件验证查看器启动参数、构建缓存隔离及跨平台程序定位；不启动图形窗口或实际编译。
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("comparison_launcher", ROOT / "tools/compare_projectiles.py")
LAUNCHER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LAUNCHER)


class ComparisonLauncherTests(unittest.TestCase):
    def test_cache_and_multiconfig_executable(self):
        with tempfile.TemporaryDirectory() as folder:
            directory = Path(folder)
            (directory / "CMakeCache.txt").write_text("# ignored\n// ignored\nCMAKE_HOME_DIRECTORY:INTERNAL=/repo with spaces\n", encoding="utf-8")
            self.assertEqual(LAUNCHER.cache_values(directory)["CMAKE_HOME_DIRECTORY"], "/repo with spaces")
            target = directory / "Debug/projectile_comparison.exe"
            target.parent.mkdir()
            target.touch()
            self.assertEqual(LAUNCHER.executable(directory, "Debug"), target)

    def test_no_build_preserves_argument_boundaries(self):
        with patch.object(LAUNCHER, "cache_values", return_value={}), \
             patch.object(LAUNCHER, "executable", return_value=Path("/viewer with spaces")), \
             patch.object(LAUNCHER.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as run:
            self.assertEqual(LAUNCHER.main(["--no-build", "--verify", "--capture-dir", "/tmp/对照 图", "--entry", "marisa-laser", "--variant", "2", "--view", "back"]), 0)
            arguments = run.call_args.args[0]
            self.assertEqual(arguments[0], str(Path("/viewer with spaces")))
            self.assertIn(str(Path("/tmp/对照 图").resolve()), arguments)
            self.assertEqual(arguments[arguments.index("--entry") + 1], "marisa-laser")
            self.assertNotIn("shell", run.call_args.kwargs)
            run.assert_called_once()

    def test_foreign_cache_is_rejected_before_build(self):
        with patch.object(LAUNCHER, "cache_values", return_value={"CMAKE_HOME_DIRECTORY": "/another/project"}), \
             patch.object(LAUNCHER.subprocess, "run") as run:
            with self.assertRaisesRegex(RuntimeError, "another project"):
                LAUNCHER.main([])
            run.assert_not_called()

    def test_build_reuses_local_raylib_and_launches(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "raylib source"
            source.mkdir()
            (source / "CMakeLists.txt").touch()
            cache = {"CMAKE_HOME_DIRECTORY": str(ROOT), "FETCHCONTENT_SOURCE_DIR_RAYLIB": str(source)}
            with patch.object(LAUNCHER, "cache_values", return_value=cache), \
                 patch.object(LAUNCHER.shutil, "which", return_value="/cmake path/cmake"), \
                 patch.object(LAUNCHER, "executable", return_value=Path("/viewer")), \
                 patch.object(LAUNCHER.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as run:
                self.assertEqual(LAUNCHER.main(["--config", "Release"]), 0)
                self.assertEqual(run.call_count, 3)
                self.assertIn(f"-DFETCHCONTENT_SOURCE_DIR_RAYLIB={source.resolve()}", run.call_args_list[0].args[0])
                self.assertIn("projectile_comparison", run.call_args_list[1].args[0])
                self.assertIn("-DCMAKE_BUILD_TYPE=Release", run.call_args_list[0].args[0])


if __name__ == "__main__":
    unittest.main()

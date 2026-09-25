from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RendererBoundaryTests(unittest.TestCase):
    def test_retired_reference_package_is_absent(self) -> None:
        self.assertFalse((ROOT / "pyproject.toml").exists())
        self.assertFalse(any((ROOT / "src" / "analytic_continuation").glob("*.py")))
        self.assertFalse(any((ROOT / "examples").glob("*.json")))

    def test_active_workflows_do_not_invoke_retired_renderer(self) -> None:
        workflows = "\n".join(
            path.read_text()
            for path in sorted((ROOT / ".github" / "workflows").glob("*.yml"))
        ).lower()
        self.assertNotIn("manimgl", workflows)
        self.assertNotIn("src/analytic_continuation/scene.py", workflows)
        self.assertNotIn("prepare_android_movies", workflows)
        self.assertNotIn("test_continuation_path.c", workflows)
        self.assertNotIn("test_factor_state.c", workflows)
        self.assertNotIn("test_factor_snap.c", workflows)
        self.assertNotIn("test_gesture_state.c", workflows)

    def test_dead_wegert_interface_copies_are_absent(self) -> None:
        cpp = ROOT / "android" / "app" / "src" / "main" / "cpp"
        for name in (
            "continuation_path.h",
            "factor_snap.h",
            "factor_state.h",
            "gesture_state.h",
            "polynomial_overlay.h",
        ):
            with self.subTest(name=name):
                self.assertFalse((cpp / name).exists())

    def test_android_launches_only_the_native_explorer(self) -> None:
        manifest = (
            ROOT / "android" / "app" / "src" / "main" / "AndroidManifest.xml"
        ).read_text()
        self.assertIn("ExplorerActivity", manifest)
        self.assertNotIn("MainActivity", manifest)


if __name__ == "__main__":
    unittest.main()

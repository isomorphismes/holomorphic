from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RendererBoundaryTests(unittest.TestCase):
    def test_package_has_no_desktop_animation_dependency(self) -> None:
        project = (ROOT / "pyproject.toml").read_text().lower()
        self.assertNotIn("manimgl", project)
        self.assertNotIn("manimlib", project)

    def test_active_workflows_do_not_invoke_retired_renderer(self) -> None:
        workflows = "\n".join(
            path.read_text()
            for path in sorted((ROOT / ".github" / "workflows").glob("*.yml"))
        ).lower()
        self.assertNotIn("manimgl", workflows)
        self.assertNotIn("src/analytic_continuation/scene.py", workflows)
        self.assertNotIn("prepare_android_movies", workflows)

    def test_android_launches_only_the_native_explorer(self) -> None:
        manifest = (
            ROOT / "android" / "app" / "src" / "main" / "AndroidManifest.xml"
        ).read_text()
        self.assertIn("ExplorerActivity", manifest)
        self.assertNotIn("MainActivity", manifest)


if __name__ == "__main__":
    unittest.main()

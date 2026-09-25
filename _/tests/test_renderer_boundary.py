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
        self.assertNotIn("cc -std=c11 -wall -wextra -werror \\\n            -iandroid/app/src/main/cpp \\\n            tests/test_holomorphic_walk.c", workflows)

    def test_dead_wegert_interface_copies_are_absent(self) -> None:
        cpp = ROOT / "android" / "app" / "src" / "main" / "cpp"
        for name in (
            "continuation_path.h",
            "factor_snap.h",
            "factor_state.h",
            "gesture_state.h",
            "polynomial_overlay.h",
            "holomorphic_walk.c",
            "holomorphic_walk.h",
        ):
            with self.subTest(name=name):
                self.assertFalse((cpp / name).exists())

    def test_cauchy_field_is_gpu_driven(self) -> None:
        cpp = (
            ROOT
            / "android"
            / "app"
            / "src"
            / "main"
            / "cpp"
            / "analytic_continuation_random.c"
        ).read_text()
        shader = (
            ROOT
            / "android"
            / "app"
            / "src"
            / "main"
            / "assets"
            / "continuation.frag.in"
        ).read_text()
        cmake = (
            ROOT / "android" / "app" / "src" / "main" / "cpp" / "CMakeLists.txt"
        ).read_text()

        self.assertIn('glGetUniformLocation(engine->program, "u_time")', cpp)
        self.assertIn("glUniform1f(engine->time_location, animation_time)", cpp)
        self.assertNotIn("holomorphic_walk", cpp)
        self.assertNotIn("holomorphic_walk", cmake)
        self.assertNotIn("u_holomorphic_coefficients", shader)

        self.assertIn("#define SOURCE_COUNT 24", shader)
        self.assertIn("uniform float u_time;", shader)
        self.assertIn("vec2 source_position", shader)
        self.assertIn("vec2 source_weight", shader)
        self.assertIn("vec2 holomorphic_field", shader)
        self.assertIn("float source_radius_scale", shader)
        self.assertIn("return mix(2.75, 4.00", shader)
        self.assertIn("float source_orbit_speed", shader)
        self.assertIn("float source_handedness", shader)
        self.assertIn("return view_radius * radius * orbit;", shader)

    def test_zoom_range_is_not_artificially_tight(self) -> None:
        cpp = (
            ROOT
            / "android"
            / "app"
            / "src"
            / "main"
            / "cpp"
            / "analytic_continuation_random.c"
        ).read_text()
        self.assertIn("if (zoom < 0.1f) zoom = 0.1f;", cpp)
        self.assertIn("if (zoom > 32.0f) zoom = 32.0f;", cpp)

    def test_android_launches_only_the_native_explorer(self) -> None:
        manifest = (
            ROOT / "android" / "app" / "src" / "main" / "AndroidManifest.xml"
        ).read_text()
        self.assertIn("ExplorerActivity", manifest)
        self.assertNotIn("MainActivity", manifest)


if __name__ == "__main__":
    unittest.main()

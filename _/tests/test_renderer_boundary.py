from __future__ import annotations

import re
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

    def test_gpu_flow_source_invariants(self) -> None:
        shader = (
            ROOT / "android" / "app" / "src" / "main" / "assets" /
            "continuation.frag.in"
        ).read_text()
        native = (
            ROOT / "android" / "app" / "src" / "main" / "cpp" /
            "analytic_continuation_random.c"
        ).read_text()
        renderer = (
            ROOT / "android" / "app" / "src" / "main" / "cpp" /
            "analytic_continuation.c"
        ).read_text()
        native_build = (
            ROOT / "android" / "app" / "src" / "main" / "cpp" / "CMakeLists.txt"
        ).read_text()

        # One scalar clock crosses the CPU/GPU boundary.  Both complex
        # coefficients, q_t(w), and q_t'(w) are evaluated by each fragment.
        self.assertIn("uniform float u_flow_time;", shader)
        self.assertRegex(
            shader,
            r"vec2\s+flowing_descriptor\s*\(vec2\s+w,\s*out\s+vec2\s+derivative\)",
        )
        self.assertIn("complex_multiply(linear_coefficient, w)", shader)
        self.assertIn("complex_multiply(quadratic_coefficient, w_squared)", shader)
        self.assertRegex(
            re.sub(r"\s+", " ", shader),
            r"derivative\s*=\s*linear_coefficient\s*\+\s*"
            r"2\.0\s*\*\s*complex_multiply\(quadratic_coefficient, w\)",
        )

        # exp(q_t) is applied as Re(q_t) to log-modulus and Im(q_t) to phase.
        # This is the source-level boundary that preserves the stored zeros and
        # poles without constructing an exponential or changing factor counts.
        compact_shader = re.sub(r"\s+", " ", shader)
        self.assertRegex(
            compact_shader,
            r"float phase\s*=.*\+\s*flow_log_multiplier\.y\s*;",
        )
        self.assertRegex(
            compact_shader,
            r"float log_modulus\s*=.*\+\s*flow_log_multiplier\.x\s*;",
        )
        shader_without_comments = re.sub(r"//.*", "", shader)
        self.assertNotIn("exp(", shader_without_comments)

        self.assertIn("engine->flow_time += dt;", native)
        self.assertEqual(
            re.findall(r"glUniform[^;]*flow[^;]*;", renderer),
            ["glUniform1f(engine->flow_time_location, engine->flow_time);"],
        )
        self.assertNotIn("linear_coefficient", native)
        self.assertNotIn("quadratic_coefficient", native)
        self.assertNotRegex(renderer, r"flow_(?:linear|quadratic)_coefficient_location")
        self.assertNotIn("holomorphic_walk_", native)
        self.assertNotIn("holomorphic_walk.c", native_build)
        self.assertNotIn("dFdx", shader)
        self.assertNotIn("dFdy", shader)


if __name__ == "__main__":
    unittest.main()

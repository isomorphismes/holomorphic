from __future__ import annotations

import cmath
import hashlib
import json
import math
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
FIXTURE_PATH = ROOT / "tests" / "fixtures" / "wegert_color_values.json"
CORE_PATH = ROOT / "android" / "app" / "src" / "main" / "assets" / "wegert_color.glsl"
TEMPLATE_PATH = ROOT / "android" / "app" / "src" / "main" / "assets" / "continuation.frag.in"
WEGERT_MARKER = "/*__WEGERT_COLOR_CORE__*/"
WEGERT_CALL = "vec3 color = wegert_color_from_phase_log_modulus(phase, log_modulus);"


def positive_fract(value: float) -> float:
    return value - math.floor(value)


def srgb_component(linear_value: float) -> float:
    value = max(linear_value, 0.0)
    if value <= 0.0031308:
        return 12.92 * value
    return 1.055 * value ** (1.0 / 2.4) - 0.055


def hcl_to_srgb(hue_degrees: float, chroma: float, lightness: float) -> tuple[float, float, float]:
    hue = math.radians(hue_degrees)
    u_star = chroma * math.cos(hue)
    v_star = chroma * math.sin(hue)

    white_u_prime = 0.19783982482140777
    white_v_prime = 0.46833630293240974

    if lightness > 8.0:
        y = ((lightness + 16.0) / 116.0) ** 3.0
    else:
        y = lightness / 903.2962962962963

    u_prime = u_star / (13.0 * lightness) + white_u_prime
    v_prime = v_star / (13.0 * lightness) + white_v_prime

    x = (9.0 * y * u_prime) / (4.0 * v_prime)
    z = y * (12.0 - 3.0 * u_prime - 20.0 * v_prime) / (4.0 * v_prime)

    linear_r = 3.2404542 * x - 1.5371385 * y - 0.4985314 * z
    linear_g = -0.9692660 * x + 1.8760108 * y + 0.0415560 * z
    linear_b = 0.0556434 * x - 0.2040259 * y + 1.0572252 * z

    return tuple(
        min(max(srgb_component(component), 0.0), 1.0)
        for component in (linear_r, linear_g, linear_b)
    )


def canonical_wegert_color(value: complex) -> tuple[float, float, float]:
    tau = 6.28318530717958647692
    log_10 = 2.30258509299404568402
    phase = math.atan2(value.imag, value.real)
    log_modulus = math.log(max(abs(value), 1.0e-12))
    hue_degrees = 360.0 * positive_fract(phase / tau)
    log_modulus_band = positive_fract(log_modulus / log_10)
    lightness = (
        66.0
        + 4.0 * log_modulus_band
        + 3.0 * positive_fract(hue_degrees / 100.0)
    )
    return hcl_to_srgb(hue_degrees, 45.0, lightness)


class WegertColorParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.fixture = json.loads(FIXTURE_PATH.read_text())

    def test_behavior_oracle_is_value_to_color(self) -> None:
        self.assertEqual(self.fixture["oracle"], "complex value -> Wegert color")

    def test_fixture_spans_phase_quadrants_and_modulus_decades(self) -> None:
        values = [complex(*case["value"]) for case in self.fixture["cases"]]
        quadrants = {
            (1 if value.real > 0.0 else -1, 1 if value.imag > 0.0 else -1)
            for value in values
            if value.real != 0.0 and value.imag != 0.0
        }
        self.assertTrue({(1, 1), (-1, 1), (-1, -1), (1, -1)}.issubset(quadrants))

        decades = [math.log10(abs(value)) for value in values if value != 0.0]
        self.assertGreaterEqual(max(decades) - min(decades), 11.9)

    def test_fixture_includes_non_rational_evaluator(self) -> None:
        case = next(case for case in self.fixture["cases"] if case.get("evaluator") == "exp")
        evaluator_input = complex(*case["input"])
        expected_value = complex(*case["value"])
        actual_value = cmath.exp(evaluator_input)
        self.assertAlmostEqual(actual_value.real, expected_value.real, delta=1.0e-12)
        self.assertAlmostEqual(actual_value.imag, expected_value.imag, delta=1.0e-12)

    def test_fixture_matches_canonical_colors_with_explicit_tolerance(self) -> None:
        tolerance = float(self.fixture["color_tolerance"])
        self.assertEqual(tolerance, 2.0e-5)

        for case in self.fixture["cases"]:
            with self.subTest(case=case["name"]):
                actual = canonical_wegert_color(complex(*case["value"]))
                for channel, expected in zip(actual, case["rgb"]):
                    self.assertAlmostEqual(channel, expected, delta=tolerance)

    def test_canonical_glsl_core_is_locked_to_fixture(self) -> None:
        digest = hashlib.sha256(CORE_PATH.read_bytes()).hexdigest()
        self.assertEqual(digest, self.fixture["canonical_core_sha256"])

    def test_analytic_shader_uses_canonical_core_instead_of_copying_palette(self) -> None:
        template = TEMPLATE_PATH.read_text()
        self.assertEqual(template.count(WEGERT_MARKER), 1)
        self.assertEqual(template.count(WEGERT_CALL), 1)

        copied_color_implementation_tokens = (
            "const float TAU",
            "const float LOG_10",
            "float positive_fract(",
            "float srgb_component(",
            "vec3 hcl_to_srgb(",
            "white_u_prime",
            "white_v_prime",
            "3.2404542",
        )
        for token in copied_color_implementation_tokens:
            with self.subTest(token=token):
                self.assertNotIn(token, template)

    def test_app_overlays_are_outside_value_to_color_oracle(self) -> None:
        template = TEMPLATE_PATH.read_text()
        color_boundary = template.index(WEGERT_CALL)
        overlay_tokens = (
            "color = mix(color, vec3(0.97, 0.97, 0.94), boundary);",
            "length(z - u_zero_positions[index])",
            "length(z - u_pole_positions[index])",
            "bool zero_selected = u_placement_kind == 0;",
        )
        for token in overlay_tokens:
            with self.subTest(token=token):
                self.assertGreater(template.index(token), color_boundary)

    def test_canonical_core_contains_no_app_overlay_state(self) -> None:
        core = CORE_PATH.read_text()
        for token in (
            "uniform ",
            "gl_FragCoord",
            "u_zero_positions",
            "u_pole_positions",
            "u_placement_kind",
            "circle_mask",
            "ring_mask",
            "line_mask",
        ):
            with self.subTest(token=token):
                self.assertNotIn(token, core)


if __name__ == "__main__":
    unittest.main()

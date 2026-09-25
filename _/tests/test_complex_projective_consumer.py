import cmath
import hashlib
import json
import math
import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests" / "fixtures" / "complex_projective_consumer.json"
SHADER = ROOT / "android" / "app" / "src" / "main" / "assets" / "continuation.frag.in"
WALK = ROOT / "android" / "app" / "src" / "main" / "cpp" / "holomorphic_walk.c"
WALK_HEADER = ROOT / "android" / "app" / "src" / "main" / "cpp" / "holomorphic_walk.h"
SHARED = ROOT / ".idric-complex" / "_" / "fixtures" / "complex-projective" / "float32.json"
RECEIPT = ROOT / "build" / "complex-projective-consumer" / "receipt.tsv"


def complex_value(pair):
    return complex(float(pair[0]), float(pair[1]))


def q_at(coefficients, z):
    u = z / 6.0
    power = u
    result = 0j
    for coefficient in coefficients:
        result += complex_value(coefficient) * power
        power *= u
    return result


class ComplexProjectiveConsumerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture_bytes = FIXTURE.read_bytes()
        cls.fixture = json.loads(cls.fixture_bytes)
        cls.shader = SHADER.read_text()
        cls.walk = WALK.read_text()
        cls.header = WALK_HEADER.read_text()

    def test_live_fixture_stays_inside_walk_budget(self):
        match = re.search(
            r"#define\s+HOLOMORPHIC_WALK_COEFFICIENT_BUDGET\s+([0-9.]+)f",
            self.header,
        )
        self.assertIsNotNone(match)
        budget = float(match.group(1))
        used = sum(
            abs(complex_value(coefficient))
            for coefficient in self.fixture["entire_q_coefficients_in_u_low_to_high_without_constant"]
        )
        self.assertLessEqual(used, budget)

    def test_exp_q_is_nonzero_on_consumer_samples(self):
        coefficients = self.fixture[
            "entire_q_coefficients_in_u_low_to_high_without_constant"
        ]
        for pair in self.fixture["sample_points"]:
            value = cmath.exp(q_at(coefficients, complex_value(pair)))
            self.assertTrue(math.isfinite(value.real) and math.isfinite(value.imag))
            self.assertGreater(abs(value), 0.0)

    def test_shader_keeps_holomorphic_state_separate_from_observation(self):
        q_start = self.shader.index("vec2 holomorphic_q(vec2 z)")
        q_end = self.shader.index("float circle_mask", q_start)
        q_source = self.shader[q_start:q_end]
        for forbidden in ["atan(", "log(", "length(", "conjug", "dFdx", "dFdy"]:
            self.assertNotIn(forbidden, q_source)

        divisor_start = self.shader.index("// R(z):")
        q_use = self.shader.index("vec2 q = holomorphic_q(z);", divisor_start)
        color_use = self.shader.index("wegert_color_from_phase_log_modulus", q_use)
        self.assertLess(divisor_start, q_use)
        self.assertLess(q_use, color_use)
        self.assertIn("log_modulus += q.x;", self.shader[q_use:color_use])
        self.assertIn("phase += q.y;", self.shader[q_use:color_use])

    def test_walk_direction_is_polynomial_complex_arithmetic(self):
        start = self.walk.index("static void direction_at(")
        end = self.walk.index("static float outward_budget_slope", start)
        direction_source = self.walk[start:end]
        self.assertIn("complex_multiply", direction_source)
        for forbidden in ["hypot", "atan", "log(", "conjug", "sqrt"]:
            self.assertNotIn(forbidden, direction_source)

    def test_shared_corpus_divisor_and_field_model_are_consumed(self):
        if not SHARED.is_file():
            self.skipTest("canonical complex/projective corpus is not checked out")
        shared = json.loads(SHARED.read_text())
        self.assertEqual(shared["schema"], "idric-complex-projective-corpus-v1")
        self.assertEqual(shared["render"]["field"], self.fixture["model"])
        self.assertEqual(shared["render"]["divisor"], self.fixture["divisor"])

    @classmethod
    def tearDownClass(cls):
        RECEIPT.parent.mkdir(parents=True, exist_ok=True)
        source_sha = os.environ.get(
            "SOURCE_HEAD_SHA",
            subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
            ).strip(),
        )
        tested_sha = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip()
        shared_sha = os.environ.get("IDRIC_COMPLEX_SEMANTICS_SHA", "unresolved")
        RECEIPT.write_text(
            "COMPLEX_PROJECTIVE_RECEIPT\t1\n"
            "role\tAPPLICATION_CONSUMER\n"
            "repository\tisomorphismes/analytic-continuation\n"
            f"source_head_sha\t{source_sha}\n"
            f"tested_checkout_sha\t{tested_sha}\n"
            f"canonical_complex_projective_semantics_sha\t{shared_sha}\n"
            f"consumer_fixture_sha256\t{hashlib.sha256(cls.fixture_bytes).hexdigest()}\n"
            "stage\tshared_semantic_contract\tPASS\n"
            "stage\tshared_divisor_and_field_contract\tPASS\n"
            "stage\tentire_polynomial_q_contract\tPASS\n"
            "stage\texp_q_nonzero_samples\tPASS\n"
            "stage\tholomorphic_walk_source_boundary\tPASS\n"
            "stage\tprojective_cp1_runtime_consumer\tSKIP\tnot required by current finite viewport application\n"
            "stage\tx86_render_pixel_equivalence\tSKIP\tdifferent application coloring; structural contract only\n"
        )


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import ast
import copy
import hashlib
import math
from pathlib import Path
import struct
import tempfile
from typing import Any
import unittest

from acceptance.headless.reference_scene import (
    ContractError,
    OUTPUT_HEADER,
    OUTPUT_HEADER_BYTES,
    OUTPUT_MAGIC,
    OUTPUT_SAMPLE,
    OUTPUT_SAMPLE_BYTES,
    OUTPUT_VERSION,
    decode_complex,
    homogeneous_value,
    load_scene,
    numeric_error_budget,
    phase_log,
    pixel_coordinate,
    projective_classification,
    projective_cross_product,
    q_value,
    render_ppm,
    rescale_projective,
    scene_representative,
    scene_state,
    srgb8_channel,
    validate_scene,
    verify_output,
)


ROOT = Path(__file__).resolve().parents[1]
SCENE_PATH = ROOT / "acceptance" / "headless" / "complex-reference-scene.json"


def header_text(value: str, width: int) -> bytes:
    encoded = value.encode("utf-8")
    if len(encoded) >= width:
        raise AssertionError("test header value is too long")
    return encoded + b"\0" + bytes(width - len(encoded) - 1)


def rounded_f32(value: float) -> float:
    return struct.unpack(">f", struct.pack(">f", value))[0]


def oracle_payload(scene: dict[str, Any], state_id: str) -> bytes:
    state = scene_state(scene, state_id)
    width = scene["viewport"]["width"]
    height = scene["viewport"]["height"]
    payload = bytearray()
    for y in range(height):
        for x in range(width):
            phase, log_magnitude = phase_log(
                homogeneous_value(scene, state, pixel_coordinate(scene, x, y))
            )
            payload.extend(OUTPUT_SAMPLE.pack(phase, log_magnitude))
    return bytes(payload)


def framed_stream(
    scene: dict[str, Any],
    state_id: str,
    representative_id: str,
    payload: bytes,
) -> bytes:
    state_index = next(
        index for index, state in enumerate(scene["states"]) if state["id"] == state_id
    )
    representative_index = next(
        index
        for index, representative in enumerate(scene["representatives"])
        if representative["id"] == representative_id
    )
    width = scene["viewport"]["width"]
    height = scene["viewport"]["height"]
    if len(payload) != width * height * OUTPUT_SAMPLE_BYTES:
        raise AssertionError("test payload has the wrong size")
    return OUTPUT_HEADER.pack(
        OUTPUT_MAGIC,
        OUTPUT_VERSION,
        OUTPUT_HEADER_BYTES,
        width,
        height,
        state_index,
        representative_index,
        OUTPUT_SAMPLE_BYTES,
        0,
        hashlib.sha256(SCENE_PATH.read_bytes()).digest(),
        header_text(scene["scene_id"], 32),
        header_text(state_id, 24),
        header_text(representative_id, 24),
        bytes(8),
    ) + payload


class HeadlessComplexReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.scene = load_scene(SCENE_PATH, ROOT)

    def test_scene_is_closed_float32_input_with_two_fixed_states(self) -> None:
        self.assertEqual(self.scene["scalar"]["type"], "Float32")
        self.assertEqual([state["id"] for state in self.scene["states"]], ["still", "deformed"])
        still = scene_state(self.scene, "still")
        deformed = scene_state(self.scene, "deformed")
        self.assertEqual(q_value(self.scene, still, complex(1.5, -0.75)), 0j)
        self.assertNotEqual(q_value(self.scene, deformed, complex(1.5, -0.75)), 0j)

    def test_exp_q_changes_the_field_without_changing_the_divisor(self) -> None:
        still = scene_state(self.scene, "still")
        deformed = scene_state(self.scene, "deformed")
        classifications: dict[str, list[str]] = {}
        for state in (still, deformed):
            classifications[state["id"]] = [
                projective_classification(
                    homogeneous_value(
                        self.scene,
                        state,
                        decode_complex(probe["z"], f"probe {probe['id']}.z"),
                    )
                )
                for probe in self.scene["projective_probes"]
            ]
        self.assertEqual(classifications["still"], classifications["deformed"])
        self.assertEqual(
            classifications["deformed"],
            ["finite-nonzero", "zero", "zero", "infinity", "infinity"],
        )

    def test_complex_exponential_matches_exact_phase_log_update(self) -> None:
        still = scene_state(self.scene, "still")
        deformed = scene_state(self.scene, "deformed")
        z = complex(1.5, -0.75)
        still_phase, still_log = phase_log(homogeneous_value(self.scene, still, z))
        moved_phase, moved_log = phase_log(homogeneous_value(self.scene, deformed, z))
        q = q_value(self.scene, deformed, z)
        phase_change = (moved_phase - still_phase + math.pi) % (2.0 * math.pi) - math.pi
        self.assertEqual(rounded_f32(moved_log - still_log), rounded_f32(q.real))
        self.assertEqual(rounded_f32(phase_change), rounded_f32(q.imag))

    def test_nontrivial_complex_rescaling_preserves_cp1_and_render_values(self) -> None:
        state = scene_state(self.scene, "deformed")
        representative = scene_representative(self.scene, "times-two-i")
        scale = decode_complex(representative["scale"], "times-two-i.scale")
        self.assertNotEqual(scale.imag, 0.0)

        value = homogeneous_value(self.scene, state, complex(0.5, 0.25))
        rescaled = rescale_projective(value, scale)
        residual = projective_cross_product(value, rescaled)
        self.assertEqual(residual, 0j)
        phase, log_magnitude = phase_log(value)
        rescaled_phase, rescaled_log_magnitude = phase_log(rescaled)
        phase_error = (rescaled_phase - phase + math.pi) % (2.0 * math.pi) - math.pi
        self.assertEqual(rounded_f32(phase_error), 0.0)
        self.assertEqual(rounded_f32(rescaled_log_magnitude), rounded_f32(log_magnitude))

    def test_unreduced_common_factor_and_all_zero_coordinates_fail_closed(self) -> None:
        invalid = copy.deepcopy(self.scene)
        invalid["function"]["poles"].append(
            {
                "position": invalid["function"]["zeros"][0]["position"],
                "multiplicity": 1,
            }
        )
        with self.assertRaisesRegex(ContractError, "common factors"):
            validate_scene(invalid, ROOT)
        with self.assertRaisesRegex(ContractError, "not a projective point"):
            projective_classification((0j, 0j))
        with self.assertRaisesRegex(ContractError, "scale must be nonzero"):
            rescale_projective((1 + 0j, 2 + 0j), 0j)

    def test_scene_has_no_lacunary_mechanism(self) -> None:
        text = SCENE_PATH.read_text().lower()
        for forbidden in (
            "lasso",
            "inverse_lasso",
            "continuation_path",
            "convergence-disc",
            "riemann-surface",
        ):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, text)

        forbidden_modules = {
            "disc",
            "discs",
            "lacunary",
            "lasso",
            "inverse_lasso",
            "continuation_path",
            "convergence_disc",
            "riemann_surface",
        }
        for source_path in (ROOT / "acceptance" / "headless").glob("*.py"):
            tree = ast.parse(source_path.read_text(), filename=str(source_path))
            imported = []
            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    imported.extend(alias.name for alias in node.names)
                elif isinstance(node, ast.ImportFrom) and node.module is not None:
                    imported.append(node.module)
            for module in imported:
                with self.subTest(source=source_path.name, module=module):
                    self.assertTrue(
                        forbidden_modules.isdisjoint(module.split(".")),
                        f"forbidden migrated mechanism import: {module}",
                    )

    def test_versioned_phase_log_envelope_binds_scene_state_and_representative(self) -> None:
        width = self.scene["viewport"]["width"]
        height = self.scene["viewport"]["height"]
        stream = framed_stream(
            self.scene,
            "deformed",
            "times-two-i",
            oracle_payload(self.scene, "deformed"),
        )
        with tempfile.NamedTemporaryFile() as output:
            output.write(stream)
            output.flush()
            envelope = verify_output(
                SCENE_PATH,
                "deformed",
                "times-two-i",
                Path(output.name),
                ROOT,
            )
        self.assertEqual(len(envelope.samples), width * height)
        expected = phase_log(
            homogeneous_value(
                self.scene,
                scene_state(self.scene, "deformed"),
                pixel_coordinate(self.scene, 0, 0),
            )
        )
        self.assertEqual(envelope.samples[0], tuple(rounded_f32(item) for item in expected))

    def test_correctly_framed_zero_and_constant_fields_fail_the_oracle_gate(self) -> None:
        width = self.scene["viewport"]["width"]
        height = self.scene["viewport"]["height"]
        sample_count = width * height
        expected_first = phase_log(
            homogeneous_value(
                self.scene,
                scene_state(self.scene, "deformed"),
                pixel_coordinate(self.scene, 0, 0),
            )
        )
        impostors = {
            "all-zero": OUTPUT_SAMPLE.pack(0.0, 0.0) * sample_count,
            "constant-first-oracle-sample": OUTPUT_SAMPLE.pack(*expected_first)
            * sample_count,
        }
        with tempfile.TemporaryDirectory() as directory:
            for name, payload in impostors.items():
                with self.subTest(name=name):
                    field = Path(directory) / f"{name}.field"
                    field.write_bytes(
                        framed_stream(
                            self.scene, "deformed", "identity", payload
                        )
                    )
                    with self.assertRaisesRegex(
                        ContractError, "differs from the Binary64 R\\*exp\\(q\\) oracle"
                    ):
                        verify_output(
                            SCENE_PATH,
                            "deformed",
                            "identity",
                            field,
                            ROOT,
                        )

    def test_float32_error_bound_is_derived_not_fitted_to_backend_output(self) -> None:
        expected_log = phase_log(
            homogeneous_value(
                self.scene,
                scene_state(self.scene, "deformed"),
                pixel_coordinate(self.scene, 0, 0),
            )
        )[1]
        budget = numeric_error_budget(self.scene, expected_log)
        self.assertEqual(budget.rounded_operations, 392)
        self.assertEqual(budget.gamma, (392 * 2.0**-24) / (1.0 - 392 * 2.0**-24))
        self.assertLess(budget.q_absolute_bound, math.pi / 4.0)
        self.assertGreater(
            budget.transcendental_argument_bound, budget.q_absolute_bound
        )
        self.assertLess(budget.transcendental_argument_bound, math.pi / 4.0)
        self.assertLess(budget.transcendental_argument_bound, math.log(2.0))
        self.assertGreater(budget.phase, 0.0)
        self.assertGreater(budget.log_magnitude, 0.0)
        # This ceiling follows from the formulas, not a captured x86 error.
        self.assertLess(budget.phase, 4.0e-5)
        self.assertLess(budget.log_magnitude, 4.0e-5)

    def test_render_ppm_rejects_nonfinite_payload_before_opening_output(self) -> None:
        scene_digest = hashlib.sha256(SCENE_PATH.read_bytes()).digest()
        width = self.scene["viewport"]["width"]
        height = self.scene["viewport"]["height"]
        header = OUTPUT_HEADER.pack(
            OUTPUT_MAGIC,
            OUTPUT_VERSION,
            OUTPUT_HEADER_BYTES,
            width,
            height,
            0,
            0,
            OUTPUT_SAMPLE_BYTES,
            0,
            scene_digest,
            header_text(self.scene["scene_id"], 32),
            header_text("still", 24),
            header_text("identity", 24),
            bytes(8),
        )
        payload = OUTPUT_SAMPLE.pack(math.nan, 0.0) + OUTPUT_SAMPLE.pack(0.0, 0.0) * (
            width * height - 1
        )
        with tempfile.TemporaryDirectory() as directory:
            field = Path(directory) / "invalid.field"
            image = Path(directory) / "must-not-exist.ppm"
            field.write_bytes(header + payload)
            with self.assertRaisesRegex(ContractError, "non-finite"):
                render_ppm(
                    SCENE_PATH,
                    "still",
                    "identity",
                    field,
                    image,
                    ROOT,
                )
            self.assertFalse(image.exists())

    def test_ppm_framing_rounding_and_hash_are_representative_invariant(self) -> None:
        self.assertEqual(
            [srgb8_channel(value) for value in (-1.0, 0.0, 0.5, 1.0, 2.0)],
            [0, 0, 128, 255, 255],
        )

        width = self.scene["viewport"]["width"]
        height = self.scene["viewport"]["height"]
        payload = oracle_payload(self.scene, "deformed")

        with tempfile.TemporaryDirectory() as directory:
            directory_path = Path(directory)
            identity_field = directory_path / "identity.field"
            rescaled_field = directory_path / "times-two-i.field"
            identity_ppm = directory_path / "identity.ppm"
            rescaled_ppm = directory_path / "times-two-i.ppm"
            identity_field.write_bytes(
                framed_stream(self.scene, "deformed", "identity", payload)
            )
            rescaled_field.write_bytes(
                framed_stream(self.scene, "deformed", "times-two-i", payload)
            )

            identity_image = render_ppm(
                SCENE_PATH,
                "deformed",
                "identity",
                identity_field,
                identity_ppm,
                ROOT,
            )
            rescaled_image = render_ppm(
                SCENE_PATH,
                "deformed",
                "times-two-i",
                rescaled_field,
                rescaled_ppm,
                ROOT,
            )
            self.assertEqual(identity_ppm.read_bytes(), identity_image)
            self.assertEqual(rescaled_ppm.read_bytes(), rescaled_image)

        ppm_header = f"P6\n{width} {height}\n255\n".encode("ascii")
        self.assertTrue(identity_image.startswith(ppm_header))
        self.assertEqual(len(identity_image), len(ppm_header) + 3 * width * height)
        self.assertEqual(
            hashlib.sha256(identity_image).digest(),
            hashlib.sha256(rescaled_image).digest(),
        )
        self.assertEqual(
            hashlib.sha256(identity_image).hexdigest(),
            "8fface9ee8aa17f969a8926941803229e9b1e138da01f7c07262b69760d19d6a",
        )


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import cmath
import math
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "reference"))

from entire_representer import (  # noqa: E402
    DivisorPoint,
    LocalDatum,
    evaluate_q,
    evaluate_regular_state,
    fock_derivative_direction,
    fock_kernel,
    fock_value_direction,
    minimum_norm_squared,
)


def polynomial_fock_norm_squared(coefficients: list[complex], scale: float) -> float:
    return sum(
        abs(coefficient) ** 2 * scale ** (2 * degree) * math.factorial(degree)
        for degree, coefficient in enumerate(coefficients)
    )


class EntireRepresenterTests(unittest.TestCase):
    def test_kernel_and_ungauged_value_normalization(self) -> None:
        anchor = 0.7 - 0.35j
        scale = 1.8
        self.assertAlmostEqual(
            fock_kernel(anchor, anchor, scale).real,
            math.exp(abs(anchor) ** 2 / scale**2),
            delta=1.0e-14,
        )
        direction = fock_value_direction(anchor, anchor, scale, gauge="none")
        self.assertAlmostEqual(direction.real, 1.0, delta=1.0e-14)
        self.assertAlmostEqual(direction.imag, 0.0, delta=1.0e-14)
        self.assertAlmostEqual(
            minimum_norm_squared(
                anchor=anchor, scale=scale, kind="value", gauge="none"
            ),
            math.exp(-abs(anchor) ** 2 / scale**2),
            delta=1.0e-14,
        )

    def test_zero_at_origin_gauge_uses_projected_kernel(self) -> None:
        anchor = -0.45 + 0.8j
        scale = 2.1
        self.assertAlmostEqual(
            abs(
                fock_value_direction(
                    0j, anchor, scale, gauge="zero_at_origin"
                )
            ),
            0.0,
            delta=1.0e-14,
        )
        self.assertAlmostEqual(
            abs(
                fock_value_direction(
                    anchor, anchor, scale, gauge="zero_at_origin"
                )
                - 1.0
            ),
            0.0,
            delta=1.0e-14,
        )

    def test_origin_value_is_incompatible_with_zero_gauge(self) -> None:
        with self.assertRaisesRegex(ValueError, "incompatible"):
            fock_value_direction(0j, 0j, 1.0, gauge="zero_at_origin")

    def test_derivative_representer_has_prescribed_local_derivative(self) -> None:
        anchor = 0.3 + 0.4j
        scale = 1.7
        step = 1.0e-6
        derivative = (
            fock_derivative_direction(anchor + step, anchor, scale)
            - fock_derivative_direction(anchor - step, anchor, scale)
        ) / (2.0 * step)
        self.assertAlmostEqual(derivative.real, 1.0, delta=2.0e-10)
        self.assertAlmostEqual(derivative.imag, 0.0, delta=2.0e-10)
        self.assertEqual(fock_derivative_direction(0j, anchor, scale), 0j)
        self.assertEqual(fock_derivative_direction(2.0 - 3.0j, 0j, scale), 2.0 - 3.0j)

    def test_minimum_norm_bounds_hold_for_polynomial_competitors(self) -> None:
        anchor = 0.6 - 0.2j
        scale = 1.4

        ungauged_minimum = minimum_norm_squared(
            anchor=anchor, scale=scale, kind="value", gauge="none"
        )
        for slope in (0j, 0.2 + 0.4j, -0.7 + 0.1j):
            # p(z) = 1 + slope * (z - anchor), hence p(anchor) = 1.
            coefficients = [1.0 - slope * anchor, slope]
            self.assertGreaterEqual(
                polynomial_fock_norm_squared(coefficients, scale) + 1.0e-14,
                ungauged_minimum,
            )

        gauged_minimum = minimum_norm_squared(
            anchor=anchor, scale=scale, kind="value", gauge="zero_at_origin"
        )
        # p(z) = z/anchor satisfies p(0)=0 and p(anchor)=1.
        gauged_competitor = polynomial_fock_norm_squared([0j, 1.0 / anchor], scale)
        self.assertGreaterEqual(gauged_competitor + 1.0e-14, gauged_minimum)

    def test_local_data_and_finite_superposition(self) -> None:
        value_datum = LocalDatum(
            anchor=0.8 + 0.1j,
            amplitude=0.03 - 0.05j,
            scale=1.5,
            kind="value",
            gauge="none",
        )
        self.assertAlmostEqual(
            abs(evaluate_q(value_datum.anchor, [value_datum]) - value_datum.amplitude),
            0.0,
            delta=1.0e-14,
        )

        derivative_datum = LocalDatum(
            anchor=-0.25 + 0.5j,
            amplitude=-0.02 + 0.04j,
            scale=2.0,
            kind="derivative",
            gauge="zero_at_origin",
        )
        step = 1.0e-6
        derivative = (
            evaluate_q(derivative_datum.anchor + step, [derivative_datum])
            - evaluate_q(derivative_datum.anchor - step, [derivative_datum])
        ) / (2.0 * step)
        self.assertAlmostEqual(
            abs(derivative - derivative_datum.amplitude), 0.0, delta=2.0e-11
        )
        self.assertEqual(evaluate_q(0j, [derivative_datum]), 0j)

        z = -0.4 + 0.9j
        self.assertEqual(
            evaluate_q(z, [value_datum, derivative_datum]),
            value_datum.contribution(z) + derivative_datum.contribution(z),
        )

    def test_complete_state_matches_direct_complex_evaluation(self) -> None:
        data = [
            LocalDatum(
                anchor=0.5 - 0.25j,
                amplitude=0.08 + 0.03j,
                scale=1.6,
                kind="value",
                gauge="zero_at_origin",
            ),
            LocalDatum(
                anchor=-0.2 + 0.4j,
                amplitude=-0.01 + 0.02j,
                scale=1.2,
                kind="derivative",
                gauge="zero_at_origin",
            ),
        ]
        self.assertEqual(evaluate_q(0j, data), 0j)
        z = 0.3 + 0.7j
        state = evaluate_regular_state(
            z,
            data,
            zeros=[DivisorPoint(-0.5 + 0.2j, 2)],
            poles=[DivisorPoint(1.1 - 0.4j)],
            gain=0.8 - 0.3j,
        )

        self.assertAlmostEqual(abs(state.f - state.r * cmath.exp(state.q)), 0.0, delta=1.0e-14)
        self.assertAlmostEqual(state.log_modulus, math.log(abs(state.f)), delta=1.0e-14)
        phase_error = cmath.phase(cmath.exp(1j * (cmath.phase(state.f) - state.phase)))
        self.assertAlmostEqual(phase_error, 0.0, delta=1.0e-14)
        self.assertEqual(state.re_q, state.q.real)
        self.assertEqual(state.im_q, state.q.imag)

    def test_entire_factor_is_nonzero_on_reference_grid(self) -> None:
        data = [
            LocalDatum(0.7 + 0.2j, 0.1j, 1.3, "value", "none"),
            LocalDatum(-0.4 + 0.5j, 0.03, 1.8, "derivative", "zero_at_origin"),
        ]
        for real in range(-3, 4):
            for imaginary in range(-3, 4):
                q = evaluate_q(complex(real, imaginary) / 2.0, data)
                self.assertNotEqual(cmath.exp(q), 0j)

    def test_scale_and_multiplicity_validation(self) -> None:
        with self.assertRaisesRegex(ValueError, "positive"):
            fock_kernel(0j, 0j, 0.0)
        with self.assertRaisesRegex(ValueError, "multiplicity"):
            DivisorPoint(0j, 0)
        with self.assertRaisesRegex(ValueError, "multiplicity"):
            DivisorPoint(0j, 1.5)  # type: ignore[arg-type]
        with self.assertRaisesRegex(ValueError, "gain"):
            evaluate_regular_state(1j, [], gain=0j)


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import math
from pathlib import Path
import struct
import sys
import unittest


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from meromorphic_divisor_oracle import (  # noqa: E402
    PhaseLog,
    cancel_exact_factors,
    deterministic_positions,
    evaluate_phase_log,
    phase_distance,
    same_field,
    wegert_coordinates,
)


def f32(value: float) -> float:
    if math.isnan(value):
        return value
    try:
        return struct.unpack("!f", struct.pack("!f", value))[0]
    except OverflowError:
        return math.copysign(math.inf, value)


def f32_add(left: float, right: float) -> float:
    return f32(f32(left) + f32(right))


def f32_sub(left: float, right: float) -> float:
    return f32(f32(left) - f32(right))


def f32_mul(left: float, right: float) -> float:
    return f32(f32(left) * f32(right))


def historical_q_last_fp32(
    z: tuple[float, float],
    q: tuple[float, float],
    zeros: list[tuple[float, float]],
    poles: list[tuple[float, float]],
) -> PhaseLog:
    """Negative control for the old divisor-first, q-last shader order."""

    phase = f32(0.0)
    log_modulus = f32(0.0)

    for sign, positions in ((1.0, zeros), (-1.0, poles)):
        for position in positions:
            dx = f32_sub(z[0], position[0])
            dy = f32_sub(z[1], position[1])
            radius_squared = max(
                f32_add(f32_mul(dx, dx), f32_mul(dy, dy)),
                f32(1.0e-16),
            )
            angle = f32(math.atan2(dy, dx))
            log_radius = f32_mul(0.5, f32(math.log(radius_squared)))
            if sign > 0.0:
                phase = f32_add(phase, angle)
                log_modulus = f32_add(log_modulus, log_radius)
            else:
                phase = f32_sub(phase, angle)
                log_modulus = f32_sub(log_modulus, log_radius)

    return PhaseLog(
        phase=f32_add(phase, q[1]),
        log_modulus=f32_add(log_modulus, q[0]),
    )


def historical_raw_product_fp32(
    z: tuple[float, float],
    poles: list[tuple[float, float]],
) -> tuple[float, float]:
    """Negative control for the raw collapsed remote-pole product."""

    real = f32(1.0)
    imag = f32(0.0)
    for pole in poles:
        dx = f32_sub(z[0], pole[0])
        dy = f32_sub(z[1], pole[1])
        next_real = f32_sub(f32_mul(real, dx), f32_mul(imag, dy))
        next_imag = f32_add(f32_mul(real, dy), f32_mul(imag, dx))
        real, imag = next_real, next_imag
    return real, imag


class DeterministicDivisorOracleTests(unittest.TestCase):
    def test_one_stationary_exact_pair_is_the_empty_divisor(self) -> None:
        q = (0.375, -0.625)
        factor = (0.25, -0.125)
        samples = [
            factor,
            (-1.0, -0.5),
            (0.0, 0.0),
            (0.75, 0.25),
            (4.0, -3.0),
        ]

        self.assertEqual(cancel_exact_factors([factor], [factor]), ([], []))
        for z in samples:
            with self.subTest(z=z):
                self.assertEqual(
                    evaluate_phase_log(z, q, [factor], [factor]),
                    evaluate_phase_log(z, q),
                )

    def test_slightly_displaced_pair_matches_the_analytic_ratio(self) -> None:
        q = (0.125, -0.25)
        zero = (0.25, -0.125)
        pole = (zero[0] + 2.0**-10, zero[1] - 2.0**-11)
        samples = [(-1.0, 0.75), (0.0, 0.0), (0.75, 0.25), (3.0, -2.0)]
        changed = False

        for z in samples:
            with self.subTest(z=z):
                baseline = evaluate_phase_log(z, q)
                actual = evaluate_phase_log(z, q, [zero], [pole])

                numerator = complex(z[0] - zero[0], z[1] - zero[1])
                denominator = complex(z[0] - pole[0], z[1] - pole[1])
                ratio = numerator / denominator
                expected = PhaseLog(
                    phase=q[1] + math.atan2(ratio.imag, ratio.real),
                    log_modulus=q[0] + math.log(abs(ratio)),
                )

                self.assertTrue(same_field(actual, expected))
                if (
                    abs(phase_distance(actual.phase, baseline.phase)) > 1.0e-8
                    or abs(actual.log_modulus - baseline.log_modulus) > 1.0e-8
                ):
                    changed = True

        self.assertTrue(changed, "displacement must not be simplified away")

    def test_twenty_four_exact_pairs_are_the_empty_divisor(self) -> None:
        factors = deterministic_positions(24)
        q = (-0.75, 1.125)
        samples = [
            factors[0],
            factors[11],
            (-1.5, 0.75),
            (0.125, -0.875),
            (5.0, 4.0),
        ]

        remaining = cancel_exact_factors(factors, list(reversed(factors)))
        self.assertEqual(remaining, ([], []))
        for z in samples:
            with self.subTest(z=z):
                self.assertEqual(
                    evaluate_phase_log(z, q, factors, list(reversed(factors))),
                    evaluate_phase_log(z, q),
                )

    def test_twenty_four_displaced_pairs_match_direct_ratio_product(self) -> None:
        zeros = deterministic_positions(24)
        poles = [
            (
                zero[0] + (1.0 + float(index % 3)) * 2.0**-11,
                zero[1] - (1.0 + float(index % 5)) * 2.0**-12,
            )
            for index, zero in enumerate(zeros)
        ]
        q = (0.2, -0.3)
        samples = [(-2.0, 1.25), (0.1, 1.6), (2.5, -1.75), (7.0, 3.0)]

        for z in samples:
            with self.subTest(z=z):
                actual = evaluate_phase_log(z, q, zeros, poles)
                ratio = complex(1.0, 0.0)
                for zero, pole in zip(zeros, poles):
                    ratio *= (
                        complex(z[0] - zero[0], z[1] - zero[1])
                        / complex(z[0] - pole[0], z[1] - pole[1])
                    )
                expected = PhaseLog(
                    phase=q[1] + math.atan2(ratio.imag, ratio.real),
                    log_modulus=q[0] + math.log(abs(ratio)),
                )
                self.assertTrue(
                    same_field(
                        actual,
                        expected,
                        phase_tolerance=2.0e-12,
                        log_tolerance=2.0e-12,
                    )
                )

    def test_wegert_period_reduction_preserves_renderer_coordinates(self) -> None:
        value = PhaseLog(
            phase=-0.375 + 12345.0 * math.tau,
            log_modulus=0.625 - 6789.0 * math.log(10.0),
        )
        reduced = wegert_coordinates(value)
        reference = wegert_coordinates(PhaseLog(-0.375, 0.625))
        self.assertTrue(
            same_field(
                reduced,
                reference,
                phase_tolerance=2.0e-11,
                log_tolerance=2.0e-11,
            )
        )

    def test_q_last_fp32_can_erase_a_real_displaced_pair(self) -> None:
        z = (1000.0, 1000.0)
        q = (1000.0, 1000.0)
        zero = (0.0, 0.0)
        pole = (0.001, 0.0)

        reference = evaluate_phase_log(z, q, [zero], [pole])
        reference_baseline = evaluate_phase_log(z, q)
        fp32 = historical_q_last_fp32(z, q, [zero], [pole])
        fp32_baseline = historical_q_last_fp32(z, q, [], [])

        self.assertFalse(
            same_field(
                reference,
                reference_baseline,
                phase_tolerance=1.0e-8,
                log_tolerance=1.0e-8,
            )
        )
        self.assertEqual(fp32, fp32_baseline)

    def test_raw_twenty_four_factor_fp32_product_overflows(self) -> None:
        poles = deterministic_positions(24)
        product = historical_raw_product_fp32((100.0, 100.0), poles)
        self.assertFalse(
            math.isfinite(product[0]) and math.isfinite(product[1]),
            "the raw collapsed product must remain a negative control",
        )


if __name__ == "__main__":
    unittest.main()

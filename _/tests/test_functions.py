from __future__ import annotations

import importlib.util
import math
import unittest

from analytic_continuation.functions import (
    ComplexFunction,
    FunctionSpecError,
    make_complex_function,
)


class ElementaryFunctionTests(unittest.TestCase):
    def test_existing_positional_poles_and_branch_points_remain_compatible(self) -> None:
        function = ComplexFunction(
            "legacy",
            "legacy(z)",
            lambda value: value,
            "test function",
            (1 + 0j,),
            (0 + 0j,),
        )
        self.assertEqual(function.poles, (1 + 0j,))
        self.assertEqual(function.branch_points, (0 + 0j,))
        self.assertEqual(function.zeros, ())

    def test_polynomial_coefficients_are_in_ascending_powers(self) -> None:
        function = make_complex_function(
            {
                "name": "polynomial",
                "parameters": {"coefficients": [1, 2, 3]},
            }
        )
        self.assertEqual(function.evaluate(2), 17)

    def test_wegert_zero_and_pole_factors(self) -> None:
        function = make_complex_function(
            {
                "name": "rational",
                "parameters": {
                    "gain": [2, 0],
                    "zeros": [[1, 0]],
                    "poles": [[-1, 0]],
                },
            }
        )
        self.assertEqual(function.evaluate(3), 1)
        self.assertEqual(function.zeros, (1 + 0j,))
        self.assertEqual(function.poles, (-1 + 0j,))
        self.assertTrue(function.finite_singularities_complete)

    def test_repeated_factors_retain_multiplicity(self) -> None:
        function = make_complex_function(
            {
                "name": "rational",
                "parameters": {
                    "zeros": [[1, 0], [1, 0]],
                    "poles": [[-1, 0], [-1, 0]],
                },
            }
        )
        self.assertEqual(function.zeros, (1 + 0j, 1 + 0j))
        self.assertEqual(function.poles, (-1 + 0j, -1 + 0j))
        self.assertEqual(function.evaluate(1), 0)
        self.assertEqual(function.evaluate(3), 0.25)

    def test_exact_shared_factors_cancel_one_for_one(self) -> None:
        function = make_complex_function(
            {
                "name": "rational",
                "parameters": {
                    "zeros": [[1, 0], [1.000001, 0]],
                    "poles": [[1, 0], [1, 0]],
                },
            }
        )
        self.assertEqual(function.zeros, (1.000001 + 0j,))
        self.assertEqual(function.poles, (1 + 0j,))
        self.assertEqual(function.label, "1 zero factor; 1 pole factor")
        self.assertEqual(function.evaluate(1.000001), 0)

    def test_fully_cancelled_factor_is_removed_from_evaluator(self) -> None:
        function = make_complex_function(
            {
                "name": "rational",
                "parameters": {
                    "gain": 2,
                    "zeros": [[1, 0]],
                    "poles": [[1, 0]],
                },
            }
        )
        self.assertEqual(function.zeros, ())
        self.assertEqual(function.poles, ())
        self.assertEqual(function.evaluate(1), 2)

    def test_rational_gain_must_be_nonzero(self) -> None:
        with self.assertRaisesRegex(FunctionSpecError, "gain must be nonzero"):
            make_complex_function(
                {
                    "name": "rational",
                    "parameters": {
                        "gain": [0, 0],
                        "zeros": [[1, 0]],
                        "poles": [[-1, 0]],
                    },
                }
            )

    def test_unknown_function_fails_closed(self) -> None:
        with self.assertRaises(FunctionSpecError):
            make_complex_function({"name": "__import__"})


@unittest.skipUnless(importlib.util.find_spec("mpmath"), "mpmath is not installed")
class SpecialFunctionTests(unittest.TestCase):
    def test_zeta_uses_continued_values(self) -> None:
        function = make_complex_function({"name": "zeta"})
        self.assertAlmostEqual(function.evaluate(2).real, math.pi**2 / 6, places=12)
        self.assertAlmostEqual(function.evaluate(-1).real, -1 / 12, places=12)
        self.assertTrue(function.finite_singularities_complete)

    def test_airy_ai_is_available(self) -> None:
        function = make_complex_function({"name": "airy_ai"})
        self.assertAlmostEqual(function.evaluate(0).real, 0.355028053887817, places=12)
        self.assertTrue(function.finite_singularities_complete)

    def test_integer_bessel_j_is_entire(self) -> None:
        function = make_complex_function(
            {"name": "bessel_j", "parameters": {"order": 0}}
        )
        self.assertEqual(function.branch_points, ())
        self.assertTrue(function.finite_singularities_complete)
        self.assertAlmostEqual(function.evaluate(0).real, 1.0, places=12)

    def test_noninteger_bessel_requires_branch_tracking(self) -> None:
        function = make_complex_function(
            {"name": "bessel_j", "parameters": {"order": 0.5}}
        )
        self.assertEqual(function.branch_points, (0j,))
        self.assertFalse(function.finite_singularities_complete)

    def test_gamma_does_not_claim_an_enumerated_pole_list(self) -> None:
        function = make_complex_function({"name": "gamma"})
        self.assertFalse(function.finite_singularities_complete)


if __name__ == "__main__":
    unittest.main()

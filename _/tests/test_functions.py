from __future__ import annotations

import importlib.util
import math
import unittest

from analytic_continuation.functions import FunctionSpecError, make_complex_function


class ElementaryFunctionTests(unittest.TestCase):
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
        self.assertEqual(function.poles, (-1 + 0j,))

    def test_unknown_function_fails_closed(self) -> None:
        with self.assertRaises(FunctionSpecError):
            make_complex_function({"name": "__import__"})


@unittest.skipUnless(importlib.util.find_spec("mpmath"), "mpmath is not installed")
class SpecialFunctionTests(unittest.TestCase):
    def test_zeta_uses_continued_values(self) -> None:
        function = make_complex_function({"name": "zeta"})
        self.assertAlmostEqual(function.evaluate(2).real, math.pi**2 / 6, places=12)
        self.assertAlmostEqual(function.evaluate(-1).real, -1 / 12, places=12)

    def test_airy_ai_is_available(self) -> None:
        function = make_complex_function({"name": "airy_ai"})
        self.assertAlmostEqual(function.evaluate(0).real, 0.355028053887817, places=12)

    def test_integer_bessel_j_is_entire(self) -> None:
        function = make_complex_function(
            {"name": "bessel_j", "parameters": {"order": 0}}
        )
        self.assertEqual(function.branch_points, ())
        self.assertAlmostEqual(function.evaluate(0).real, 1.0, places=12)


if __name__ == "__main__":
    unittest.main()

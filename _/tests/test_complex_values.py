from __future__ import annotations

import unittest

from analytic_continuation.complex_values import ComplexValueError, parse_complex


class ParseComplexTests(unittest.TestCase):
    def test_accepts_pair(self) -> None:
        self.assertEqual(parse_complex([2, -3], "value"), 2 - 3j)

    def test_accepts_object(self) -> None:
        self.assertEqual(
            parse_complex({"real": 2, "imag": -3}, "value"),
            2 - 3j,
        )

    def test_rejects_strings(self) -> None:
        with self.assertRaises(ComplexValueError):
            parse_complex("2-3i", "value")

    def test_rejects_nonfinite_components(self) -> None:
        with self.assertRaises(ComplexValueError):
            parse_complex([float("nan"), 0], "value")

    def test_rejects_nonfinite_scalar(self) -> None:
        for value in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ComplexValueError, "must be finite"):
                    parse_complex(value, "value")

    def test_rejects_scalar_integer_that_overflows_float(self) -> None:
        with self.assertRaisesRegex(ComplexValueError, "must be finite"):
            parse_complex(10**1000, "value")


if __name__ == "__main__":
    unittest.main()

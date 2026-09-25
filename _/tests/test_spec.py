from __future__ import annotations

import unittest

from analytic_continuation.spec import MovieSpecError, parse_movie_spec


class MovieSpecTests(unittest.TestCase):
    def test_defaults_are_small_and_reversible(self) -> None:
        movie = parse_movie_spec({"function": {"name": "exp"}})
        self.assertEqual(movie.view.center, 0j)
        self.assertEqual(movie.view.half_height, 4.0)
        self.assertTrue(movie.animation.close)

    def test_probes_are_input_values(self) -> None:
        movie = parse_movie_spec(
            {
                "function": {"name": "sin"},
                "probes": [[1, 2], [-3, 4]],
            }
        )
        self.assertEqual(movie.probes, (1 + 2j, -3 + 4j))

    def test_rejects_nonpositive_view(self) -> None:
        with self.assertRaises(MovieSpecError):
            parse_movie_spec(
                {
                    "function": {"name": "exp"},
                    "view": {"half_height": 0},
                }
            )

    def test_rejects_nonfinite_timing(self) -> None:
        with self.assertRaises(MovieSpecError):
            parse_movie_spec(
                {
                    "function": {"name": "exp"},
                    "animation": {"open_seconds": float("nan")},
                }
            )

    def test_rejects_unknown_fields(self) -> None:
        with self.assertRaises(MovieSpecError):
            parse_movie_spec(
                {
                    "function": {"name": "exp"},
                    "animaton": {},
                }
            )


if __name__ == "__main__":
    unittest.main()

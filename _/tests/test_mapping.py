from __future__ import annotations

import unittest

from analytic_continuation.mapping import ViewBox, clip_to_view, make_visible_map


class MappingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.view = ViewBox(center=0j, half_width=8, half_height=4, margin=0.9)

    def test_clips_without_changing_direction(self) -> None:
        clipped_real = clip_to_view(20 + 0j, self.view)
        self.assertAlmostEqual(clipped_real.real, 7.2)
        self.assertAlmostEqual(clipped_real.imag, 0.0)
        clipped = clip_to_view(20 + 20j, self.view)
        self.assertAlmostEqual(clipped.real, 3.6)
        self.assertAlmostEqual(clipped.imag, 3.6)

    def test_pole_becomes_a_finite_boundary_point(self) -> None:
        visible = make_visible_map(lambda value: 1 / value, self.view)
        self.assertEqual(visible(0), 7.2 + 0j)


if __name__ == "__main__":
    unittest.main()

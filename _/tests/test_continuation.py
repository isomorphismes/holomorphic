from __future__ import annotations

import math
import unittest

from analytic_continuation.continuation import (
    ContinuationDisc,
    ContinuationPlanError,
    merge_intervals,
    plan_continuation_discs,
    segment_interval_inside_disc,
)
from analytic_continuation.functions import ComplexFunction, make_complex_function


class ContinuationPlanningTests(unittest.TestCase):
    def test_interval_union_removes_overlap_and_containment(self) -> None:
        self.assertEqual(
            merge_intervals(
                [
                    (0.0, 0.6),
                    (0.2, 0.4),
                    (0.5, 0.8),
                    (0.8, 1.0),
                ]
            ),
            ((0.0, 1.0),),
        )

    def test_interval_union_preserves_disjoint_coverage(self) -> None:
        self.assertEqual(
            merge_intervals([(0.7, 0.9), (0.1, 0.2), (0.3, 0.4)]),
            ((0.1, 0.2), (0.3, 0.4), (0.7, 0.9)),
        )

    def test_line_segment_is_clipped_to_open_disc(self) -> None:
        interval = segment_interval_inside_disc(
            -3 + 0j,
            3 + 0j,
            ContinuationDisc(center=0j, radius=2.0),
        )
        self.assertIsNotNone(interval)
        assert interval is not None
        self.assertAlmostEqual(interval[0], 1 / 6)
        self.assertAlmostEqual(interval[1], 5 / 6)

    def test_line_segment_outside_disc_is_omitted(self) -> None:
        interval = segment_interval_inside_disc(
            -3 + 3j,
            3 + 3j,
            ContinuationDisc(center=0j, radius=2.0),
        )
        self.assertIsNone(interval)

    def test_entire_disc_keeps_the_complete_source_line(self) -> None:
        interval = segment_interval_inside_disc(
            -3 + 3j,
            3 + 3j,
            ContinuationDisc(center=0j, radius=math.inf),
        )
        self.assertEqual(interval, (0.0, 1.0))

    def test_radius_is_distance_to_nearest_pole(self) -> None:
        function = make_complex_function(
            {
                "name": "rational",
                "parameters": {"poles": [[0, 0], [10, 0]]},
            }
        )
        discs = plan_continuation_discs(function, [2 + 0j, 2 + 1j])
        self.assertEqual(discs[0].radius, 2.0)
        self.assertAlmostEqual(discs[1].radius, math.sqrt(5))

    def test_radius_uses_canonical_factors_from_wegert_state(self) -> None:
        function = make_complex_function(
            {
                "name": "rational",
                "parameters": {
                    "zeros": [[1, 0], [1.000001, 0]],
                    "poles": [[1, 0], [1, 0]],
                },
            }
        )
        discs = plan_continuation_discs(function, [2 + 0j])
        self.assertEqual(function.zeros, (1.000001 + 0j,))
        self.assertEqual(function.poles, (1 + 0j,))
        self.assertEqual(discs[0].radius, 1.0)

    def test_entire_function_has_no_finite_disc_bound(self) -> None:
        function = make_complex_function({"name": "exp"})
        discs = plan_continuation_discs(function, [0j, 100 + 100j])
        self.assertTrue(math.isinf(discs[0].radius))
        self.assertTrue(math.isinf(discs[1].radius))

    def test_next_center_must_be_strictly_inside_preceding_disc(self) -> None:
        function = make_complex_function(
            {"name": "rational", "parameters": {"poles": [[0, 0]]}}
        )
        with self.assertRaisesRegex(ContinuationPlanError, "strictly inside"):
            plan_continuation_discs(function, [2 + 0j, 4 + 0j])

    def test_center_cannot_be_a_pole(self) -> None:
        function = make_complex_function(
            {"name": "rational", "parameters": {"poles": [[0, 0]]}}
        )
        with self.assertRaisesRegex(ContinuationPlanError, "is the pole"):
            plan_continuation_discs(function, [0j])

    def test_incomplete_finite_singularities_are_rejected(self) -> None:
        function = ComplexFunction(
            name="incomplete",
            label="incomplete meromorphic function",
            evaluate=lambda value: value,
            analytic_status="meromorphic",
        )
        self.assertFalse(function.finite_singularities_complete)
        with self.assertRaisesRegex(
            ContinuationPlanError,
            "singularities are not completely represented",
        ):
            plan_continuation_discs(function, [1 + 0j])

    def test_branch_tracking_requirement_is_rejected(self) -> None:
        function = ComplexFunction(
            name="branched",
            label="branched function",
            evaluate=lambda value: value,
            analytic_status="branched",
            branch_points=(0j,),
        )
        with self.assertRaisesRegex(ContinuationPlanError, "branch tracking"):
            plan_continuation_discs(function, [1 + 0j])

    def test_path_can_loop_around_a_pole_with_overlapping_discs(self) -> None:
        function = make_complex_function(
            {"name": "rational", "parameters": {"poles": [[0, 0]]}}
        )
        path = [
            2 + 0j,
            2 + 1j,
            1 + 2j,
            0 + 2j,
            -1 + 2j,
            -2 + 1j,
            -2 + 0j,
            -2 - 1j,
            -1 - 2j,
            0 - 2j,
            1 - 2j,
            2 - 1j,
            2 + 0j,
        ]
        discs = plan_continuation_discs(function, path)
        self.assertEqual(tuple(disc.center for disc in discs), tuple(path))


if __name__ == "__main__":
    unittest.main()

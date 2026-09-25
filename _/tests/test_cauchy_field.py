from __future__ import annotations

import math
import unittest


SOURCE_COUNT = 24
TAU = 2.0 * math.pi


def fract(value: float) -> float:
    return value - math.floor(value)


def hash1(value: float) -> float:
    return fract(math.sin(value * 127.1 + 31.7) * 43758.5453123)


def source_orbit_speed(index: int) -> float:
    k = float(index)
    return 0.11 + (0.19 - 0.11) * hash1(k + 4.37)


def source_handedness(index: int) -> float:
    k = float(index)
    return -1.0 if hash1(k + 7.91) < 0.5 else 1.0


def source_position(index: int, time: float, view_radius: float) -> complex:
    k = float(index)
    initial_angle = TAU * hash1(k + 0.11)
    angle = initial_angle + source_handedness(index) * source_orbit_speed(index) * time

    bend_phase = TAU * hash1(k + 11.23)
    base = 1.8 * view_radius
    radial = 0.35 * view_radius * math.sin(2.0 * angle + bend_phase)
    radius = base + radial

    ellipticity = -0.08 + 0.16 * hash1(k + 14.67)
    return radius * complex(
        (1.0 + ellipticity) * math.cos(angle),
        (1.0 - ellipticity) * math.sin(angle),
    )


def source_weight(index: int) -> complex:
    k = float(index)
    amplitude = 0.08 + (0.35 - 0.08) * hash1(k + 17.8)
    phase = TAU * hash1(k + 21.4)
    return amplitude * complex(math.cos(phase), math.sin(phase))


def cauchy_field(z: complex, time: float, view_radius: float) -> complex:
    return sum(
        source_weight(index) / (source_position(index, time, view_radius) - z)
        for index in range(SOURCE_COUNT)
    )


class CauchyFieldTests(unittest.TestCase):
    def test_sources_stay_outside_the_visible_disk(self) -> None:
        view_radius = 3.0
        for time in (0.0, 4.0, 17.0, 60.0, 120.0):
            for index in range(SOURCE_COUNT):
                with self.subTest(time=time, index=index):
                    self.assertGreater(
                        abs(source_position(index, time, view_radius)),
                        view_radius,
                    )

    def test_residues_are_fixed_while_sources_move(self) -> None:
        for index in range(SOURCE_COUNT):
            with self.subTest(index=index):
                self.assertEqual(source_weight(index), source_weight(index))
                self.assertNotEqual(
                    source_position(index, 0.0, 3.0),
                    source_position(index, 6.0, 3.0),
                )

    def test_field_motion_is_independent_of_the_divisor(self) -> None:
        # Evaluate q_t directly. There are intentionally no zero/pole inputs,
        # so repeated roots in R cannot manufacture or hide this signal.
        width = 1080.0
        height = 2400.0
        pixel_radius = 0.42 * min(width, height)
        view_radius = math.hypot(0.5 * width, 0.5 * height) / pixel_radius
        sample_points = [
            complex(x * 0.30, y * 0.60)
            for y in range(-3, 4)
            for x in range(-3, 4)
        ]
        sample_points = [point for point in sample_points if abs(point) <= view_radius]

        for start_time in (0.0, 4.0, 17.0, 60.0):
            deltas = [
                abs(
                    cauchy_field(point, start_time + 6.0, view_radius)
                    - cauchy_field(point, start_time, view_radius)
                )
                for point in sample_points
            ]
            mean_delta = sum(deltas) / len(deltas)
            with self.subTest(start_time=start_time):
                self.assertGreater(mean_delta, 0.04)


if __name__ == "__main__":
    unittest.main()

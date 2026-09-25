from __future__ import annotations

import math
import unittest


SOURCE_COUNT = 24
TAU = 2.0 * math.pi


def fract(value: float) -> float:
    return value - math.floor(value)


def field_hash1(value: float) -> float:
    return fract(math.sin(value * 127.1) * 43758.5453123)


def orbit_hash1(value: float) -> float:
    return fract(math.sin(value * 127.1 + 31.7) * 43758.5453123)


def source_radius_scale(index: int) -> float:
    source_index = float(index)
    return 2.75 + (4.00 - 2.75) * orbit_hash1(source_index + 1.73)


def source_orbit_speed(index: int) -> float:
    source_index = float(index)
    return 0.11 + (0.19 - 0.11) * orbit_hash1(source_index + 4.37)


def source_handedness(index: int) -> float:
    source_index = float(index)
    return -1.0 if orbit_hash1(source_index + 7.91) < 0.5 else 1.0


def source_position(index: int, time: float, view_radius: float) -> complex:
    source_index = float(index)
    initial_angle = TAU * orbit_hash1(source_index + 0.11)
    angle = (
        initial_angle
        + source_handedness(index) * source_orbit_speed(index) * time
    )

    bend_phase = TAU * orbit_hash1(source_index + 11.23)
    radial_bend = 0.22 * math.sin(2.0 * angle + bend_phase)
    radius = source_radius_scale(index) + radial_bend

    ellipticity = -0.08 + 0.16 * orbit_hash1(source_index + 14.67)
    orbit = complex(
        (1.0 + ellipticity) * math.cos(angle),
        (1.0 - ellipticity) * math.sin(angle),
    )
    return view_radius * radius * orbit


def source_weight(index: int, time: float) -> complex:
    source_index = float(index)
    amplitude = 0.08 + (0.35 - 0.08) * field_hash1(source_index + 11.3)
    omega = 0.08 + (0.60 - 0.08) * field_hash1(source_index + 17.8)
    phi = TAU * field_hash1(source_index + 21.4) + omega * time
    return amplitude * complex(math.cos(phi), math.sin(phi))


def cauchy_field(z: complex, time: float, view_radius: float) -> complex:
    # Wandering orbits are the xi_k(t) motion.  The Cauchy field is
    # q_t(z) = sum_k a_k(t) / (xi_k(t) - z).
    return sum(
        source_weight(index, time) / (source_position(index, time, view_radius) - z)
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

    def test_sources_make_large_orbits_not_local_wiggles(self) -> None:
        view_radius = 3.0
        for index in range(SOURCE_COUNT):
            half_orbit_time = math.pi / source_orbit_speed(index)
            start = source_position(index, 0.0, view_radius)
            opposite = source_position(index, half_orbit_time, view_radius)
            with self.subTest(index=index):
                self.assertGreater(abs(opposite - start), 3.0 * view_radius)

    def test_field_motion_is_independent_of_the_divisor(self) -> None:
        # This test evaluates q_t directly. It intentionally has no zero/pole
        # inputs, so repeated roots in R cannot make genuine Cauchy-field motion
        # disappear from the acceptance signal.
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

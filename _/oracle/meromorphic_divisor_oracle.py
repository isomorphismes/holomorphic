from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Sequence


Position = tuple[float, float]


@dataclass(frozen=True)
class PhaseLog:
    """A nonzero complex value represented by phase and log modulus."""

    phase: float
    log_modulus: float


class SingularSample(ValueError):
    """The reduced divisor has an actual zero or pole at the sample point."""


def cancel_exact_factors(
    zeros: Sequence[Position],
    poles: Sequence[Position],
) -> tuple[list[Position], list[Position]]:
    """Cancel equal zero/pole factors one-for-one before numeric evaluation.

    This is an algebraic divisor reduction. In particular, an exact pair at
    ``a`` represents (z-a)/(z-a) = 1 after removal of the removable
    singularity; it must not be evaluated as 0/0 first.
    """

    pole_used = [False] * len(poles)
    remaining_zeros: list[Position] = []

    for zero in zeros:
        match = None
        for index, pole in enumerate(poles):
            if not pole_used[index] and zero == pole:
                match = index
                break
        if match is None:
            remaining_zeros.append(zero)
        else:
            pole_used[match] = True

    remaining_poles = [
        pole for index, pole in enumerate(poles) if not pole_used[index]
    ]
    return remaining_zeros, remaining_poles


def evaluate_phase_log(
    z: Position,
    q: Position,
    zeros: Sequence[Position] = (),
    poles: Sequence[Position] = (),
) -> PhaseLog:
    """Evaluate exp(q) times the reduced meromorphic divisor.

    ``q`` is supplied as ``(Re(q), Im(q))``. Exact cancellations are removed
    first, then the remaining divisor is evaluated in log-polar form. The
    result avoids forming huge products or exp(q).
    """

    zeros, poles = cancel_exact_factors(zeros, poles)
    zero_phases: list[float] = []
    zero_logs: list[float] = []
    pole_phases: list[float] = []
    pole_logs: list[float] = []

    for position in zeros:
        dx = z[0] - position[0]
        dy = z[1] - position[1]
        radius = math.hypot(dx, dy)
        if radius == 0.0:
            raise SingularSample("sample lies on an uncancelled zero")
        zero_phases.append(math.atan2(dy, dx))
        zero_logs.append(math.log(radius))

    for position in poles:
        dx = z[0] - position[0]
        dy = z[1] - position[1]
        radius = math.hypot(dx, dy)
        if radius == 0.0:
            raise SingularSample("sample lies on an uncancelled pole")
        pole_phases.append(math.atan2(dy, dx))
        pole_logs.append(math.log(radius))

    return PhaseLog(
        phase=q[1] + math.fsum(zero_phases) - math.fsum(pole_phases),
        log_modulus=q[0] + math.fsum(zero_logs) - math.fsum(pole_logs),
    )


def phase_distance(left: float, right: float) -> float:
    """Smallest signed difference between two phases."""

    return math.remainder(left - right, math.tau)


def same_field(
    left: PhaseLog,
    right: PhaseLog,
    *,
    phase_tolerance: float = 1.0e-12,
    log_tolerance: float = 1.0e-12,
) -> bool:
    return (
        abs(phase_distance(left.phase, right.phase)) <= phase_tolerance
        and abs(left.log_modulus - right.log_modulus) <= log_tolerance
    )


def wegert_coordinates(value: PhaseLog) -> PhaseLog:
    """Coordinates seen by the canonical Wegert color map.

    The color map is periodic in phase by 2*pi and in log modulus by log(10).
    Reducing these large terms before adding a small divisor contribution is
    therefore a semantics-preserving numerical operation for this renderer.
    """

    return PhaseLog(
        phase=value.phase % math.tau,
        log_modulus=value.log_modulus % math.log(10.0),
    )


def deterministic_positions(count: int) -> list[Position]:
    """Small exact coordinates for repeatable many-factor cases."""

    if count < 0:
        raise ValueError("count must be nonnegative")
    return [
        (
            0.25 * float((index % 6) - 2),
            0.20 * float((index // 6) - 2),
        )
        for index in range(count)
    ]

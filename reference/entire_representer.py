"""Host oracle for candidate whole-plane canonical perturbations.

This module is deliberately independent of Android, GLSL, and Idric backend
types.  It fixes one Bargmann--Fock convention so that proposed descriptors can
be checked before a backend optimizes them.  It is candidate mathematics, not
the live motion policy and not a choice of visual parameters.

The norm convention is

    ||h||_s^2 = 1/(pi s^2) integral_C |h(z)|^2 exp(-|z|^2/s^2) dA(z).

All state evaluation is at regular points of the explicit meromorphic factor.
"""

from __future__ import annotations

from dataclasses import dataclass
import cmath
import math
from typing import Iterable, Literal


Gauge = Literal["none", "zero_at_origin"]
DatumKind = Literal["value", "derivative"]


def _scale_squared(scale: float) -> float:
    if not math.isfinite(scale) or scale <= 0.0:
        raise ValueError("the Bargmann--Fock scale must be finite and positive")
    return scale * scale


def fock_kernel(z: complex, anchor: complex, scale: float) -> complex:
    """Return K_s(z,a) = exp(z conjugate(a) / s^2)."""

    scale_squared = _scale_squared(scale)
    return cmath.exp(z * anchor.conjugate() / scale_squared)


def fock_value_direction(
    z: complex,
    anchor: complex,
    scale: float,
    *,
    gauge: Gauge,
) -> complex:
    """Return the minimum-norm direction whose value at ``anchor`` is one.

    ``gauge="none"`` uses the full Fock space.  ``zero_at_origin`` uses the
    closed subspace h(0)=0 and therefore rejects a value constraint at zero.
    """

    scale_squared = _scale_squared(scale)
    anchor_squared = abs(anchor) ** 2

    if gauge == "none":
        return cmath.exp(
            (z * anchor.conjugate() - anchor_squared) / scale_squared
        )
    if gauge == "zero_at_origin":
        if anchor == 0.0:
            raise ValueError("h(0)=1 is incompatible with the h(0)=0 gauge")
        numerator = cmath.exp(z * anchor.conjugate() / scale_squared) - 1.0
        denominator = math.expm1(anchor_squared / scale_squared)
        return numerator / denominator
    raise ValueError(f"unknown gauge: {gauge}")


def fock_derivative_direction(
    z: complex,
    anchor: complex,
    scale: float,
) -> complex:
    """Return the minimum-norm direction whose derivative at ``anchor`` is one.

    The derivative representer is

        r_a(z) = (z/s^2) exp(z conjugate(a)/s^2).

    It already vanishes at the origin, so projection to the h(0)=0 subspace
    does not change it.
    """

    scale_squared = _scale_squared(scale)
    exponent = abs(anchor) ** 2 / scale_squared
    representer = (z / scale_squared) * cmath.exp(
        z * anchor.conjugate() / scale_squared
    )
    representer_derivative_at_anchor = (
        (scale_squared + abs(anchor) ** 2)
        / (scale_squared * scale_squared)
        * math.exp(exponent)
    )
    return representer / representer_derivative_at_anchor


def minimum_norm_squared(
    *,
    anchor: complex,
    scale: float,
    kind: DatumKind,
    gauge: Gauge,
) -> float:
    """Return the squared norm of the corresponding normalized representer."""

    scale_squared = _scale_squared(scale)
    exponent = abs(anchor) ** 2 / scale_squared

    if kind == "value":
        if gauge == "none":
            return math.exp(-exponent)
        if gauge == "zero_at_origin":
            if anchor == 0.0:
                raise ValueError("h(0)=1 is incompatible with the h(0)=0 gauge")
            return 1.0 / math.expm1(exponent)
        raise ValueError(f"unknown gauge: {gauge}")

    if kind == "derivative":
        if gauge not in ("none", "zero_at_origin"):
            raise ValueError(f"unknown gauge: {gauge}")
        return (
            scale_squared
            * scale_squared
            * math.exp(-exponent)
            / (scale_squared + abs(anchor) ** 2)
        )

    raise ValueError(f"unknown local datum kind: {kind}")


@dataclass(frozen=True)
class LocalDatum:
    """One prescribed local value or derivative contribution to q."""

    anchor: complex
    amplitude: complex
    scale: float
    kind: DatumKind
    gauge: Gauge

    def direction(self, z: complex) -> complex:
        if self.kind == "value":
            return fock_value_direction(
                z, self.anchor, self.scale, gauge=self.gauge
            )
        if self.kind == "derivative":
            if self.gauge not in ("none", "zero_at_origin"):
                raise ValueError(f"unknown gauge: {self.gauge}")
            return fock_derivative_direction(z, self.anchor, self.scale)
        raise ValueError(f"unknown local datum kind: {self.kind}")

    def contribution(self, z: complex) -> complex:
        return self.amplitude * self.direction(z)


def evaluate_q(z: complex, data: Iterable[LocalDatum]) -> complex:
    """Evaluate a finite superposition of admitted entire descriptors."""

    return sum((datum.contribution(z) for datum in data), start=0j)


@dataclass(frozen=True)
class DivisorPoint:
    location: complex
    multiplicity: int = 1

    def __post_init__(self) -> None:
        if not isinstance(self.multiplicity, int) or isinstance(
            self.multiplicity, bool
        ) or self.multiplicity <= 0:
            raise ValueError("divisor multiplicity must be a positive integer")


def evaluate_r(
    z: complex,
    *,
    zeros: Iterable[DivisorPoint] = (),
    poles: Iterable[DivisorPoint] = (),
    gain: complex = 1.0,
) -> complex:
    """Evaluate the explicitly factored meromorphic part R at a regular point."""

    if gain == 0.0:
        raise ValueError("the meromorphic factor gain must be nonzero")
    value = complex(gain)
    for zero in zeros:
        value *= (z - zero.location) ** zero.multiplicity
    for pole in poles:
        value /= (z - pole.location) ** pole.multiplicity
    return value


@dataclass(frozen=True)
class RegularPointState:
    q: complex
    re_q: float
    im_q: float
    r: complex
    f: complex
    log_modulus: float
    phase: float


def evaluate_regular_state(
    z: complex,
    data: Iterable[LocalDatum],
    *,
    zeros: Iterable[DivisorPoint] = (),
    poles: Iterable[DivisorPoint] = (),
    gain: complex = 1.0,
) -> RegularPointState:
    """Evaluate q, R exp(q), and the exact phase/log-modulus handoff.

    ``phase`` is an unwrapped representative.  Rendering consumes it modulo
    2*pi.  A point on the explicit divisor is rejected because phase and finite
    log modulus are not defined there.
    """

    q = evaluate_q(z, data)
    r = evaluate_r(z, zeros=zeros, poles=poles, gain=gain)
    if r == 0.0:
        raise ValueError("phase/log-modulus evaluation requires a regular point")

    f = r * cmath.exp(q)
    return RegularPointState(
        q=q,
        re_q=q.real,
        im_q=q.imag,
        r=r,
        f=f,
        log_modulus=math.log(abs(r)) + q.real,
        phase=cmath.phase(r) + q.imag,
    )

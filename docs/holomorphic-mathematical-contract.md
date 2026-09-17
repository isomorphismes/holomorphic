# Holomorphic explorer mathematical contract

This document fixes the mathematics of the live explorer independently of CPU threads, GPU backends, shader languages, and performance experiments.

The renderer may approximate these formulas numerically. It must not redefine the mathematical object in order to fit a particular implementation.

## Status labels used by this contract

To keep theorem, prototype, and taste from drifting together, statements in this repository have these roles:

- **MATHEMATICAL CONTRACT** — invariants every implementation must preserve, including `f = R exp(q)`, entire `q`, divisor preservation, and the phase/log-modulus handoff.
- **CURRENT REFERENCE IMPLEMENTATION** — the small polynomial `q`, coefficient envelope, and host/shader evaluators used today.
- **HISTORICAL EXPERIMENT** — the unit-disc Bergman workers and the old three-workers-plus-coordinator organization.
- **CANDIDATE FUTURE MATHEMATICS** — the Bargmann-Fock coordinate system below, pending host comparison and visual judgment.
- **BACKEND OPTIMIZATION IDEA** — scheduling, precision, descriptor packing, and CPU/GPU execution strategies; these do not define admissible functions.
- **VISUAL / ART-DIRECTION CHOICE** — scale, anchor distribution, amplitude, phase/sign, lifetime, temporal correlation, overlap, active count, and cadence.

## 1. The live object

The explorer displays

```text
f_t(z) = R(z) H_t(z)
```

on the ordinary complex plane.

`R` carries the explicit finite meromorphic divisor chosen by the user:

```text
R(z) = gain * product_i (z - a_i)^(m_i)
              / product_j (z - b_j)^(n_j)
```

where the `a_i` are zeros, the `b_j` are poles, and multiplicity is explicit.

The live freedom is a nonvanishing entire factor

```text
H_t(z) = exp(q_t(z))
```

with `q_t` entire.

Therefore every displayed state is meromorphic on the ordinary complex plane and has exactly the finite zeros and poles supplied by `R`. The holomorphic motion cannot create, remove, or move that divisor.

The current implementation uses a small polynomial `q_t`. That is a finite computational coordinate system, not a claim that all entire functions are finite polynomials.

## 2. Why `exp(q)` is structural rather than cosmetic

If two nonzero meromorphic functions on `C` have the same finite zeros and poles with the same multiplicities, their quotient extends across the cancelled divisor to an entire, nonvanishing function. Every nonvanishing entire function `H` has an entire logarithm: `H'/H` is entire, its integral on the simply connected plane is an entire function `g`, and `H exp(-g)` is a nonzero constant whose logarithm can be absorbed into `g`. Thus `H = exp(q)` for an entire `q`.

The simply connected hypothesis matters. A nonvanishing holomorphic function on an arbitrary multiply connected domain need not have a single-valued holomorphic logarithm. This contract makes the claim on `C`, not on every domain.

So

```text
explicit divisor * exp(entire freedom)
```

is the natural whole-plane model for this explorer.

## 3. Whole-plane requirement

For this repository, `q` must be entire, not merely holomorphic on the current viewport.

A basis function with a pole outside the screen is still not an admissible whole-plane perturbation. Exponentiating such a function generally turns that pole into an essential singularity, so the result is no longer the intended meromorphic plane with only the explicit poles of `R`.

Local-domain, chart, lasso, and bounded-disc constructions belong in `isomorphismes/lacunary`.

## 4. Gauge: do not mistake a global recoloring for mathematical motion

Adding a complex constant `c` to `q` multiplies the entire portrait by `exp(c)`:

```text
Re(c)  -> global modulus scale
Im(c)  -> global phase rotation
```

Those degrees of freedom can be useful controls, but they should not masquerade as interesting local holomorphic motion.

The present polynomial prototype omits the constant term, equivalently fixing `q(0) = 0`. A future basis may use another explicit gauge, but the gauge must be stated.

## 5. Exact quantities supplied to Wegert

There is no mathematical reason for the renderer to evaluate the complex exponential explicitly.

Away from the explicit zeros and poles, for

```text
f(z) = R(z) exp(q(z))
```

we have exactly

```text
log|f(z)| = log|R(z)| + Re(q(z))
phase(f(z)) = phase(R(z)) + Im(q(z))  (mod 2 pi)
```

The mathematical renderer boundary is therefore

```text
explicit zero/pole contribution
+ Re(q), Im(q)
-> phase, log modulus
-> canonical Wegert value/color behavior
-> interaction overlays
```

RGB differences and screen derivatives are not substitutes for complex derivatives or holomorphy.

## 6. Local infinitesimal motion

For an infinitesimal change `delta q`,

```text
delta log|H(z)| = Re(delta q(z))
delta phase(H(z)) = Im(delta q(z))
```

These are exact analytic sensitivities.

A local derivative may also be used when the desired perturbation is stated in terms of local slope rather than local value. Which local functional is prescribed is part of the mathematical question; it must not be inferred from GPU convenience.

## 7. Canonical least-disturbing direction

A canonical direction does not require a visual-energy heuristic.

Choose a Hilbert space `A` of admissible holomorphic perturbations with a stated norm, gauge, and continuous point evaluation. Use the convention `h(a) = <h, K(.,a)>`, with the inner product linear in its first argument. If we require a perturbation `phi` to satisfy

```text
phi(a) = 1
```

then the unique minimum-norm solution is

```text
phi_a(z) = K(z,a) / K(a,a).
```

This is the precise meaning of a canonical direction of least holomorphic disturbance for a prescribed local value change.

Indeed, `||K(.,a)||^2 = K(a,a)`. Every admissible `h` with `h(a) = 1` decomposes as

```text
h = K(.,a) / K(a,a) + g,
g(a) = 0.
```

The reproducing identity makes `g` orthogonal to `K(.,a)`, so

```text
||h||^2 = 1 / K(a,a) + ||g||^2.
```

The normalized representer is therefore the unique minimum-norm solution. “Canonical” means canonical relative to the declared Hilbert space, norm, local functional, and gauge; it does not mean independent of those modeling choices.

More generally, if the prescribed local datum is a derivative or another continuous linear functional, its Riesz/reproducing representer gives the corresponding unique minimum-norm direction after normalization.

Thus the mathematical pipeline can be

```text
choose anchor/local datum
-> compute its canonical minimum-norm holomorphic representer
-> choose a small amplitude/sign/time law
-> add that direction to q
-> render the resulting exact holomorphic state
```

The user remains the visual arbiter of anchor selection, amplitude, timing, overlap, persistence, and whether the resulting motion looks good.

## 8. Historical Bergman-disk construction

The historical `local-holomorphic-perturbations` experiment used the unit-disc Bergman space

```text
A^2(D) = {h holomorphic on D : (1/pi) integral_D |h(z)|^2 dA(z) < infinity}.
```

For this normalized area convention its kernel is

```text
K_D(z,a) = 1 / (1 - conjugate(a) z)^2,
```

and its value-normalized extremal is

```text
phi_a(z) = (1 - |a|^2)^2 / (1 - conjugate(a) z)^2
```

which satisfies `phi_a(a) = 1` and is the minimum Bergman-norm holomorphic function on the disc with that value. Using unnormalized area instead multiplies `K_D` by `1/pi` and leaves `K_D(z,a)/K_D(a,a)` unchanged.

That correctly demonstrated the canonical-direction idea on a bounded disc. It is not, by itself, the whole-plane basis for this repository: for `a != 0` it has a pole at `1 / conjugate(a)` outside the unit disc, and `exp(phi_a)` would have an essential singularity there.

Do not revive that hidden singularity merely because the kernel was useful in the old local experiment.

## 9. Whole-plane reproducing-kernel direction

If we want the same extremal construction using entire functions, the admissible Hilbert space must itself consist of entire functions.

A natural candidate is the Bargmann-Fock space, for `s > 0`, with the explicit convention

```text
F_s^2 = {h entire : ||h||_s^2 < infinity},
||h||_s^2 = (1 / (pi s^2)) integral_C |h(z)|^2 exp(-|z|^2 / s^2) dA(z).
```

The functions `z^n / (s^n sqrt(n!))` form an orthonormal basis for this convention, so the reproducing kernel is exactly

```text
K_s(z,a) = exp(z conjugate(a) / s^2),
K_s(a,a) = exp(|a|^2 / s^2).
```

The value-normalized extremal is therefore

```text
phi_a(z) = exp((z conjugate(a) - |a|^2) / s^2)
```

which is entire and satisfies `phi_a(a) = 1`.

Its squared norm is `exp(-|a|^2 / s^2)`. This is minimum disturbance in the stated Gaussian-weighted Fock norm. It is not a proof of visual localization in an unweighted viewport: its modulus can grow in the direction selected by the anchor.

This is a mathematically clean whole-plane candidate for canonical local perturbations. The choice of function-space norm and the scale `s` are modeling choices, not universal aesthetic truths. They should be exposed to visual evaluation rather than smuggled in as GPU constants.

The displayed formula is **ungauged**. In particular, `phi_a(0) = exp(-|a|^2 / s^2)`, so it does not belong to the present prototype's `q(0) = 0` gauge. If that gauge is retained, use the closed subspace

```text
F_{s,0}^2 = {h in F_s^2 : h(0) = 0}
```

whose kernel is

```text
K_{s,0}(z,a) = exp(z conjugate(a) / s^2) - 1.
```

For a nonzero anchor, its normalized value representer is

```text
phi_{a,0}(z)
    = (exp(z conjugate(a) / s^2) - 1)
      / (exp(|a|^2 / s^2) - 1).
```

It satisfies both `phi_{a,0}(0) = 0` and `phi_{a,0}(a) = 1`. A value constraint `h(0) = 1` is incompatible with this gauge. An origin derivative constraint is compatible; the minimum-norm direction satisfying `h'(0) = 1` is `h(z) = z`.

The exact whole-plane perturbation space is therefore a mathematical/design decision to settle before optimizing a GPU implementation. The invariant that its elements are entire is not optional.

## 10. Superposition

Every finite sum of admitted entire perturbation descriptors is entire. Therefore several value or derivative disturbances may overlap in `q` before exponentiation without creating finite singularities or changing the explicit divisor.

For an infinite series, termwise entireness alone is insufficient: locally uniform convergence on `C` is a sufficient condition for the sum to remain entire. The current implementation and the candidate host reference use finite superpositions.

## 11. What randomness means

Randomness may choose:

- an anchor point;
- whether the local datum is phase/value/derivative oriented;
- sign or phase of the infinitesimal change;
- amplitude within an accepted bound;
- lifetime and temporal overlap with other perturbations.

Randomness does not certify holomorphy. The admitted function space and exact formulas do that.

The number of CPU workers is not mathematics. Three workers plus a coordinator was a useful implementation shape for a four-thread phone, but another CPU, x86-64 implementation, or GPU backend may schedule the same mathematical descriptors differently.

## 12. Current polynomial prototype

The current implementation uses

```text
q(u) = c1 u + c2 u^2 + ... + c5 u^5
u = z / 6
```

and a coefficient envelope

```text
sum_k |c_k| <= 0.72.
```

Because the basis is polynomial, every state is entire regardless of this coefficient bound. The bound is therefore not a holomorphy test.

On `|u| <= 1`, the triangle inequality gives

```text
|q(u)| <= sum_k |c_k| <= 0.72.
```

So the bound is usefully interpreted as an amplitude/numerical envelope on the reference disc. Any stronger meaning must be proved separately.

The present 128-candidate, three-worker score is a provisional CPU exploration strategy. It is not the definition of the canonical holomorphic direction and must not become part of the mathematical semantics merely because it exists in working code.

## 13. Safe motion descriptors

If an implementation publishes a segment

```text
c(tau) = c0 + tau d,
0 <= tau <= tau_max,
```

then every intermediate `q_tau` remains entire automatically when the basis functions are entire.

If the accepted coefficient set is convex, such as the current `sum |c_k| <= B` ball, endpoints inside the set imply the whole line segment stays inside that particular bound.

Other numerical, amplitude, derivative, or application-specific bounds must be named separately. Do not call them holomorphy checks.

## 14. Separation of responsibilities

The mathematical evolution engine owns:

- the admissible entire perturbation space;
- canonical local representers/directions;
- random selection of local data when desired;
- amplitude and safe-extent rules;
- the resulting `q_t` or compact descriptor.

The GPU/backend owns:

- numerically evaluating an already-defined mathematical descriptor efficiently;
- producing `Re(q)` and `Im(q)` or equivalent exact quantities;
- preserving stated error bounds.

Wegert owns the reusable rendering preference boundary from complex value / phase / log modulus to the canonical phase portrait.

The user owns the final visual judgment about what motion is worth keeping.

## 15. Acceptance before GPU optimization

Before backend-specific optimization, host/reference tests should establish at least:

1. the chosen basis/representers are entire;
2. the canonical representer satisfies its prescribed local value or derivative condition;
3. the claimed minimum-norm property matches the chosen Hilbert space;
4. `exp(q)` never changes the explicit zero/pole divisor;
5. phase/log-modulus updates agree with `Re(q)`/`Im(q)`;
6. any coefficient/amplitude bound is described by the theorem it actually satisfies;
7. safe segments remain inside every claimed convex bound;
8. the current CPU heuristic is clearly labeled as an approximation/scheduling strategy rather than the mathematical definition.

Only after those are stable should PowerVR, FP16/FP32, fragment/compute division, register pressure, or other backend details be allowed to influence implementation choices.

The dependency-free host oracle in [`reference/entire_representer.py`](../reference/entire_representer.py) evaluates the two Fock value conventions, derivative representers, finite descriptor sums, and the complete regular-point state `R(z) exp(q(z))`. It is a **CANDIDATE FUTURE MATHEMATICS** oracle, not Android runtime code and not approval of a visual parameter set.

## 16. Mathematical references

- N. Aronszajn, “Theory of Reproducing Kernels,” *Transactions of the American Mathematical Society* 68 (1950), 337–404, [doi:10.1090/S0002-9947-1950-0051437-7](https://doi.org/10.1090/S0002-9947-1950-0051437-7).
- V. Bargmann, “On a Hilbert Space of Analytic Functions and an Associated Integral Transform,” *Communications on Pure and Applied Mathematics* 14 (1961), 187–214, [doi:10.1002/cpa.3160140303](https://doi.org/10.1002/cpa.3160140303).

The repository named `isomorphismes/Conway` is a planar-symmetry/orbifold project, not a source for these complex-analysis results; no Conway chapter citation is asserted here.

# Analytic Continuation

## Repository layout

The source-facing root exposes the maintained code directly. Build, packaging, generated, test, release, and Android project machinery is canonical under [`_/`](_/). The old directory names remain only as compatibility symlinks. GitHub Actions stays under `.github/workflows/` because GitHub requires that location.

See [`LAYOUT.md`](LAYOUT.md) for the rule used across branches.

Native Android explorer for a meromorphic complex function whose holomorphic freedom stays alive.

The picture is the ordinary Wegert-style complex plane with explicit zeros and poles, multiplied by a continuously varying holomorphic/nonvanishing factor:

```text
f_t(z) = R(z) H_t(z)
```

`R` carries the visible meromorphic divisor. `H_t` moves through legitimate holomorphic states without introducing accidental zeros or poles. The current experimental family is

```text
H_t(z) = exp(q_t(z))
```

with a small polynomial `q_t`; that convenient family is not intended as a complete parameterization of holomorphic functions. The motion changes the mathematical function, not merely the hue.

The backend-independent mathematics is specified in [`docs/holomorphic-mathematical-contract.md`](docs/holomorphic-mathematical-contract.md). In particular, whole-plane perturbations in this repository must be entire; bounded-disc kernels with hidden exterior singularities are historical/local constructions rather than the live whole-plane semantics.

## Direction search

Randomness may choose local data and motion parameters, but it does not certify holomorphy. The mathematical contract defines canonical least-disturbing directions through normalized reproducing/Riesz representers once the admissible entire function space and the prescribed local value or derivative are fixed.

The current CPU prototype still uses three small search workers and 128 nearby/random coefficient candidates as a provisional computational strategy. That heuristic is not the mathematical definition of the canonical direction. Search snapshots run more slowly than display frames; the fragment shader evaluates the accepted coefficients over the whole visible field.

For the current `exp(q)` family,

```text
delta log|H(z)| = Re(delta q(z))
delta phase(H(z)) = Im(delta q(z))
```

so exact phase/log-modulus sensitivities are available without treating RGB or screen-space differences as mathematics.

## Wegert boundary

[Wegert](https://github.com/isomorphismes/wegert) owns reusable phase-portrait behavior and rendering preferences, including the canonical complex-value to Wegert-color mapping and ordinary zero/pole interaction pieces.

This repository consumes Wegert's exported coloring core and checks it byte-for-byte against Wegert in CI. Its own responsibility is the evolving holomorphic factor, mathematical evolution, GPU evaluation, and thin Android integration.

The inherited lasso/domain-warp engine has been removed from the live source. The remaining integration cleanup is to replace the app-local ordinary zero/pole interaction code with reusable Wegert components without changing the meromorphic playground itself; see issue #25 and [`docs/cleanup.md`](docs/cleanup.md).

## Lacunary boundary

[Lacunary](https://github.com/isomorphismes/lacunary) owns the experiments that change the domain/chart/continuation problem rather than simply changing the holomorphic factor on the ordinary meromorphic plane:

- lasso and deformed-domain constructions;
- overlapping convergence discs and reveal geometry;
- path-dependent germ transport;
- branches, sheets, monodromy, and broader Riemann-surface experiments.

Reusable historical mathematics from those experiments has been archived there. Git history here still records the old branches, but none of that machinery is part of the shipping explorer.

## Runtime

The canonical Android project is under `_/android/`; the top-level `android` entry is only a compatibility symlink. It uses a C `NativeActivity`, EGL, and OpenGL ES 3. No Python runtime or desktop movie renderer owns the live interaction.

Current checks cover the holomorphic direction search, absence of migrated lasso/disc machinery, and the Wegert color boundary. Android emulator evidence and target-phone GPU evidence remain separate.

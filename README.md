# Analytic continuation

## Repository layout

The source-facing root exposes maintained code directly. Build, packaging, generated, test, release, and Android project machinery is canonical under [`_/`](_/). Old directory names remain only as compatibility symlinks. GitHub Actions stays under `.github/workflows/` because GitHub requires that location.

See [`LAYOUT.md`](LAYOUT.md).

Make a short movie in which a complex grid opens from the input plane into the
map `z → f(z)`, pauses, and closes again.

The renderer uses [ManimGL](https://github.com/3b1b/manim), the version of
Manim maintained by 3Blue1Brown.  The 2016 zeta video source is
[`3b1b/videos/_2016/zeta.py`](https://github.com/3b1b/videos/blob/master/_2016/zeta.py).
That source calls `mpmath.zeta` to obtain already-continued values and then
warps a sampled grid.  Its movie interpolation is not itself an algorithm for
analytic continuation.  This project keeps the same distinction explicit.

## First slice

- choose a named complex function in JSON
- set the visible complex domain
- optionally mark a few input points as yellow probes
- render the grid and probes opening under `f` and closing back to their inputs
- clip poles and enormous outputs at the movie boundary so Manim always gets
  finite drawing coordinates

The registry currently contains:

| input name | function | status in the complex variable |
| --- | --- | --- |
| `exp` | exponential | entire |
| `sin` | sine | entire |
| `polynomial` | coefficients in ascending powers | entire |
| `rational` | gain, zeros, and poles | meromorphic |
| `zeta` | Riemann zeta | meromorphic continuation; one pole at `1` |
| `gamma` | gamma | meromorphic |
| `airy_ai`, `airy_bi` | Airy functions | entire |
| `bessel_j` | Bessel J with an order parameter | entire for integer order; branched at `0` otherwise |

There is no Python `eval` input.  A new function gets an explicit registry
entry and parameter contract.

## Run it

Python 3.10 or later is required.

On Ubuntu or Debian, ManimGL also requires FFmpeg and the Pango development
headers:

```sh
sudo apt install ffmpeg libpango1.0-dev
```

```sh
python -m venv .venv
. .venv/bin/activate
python -m pip install -e '.[movie]'

analytic-continuation list-functions
analytic-continuation validate examples/zeta.json
analytic-continuation render examples/zeta.json
```

Use `--preview` on the render command for ManimGL's interactive window.  The
examples include zeta, Airy Ai, Bessel J order zero, and the current Wegert
factorization `(z - 1)(z - 2)(z - 5)`.

Run the small non-rendering test suite with:

```sh
python -m unittest discover -s tests -v
```

## Movie JSON

Complex values are JSON numbers, `[real, imag]` pairs, or objects with `real`
and `imag` fields.  They are never expression strings.

```json
{
  "schema": "analytic-continuation/movie-v1",
  "title": "Bessel J₀(z)",
  "function": {
    "name": "bessel_j",
    "parameters": {"order": 0}
  },
  "view": {
    "center": [0, 0],
    "half_height": 6,
    "grid_step": 1
  },
  "probes": [[0, 0], [2.4048255577, 0]],
  "animation": {
    "open_seconds": 5,
    "hold_seconds": 1,
    "close_seconds": 5,
    "close": true,
    "curve_density": 80,
    "output_margin": 0.94
  }
}
```

## Wegert boundary

Wegert already has the right input for rational maps.  Its zeros and poles,
together with one complex gain, determine

```text
f(z) = gain × product(z - zero) / product(z - pole).
```

`examples/wegert_rational.json` is the proposed export shape.  The Wegert
camera maps directly to `view.center` and `view.half_height`.  Tapped portrait
locations can be exported as `probes`; their output values are determined by
the selected function.

A few freely chosen input/output pixel pairs do **not** determine an analytic
function.  Infinitely many polynomials, and still more analytic functions,
can pass through any finite set.  If the user is constructing a function,
the UI must also choose a model such as a rational degree, a differential
equation with initial data, or a Taylor germ.

## Later: continuation rather than only deformation

A genuine continuation movie needs different data:

1. a base point and a Taylor germ (or an equation that determines one),
2. a path through the complex plane,
3. overlapping convergence discs avoiding singularities,
4. branch tracking when different paths produce different values.

That mode can reveal the continued Wegert portrait one disc at a time.  It is
especially useful for `log`, `sqrt`, non-integer Bessel functions, and other
multivalued functions.  Airy Ai/Bi and integer-order Bessel J are already
entire, so their whole-plane movies are complex-map deformations, not dramatic
examples of continuation across a barrier.

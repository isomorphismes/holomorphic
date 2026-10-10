# Generated Bargmann–Fock value-direction probe

This opt-in experiment follows the whole-plane mathematical contract in
[holomorphic-mathematical-contract.md](holomorphic-mathematical-contract.md).
The application continues to run its five-term polynomial until the generated
polynomial fragment has passed app and physical-device acceptance.

## Mathematical input

For scale `s > 0` and nonzero anchor `a`, the constrained minimum-norm value
direction in Bargmann–Fock space with `q(0) = 0` is

```text
phi(z,a,s) = (exp(z conj(a) / s²) - 1) / (exp(|a|² / s²) - 1).
q(z) = amplitude * phi(z,a,s).
```

This is an **entire** function of `z`; unlike the historical Bergman-disc
construction it has no off-screen pole. `phi(0)=0` and `phi(a)=1`.
Both properties are already tested in the independent host oracle
`reference/entire_representer.py`.

The checked Idris source lives in `gpu/Example/FockValueProbe.idr`. It consumes
the source-level `expF` primitive through the real shader backend. No
handwritten GLSL is an accepted replacement.

## First numerical corpus

Take `a=0.5+0.25i`, `s=1`, and unit amplitude. The binary64 host values
(rounded here for readability) are:

| z | Real(phi) | Imag(phi) |
|---|---:|---:|
| 0 | 0 | 0 |
| 0.5+0.25i | 1 | 0 |
| -0.3+0.2i | -0.2970864483566 | 0.4294527364731 |
| 1 | 1.6286933603655 | -1.1119356105128 |

The diagnostic shader writes `0.5 + 0.25*Re(q)` and
`0.5 + 0.25*Im(q)` to red and green. This is deliberately **not** Wegert
coloring: it exists only for subsequent numerical framebuffer readback.
Inputs whose encoded channels clip to 0 or 1 are not useful as a color-parity
oracle.

## Required refusal and error boundaries

The host must reject a zero anchor under the `q(0)=0, q(a)=1` constraint,
nonpositive scale, nonfinite amplitude, and arguments likely to overflow the
target's floating representation. The shader currently evaluates
`exp(x)-1`, not a compensated `expm1`; near-zero anchors have cancellation
error and **are not accepted** for this first probe. Preserve a binary64 host
reference and compare the actual generated shader stage on Mesa and the
physical PowerVR before selecting a numeric range or animation parameters.

A successful compiler run or `glslangValidator` check proves only generation,
syntax and link. It does not prove runtime numerical agreement, useful motion,
or physical PowerVR performance. Do not make this the live motion law or close
Holomorphic #54 or #55 based solely on those checks.

Tracking: [Flexible Pipes #66](https://github.com/isomorphisms/flexible-pipes/issues/66)
and shader backend [#70](https://github.com/fuego-ironworks/idris-shader-backend/pull/70).

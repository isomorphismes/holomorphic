# Retained Wegert fixture: binary64 norm boundary

The existing `quadrant-4-hundred` fixture failed in both the
[application tests](https://github.com/isomorphismes/holomorphic/actions/runs/39313671612/job/118269485088)
and the
[native publication tests](https://github.com/isomorphismes/holomorphic/actions/runs/39313672148/job/118270493776).
Both used CPython 3.12.15 and reported red `0.837652631356116` instead of
`0.795009385025`, with the existing `2e-5` tolerance. The test and fixture
blobs were already identical to main; this was not a shader-source change.

The transported coordinates are `(70.71067811865476, -70.71067811865474)`.
For the parsed binary64 coordinates, the exact squared radius is

```
10000 - 6291674759726319 / 4951760157141521099596496896
```

Its radius is `99.999999999999993647032004718212974...`. This is closer to
binary64 `100` than to the preceding representable number: the distances are
approximately `6.353e-15` and `7.858e-15`, respectively. Thus the correctly
rounded binary64 norm is `100`.

The canonical shared Wegert core uses
`fract(log_modulus / log(10))`. At radius `100`, the binary64 logarithm ratio
is `2`, the modulus band is `0`, and the fixture hue gives lightness `66.45`.
The existing HCL conversion then returns
`(0.7950093850254044, 0.5571446716767713, 0.7595745219618744)`.
With the immediately preceding radius, the ratio is `1.9999999999999996`,
the band is nearly `1`, and lightness is `70.45`. The red channel becomes
exactly the observed failing value `0.837652631356116`. This explains the
failure without changing the palette.

The application reference used `abs(complex)`. In
[CPython 3.12.15](https://github.com/python/cpython/blob/v3.12.15/Objects/complexobject.c),
that delegates to the platform C `hypot`. Its separate
[`math.hypot` implementation](https://github.com/python/cpython/blob/v3.12.15/Modules/mathmodule.c)
uses compensated `vector_norm`. The frozen independent AICI palette oracle
already uses `math.hypot(real, imag)`. The bounded repair makes the existing
application reference use that same binary64 norm boundary. It adds no epsilon,
seam exemption, changed fixture, or increased tolerance. Canonical GLSL and the
checked shader remain unchanged.

This fixture describes the existing binary64 measurement followed by colour
conversion. The exact real radius is slightly below the decade boundary;
claiming its fixture RGB as an exact-real, precision-independent colour would
be stronger than this test establishes. The colour mapping is discontinuous at
decade boundaries, so tiny measurement errors can produce a finite colour jump.

Local CPython 3.12.14 on this diagnostic host returned `100` for both norm
implementations and passed all ten existing tests before and after the repair.
The six retained RGB fixtures, core digest and `2e-5` tolerance are unchanged.
The exact failing CI output, exact-coordinate analysis and independent oracle
justify the norm repair; confirmation on the failing hosted execution target
is still pending. These host tests are not shader or physical-GPU acceptance.

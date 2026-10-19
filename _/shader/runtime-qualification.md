# Generated source binding and numerical readback

`bind-generated-fragment.grease` accepts the generated fragment, its exact
compiler/source receipt, and a header output path. Kitchen calls it through the
qualified Grease runtime. It verifies the fragment's SHA-256 and strict receipt
fields before emitting `holomorphic_shader_binding.h`. The native ICK stage
includes that header and `runtime_source_binding.h`.

The native library binds mode, byte length, fragment SHA-256 and compiler/source
commits independently of the APK assets. In checked mode a missing receipt,
changed source, changed receipt identity, embedded NUL, or driver source readback
mismatch rejects renderer creation. The exact byte pointer and explicit length
checked at the submission boundary go to `glShaderSource`. `glGetShaderSource`
then supplies a second digest from the same GL shader object. Generated mode
never changes to the handwritten renderer. Legacy mode is explicitly named and
retains its original remote-time uniform path; it supplies no generated evidence.

`exercise-generated-emulator.grease` replaces the installed APK in place twice,
requires the selected program and both byte digests in actual process logs, and
captures the running process. The Android emulator action invokes this program
through Grease; its fixed outer `/usr/bin/sh` interpreter receives no `pipefail`
or shell-language implementation. Launch and screenshots alone cannot pass
mathematical acceptance.

The generated live shader contains a diagnostic output branch. It uses the same
divisor evaluation and polynomial as the colour path and returns phase, natural
logarithmic modulus, signed singular order and a produced-sample marker. Native
startup uses this same linked program with a one-pixel `GL_RGBA32F` framebuffer
and `GL_FLOAT` readback. It does not load a second shader. Unsupported float
attachments produce `NOT_RUN`. A missing result, nonfinite number, GL error or
produced marker other than one produces `FAIL`. `OUTPUT_READY` means numbers
exist; independent comparison has not yet run.

Diagnostic mode 2 uses the same selected program and sample point but returns
the canonical Wegert colour before markers and placement controls. Each raw
sample is followed by a separate draw to an `RGBA8` attachment and an actual
`GL_UNSIGNED_BYTE` readback. Dithering is disabled during these probes, then
restored. Positive singular order uses the live shader's black endpoint and
negative order its white endpoint. The 34 `HOLO_GPU_COLOR` rows need independent
comparison with the declared colour policy; merely producing the bytes does not
pass framebuffer parity.

The retained corpus has 34 numerical rows across ten cases. Every case logs its
five coefficient pairs, original zero/pole/remote positions, and reduced uploaded
factors before its numerical rows. `HOLO_GPU_INPUT` is the original inventory;
`HOLO_GPU_FACTOR` is the host's cancellation result. Independent acceptance must
check both to avoid trusting a mistaken cancellation just because the shader
evaluates the mistaken reduced input consistently.

| Case | Original divisor |
| --- | --- |
| `one_exact` | Zero and pole at (0.25, −0.375) |
| `one_displaced` | Zero at (0.25, −0.375), pole at (0.3125, −0.40625) |
| `24_exact` | Matching positions (−0.75 + i × 0.0625, −0.5 + i × 0.03125), i = 0…23 |
| `24_displaced` | Same zeros; poles displaced by (0.015625, −0.0078125) |
| `multiplicity` | Three zeros and one pole at (0.25, −0.375) |
| `moving0` | Zero at (−0.34375, 0), pole at (0.34375, 0), 24 remote poles at time 0 |
| `moving7_5` | Same ordinary factors, remote time 7.5 |
| `moving7_5_zoom` | Same factors and time as `moving7_5`, zoom 5.5 |
| `24_displaced_large` | Scale 2⁶⁰ times zeros (1 + i × 0.0625, −1 − i × 0.03125); poles displaced by scale × (0.015625, −0.0078125) |
| `24_displaced_small` | Same construction at scale 2⁻⁶⁰ |

The first eight cases sample (0.125, 0.1875), (0.25, −0.375), (−0.75, −0.5),
and (0.3125, −0.40625). Their zooms are 0.5, 1, 3, 3, except the zoom twin
uses 5.5 throughout. The scale cases sample the origin. The coefficient pairs
in ascending powers of z/3 are:

| Power | Real | Imaginary |
| --- | ---: | ---: |
| 1 | 0.125 | −0.0625 |
| 2 | −0.03125 | 0.015625 |
| 3 | 0.0078125 | 0.00390625 |
| 4 | −0.001953125 | 0.0009765625 |
| 5 | 0.00048828125 | −0.000244140625 |

At a nonzero singular order the phase/log components are placeholders, not a
regular value. Acceptance checks the signed order before comparing a regular
field. Regular phase needs circular error; raw natural log modulus needs its own
error. Neither is inferred from RGB or reduced modulo a colour band.

`readback_cpu_ms` measures elapsed host time through draw submission and readback.
It includes waiting and is not GPU execution time, CPU search time, or EGL
presentation time. These startup probes do not qualify sustained performance.
The renderer also records vendor/renderer/version, separate CPU wall times for
shader compilation and linking, and 60-frame windows separating draw submission
from EGL presentation. Each window records original and reduced factor counts.
GPU execution time, CPU search time and thermal qualification remain explicitly
unmeasured in these native timing rows; their values cannot be inferred from an
EGL wait or a readback stall.

`test-generated-binding.grease` checks the shell generator with labelled contract
fixtures. `test-runtime-binding.grease` compiles the actual portable native
binding boundary through the exact ICK supplied by Kitchen, checks SHA-256
vectors and semantic negative inputs, and executes the resulting host program.
Fixture bytes never count as checked generation or GPU evidence.

# HOLO-GPU-E1 mathematical boundary

The maintained checked source is `gpu/Example/HolomorphicLive.idr`. The host
preprocessor in `android/app/src/main/cpp/holomorphic_divisor.h` constructs its
reduced divisor; it is not an alternate renderer. The retained binary64 oracle
comes from `fix/issue-54-deterministic-divisor-oracle`, not a shader execution.

The host receives at most 32 zeros and 32 manually placed poles, plus exactly
24 wandering poles in live mode. It groups positions by exact equality of the
binary32 coordinates actually uploaded, sums their integer orders, then removes
zero-order groups before evaluating any numerical function. Multiplicity is
retained. Displaced positions are never epsilon-cancelled. A zero can cancel a
wandering pole as well as a manually placed pole. Invalid counts, nonfinite
positions, and capacity overflow reject the frame; no input is truncated.

The shader interface adds `u_divisor_count` and `u_divisor_factors[96]`, whose
three components are world x, world y, and signed integer order. There can be at
most 88 distinct unreduced factors. Marker arrays each contain 32 vec2 values,
matching the runtime limit, and `u_remote_pole_positions[24]` receives the same
authoritative array used for reduction. Remote marks retain pole identity even
at a removable mathematical point. The complete live shader uses 197 uniform
vectors rather than exceeding the GLES 3 minimum by duplicating 64-entry arrays.
Actual hardware limits must still be queried during GPU qualification.

The orbit family retains the previous handedness, angular speeds, radial bend,
and ellipticity. Its world radius scale is explicitly 2.0 world units. Resolution,
zoom, and pan do not enter the trajectory. Orbits need not remain offscreen when
the viewer changes the viewport. The host evaluates the deterministic hash in
binary64 and uploads binary32 positions once per frame, so different GPU sin
implementations no longer choose different pole locations.

The shader computes `log(max(abs(dx),abs(dy)))` plus the logarithm of a scaled
unit-size squared radius. It does not form a product of factors or square large
or tiny unscaled coordinates. At an actual singularity it records signed order
and feeds safe operands to atan/log. Removable points have no retained factor,
so no undefined phase is evaluated. Numerical output is phase, logarithmic
modulus, signed singularity order, and a usable-sample marker. Marker zero rejects
nonfinite output and detects a nonzero divisor component completely erased by
the final addition to q. The marker is independent of singularity order and
does not certify an error bound. Partial accumulation error still needs the
independent reference comparison.

The raw numerical output still uses binary32 on this backend. A large entire
term can exceed its ability to retain a small divisor contribution. The colour
path reduces the entire term by the canonical phase/log periods before adding
the divisor. That preserves the established Wegert colour coordinates; it is
not a promise of arbitrarily accurate raw binary32 logarithmic modulus. The
independent numerical acceptance must reject samples exceeding its unchanged
tolerances. No compiler, APK, emulator, GPU, or performance pass follows from
the retained oracle's host tests. A portable binary32 qualification interval
requires finite inputs and finite polynomial intermediates, representable
coordinate differences, and nonzero radii at least 1.1754943508222875e-38;
physical GPUs may flush smaller subnormal values. The scaled-radius method
handles tested radii 1e-30 through 1e30 without unscaled squaring. It does not
make a universal accuracy claim for arbitrary points throughout that interval.

The live polynomial remains the five coefficients evaluated at z/3. The CPU
search worker's `disturbance_samples` are already normalized u-coordinates. Its
comment says z/6, but no z-to-u division actually occurs in `direction_at` or
`disturbance_score`. Changing that comment alone would not reconcile a world
sampling policy. This repair leaves the search objective and live coefficients
unchanged; the separate search-policy decision needs an explicitly defined
world-space sample set and derivative weighting, followed by PR #65's semantic
direction tests. It must not silently change the live field to z/6.

The ICK driver `_/tests/holo-divisor-sampler.c` accepts the independent AICI TSV
corpus path as its only argument. It exercises host reduction and computes a
binary64 host reference. Its output is explicitly not generated-shader output.
Required input columns are case_id, world_x, world_y, time, pan_x, pan_y, zoom,
coefficients, zeros, poles, remote_count. Coordinate lists use x,y;x,y; empty
lists use `-`; coefficients contain exactly five positions. Output columns are
case_id, phase, log_modulus, singularity_order, reduced_zero_count, and
reduced_pole_count. Actual ICK compilation and execution remain separate gates.

# Explicit frame performance capture

Launch the checked application with the Boolean intent extra
`holo_performance=true`. Ordinary interactive launches do not allocate or write
the raw frame capture. Qualification readbacks are a separate launch mode and
must not run during this performance capture.

For a paired frozen-pose diagnostic, additionally select `holo_frozen_pose=true`.
Both runs use time 0.375, the initial user zero/pole, all 24 remote poles, zoom 1,
and the same nonzero five-term coefficient array. The reference run additionally
selects `holo_reference_fullscreen=true`; the candidate leaves it absent. These
flags only take effect in an explicit performance capture. Ordinary rendering
always uses localized marker regions. Match surface dimensions, actual display
mode, temperature and APK identity before comparing receipts. The frozen run
does not qualify live motion or interaction performance.

The native sampler retains at most 1,800 frame attempts in heap memory. It records
scene preparation/divisor reduction, uniform uploads, GL submission and
`eglSwapBuffers` CPU wall durations, followed by the monotonic timestamp after
the swap call returns. Before collection, it creates a unique
`holo-performance-XXXXXX` directory beneath the activity's `internalDataPath`;
the READY record announces its exact `frames.tsv` path. Collection performs no
receipt I/O or logging. After the last attempt, it writes that unique TSV, then
reports its exact path and summary in logcat. Application commit, selected
fragment SHA-256, overlay mode and frozen-pose flag are recorded in the receipt.

The TSV retains every attempt and its validity flags. Failed swaps remain rows;
they do not replace the previous successful-swap timestamp. The first successful
swap supplies the interval origin and contributes no interval sample. Nearest-rank
p50, p95 and p99 are computed independently for each phase and valid interval.
The count of intervals over 16.6667 ms uses an explicitly nominal 60 Hz budget;
the actual display refresh rate remains `UNKNOWN` until separately observed.

A successful swap return is a CPU-side boundary. These intervals do not establish
GPU execution time or the actual panel presentation timestamp. Record device,
renderer, source, shader and APK identity separately with the raw receipt.

Final app-thread cleanup writes an interrupted capture that has at least one row
with `collection_status=INCOMPLETE`. A capture stopped before its first attempt
has no receipt at its announced unique path. Earlier COMPLETE receipts remain
in different capture directories and cannot substitute for that missing result.
Focus changes and window recreation should keep
the sampler alive; a background pause remains visible in the next successful
interval. Retrieve the exact TSV through the installed development package's
authorized app-data channel. No physical C67 result is asserted by this document.

Raw capture disables GPU queries and periodic state/frame logs during collection;
the existing asynchronous disjoint-checked GPU timer is a separate diagnostic
run. Cold shader compile/link durations remain separate launch records. The raw
capture includes its first rendered frames, with no hidden warm-up deletion.

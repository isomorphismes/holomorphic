# Selected-program overlay qualification

`holo_overlay_qualification.h` is an explicit in-app GPU qualification adapter.
It uses the app's selected generated fragment and the production
`draw_checked_scene` function. It contains no replacement shader or CPU image
renderer. Both observations use one linked program and identical frozen
coefficients, zoom, positions, reduced divisor and RGBA8 framebuffer.

The reference is the retained complete mode-0 draw. The candidate is the
production localized path: mode-4 field followed by bounded mode-0 replays.
Comparison requires exact equality of every RGBA8 byte. It reports differing
pixel count and maximum channel difference; it does not relax a mismatch into
an accepted numerical tolerance.

The twelve cases cover visible markers, each clipped edge, a clipped corner,
fully offscreen markers, overlapping markers, coincident zero/pole identities,
controls over markers, zoomed-out remote markers, and all 32 user zeros plus
32 user poles. Every case retains all 24 remote mathematical factors before
exact-coordinate reduction. The coincident case intentionally exercises the
same signed-multiplicity reduction used by ordinary rendering.

Two negative observations alter actual GPU inputs after capturing an unchanged
reference. The first omits a clipped zero's overlay while retaining its
mathematical factor. The second removes precisely one negative-order remote
factor whose marker is independently checked to be offscreen, retaining every
other factor and every marker position. Each must produce different readback bytes. A shader
error, incomplete framebuffer, missing observation or GL error fails the test;
none counts as a rejected mutant.

The adapter saves and restores engine state, framebuffer bindings, viewport,
scissor, program, vertex array, pixel-pack state, texture binding and modified
render state. Its readbacks execute only after explicit
`--ez holo_qualification true`. Ordinary startup runs without them.

The existing `exercise-generated-emulator.grease` first tests ordinary startup
in one process, retaining its separate log and presented screenshot. It then
force-stops that process and launches the explicit qualification process. A
complete pre-launch logcat stream retains all original 34 numerical/colour
observations and the separate large-q observations. The existing independent
mathematical and provenance audit remains mandatory. The procedure additionally
requires twelve passing overlay observations and two rejected GPU-input
mutants. The two processes supply separate evidence and are not a continuous
motion demonstration.

These tests establish localized-versus-reference GPU pixels only on the target
where the app actually executes. A hosted emulator result does not establish
physical C67 frame time, PowerVR behavior, thermal performance or continuous
motion. The 192 by 128 qualification framebuffer is a bounded correctness
fixture, not a C67 performance workload.

The values are frozen scene states, a reduced signed divisor, RGBA8 byte
sequences, and an exact difference result. The central operations are scene
upload, production draw, GPU readback and byte comparison; OpenGL calls are
effects. This adapter belongs in the existing native C runtime because its
contract specifically exercises that runtime's actual GL state and draw helper.
It does not introduce another rendering implementation.

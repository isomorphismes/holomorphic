# Native timer declaration repair

The first charcoal producer successfully generated and independently qualified
the checked shader, then failed actual ARM64 C compilation in
`holo_gpu_timer.h`. The declarations used `GL_APIENTRYP *`, although
`GL_APIENTRYP` already includes the function-pointer indirection. The fields and
casts therefore declared pointers to function pointers and could not be called.

Remove the additional indirection in the three fields and matching casts. Keep
the calling convention, argument types, extension discovery, asynchronous polling,
query cadence and all timing logic. No CPU/EGL surrogate replaces GPU timing,
and the optional timer is not disabled to obtain a build.

The retained failed current-source runs are 41959704963 (checked producer) and
41959704946 (unsigned release packaging). This repair requires fresh native
compilation and actual application execution; prior shader qualification does
not establish timer execution or physical GPU timing.

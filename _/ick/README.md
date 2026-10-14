# ICK source production

Owned Android C and the existing worker regression use the division glyph directly.
The compiler is ICK c61e448251744a2f40ad743ebef1a027bdcd2f9d, with the Android
source boundary pinned to ai-ci 903b2cb27ea572c9c6cb2ffa9f39e0fbf06ec9f8.

All maintained APK, unsigned-release and Play producers qualify the same three
ABI stages and restore them under `_/build/ick/<abi>`. They check out the shared
rules under `_/ai-ci-ick`. The existing CMake source route calls those rules,
emits assembly with ICK, and keeps NDK r29 assembly, platform linking and unchanged
NativeActivity glue. Fortify2, stack protection, debug information, API26, the
three-ABI payload and existing signing/interaction checks remain active.

Local Gradle builds require these qualified stages and the shared checkout first.
Explicit CMake `ICK_COMPILER`, `ICK_COMPILER_OPTIONS`, `ICK_TARGET_FLAGS`,
`ICK_HEADER_TARGET` and `ICK_HEADER_OVERLAY` support qualified local compilers.
The native worker test is `make -f _/ick/Makefile test ICK=/path/to/qualified/ick`.
No source normalization or alternate stock-C compilation is used.

The F-Droid template builds ICK and ai-ci from pinned source libraries. The
definitions under `_/fdroid/srclibs` accompany that metadata when submitted;
the template is not evidence of an actual F-Droid release.

`_/ci/division-glyph-local.tsv` records the bounded native tests. Hosted APK and
emulator checks and physical-device acceptance are separate evidence levels.
This syntax/producer migration leaves the mathematical model, continuous state,
canonical Wegert coloring and renderer implementation unchanged.

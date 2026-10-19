# Qualification APK publication and deferred Cat Food acquisition

These procedures publish a qualification prerelease after the actual checked
APK and independent emulator acceptance pass. They do not install an Android
package, qualify a physical GPU, promote a production release, merge a branch,
or close a correctness issue.

`prepare-qualification-apk.grease ROOT APK SOURCE BACKEND OUTPUT` runs on the
producer after stable-signature verification. All paths are absolute. It reads
the actual signed APK with the declared SDK 36 and NDK 29 tools, compares its
shader assets with the generated inputs, and records manifest package/version,
signer, native ABI/ELF entries, packed native sizes, APK size and SHA-256. Debug
or static symbol sections in a packed ELF reject publication. An absent
pre-strip intermediate is recorded as `NOT_RECORDED`; no reduction is inferred.
The APK is hashed again after inspection and is never modified.

`publish-qualification-apk.grease APK_ARTIFACT EMULATOR_ARTIFACT SOURCE BACKEND OUTPUT`
runs only after `emulate` succeeds, using runner `gh` and ephemeral `GH_TOKEN`.
It checks the producer identity, exact APK bytes, exact shader selection,
independent provenance PASS and the predeclared phase/log/RGBA8 acceptance PASS.
Its deterministic tag is `qualification-holomorphic-SOURCE-APK_SHA256`, with
both full digests. It creates a draft prerelease, uploads without `--clobber`,
downloads and compares both assets, then publishes. Existing mismatched tags,
assets or receipts reject the operation. A rerun with different evidence does
not overwrite an earlier receipt. Unauthenticated public raw downloads are
compared before publication is reported as complete.

The new publication job must depend on `[generated-apk, emulate]`, require
`needs.emulate.result == 'success'`, require the exact repository
`isomorphismes/holomorphic`, and accept only `workflow_dispatch` or a
`pull_request` whose head repository is that same repository. Its only write
permission is job-local `contents: write`; it also needs `actions: read`.
Check out the exact application head with credentials persistence disabled.
Use the existing verified Grease artifact and engine digest through the current
`_/runtime/ysh` engine boundary. The bare multicall engine cannot be renamed
`grease`; the local Grease wrapper invokes its sibling `ysh`. Download
`checked-idris-holomorphic-apk` and `checked-idris-holomorphic-emulator` into
separate absolute artifact roots. Set `HOLOMORPHIC_EMULATOR_ACCEPTED` from
`needs.emulate.result`, `GH_TOKEN` from `github.token`, and
`ASAN_OPTIONS=detect_leaks=0`. Invoke the publication program through that
verified Grease engine. Upload only `publication.tsv`, `assets/*.tsv` and
`gh-version.txt` as publication evidence; never upload `gh-auth-status.txt`.

The APK producer artifact must additionally retain
`_/build/qualification-apk/producer-identity.tsv`, `manifest-badging.txt`,
`signature-verification.txt`, `native-entries.txt`, and packed-ELF inspection
`.sections`/`.header` files. Keep the existing artifact paths for the actual APK
and generated shader. The producer invokes the inspection program immediately
after the existing stable-signer check. The seven rejection fixtures in
`test-qualification-publication.grease` exercise only local publication guards;
they are never APK, shader, emulator or physical evidence.

## Isolated deferred acquisition after publication

Use unmodified `isomorphisms/catfood` at
`b387f90d3973aae39ba19d370a830da627299ab1`. Verify its exact Git head and clean
tracked files first. Keep a new manifest and prefix outside its checkout. The
public `.apk` URL, source SHA, APK SHA/size and receipt URL come from the actual
`publication.tsv` and matching signed-producer receipt. An Actions artifact
download is a ZIP and cannot replace that raw APK URL in Cat Food's `file` mode.

The isolated `tools.tsv` contains the existing `analytic-continuation` inventory
row bound to the exact application source. The isolated `delivery.tsv` has a
`grease` runtime row with `gap:qualification-scope` in both ABI lanes, and an
`analytic-continuation` runtime row referencing only two candidate package IDs,
one per ABI lane. The 18-field `packages.tsv` uses `mode=file`, the actual
public APK URL and SHA, source repository `isomorphismes/holomorphic`, and the
application commit for both source and packaging refs. The lanes are
`armeabi-v7a` and `arm64-v8a`; the unchanged universal APK contains both and
x86_64. Use `files/holomorphic.apk` as the entrypoint, `curl` as the sole
installation requirement and `-` for the DEX fields and other dependencies.
The required command column is an inert candidate label because command
exposure is mandatorily deferred.

`acquire-qualification-apk.grease CATFOOD PUBLICATION APK_ARTIFACT OUTPUT`
implements this bounded host check. `PUBLICATION` is the publisher output root
containing `publication.tsv` and its `assets/*.tsv` receipt. `APK_ARTIFACT` is the
downloaded checked APK artifact root. `OUTPUT` must not already exist. The
program checks the pinned clean Cat Food checkout, validates the real identity
and public URLs, checks its isolated manifest with Cat Food, and performs a
fresh download through the unchanged installer. Its `acquisition.tsv` and
`catfood-receipt.tsv` distinguish file acquisition from Android installation and
all physical acceptance. The downloaded APK remains non-executable and no bin
entry is exposed.

For a host-only acquisition check, the exact existing Cat Food boundary is the
following invocation from Grease, with absolute paths to the verified checkout
and fresh isolated manifest/prefix. `candidate` is the ARMv7 candidate package
ID. `tests/android-properties.sh` is Cat Food's own synthetic identity fixture;
this run makes no observation about a handset.

```grease
cp "$catfood/tests/android-properties.sh" "$isolated/getprop-fixture"
chmod 755 "$isolated/getprop-fixture"
env CATFOOD_TARGET=phone \
  CATFOOD_GETPROP="$isolated/getprop-fixture" \
  CATFOOD_DEVICE_ID=holomorphic-qualification-host-fixture \
  CATFOOD_ROOT="$isolated/prefix" CATFOOD_CACHE="$isolated/cache" \
  CATFOOD_TOOLS="$isolated/manifest/tools.tsv" \
  CATFOOD_ANDROID_DELIVERY="$isolated/manifest/delivery.tsv" \
  CATFOOD_ANDROID_PACKAGES="$isolated/manifest/packages.tsv" \
  CATFOOD_ANDROID_ONLY="$candidate" CATFOOD_ANDROID_DEFER_COMMANDS=1 \
  sh "$catfood/android/install.sh"
```

Use a fresh prefix/cache so Cat Food actually fetches the public URL. Verify
the resulting `prefix/packages/CANDIDATE/SOURCE/files/holomorphic.apk` against
the producer's SHA and size, compare its bytes to the qualified signed APK,
require that it is non-executable and that `prefix/bin` has no candidate link.
Keep Cat Food's receipt and the producer/qualification receipt separately.
Cat Food's file materialization `installation_result=PASS` is not Android
package-manager installation or application launch. Its physical-device result
remains `NOT_VERIFIED`; the SUN qualification receipt remains `NOT_RUN` for
all physical GPUs and sustained performance. Physical access, replacement
installation and per-device numerical/performance receipts are separate work.

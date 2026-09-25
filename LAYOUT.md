# Repository layout

The source-facing root is intentionally small.

Maintained files useful to read or edit directly are exposed at the repository root as symbolic links. Their canonical locations remain inside the implementation tree.

Build, packaging, generated, test, release, and platform-support machinery lives under `_/`. The Android project is canonical at `_/android/`. Root directory links such as `android` or `tests` exist only for compatibility with older commands and external tooling.

`.github/workflows/` remains at the root because GitHub only discovers Actions workflows there. It is the platform-required exception to the `_/` machinery boundary.

Put new machinery under `_/`; expose only maintained human-facing source at the top level.

# Repository layout

The source-facing root is intentionally small.

Maintained files that are useful to read or edit directly are exposed at the repository root as symbolic links. Their canonical locations remain inside the implementation tree.

Build, packaging, generated, test, release, and platform-support machinery lives under `_/`. In particular, the Android project is canonical at `_/android/`; compatibility directory links at the root exist only so older commands and external tooling do not break while branches are migrated.

`.github/workflows/` remains at the repository root because GitHub only discovers Actions workflows there. It is the one platform-required exception to the `_/` machinery boundary.

Do not add new build or generated material beside the source-facing links. Put it under `_/` and expose only maintained human-facing source at the top level.

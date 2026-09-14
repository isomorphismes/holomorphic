#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = Path(args.repository_root).resolve()
    output = Path(args.output).resolve()
    identity = json.loads((root / "_/shader/backend.json").read_text())
    backend = root / "_/.idris-shader-backend"
    compiler = backend / "build/exec/idris2-glsles"
    source = root / "math/HolomorphicField.idr"
    staged_source = backend / "src/HolomorphicField.idr"

    if not compiler.is_file():
        raise SystemExit(
            f"missing pinned shader compiler at {compiler}; "
            "run the repository shader-backend setup first"
        )

    actual_backend_commit = subprocess.check_output(
        ["git", "-C", str(backend), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual_backend_commit != identity["commit"]:
        raise SystemExit(
            "shader backend commit mismatch: "
            f"expected {identity['commit']}, got {actual_backend_commit}"
        )

    if staged_source.exists():
        raise SystemExit(f"refusing to overwrite backend source {staged_source}")

    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, staged_source)
    try:
        command = [
            str(compiler),
            "--cg",
            "glsles",
            "--source-dir",
            str(backend / "src"),
            "--output-dir",
            str(output.parent),
            str(staged_source),
            "-o",
            output.stem,
        ]
        subprocess.run(command, cwd=backend, check=True)
    finally:
        staged_source.unlink(missing_ok=True)

    generated = output.parent / f"{output.stem}.frag"
    if generated != output:
        generated.replace(output)
    if not output.is_file():
        raise SystemExit(f"compiler did not produce {output}")


if __name__ == "__main__":
    main()

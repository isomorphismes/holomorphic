#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import gzip
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_gzip(data: bytes) -> bytes:
    compressed = bytearray(gzip.compress(data, compresslevel=9, mtime=0))
    if len(compressed) < 10 or compressed[0:3] != b"\x1f\x8b\x08":
        raise SystemExit("Python did not produce the expected gzip header")
    # gzip header byte 9 is the originating-OS identifier.  CPython/zlib
    # versions disagree about it when mtime=0 even when the compressed stream
    # is identical.  Canonicalize that metadata byte; it does not affect the
    # DEFLATE payload, CRC, size, or decompression semantics.
    compressed[9] = 255
    return bytes(compressed)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--record")
    parser.add_argument("--packed-output")
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

    output_bytes = output.read_bytes()
    packed_bytes = None
    if args.packed_output:
        packed_path = Path(args.packed_output).resolve()
        packed_path.parent.mkdir(parents=True, exist_ok=True)
        packed_bytes = base64.b64encode(canonical_gzip(output_bytes)) + b"\n"
        packed_path.write_bytes(packed_bytes)

    if args.record:
        record_path = Path(args.record).resolve()
        record_path.parent.mkdir(parents=True, exist_ok=True)
        record = {
            "schema": "holomorphic-compiler-output-v1",
            "backend": identity["backend"],
            "backend_repository": identity["repository"],
            "backend_commit": identity["commit"],
            "compiler": identity["compiler"],
            "idris2_version": identity["idris2_version"],
            "precision_policy": identity["precision_policy"],
            "authoritative_source": "math/HolomorphicField.idr",
            "authoritative_source_sha256": sha256(source.read_bytes()),
            "compiler_output": "continuation-math.frag",
            "compiler_output_sha256": sha256(output_bytes),
            "compiler_output_transport": (
                Path(args.packed_output).name if args.packed_output else None
            ),
            "compiler_output_transport_sha256": (
                sha256(packed_bytes) if packed_bytes is not None else None
            ),
        }
        record_path.write_text(
            json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n"
        )


if __name__ == "__main__":
    main()

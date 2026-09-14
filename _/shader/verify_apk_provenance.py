#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import zipfile
from pathlib import Path


SHADER_PATH = "assets/continuation.frag"
PROVENANCE_PATH = "assets/shader-provenance.json"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("apk")
    parser.add_argument("--expected-source-commit")
    parser.add_argument("--print-shader-hash", action="store_true")
    args = parser.parse_args()

    apk = Path(args.apk)
    with zipfile.ZipFile(apk) as archive:
        names = set(archive.namelist())
        if SHADER_PATH not in names:
            raise SystemExit(f"{SHADER_PATH} is not packaged")
        if PROVENANCE_PATH not in names:
            raise SystemExit(f"{PROVENANCE_PATH} is not packaged")
        shader = archive.read(SHADER_PATH)
        provenance = json.loads(archive.read(PROVENANCE_PATH))

    actual_hash = hashlib.sha256(shader).hexdigest()
    expected_hash = provenance.get("generated_shader_sha256")
    if actual_hash != expected_hash:
        raise SystemExit(
            f"packaged shader hash mismatch: provenance={expected_hash} actual={actual_hash}"
        )

    if provenance.get("backend") != "idris-shader-backend/glsles":
        raise SystemExit(f"unexpected backend: {provenance.get('backend')}")

    if args.expected_source_commit:
        actual_commit = provenance.get("source_commit")
        if actual_commit != args.expected_source_commit:
            raise SystemExit(
                "source commit mismatch: "
                f"provenance={actual_commit} expected={args.expected_source_commit}"
            )

    if args.print_shader_hash:
        print(actual_hash)
    else:
        print(
            "verified packaged shader provenance: "
            f"backend={provenance['backend']} sha256={actual_hash} "
            f"source_commit={provenance['source_commit']}"
        )


if __name__ == "__main__":
    main()

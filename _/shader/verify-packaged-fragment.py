#!/usr/bin/env python3
"""Verify exact generated GLSL and provenance bytes in an actual APK or AAB.

This is a packaging check, not proof that a program ran on a physical GPU.
"""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path
from zipfile import BadZipFile, ZipFile


def verify_package(package: Path, shader: Path, receipt: Path) -> None:
    package = Path(package)
    shader = Path(shader)
    receipt = Path(receipt)
    if package.suffix not in (".apk", ".aab"):
        raise ValueError("expected an APK or AAB")
    prefix = "assets/" if package.suffix == ".apk" else "base/assets/"
    shader_name = prefix + "continuation.frag"
    receipt_name = prefix + "continuation.provenance"

    source_bytes = shader.read_bytes()
    receipt_bytes = receipt.read_bytes()
    if not source_bytes or b"\x00" in source_bytes:
        raise ValueError("invalid generated fragment")
    if not receipt_bytes or b"\x00" in receipt_bytes:
        raise ValueError("invalid generated provenance")
    lines = receipt_bytes.decode("utf-8").splitlines()
    fields = {}
    for line in lines:
        if "=" not in line:
            raise ValueError("malformed provenance line")
        name, value = line.split("=", 1)
        if not name or name in fields:
            raise ValueError("missing/duplicate provenance name")
        fields[name] = value
    for name, value in (
        ("compiler", "idris2-glsles"),
        ("backend_repo", "fuego-ironworks/idris-shader-backend"),
        ("application_repo", "isomorphismes/holomorphic"),
        ("fragment_sha256", hashlib.sha256(source_bytes).hexdigest()),
    ):
        if fields.get(name) != value:
            raise ValueError(f"generated provenance mismatch: {name}")
    for name in ("backend_commit", "application_commit"):
        value = fields.get(name, "")
        if len(value) != 40 or any(char not in "0123456789abcdef" for char in value):
            raise ValueError(f"invalid {name}")

    with ZipFile(package) as archive:
        names = archive.namelist()
        for name, expected in ((shader_name, source_bytes), (receipt_name, receipt_bytes)):
            if names.count(name) != 1:
                raise ValueError(f"missing/duplicate packaged asset: {name}")
            if archive.read(name) != expected:
                raise ValueError(f"packaged bytes mismatch: {name}")
        if any("/*__WEGERT_COLOR_CORE__*/" in archive.read(shader_name).decode("utf-8")
               for _ in range(1)):
            raise ValueError("legacy template marker in selected fragment")


def main(argv: list[str]) -> int:
    if len(argv) != 4:
        print(f"Usage: {argv[0]} PACKAGE.apk|PACKAGE.aab SHADER.frag PROVENANCE", file=sys.stderr)
        return 2
    try:
        verify_package(Path(argv[1]), Path(argv[2]), Path(argv[3]))
    except (OSError, ValueError, UnicodeError, BadZipFile) as exc:
        print(f"PACKAGED_FRAGMENT_FAIL {exc}", file=sys.stderr)
        return 1
    print("PACKAGED_FRAGMENT_PASS exact-generated-source-and-provenance")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path


GENERATED_MAIN = "void main() {"
GENERATED_MAIN_REPLACEMENT = "void generated_holomorphic_field() {"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def source_commit(root: Path) -> str:
    explicit = os.environ.get("SOURCE_HEAD_SHA")
    if explicit:
        return explicit
    return subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", required=True)
    parser.add_argument("--compiler-output", required=True)
    parser.add_argument("--output-directory", required=True)
    args = parser.parse_args()

    root = Path(args.repository_root).resolve()
    compiler_output_path = Path(args.compiler_output).resolve()
    output_directory = Path(args.output_directory).resolve()
    authoritative_source = root / "math/HolomorphicField.idr"
    wegert_source = root / "android/app/src/main/assets/wegert_color.glsl"
    overlay_source = root / "android/app/src/main/shader/continuation_overlay.glsl"
    backend_identity = json.loads((root / "_/shader/backend.json").read_text())

    compiler_output = compiler_output_path.read_text()
    if compiler_output.count(GENERATED_MAIN) != 1:
        raise SystemExit("compiler output must contain exactly one fragment main")
    required_generated_tokens = (
        "#version 300 es",
        "precision highp float;",
        "in vec2 v_ndc;",
        "uniform vec2 u_resolution;",
        "uniform int u_zero_count;",
        "uniform int u_pole_count;",
        "uniform vec2 u_zero_positions[32];",
        "uniform vec2 u_pole_positions[32];",
        "uniform vec2 u_holomorphic_coefficients[5];",
        "uniform float u_remote_pole_time;",
        "uniform float u_zoom;",
        "layout(location = 0) out vec4 _idris_fragColor;",
    )
    missing = [token for token in required_generated_tokens if token not in compiler_output]
    if missing:
        raise SystemExit(f"compiler output missing expected interface tokens: {missing}")

    generated_callable = compiler_output.replace(
        GENERATED_MAIN, GENERATED_MAIN_REPLACEMENT, 1
    ).rstrip()

    wegert = wegert_source.read_text().rstrip()
    overlay = overlay_source.read_text().rstrip()
    final_text = (
        generated_callable
        + "\n\n// ---- handwritten Wegert coloring policy ----\n"
        + wegert
        + "\n\n// ---- handwritten interaction/presentation overlay ----\n"
        + overlay
        + "\n"
    )
    final_bytes = final_text.encode("utf-8")

    output_directory.mkdir(parents=True, exist_ok=True)
    final_shader = output_directory / "continuation.frag"
    final_shader.write_bytes(final_bytes)

    compiler_bytes = compiler_output_path.read_bytes()
    source_bytes = authoritative_source.read_bytes()
    provenance = {
        "schema": "holomorphic-shader-provenance-v1",
        "backend": backend_identity["backend"],
        "authoritative_source": "math/HolomorphicField.idr",
        "authoritative_source_sha256": sha256(source_bytes),
        "compiler_output": "continuation-math.frag",
        "compiler_output_sha256": sha256(compiler_bytes),
        "generated_shader": "continuation.frag",
        "generated_shader_sha256": sha256(final_bytes),
        "source_commit": source_commit(root),
        "compiler": {
            "repository": backend_identity["repository"],
            "commit": backend_identity["commit"],
            "executable": backend_identity["compiler"],
            "idris2_version": backend_identity["idris2_version"],
            "precision_policy": backend_identity["precision_policy"],
        },
        "composition": [
            {
                "role": "GENERATED",
                "source": "math/HolomorphicField.idr",
                "description": "mathematical field observed as phase and log modulus",
            },
            {
                "role": "APPLICATION PARAMETER/POLICY",
                "source": "android/app/src/main/assets/wegert_color.glsl",
                "description": "handwritten Wegert coloring",
            },
            {
                "role": "APPLICATION PARAMETER/POLICY",
                "source": "android/app/src/main/shader/continuation_overlay.glsl",
                "description": "handwritten interaction overlay",
            },
        ],
        "shared_semantic_contract_dependency": None,
    }
    (output_directory / "shader-provenance.json").write_text(
        json.dumps(provenance, sort_keys=True, separators=(",", ":")) + "\n"
    )


if __name__ == "__main__":
    main()

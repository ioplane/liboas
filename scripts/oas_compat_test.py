#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14,<3.15"
# dependencies = []
# ///
"""Validate an OpenAPI specification and compile its generated C client."""

from __future__ import annotations

import sys
from pathlib import Path

from oas_tooling import (
    GateSummary,
    ToolVersions,
    ensure_tools,
    require_java,
    validate_generator,
    validate_style,
    generate_c_client,
)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    spec = (
        Path(sys.argv[1])
        if len(sys.argv) > 1
        else root / "tests" / "fixtures" / "petstore-3.2.json"
    )
    summary = GateSummary()
    if not spec.is_file():
        summary.fail_step(f"Spec file not found: {spec}")
        return summary.emit()
    print(f"Spec file: {spec}")
    if not require_java():
        summary.fail_step("Java not found; OpenAPI compatibility tools are required")
        return summary.emit()
    versions = ToolVersions()
    style_path, generator_path = ensure_tools(root, versions, summary)
    validate_style(style_path, spec, summary)
    validate_generator(generator_path, spec, summary)
    generate_c_client(
        generator_path,
        spec,
        root / "build" / "oas-gen-c",
        summary,
    )
    return summary.emit()


if __name__ == "__main__":
    raise SystemExit(main())

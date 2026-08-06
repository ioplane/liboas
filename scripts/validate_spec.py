#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14,<3.15"
# dependencies = []
# ///
"""Build liboas and validate an OpenAPI specification with external tools."""

from __future__ import annotations

import sys
from pathlib import Path

from oas_tooling import (
    GateSummary,
    ToolVersions,
    ensure_tools,
    require_java,
    run_command,
    validate_generator,
    validate_style,
)


def find_spec(root: Path, requested: str | None, build: Path) -> Path | None:
    candidates = ([Path(requested)] if requested else []) + [
        build / "openapi.json",
        root / "build" / "openapi.json",
        root / "tests" / "fixtures" / "petstore-3.2.json",
        root / "tests" / "fixtures" / "petstore.json",
    ]
    return next((candidate for candidate in candidates if candidate.is_file()), None)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    preset = "clang-debug"
    build = root / "build" / preset
    summary = GateSummary()
    run_command(["cmake", "--preset", preset], cwd=root, tail=3)
    run_command(["cmake", "--build", "--preset", preset], cwd=root, tail=5)
    summary.pass_step("Build succeeded")
    spec = find_spec(root, sys.argv[1] if len(sys.argv) > 1 else None, build)
    if spec is None:
        summary.fail_step("No OpenAPI spec found")
        return summary.emit()
    print(f"Spec file: {spec}")
    summary.pass_step("Spec file found")
    if not require_java():
        summary.skip_step("Java not found; external validators skipped")
        summary.skip_step("OpenAPI generator validation skipped")
        return summary.emit()
    style_path, generator_path = ensure_tools(root, ToolVersions(), summary)
    validate_style(style_path, spec, summary)
    validate_generator(generator_path, spec, summary)
    return summary.emit()


if __name__ == "__main__":
    raise SystemExit(main())

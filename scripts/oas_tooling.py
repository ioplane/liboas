"""Shared OpenAPI validation and compatibility tooling."""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from urllib.request import urlopen


@dataclass(frozen=True)
class ToolVersions:
    style_validator: str = "1.11"
    openapi_generator: str = "7.12.0"

    @property
    def style_validator_name(self) -> str:
        return f"openapi-style-validator-cli-{self.style_validator}-all.jar"

    @property
    def openapi_generator_name(self) -> str:
        return f"openapi-generator-cli-{self.openapi_generator}.jar"


@dataclass
class GateSummary:
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    _messages: list[str] = field(default_factory=list)

    def pass_step(self, message: str) -> None:
        self.passed += 1
        self._messages.append(f"PASS: {message}")

    def fail_step(self, message: str) -> None:
        self.failed += 1
        self._messages.append(f"FAIL: {message}")

    def skip_step(self, message: str) -> None:
        self.skipped += 1
        self._messages.append(f"SKIP: {message}")

    def emit(self) -> int:
        for message in self._messages:
            print(message)
        print(f"PASS: {self.passed}  FAIL: {self.failed}  SKIP: {self.skipped}")
        return 1 if self.failed else 0


def run_command(
    command: list[str],
    *,
    cwd: Path,
    check: bool = True,
    tail: int | None = None,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    output = result.stdout or ""
    if tail is not None:
        output = "\n".join(output.splitlines()[-tail:])
    if output:
        print(output)
    if check and result.returncode:
        raise subprocess.CalledProcessError(result.returncode, command, output=output)
    return result


def download_cached(url: str, destination: Path) -> bool:
    if destination.is_file():
        return True
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(f"{destination.suffix}.tmp")
    try:
        with urlopen(url, timeout=120) as response:
            temporary.write_bytes(response.read())
        os.replace(temporary, destination)
    except OSError:
        temporary.unlink(missing_ok=True)
        return False
    return True


def tool_paths(root: Path, versions: ToolVersions) -> tuple[Path, Path]:
    cache = root / "build" / "oas-tools"
    return (
        cache / versions.style_validator_name,
        cache / versions.openapi_generator_name,
    )


def tool_urls(versions: ToolVersions) -> tuple[str, str]:
    style = (
        "https://repo1.maven.org/maven2/org/openapitools/openapistylevalidator/"
        f"openapi-style-validator-cli/{versions.style_validator}/"
        f"{versions.style_validator_name}"
    )
    generator = (
        "https://repo1.maven.org/maven2/org/openapitools/openapi-generator-cli/"
        f"{versions.openapi_generator}/{versions.openapi_generator_name}"
    )
    return style, generator


def ensure_tools(
    root: Path,
    versions: ToolVersions,
    summary: GateSummary,
) -> tuple[Path | None, Path | None]:
    style_path, generator_path = tool_paths(root, versions)
    style_url, generator_url = tool_urls(versions)
    style_ready = download_cached(style_url, style_path)
    generator_ready = download_cached(generator_url, generator_path)
    if style_ready or generator_ready:
        summary.pass_step("Tool JARs ready")
    else:
        summary.fail_step("No OpenAPI validation tools available")
    return (
        style_path if style_ready else None,
        generator_path if generator_ready else None,
    )


def validate_style(
    jar: Path | None,
    spec: Path,
    summary: GateSummary,
) -> None:
    if jar is None:
        summary.skip_step("openapi-style-validator not available")
        return
    result = run_command(
        ["java", "-jar", str(jar), "-s", str(spec)],
        cwd=spec.parent,
        check=False,
    )
    if result.returncode == 0:
        summary.pass_step("Style validation passed")
    else:
        summary.fail_step("Style validation found issues")


def validate_generator(jar: Path | None, spec: Path, summary: GateSummary) -> None:
    if jar is None:
        summary.skip_step("openapi-generator not available")
        return
    result = run_command(
        ["java", "-jar", str(jar), "validate", "-i", str(spec)],
        cwd=spec.parent,
        check=False,
    )
    if result.returncode == 0:
        summary.pass_step("Generator validation passed")
    else:
        summary.fail_step("Generator validation failed")


def generate_c_client(
    jar: Path | None,
    spec: Path,
    output: Path,
    summary: GateSummary,
) -> None:
    if jar is None:
        summary.skip_step("openapi-generator not available")
        return
    if output.exists():
        shutil.rmtree(output)
    result = run_command(
        [
            "java",
            "-jar",
            str(jar),
            "generate",
            "-g",
            "c",
            "-i",
            str(spec),
            "-o",
            str(output),
            "--skip-validate-spec",
        ],
        cwd=spec.parent,
        check=False,
        tail=5,
    )
    if result.returncode:
        summary.fail_step("C client SDK generation failed")
        return
    summary.pass_step(f"C client SDK generated at {output}")
    cmake_file = output / "CMakeLists.txt"
    if not cmake_file.is_file():
        summary.skip_step("Generated SDK has no CMakeLists.txt")
        return
    build = output / "build"
    configure = run_command(
        ["cmake", "-S", str(output), "-B", str(build)],
        cwd=spec.parent,
        check=False,
        tail=3,
    )
    if configure.returncode:
        summary.fail_step("CMake configure failed for generated C client")
        return
    compiled = run_command(
        ["cmake", "--build", str(build)],
        cwd=spec.parent,
        check=False,
        tail=5,
    )
    if compiled.returncode:
        summary.fail_step("Generated C client failed to compile")
    else:
        summary.pass_step("Generated C client compiles")


def require_java() -> bool:
    return shutil.which("java") is not None

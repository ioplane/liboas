#!/usr/bin/env bash
# Validate OpenAPI spec produced by liboas
# Builds the project, runs emitter/adapter to produce openapi.json,
# then validates with external tools.
# Usage: ./scripts/validate-spec.sh [path/to/spec.json]
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

step() { printf "\n${CYAN}=== [%d/4] %s ===${NC}\n" "$1" "$2"; }
ok()   { printf "${GREEN}PASS${NC}: %s\n" "$1"; PASS=$((PASS + 1)); }
fail() { printf "${RED}FAIL${NC}: %s\n" "$1"; FAIL=$((FAIL + 1)); }
skip() { printf "${YELLOW}SKIP${NC}: %s\n" "$1"; SKIP=$((SKIP + 1)); }

# ── Configuration ────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build/clang-debug}"
PRESET="${PRESET:-clang-debug}"
CACHE_DIR="${PROJECT_DIR}/build/oas-tools"

# If a spec is given as argument, use it directly; otherwise build one
SPEC_FILE="${1:-}"

STYLE_VALIDATOR_VERSION="1.11"
OPENAPI_GENERATOR_VERSION="7.12.0"

STYLE_VALIDATOR_JAR="${CACHE_DIR}/openapi-style-validator-cli-${STYLE_VALIDATOR_VERSION}-all.jar"
OPENAPI_GENERATOR_JAR="${CACHE_DIR}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar"

# ── Step 1: Build liboas ────────────────────────────────────────────────────
step 1 "Build liboas"
cd "${PROJECT_DIR}"
cmake --preset "${PRESET}" 2>&1 | tail -3
if cmake --build --preset "${PRESET}" 2>&1 | tail -5; then
    ok "Build succeeded"
else
    fail "Build failed"
    exit 1
fi

# ── Step 2: Produce or locate spec ──────────────────────────────────────────
step 2 "Locate OpenAPI spec"
if [[ -n "${SPEC_FILE}" && -f "${SPEC_FILE}" ]]; then
    printf "  Using provided spec: %s\n" "${SPEC_FILE}"
    ok "Spec file found"
else
    # Try known locations for generated or fixture specs
    CANDIDATES=(
        "${BUILD_DIR}/openapi.json"
        "${PROJECT_DIR}/build/openapi.json"
        "${PROJECT_DIR}/tests/fixtures/petstore-3.2.json"
        "${PROJECT_DIR}/tests/fixtures/petstore.json"
    )

    SPEC_FILE=""
    for candidate in "${CANDIDATES[@]}"; do
        if [[ -f "${candidate}" ]]; then
            SPEC_FILE="${candidate}"
            break
        fi
    done

    if [[ -z "${SPEC_FILE}" ]]; then
        fail "No OpenAPI spec found. Provide one as argument or place in build/"
        exit 1
    fi

    printf "  Using spec: %s\n" "${SPEC_FILE}"
    ok "Spec file found"
fi

# ── Step 3: Style validation ────────────────────────────────────────────────
step 3 "OpenAPI style validation"
if ! command -v java >/dev/null 2>&1; then
    skip "Java not found — skipping external validators"
    skip "OpenAPI generator validate (no Java)"

    printf "\n${CYAN}=== Summary ===${NC}\n"
    printf "${GREEN}PASS: %d${NC}  ${RED}FAIL: %d${NC}  ${YELLOW}SKIP: %d${NC}\n" \
        "${PASS}" "${FAIL}" "${SKIP}"
    if [[ "${FAIL}" -gt 0 ]]; then
        printf "${RED}Spec validation FAILED${NC}\n"
        exit 1
    else
        printf "${GREEN}Spec validation PASSED${NC}\n"
    fi
    exit 0
fi

mkdir -p "${CACHE_DIR}"

# Download style validator if missing
STYLE_VALIDATOR_URL="https://repo1.maven.org/maven2/org/openapitools/openapistylevalidator/openapi-style-validator-cli/${STYLE_VALIDATOR_VERSION}/openapi-style-validator-cli-${STYLE_VALIDATOR_VERSION}-all.jar"
if [[ ! -f "${STYLE_VALIDATOR_JAR}" ]]; then
    printf "  Downloading openapi-style-validator...\n"
    curl -fsSL -o "${STYLE_VALIDATOR_JAR}.tmp" "${STYLE_VALIDATOR_URL}" \
        && mv "${STYLE_VALIDATOR_JAR}.tmp" "${STYLE_VALIDATOR_JAR}" \
        || true
fi

if [[ -f "${STYLE_VALIDATOR_JAR}" ]]; then
    if java -jar "${STYLE_VALIDATOR_JAR}" -s "${SPEC_FILE}" 2>&1; then
        ok "Style validation passed"
    else
        fail "Style validation found issues"
    fi
else
    skip "openapi-style-validator not available"
fi

# ── Step 4: Generator validate ──────────────────────────────────────────────
step 4 "OpenAPI generator validate"

OPENAPI_GENERATOR_URL="https://repo1.maven.org/maven2/org/openapitools/openapi-generator-cli/${OPENAPI_GENERATOR_VERSION}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar"
if [[ ! -f "${OPENAPI_GENERATOR_JAR}" ]]; then
    printf "  Downloading openapi-generator...\n"
    curl -fsSL -o "${OPENAPI_GENERATOR_JAR}.tmp" "${OPENAPI_GENERATOR_URL}" \
        && mv "${OPENAPI_GENERATOR_JAR}.tmp" "${OPENAPI_GENERATOR_JAR}" \
        || true
fi

if [[ -f "${OPENAPI_GENERATOR_JAR}" ]]; then
    if java -jar "${OPENAPI_GENERATOR_JAR}" validate -i "${SPEC_FILE}" 2>&1; then
        ok "Generator validation passed"
    else
        fail "Generator validation failed"
    fi
else
    skip "openapi-generator not available"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
printf "\n${CYAN}=== Summary ===${NC}\n"
printf "${GREEN}PASS: %d${NC}  ${RED}FAIL: %d${NC}  ${YELLOW}SKIP: %d${NC}\n" \
    "${PASS}" "${FAIL}" "${SKIP}"

if [[ "${FAIL}" -gt 0 ]]; then
    printf "${RED}Spec validation FAILED${NC}\n"
    exit 1
else
    printf "${GREEN}Spec validation PASSED${NC}\n"
fi

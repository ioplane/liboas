#!/usr/bin/env bash
# OpenAPI compatibility test: validate spec and generate C client SDK
# Usage: ./scripts/oas-compat-test.sh [path/to/openapi.json]
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

SPEC_FILE="${1:-${PROJECT_DIR}/tests/fixtures/petstore-3.2.json}"
CACHE_DIR="${PROJECT_DIR}/build/oas-tools"
GEN_OUTPUT="${PROJECT_DIR}/build/oas-gen-c"

# Tool versions (update as needed)
STYLE_VALIDATOR_VERSION="1.11"
OPENAPI_GENERATOR_VERSION="7.12.0"

STYLE_VALIDATOR_JAR="${CACHE_DIR}/openapi-style-validator-cli-${STYLE_VALIDATOR_VERSION}-all.jar"
OPENAPI_GENERATOR_JAR="${CACHE_DIR}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar"

STYLE_VALIDATOR_URL="https://repo1.maven.org/maven2/org/openapitools/openapistylevalidator/openapi-style-validator-cli/${STYLE_VALIDATOR_VERSION}/openapi-style-validator-cli-${STYLE_VALIDATOR_VERSION}-all.jar"
OPENAPI_GENERATOR_URL="https://repo1.maven.org/maven2/org/openapitools/openapi-generator-cli/${OPENAPI_GENERATOR_VERSION}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar"

# ── Preconditions ────────────────────────────────────────────────────────────
if [[ ! -f "${SPEC_FILE}" ]]; then
    printf "${RED}ERROR${NC}: Spec file not found: %s\n" "${SPEC_FILE}"
    exit 1
fi

printf "${CYAN}Spec file:${NC} %s\n" "${SPEC_FILE}"

# ── Step 1: Check Java ──────────────────────────────────────────────────────
step 1 "Check Java runtime"
if command -v java >/dev/null 2>&1; then
    JAVA_VER=$(java -version 2>&1 | head -1)
    ok "Java found: ${JAVA_VER}"
else
    fail "Java not found (required for openapi-style-validator and openapi-generator)"
    printf "Install: apt install default-jre / dnf install java-17-openjdk\n"
    exit 1
fi

# ── Step 2: Download JARs ───────────────────────────────────────────────────
step 2 "Download tool JARs (cached)"
mkdir -p "${CACHE_DIR}"

download_jar() {
    local url="$1"
    local dest="$2"
    local name="$3"

    if [[ -f "${dest}" ]]; then
        printf "  %s: cached\n" "${name}"
        return 0
    fi

    printf "  %s: downloading...\n" "${name}"
    if curl -fsSL -o "${dest}.tmp" "${url}"; then
        mv "${dest}.tmp" "${dest}"
        printf "  %s: downloaded\n" "${name}"
    else
        rm -f "${dest}.tmp"
        printf "  %s: download failed\n" "${name}"
        return 1
    fi
}

HAVE_STYLE=true
HAVE_GENERATOR=true

if ! download_jar "${STYLE_VALIDATOR_URL}" "${STYLE_VALIDATOR_JAR}" "openapi-style-validator"; then
    HAVE_STYLE=false
fi

if ! download_jar "${OPENAPI_GENERATOR_URL}" "${OPENAPI_GENERATOR_JAR}" "openapi-generator"; then
    HAVE_GENERATOR=false
fi

if "${HAVE_STYLE}" || "${HAVE_GENERATOR}"; then
    ok "Tool JARs ready"
else
    fail "No tools available"
    exit 1
fi

# ── Step 3: Style validation ────────────────────────────────────────────────
step 3 "OpenAPI style validation"
if "${HAVE_STYLE}"; then
    if java -jar "${STYLE_VALIDATOR_JAR}" -s "${SPEC_FILE}" 2>&1; then
        ok "Style validation passed"
    else
        fail "Style validation found issues"
    fi
else
    skip "openapi-style-validator not available"
fi

# ── Step 4: Generator validation + C SDK ────────────────────────────────────
step 4 "OpenAPI generator: validate + generate C client"
if "${HAVE_GENERATOR}"; then
    # Validate spec
    printf "  Validating spec...\n"
    if java -jar "${OPENAPI_GENERATOR_JAR}" validate -i "${SPEC_FILE}" 2>&1; then
        ok "Generator validation passed"
    else
        fail "Generator validation failed"
    fi

    # Generate C client SDK
    printf "  Generating C client SDK...\n"
    rm -rf "${GEN_OUTPUT}"
    if java -jar "${OPENAPI_GENERATOR_JAR}" generate \
        -g c \
        -i "${SPEC_FILE}" \
        -o "${GEN_OUTPUT}" \
        --skip-validate-spec \
        2>&1 | tail -5; then
        ok "C client SDK generated at ${GEN_OUTPUT}"

        # Attempt to compile the generated SDK
        if [[ -f "${GEN_OUTPUT}/CMakeLists.txt" ]]; then
            printf "  Compiling generated C client...\n"
            GEN_BUILD="${GEN_OUTPUT}/build"
            mkdir -p "${GEN_BUILD}"
            if cmake -S "${GEN_OUTPUT}" -B "${GEN_BUILD}" 2>&1 | tail -3; then
                if cmake --build "${GEN_BUILD}" 2>&1 | tail -5; then
                    ok "Generated C client compiles"
                else
                    fail "Generated C client failed to compile"
                fi
            else
                fail "CMake configure failed for generated C client"
            fi
        else
            skip "Generated SDK has no CMakeLists.txt — skipping compile"
        fi
    else
        fail "C client SDK generation failed"
    fi
else
    skip "openapi-generator not available"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
printf "\n${CYAN}=== Summary ===${NC}\n"
printf "${GREEN}PASS: %d${NC}  ${RED}FAIL: %d${NC}  ${YELLOW}SKIP: %d${NC}\n" \
    "${PASS}" "${FAIL}" "${SKIP}"

if [[ "${FAIL}" -gt 0 ]]; then
    printf "${RED}Compatibility test FAILED${NC}\n"
    exit 1
else
    printf "${GREEN}Compatibility test PASSED${NC}\n"
fi

#!/usr/bin/env bash
# shellcheck shell=bash
# Repository quality pipeline for liboas.
# Run inside the dev container: cd /workspace && ./scripts/quality.sh
# Or from host: podman run --rm --security-opt seccomp=unconfined \
#   --env-file .env \
#   -v /opt/projects/repositories/liboas:/workspace:Z \
#   localhost/liboas-dev:latest bash -c "cd /workspace && ./scripts/quality.sh"
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR

# shellcheck disable=SC1091
if [[ -f /usr/local/lib/ioplane/common.sh ]]; then
    source /usr/local/lib/ioplane/common.sh
else
    source "${SCRIPT_DIR}/lib/common.sh"
fi

ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd -P)"
readonly ROOT_DIR
readonly BUILD_DIR="${BUILD_DIR:-build/clang-debug}"
readonly PRESET="${PRESET:-clang-debug}"
readonly VENDOR_DIR="${ROOT_DIR}/vendor"
readonly TOTAL_STEPS=10

cd "${ROOT_DIR}"

# ═══════════════════════════════════════════════════════
# Step 1: Repository baseline
# ═══════════════════════════════════════════════════════

ioj_step 1 "${TOTAL_STEPS}" "Repository baseline"
ioj_check_repo_baseline

# ═══════════════════════════════════════════════════════
# Step 2: Documentation lint
# ═══════════════════════════════════════════════════════

ioj_step 2 "${TOTAL_STEPS}" "Documentation lint"
ioj_check_docs_lint

# ═══════════════════════════════════════════════════════
# Step 3: Configure and build
# ═══════════════════════════════════════════════════════

ioj_step 3 "${TOTAL_STEPS}" "Configure and build"
ioj_check_build_and_test "${PRESET}" "${BUILD_DIR}"

# ═══════════════════════════════════════════════════════
# Step 4: Format check
# ═══════════════════════════════════════════════════════

ioj_step 4 "${TOTAL_STEPS}" "Format check"
ioj_check_format "${PRESET}"

# ═══════════════════════════════════════════════════════
# Step 5: cppcheck (with vendor/ exclusion)
# ═══════════════════════════════════════════════════════

ioj_step 5 "${TOTAL_STEPS}" "cppcheck"
if ioj_has_cmake_surface; then
    if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
        ioj_record_fail "cppcheck: compile database missing"
    elif ! command -v cppcheck >/dev/null 2>&1; then
        ioj_record_skip "cppcheck not installed"
    else
        if cppcheck --enable=warning,performance,portability \
            --error-exitcode=1 --inline-suppr \
            --project="${BUILD_DIR}/compile_commands.json" \
            --suppress='*:/usr/local/src/unity/*' \
            -i vendor/ \
            -q 2>&1; then
            ioj_record_pass "cppcheck clean"
        else
            ioj_record_fail "cppcheck found issues"
        fi
    fi
else
    ioj_record_skip "Compile database unavailable before CMake bootstrap"
fi

# ═══════════════════════════════════════════════════════
# Step 6: PVS-Studio (with vendor/ exclusion)
# ═══════════════════════════════════════════════════════

ioj_step 6 "${TOTAL_STEPS}" "PVS-Studio"
if ioj_has_cmake_surface; then
    if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
        ioj_record_fail "PVS-Studio: compile database missing"
    elif ! command -v pvs-studio-analyzer >/dev/null 2>&1; then
        ioj_record_skip "PVS-Studio not installed"
    elif [[ -z "${PVS_NAME:-}" || -z "${PVS_KEY:-}" ]]; then
        ioj_record_skip "PVS-Studio: missing PVS_NAME/PVS_KEY in environment"
    else
        nproc_val="$(ioj_nproc)"
        pvs-studio-analyzer credentials "${PVS_NAME}" "${PVS_KEY}" >/dev/null 2>&1

        pvs_log="${BUILD_DIR}/pvs-studio.log"

        if pvs-studio-analyzer analyze \
            -f "${BUILD_DIR}/compile_commands.json" \
            -o "${pvs_log}" \
            -e /usr/local/src/unity/ \
            -e "${VENDOR_DIR}/" \
            -j"${nproc_val}" 2>&1 | grep -v '^\['; then

            pvs_out=$(plog-converter -t errorfile -a 'GA:1,2' "${pvs_log}" 2>/dev/null \
                | grep -v '^pvs-studio.com' | grep -v '^Analyzer log' \
                | grep -v '^PVS-Studio is' | grep -v '^$' \
                | grep -v 'Total messages' | grep -v 'Filtered messages' \
                | grep -v '^Copyright' \
                | grep -v 'V1042' || true)

            pvs_count=$(printf '%s\n' "${pvs_out}" | grep -cE '(error|warning):' || true)

            if [[ "${pvs_count}" -eq 0 ]]; then
                ioj_record_pass "PVS-Studio clean (GA:1,2)"
            else
                printf '%s\n' "${pvs_out}"
                ioj_record_fail "PVS-Studio: ${pvs_count} errors/warnings"
            fi
        else
            ioj_record_fail "PVS-Studio analysis failed"
        fi
    fi
else
    ioj_record_skip "Compile database unavailable before CMake bootstrap"
fi

# ═══════════════════════════════════════════════════════
# Step 7: CodeChecker (with vendor/ exclusion)
# ═══════════════════════════════════════════════════════

ioj_step 7 "${TOTAL_STEPS}" "CodeChecker"
if ioj_has_cmake_surface; then
    if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
        ioj_record_fail "CodeChecker: compile database missing"
    elif ! command -v CodeChecker >/dev/null 2>&1; then
        ioj_record_skip "CodeChecker not installed"
    else
        nproc_val="$(ioj_nproc)"
        cc_dir="$(ioj_mktemp -d)"
        cc_skip="$(ioj_mktemp)"

        cat > "${cc_skip}" <<'SKIP'
-/usr/local/src/unity/*
-*/vendor/*
SKIP

        if CodeChecker analyze "${BUILD_DIR}/compile_commands.json" \
            -o "${cc_dir}" \
            --analyzers clangsa clang-tidy \
            --skip "${cc_skip}" \
            -j"${nproc_val}"; then

            cc_parse=$(CodeChecker parse "${cc_dir}" \
                --trim-path-prefix "$(pwd)/" 2>&1 || true)
            cc_parse=$(printf '%s\n' "${cc_parse}" \
                | grep -v '^\[INFO\]' \
                | grep -v '^$' \
                | grep -v '/usr/local/src/unity/' || true)

            cc_high=$(printf '%s\n' "${cc_parse}" | grep -c '\[HIGH\]' || true)
            cc_med=$(printf '%s\n' "${cc_parse}" | grep -c '\[MEDIUM\]' || true)

            if [[ "${cc_high}" -gt 0 || "${cc_med}" -gt 0 ]]; then
                printf '%s\n' "${cc_parse}" | grep -E '\[(HIGH|MEDIUM)\]' || true
                ioj_record_fail "CodeChecker: ${cc_high} HIGH, ${cc_med} MEDIUM"
            else
                ioj_record_pass "CodeChecker clean (no HIGH/MEDIUM)"
            fi
        else
            ioj_record_fail "CodeChecker analysis failed"
        fi
    fi
else
    ioj_record_skip "Compile database unavailable before CMake bootstrap"
fi

# ═══════════════════════════════════════════════════════
# Step 8: GCC analyzer
# ═══════════════════════════════════════════════════════

ioj_step 8 "${TOTAL_STEPS}" "GCC analyzer"
ioj_check_gcc_analyzer

# ═══════════════════════════════════════════════════════
# Step 9: shellcheck
# ═══════════════════════════════════════════════════════

ioj_step 9 "${TOTAL_STEPS}" "Shellcheck"
ioj_check_shellcheck

# ═══════════════════════════════════════════════════════
# Step 10: Vendor directory integrity
# ═══════════════════════════════════════════════════════

ioj_step 10 "${TOTAL_STEPS}" "Vendor directory integrity"
if [[ -d vendor/ ]]; then
    ioj_record_pass "Vendor directory present"
else
    ioj_record_skip "No vendor directory"
fi

# ═══════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════

ioj_print_summary

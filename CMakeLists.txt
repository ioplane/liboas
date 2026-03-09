cmake_minimum_required(VERSION 3.25)

project(liboas
    VERSION 0.1.0
    DESCRIPTION "OpenAPI 3.2 library for C23"
    LANGUAGES C
)

# ── C23 Standard ─────────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Compile commands for IDE and static analysis ─────────────────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Common compile options ───────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wformat=2
    -Wconversion -Wsign-conversion
    -Wshadow -Wundef
    -Wstrict-prototypes -Wmissing-prototypes
    -Wnull-dereference -Wdouble-promotion
    -fstack-protector-strong
)

# GCC-specific warnings
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wformat-overflow=2 -Wformat-truncation=2
    )
endif()

# _FORTIFY_SOURCE only with optimization
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-D_FORTIFY_SOURCE=3)
endif()

# ── Hardening flags ──────────────────────────────────────────────────────────
add_link_options(-Wl,-z,relro -Wl,-z,now)

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)

# yyjson (required)
pkg_check_modules(YYJSON REQUIRED yyjson)

# ── Vendored libregexp (QuickJS) for ECMA-262 regex ───────────────────────
add_library(libregexp STATIC
    vendor/libregexp/libregexp.c
    vendor/libregexp/libunicode.c
)
target_include_directories(libregexp PUBLIC ${CMAKE_SOURCE_DIR}/vendor/libregexp)
target_compile_options(libregexp PRIVATE -w)
target_compile_definitions(libregexp PRIVATE _GNU_SOURCE)

# ── Unity test framework ────────────────────────────────────────────────────
set(UNITY_SRC "/usr/local/src/unity")
set(UNITY_INC "/usr/local/include/unity")
if(EXISTS "${UNITY_SRC}/unity.c")
    add_library(unity STATIC "${UNITY_SRC}/unity.c")
    target_include_directories(unity PUBLIC "${UNITY_INC}")
    target_compile_options(unity PRIVATE -w)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
    set(UNITY_FOUND TRUE)
elseif(EXISTS "${UNITY_SRC}/src/unity.c")
    add_library(unity STATIC "${UNITY_SRC}/src/unity.c")
    target_include_directories(unity PUBLIC "${UNITY_SRC}/src")
    target_compile_options(unity PRIVATE -w)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
    set(UNITY_FOUND TRUE)
else()
    message(STATUS "Unity not found — tests disabled")
    set(UNITY_FOUND FALSE)
endif()

# ── Test macro ───────────────────────────────────────────────────────────────
macro(oas_add_test TEST_NAME TEST_SRC)
    if(UNITY_FOUND AND BUILD_TESTING)
        add_executable(${TEST_NAME} ${TEST_SRC})
        target_link_libraries(${TEST_NAME} PRIVATE unity ${ARGN})
        target_include_directories(${TEST_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        )
        target_compile_definitions(${TEST_NAME} PRIVATE _GNU_SOURCE)
        target_compile_options(${TEST_NAME} PRIVATE
            -Wno-missing-prototypes
        )
        set_target_properties(${TEST_NAME} PROPERTIES
            POSITION_INDEPENDENT_CODE OFF
        )
        target_link_options(${TEST_NAME} PRIVATE -no-pie)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        set_tests_properties(${TEST_NAME} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()
endmacro()

# ── Source files ─────────────────────────────────────────────────────────────

# Core library: arena allocator, error handling, string interning, regex
add_library(oas_core STATIC
    src/core/oas_alloc.c
    src/core/oas_error.c
    src/core/oas_intern.c
    src/core/oas_jsonptr.c
    src/core/oas_path_match.c
    src/core/oas_regex_libregexp.c
)
target_include_directories(oas_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_core PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(oas_core PUBLIC _GNU_SOURCE)
target_include_directories(oas_core PUBLIC ${YYJSON_INCLUDE_DIRS})
target_link_directories(oas_core PUBLIC ${YYJSON_LIBRARY_DIRS})
target_link_libraries(oas_core PUBLIC ${YYJSON_LIBRARIES} libregexp)

# Model library: schema and document types
add_library(oas_model STATIC
    src/model/oas_schema.c
    src/model/oas_doc.c
    src/model/oas_schema_builder.c
    src/model/oas_doc_builder.c
)
target_include_directories(oas_model PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_model PUBLIC oas_core)

# Parser library: JSON wrapper, schema parser, doc parser (yyjson)
add_library(oas_parser STATIC
    src/parser/oas_json.c
    src/parser/oas_schema_parser.c
    src/parser/oas_doc_parser.c
    src/parser/oas_ref.c
)
target_include_directories(oas_parser PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_parser PUBLIC oas_core oas_model)

# Compiler library: format validators, instruction set, schema compiler
add_library(oas_compiler STATIC
    src/compiler/oas_format.c
    src/compiler/oas_instruction.c
    src/compiler/oas_compiler.c
    src/compiler/oas_doc_compiler.c
)
target_include_directories(oas_compiler PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_compiler PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_compiler PUBLIC oas_core oas_model)

# Validator library: validation VM executing compiled bytecode
add_library(oas_validator STATIC
    src/validator/oas_validator.c
    src/validator/oas_request.c
)
target_include_directories(oas_validator PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_validator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_validator PUBLIC oas_compiler oas_core)
target_link_libraries(oas_validator PRIVATE m)

# ── Optional YAML support via libfyaml ──────────────────────────────────────
option(OAS_YAML "Enable YAML 1.2 parsing via libfyaml" OFF)
if(OAS_YAML)
    pkg_check_modules(LIBFYAML REQUIRED libfyaml)
    add_library(oas_yaml STATIC
        src/parser/oas_yaml.c
    )
    target_include_directories(oas_yaml PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(oas_yaml PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_include_directories(oas_yaml PUBLIC ${LIBFYAML_INCLUDE_DIRS})
    target_link_directories(oas_yaml PUBLIC ${LIBFYAML_LIBRARY_DIRS})
    target_link_libraries(oas_yaml PUBLIC oas_core ${LIBFYAML_LIBRARIES})
    target_compile_definitions(oas_yaml PRIVATE _GNU_SOURCE)
endif()

# Emitter library: JSON/YAML emission
add_library(oas_emitter STATIC
    src/emitter/oas_emitter.c
)
target_include_directories(oas_emitter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_emitter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_emitter PUBLIC oas_core oas_model)

# Adapter library: unified validation facade + Scalar UI
add_library(oas_adapter STATIC
    src/adapter/oas_adapter.c
    src/adapter/oas_scalar.c
)
target_include_directories(oas_adapter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_adapter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_adapter PUBLIC oas_validator oas_parser oas_emitter oas_model oas_core)

# ── Code quality targets ────────────────────────────────────────────────────

# clang-format
file(GLOB_RECURSE ALL_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/tests/*.c"
)

add_custom_target(format
    COMMAND clang-format -i ${ALL_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format on all sources"
    VERBATIM
)

add_custom_target(format-check
    COMMAND clang-format --dry-run --Werror ${ALL_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking clang-format compliance"
    VERBATIM
)

# cppcheck
add_custom_target(cppcheck
    COMMAND cppcheck
        --enable=warning,performance,portability
        --error-exitcode=1
        --inline-suppr
        --project=${CMAKE_BINARY_DIR}/compile_commands.json
        --suppress='*:/usr/local/src/unity/*'
        --suppress='*:${CMAKE_SOURCE_DIR}/vendor/*'
        -q
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running cppcheck static analysis"
    VERBATIM
)

# PVS-Studio (compile_commands.json approach — no cmake module needed)
find_program(PVS_STUDIO pvs-studio-analyzer)
find_program(PLOG_CONVERTER plog-converter)
if(PVS_STUDIO AND PLOG_CONVERTER)
    add_custom_target(pvs-studio
        COMMAND ${PVS_STUDIO} analyze
            -f ${CMAKE_BINARY_DIR}/compile_commands.json
            -o ${CMAKE_BINARY_DIR}/pvs-studio.log
            -e /usr/local/src/unity/
            -e ${CMAKE_SOURCE_DIR}/vendor/
            -j 4
        COMMAND ${PLOG_CONVERTER}
            -t errorfile
            -a "GA:1,2"
            ${CMAKE_BINARY_DIR}/pvs-studio.log
        COMMENT "Running PVS-Studio analysis (GA errors + warnings)"
        VERBATIM
    )
endif()

# Doxygen
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation"
        VERBATIM
    )
endif()

# ── Testing ──────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    enable_testing()
    oas_add_test(test_alloc tests/unit/test_alloc.c oas_core)
    oas_add_test(test_error tests/unit/test_error.c oas_core)
    oas_add_test(test_intern tests/unit/test_intern.c oas_core)
    oas_add_test(test_json tests/unit/test_json.c oas_parser)
    oas_add_test(test_schema tests/unit/test_schema.c oas_model)
    oas_add_test(test_schema_parser tests/unit/test_schema_parser.c oas_parser)
    oas_add_test(test_doc tests/unit/test_doc.c oas_model)
    oas_add_test(test_doc_parser tests/unit/test_doc_parser.c oas_parser)
    oas_add_test(test_jsonptr tests/unit/test_jsonptr.c oas_core)
    oas_add_test(test_ref tests/unit/test_ref.c oas_parser)
    oas_add_test(test_regex tests/unit/test_regex.c oas_core)
    oas_add_test(test_format tests/unit/test_format.c oas_compiler)
    oas_add_test(test_instruction tests/unit/test_instruction.c oas_compiler)
    oas_add_test(test_compiler tests/unit/test_compiler.c oas_compiler)
    oas_add_test(test_path_match tests/unit/test_path_match.c oas_core)
    oas_add_test(test_validator tests/unit/test_validator.c oas_validator)
    oas_add_test(test_request tests/unit/test_request.c oas_validator oas_parser)
    oas_add_test(test_emitter tests/unit/test_emitter.c oas_emitter)
    oas_add_test(test_builder tests/unit/test_builder.c oas_model oas_emitter)
    oas_add_test(test_adapter tests/unit/test_adapter.c oas_adapter oas_model)
    if(OAS_YAML)
        oas_add_test(test_yaml tests/unit/test_yaml.c oas_yaml)
    endif()

    # Integration tests
    oas_add_test(test_petstore tests/integration/test_petstore.c oas_adapter oas_model)
endif()

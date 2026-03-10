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

# ── Version header generation ────────────────────────────────────────────────
configure_file(
    include/liboas/oas_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/liboas/oas_version.h
    @ONLY
)

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
            ${CMAKE_CURRENT_BINARY_DIR}/include
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
    src/core/oas_query.c
    src/core/oas_problem.c
    src/core/oas_negotiate.c
    src/core/oas_version.c
)
target_include_directories(oas_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)
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
    src/parser/oas_ref_cache.c
    src/parser/oas_ref_file.c
    src/parser/oas_ref_fetch.c
    src/parser/oas_uri.c
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
set(OAS_EMITTER_SOURCES src/emitter/oas_emitter.c)
if(OAS_YAML)
    list(APPEND OAS_EMITTER_SOURCES src/emitter/oas_yaml_emitter.c)
endif()
add_library(oas_emitter STATIC ${OAS_EMITTER_SOURCES})
target_include_directories(oas_emitter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_emitter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_emitter PUBLIC oas_core oas_model)
if(OAS_YAML)
    target_include_directories(oas_emitter PUBLIC ${LIBFYAML_INCLUDE_DIRS})
    target_link_directories(oas_emitter PUBLIC ${LIBFYAML_LIBRARY_DIRS})
    target_link_libraries(oas_emitter PUBLIC ${LIBFYAML_LIBRARIES})
    target_compile_definitions(oas_emitter PUBLIC OAS_YAML)
endif()

# Adapter library: unified validation facade + Scalar UI
add_library(oas_adapter STATIC
    src/adapter/oas_adapter.c
    src/adapter/oas_scalar.c
)
target_include_directories(oas_adapter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_adapter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_adapter PUBLIC oas_validator oas_parser oas_emitter oas_model oas_core)

# ── Combined shared library for installation ─────────────────────────────────
include(GNUInstallDirs)

add_library(oas STATIC
    src/core/oas_alloc.c
    src/core/oas_error.c
    src/core/oas_intern.c
    src/core/oas_jsonptr.c
    src/core/oas_path_match.c
    src/core/oas_regex_libregexp.c
    src/core/oas_query.c
    src/core/oas_problem.c
    src/core/oas_negotiate.c
    src/core/oas_version.c
    src/model/oas_schema.c
    src/model/oas_doc.c
    src/model/oas_schema_builder.c
    src/model/oas_doc_builder.c
    src/parser/oas_json.c
    src/parser/oas_schema_parser.c
    src/parser/oas_doc_parser.c
    src/parser/oas_ref.c
    src/parser/oas_ref_cache.c
    src/parser/oas_ref_file.c
    src/parser/oas_ref_fetch.c
    src/parser/oas_uri.c
    src/compiler/oas_format.c
    src/compiler/oas_instruction.c
    src/compiler/oas_compiler.c
    src/compiler/oas_doc_compiler.c
    src/validator/oas_validator.c
    src/validator/oas_request.c
    src/emitter/oas_emitter.c
    src/adapter/oas_adapter.c
    src/adapter/oas_scalar.c
    vendor/libregexp/libregexp.c
    vendor/libregexp/libunicode.c
)
set_target_properties(oas PROPERTIES
    OUTPUT_NAME oas
)
set_source_files_properties(
    vendor/libregexp/libregexp.c
    vendor/libregexp/libunicode.c
    TARGET_DIRECTORY oas
    PROPERTIES COMPILE_OPTIONS "-w"
)
target_include_directories(oas PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(oas PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/libregexp
)
target_compile_definitions(oas PRIVATE _GNU_SOURCE)
target_include_directories(oas PUBLIC ${YYJSON_INCLUDE_DIRS})
target_link_directories(oas PUBLIC ${YYJSON_LIBRARY_DIRS})
target_link_libraries(oas PUBLIC ${YYJSON_LIBRARIES})
target_link_libraries(oas PRIVATE m)

# ── Install rules ────────────────────────────────────────────────────────────
install(TARGETS oas
    EXPORT liboasTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/liboas/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/liboas
    FILES_MATCHING PATTERN "*.h"
    PATTERN "*.h.in" EXCLUDE
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/liboas/oas_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/liboas
)

install(EXPORT liboasTargets
    FILE liboasTargets.cmake
    NAMESPACE liboas::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liboas
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/liboasConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/liboasConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liboas
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/liboasConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/liboasConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/liboasConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liboas
)

# ── pkg-config ───────────────────────────────────────────────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/liboas.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/liboas.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/liboas.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

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

# ── OpenAPI compatibility checks (optional, requires Java) ──────────────────
find_program(JAVA_EXECUTABLE java)
if(JAVA_EXECUTABLE)
    set(OAS_TOOLS_DIR "${CMAKE_BINARY_DIR}/oas-tools")
    set(OAS_TEST_SPEC "${CMAKE_SOURCE_DIR}/tests/fixtures/petstore-3.2.json")

    set(STYLE_VALIDATOR_VERSION "1.11")
    set(OPENAPI_GENERATOR_VERSION "7.12.0")
    set(STYLE_VALIDATOR_JAR
        "${OAS_TOOLS_DIR}/openapi-style-validator-cli-${STYLE_VALIDATOR_VERSION}-all.jar")
    set(OPENAPI_GENERATOR_JAR
        "${OAS_TOOLS_DIR}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar")

    set(STYLE_VALIDATOR_URL
        "https://repo1.maven.org/maven2/org/openapitools/openapistylevalidator/openapi-style-validator-cli/${STYLE_VALIDATOR_VERSION}/openapi-style-validator-cli-${STYLE_VALIDATOR_VERSION}-all.jar")
    set(OPENAPI_GENERATOR_URL
        "https://repo1.maven.org/maven2/org/openapitools/openapi-generator-cli/${OPENAPI_GENERATOR_VERSION}/openapi-generator-cli-${OPENAPI_GENERATOR_VERSION}.jar")

    # Download JARs at build time (only if missing)
    add_custom_command(
        OUTPUT "${STYLE_VALIDATOR_JAR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OAS_TOOLS_DIR}"
        COMMAND curl -fsSL -o "${STYLE_VALIDATOR_JAR}" "${STYLE_VALIDATOR_URL}"
        COMMENT "Downloading openapi-style-validator ${STYLE_VALIDATOR_VERSION}"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${OPENAPI_GENERATOR_JAR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OAS_TOOLS_DIR}"
        COMMAND curl -fsSL -o "${OPENAPI_GENERATOR_JAR}" "${OPENAPI_GENERATOR_URL}"
        COMMENT "Downloading openapi-generator ${OPENAPI_GENERATOR_VERSION}"
        VERBATIM
    )

    add_custom_target(oas-style-check
        DEPENDS "${STYLE_VALIDATOR_JAR}"
        COMMAND ${JAVA_EXECUTABLE} -jar "${STYLE_VALIDATOR_JAR}" -s "${OAS_TEST_SPEC}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running openapi-style-validator on ${OAS_TEST_SPEC}"
        VERBATIM
    )

    add_custom_target(oas-compat-check
        DEPENDS "${OPENAPI_GENERATOR_JAR}"
        COMMAND ${JAVA_EXECUTABLE} -jar "${OPENAPI_GENERATOR_JAR}"
            validate -i "${OAS_TEST_SPEC}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running openapi-generator validate on ${OAS_TEST_SPEC}"
        VERBATIM
    )
else()
    message(STATUS "Java not found — oas-style-check and oas-compat-check targets disabled")
endif()

# ── Testing ──────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    enable_testing()
    oas_add_test(test_version tests/unit/test_version.c oas_core)
    oas_add_test(test_alloc tests/unit/test_alloc.c oas_core)
    oas_add_test(test_error tests/unit/test_error.c oas_core)
    oas_add_test(test_intern tests/unit/test_intern.c oas_core)
    oas_add_test(test_json tests/unit/test_json.c oas_parser)
    oas_add_test(test_schema tests/unit/test_schema.c oas_model)
    oas_add_test(test_schema_parser tests/unit/test_schema_parser.c oas_parser)
    oas_add_test(test_doc tests/unit/test_doc.c oas_model)
    oas_add_test(test_doc_parser tests/unit/test_doc_parser.c oas_parser)
    oas_add_test(test_jsonptr tests/unit/test_jsonptr.c oas_core)
    oas_add_test(test_uri tests/unit/test_uri.c oas_parser)
    oas_add_test(test_ref tests/unit/test_ref.c oas_parser)
    oas_add_test(test_ref_cache tests/unit/test_ref_cache.c oas_parser)
    oas_add_test(test_ref_file tests/unit/test_ref_file.c oas_parser)
    oas_add_test(test_ref_fetch tests/unit/test_ref_fetch.c oas_parser)
    oas_add_test(test_ref_conditional tests/unit/test_ref_conditional.c oas_parser)
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
    oas_add_test(test_doc_compiler tests/unit/test_doc_compiler.c oas_validator oas_parser)
    oas_add_test(test_schema_keywords tests/unit/test_schema_keywords.c oas_validator oas_parser)
    oas_add_test(test_doc_ref_integration tests/unit/test_doc_ref_integration.c oas_parser)
    oas_add_test(test_framework_core tests/unit/test_framework_core.c oas_adapter oas_model)
    if(OAS_YAML)
        oas_add_test(test_yaml tests/unit/test_yaml.c oas_yaml)
        oas_add_test(test_yaml_emitter tests/unit/test_yaml_emitter.c oas_emitter)
    endif()

    # Integration tests
    oas_add_test(test_petstore tests/integration/test_petstore.c oas_adapter oas_model)

    # LibFuzzer targets (Clang only)
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        macro(oas_add_fuzzer FUZZ_NAME FUZZ_SRC)
            add_executable(${FUZZ_NAME} ${FUZZ_SRC})
            target_link_libraries(${FUZZ_NAME} PRIVATE ${ARGN})
            target_include_directories(${FUZZ_NAME} PRIVATE
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src
            )
            target_compile_definitions(${FUZZ_NAME} PRIVATE _GNU_SOURCE)
            target_compile_options(${FUZZ_NAME} PRIVATE
                -fsanitize=fuzzer -Wno-missing-prototypes
            )
            target_link_options(${FUZZ_NAME} PRIVATE -fsanitize=fuzzer -no-pie)
            set_target_properties(${FUZZ_NAME} PROPERTIES
                POSITION_INDEPENDENT_CODE OFF
            )
        endmacro()

        oas_add_fuzzer(fuzz_json_parse tests/fuzz/fuzz_json_parse.c oas_parser)
        oas_add_fuzzer(fuzz_schema_parse tests/fuzz/fuzz_schema_parse.c oas_parser)
        oas_add_fuzzer(fuzz_regex tests/fuzz/fuzz_regex.c oas_core)
        oas_add_fuzzer(fuzz_format tests/fuzz/fuzz_format.c oas_compiler)
        oas_add_fuzzer(fuzz_emitter tests/fuzz/fuzz_emitter.c oas_emitter oas_parser)
        oas_add_fuzzer(fuzz_jsonptr tests/fuzz/fuzz_jsonptr.c oas_core)
        oas_add_fuzzer(fuzz_path_match tests/fuzz/fuzz_path_match.c oas_core)
        oas_add_fuzzer(fuzz_validator tests/fuzz/fuzz_validator.c oas_validator)
        oas_add_fuzzer(fuzz_compiler tests/fuzz/fuzz_compiler.c oas_compiler oas_parser)
        oas_add_fuzzer(fuzz_doc_compiler tests/fuzz/fuzz_doc_compiler.c oas_compiler oas_parser)
        oas_add_fuzzer(fuzz_ref_resolve tests/fuzz/fuzz_ref_resolve.c oas_parser)
        oas_add_fuzzer(fuzz_doc_parse tests/fuzz/fuzz_doc_parse.c oas_parser)
    endif()
endif()

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

# PCRE2 (optional, for pattern validation)
pkg_check_modules(PCRE2 pcre2-8)

# ── Unity test framework ────────────────────────────────────────────────────
set(UNITY_SRC "/usr/local/src/unity")
set(UNITY_INC "/usr/local/include/unity")
if(EXISTS "${UNITY_SRC}/unity.c")
    add_library(unity STATIC "${UNITY_SRC}/unity.c")
    target_include_directories(unity PUBLIC "${UNITY_INC}")
    target_compile_options(unity PRIVATE -w)
    set(UNITY_FOUND TRUE)
elseif(EXISTS "${UNITY_SRC}/src/unity.c")
    add_library(unity STATIC "${UNITY_SRC}/src/unity.c")
    target_include_directories(unity PUBLIC "${UNITY_SRC}/src")
    target_compile_options(unity PRIVATE -w)
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
    endif()
endmacro()

# ── Source files ─────────────────────────────────────────────────────────────

# Core library: arena allocator, error handling, string interning
add_library(oas_core STATIC
    src/core/oas_alloc.c
    src/core/oas_error.c
    src/core/oas_intern.c
)
target_include_directories(oas_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_core PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(oas_core PUBLIC _GNU_SOURCE)

# Parser library: JSON wrapper (yyjson)
add_library(oas_parser STATIC
    src/parser/oas_json.c
)
target_include_directories(oas_parser PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(oas_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(oas_parser PUBLIC oas_core)
target_include_directories(oas_parser PUBLIC ${YYJSON_INCLUDE_DIRS})
target_link_directories(oas_parser PUBLIC ${YYJSON_LIBRARY_DIRS})
target_link_libraries(oas_parser PUBLIC ${YYJSON_LIBRARIES})

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
        -q
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running cppcheck static analysis"
    VERBATIM
)

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
endif()

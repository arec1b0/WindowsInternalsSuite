cmake_minimum_required(VERSION 3.25)

project(WindowsInternalsSuite
    VERSION 0.1.0
    DESCRIPTION "Windows internals analysis suite (single Sysinternals-style app)"
    LANGUAGES CXX
)

# This project targets Windows 10/11 x64 exclusively.
if(NOT WIN32)
    message(FATAL_ERROR "WindowsInternalsSuite targets Windows 10/11 x64 only.")
endif()

# --- Global language standard ----------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Predictable output layout (reproducibility) ---------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Default to Debug on single-config generators (Ninja).
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Export compile commands for tooling (clang-tidy, editors).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Build options ----------------------------------------------------------
option(WIS_BUILD_TESTS         "Build unit tests"                       ON)
option(WIS_WARNINGS_AS_ERRORS  "Treat compiler warnings as errors"      OFF)
option(WIS_ENABLE_CLANG_TIDY   "Run clang-tidy during build (Ninja)"    OFF)
option(WIS_ENABLE_CAPSTONE     "Enable Capstone-based disassembler"     OFF)

# --- Shared cmake modules ---------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(CompilerWarnings)
include(StaticAnalysis)
include(Dependencies)

# INTERFACE targets carrying project-wide settings.
# Every real target links these; keeps flags in one place (DRY, DIP-friendly).
add_library(wis_project_options INTERFACE)
wis_apply_compiler_options(wis_project_options)

add_library(wis_project_warnings INTERFACE)
wis_apply_compiler_warnings(wis_project_warnings)

# --- Subdirectories (added incrementally as modules land) ------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
else()
    message(STATUS "src/ not present yet; configuring build foundation only.")
endif()

if(WIS_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

include(Packaging)
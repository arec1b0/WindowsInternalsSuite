# Optional clang-tidy integration.
# NOTE: CMAKE_CXX_CLANG_TIDY is honored only by Ninja/Makefile generators.
# The Visual Studio generator ignores it; run tidy via a dedicated target
# (added in a later step) when building with VS.

if(WIS_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
            CACHE STRING "clang-tidy command line" FORCE)
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "WIS_ENABLE_CLANG_TIDY=ON but clang-tidy was not found on PATH.")
    endif()
endif()
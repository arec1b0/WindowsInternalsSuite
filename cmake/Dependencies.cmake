# Third-party dependencies. All optional.
# The core suite depends only on the Windows SDK (Win32, ntdll, dbghelp).

include(FetchContent)

# GoogleTest — unit test framework. Declared here, made available in tests/.
if(WIS_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    # Match MSVC dynamic runtime to avoid CRT mismatch link errors.
    set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)
    set(INSTALL_GTEST         OFF CACHE BOOL "" FORCE)
endif()

# Capstone — optional disassembler backend for the PE Explorer.
if(WIS_ENABLE_CAPSTONE)
    FetchContent_Declare(
        capstone
        GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
        GIT_TAG        5.0.1
    )
endif()
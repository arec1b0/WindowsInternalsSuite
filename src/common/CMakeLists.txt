# wis_common: cross-cutting primitives (error handling, RAII, logging, text).
# Depends only on the Windows SDK. No other project modules.

add_library(wis_common STATIC
    src/error/ErrorCode.cpp
    src/error/SystemError.cpp
    src/raii/ScopedPrivilege.cpp
    src/log/Logger.cpp
    src/text/Encoding.cpp
    src/text/HexFormat.cpp
)

add_library(wis::common ALIAS wis_common)

target_include_directories(wis_common
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(wis_common
    PUBLIC
        wis_project_options
    PRIVATE
        wis_project_warnings
        advapi32   # token/privilege APIs used by ScopedPrivilege
)
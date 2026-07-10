# wis_ntapi: typed wrappers over the Windows Native API (ntdll).
# ntdll is bound at runtime via GetProcAddress, so no import library is linked.

add_library(wis_ntapi STATIC
    src/NtDll.cpp
    src/SystemInformation.cpp
    src/ProcessInformation.cpp
    src/ThreadInformation.cpp
    src/VirtualMemory.cpp
    src/ObjectInformation.cpp
    src/NativeApi.cpp
)

add_library(wis::ntapi ALIAS wis_ntapi)

target_include_directories(wis_ntapi
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(wis_ntapi
    PUBLIC
        wis_project_options
        wis::common
    PRIVATE
        wis_project_warnings
        psapi   # EnumProcessModulesEx / GetModuleFileNameExW / GetModuleInformation
)
# wis_ntapi: typed wrappers over the Windows Native API (ntdll).
# Step 4 lists only the dynamic binder; domain wrappers are added in step 5.
# ntdll is bound at runtime via GetProcAddress, so no import library is linked.

add_library(wis_ntapi STATIC
    src/NtDll.cpp
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
)
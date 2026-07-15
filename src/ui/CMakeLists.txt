# wis_ui: presentation layer.
#
# The `viewmodels` and `interfaces` sources are deliberately Win32-free: they
# depend only on wis::core and the standard library, which keeps them unit
# testable without a live desktop. Win32 views are added under src/win32 in a
# later step and are the only part that touches the OS UI surface.

add_library(wis_ui STATIC
    src/viewmodels/ProcessExplorerVM.cpp
)

add_library(wis::ui ALIAS wis_ui)

target_include_directories(wis_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(wis_ui
    PUBLIC
        wis_project_options
        wis::core
    PRIVATE
        wis_project_warnings
)

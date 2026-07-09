# Module aggregation. Each module is added incrementally; guards let the
# tree configure while later modules are still absent (matches step-by-step build).

add_subdirectory(common)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ntapi/CMakeLists.txt")
    add_subdirectory(ntapi)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt")
    add_subdirectory(core)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui/CMakeLists.txt")
    add_subdirectory(ui)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/CMakeLists.txt")
    add_subdirectory(app)
endif()
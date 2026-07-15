# wis_core: domain logic. Depends on the native seam (INativeApi) and common.
# Sources are added incrementally per domain; thread/memory/pe/... land in
# subsequent steps.

add_library(wis_core STATIC
    src/process/ProcessManager.cpp
    src/thread/ThreadManager.cpp
    src/memory/MemoryManager.cpp
    src/memory/SignatureScanner.cpp
    src/pe/PEParser.cpp
    src/pe/PEImports.cpp
    src/pe/PEExports.cpp
    src/pe/PEResources.cpp
    src/pe/PEDirectories.cpp
    src/module/ModuleManager.cpp
    src/module/FileMetadata.cpp
    src/handle/ObjectTypeTable.cpp
    src/handle/HandleManager.cpp
    src/token/TokenInspector.cpp
    src/heap/HeapInspector.cpp
    src/pebteb/PebTebReader.cpp
    src/system/SystemInfoProvider.cpp
    src/symbols/SymbolResolver.cpp
)

add_library(wis::core ALIAS wis_core)

target_include_directories(wis_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(wis_core
    PUBLIC
        wis_project_options
        wis::common
        wis::ntapi
    PRIVATE
        wis_project_warnings
        version    # GetFileVersionInfo / VerQueryValue
        wintrust   # WinVerifyTrust (Authenticode)
        crypt32    # certificate context extraction
        advapi32   # token / SID / privilege APIs
        dbghelp    # symbol resolution (SymInitialize / SymFromAddr / SymGetLineFromAddr64)
)
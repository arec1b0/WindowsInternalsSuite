#pragma once

// Domain model for a process's PEB, as read from the target address space.

#include <cstdint>
#include <string>

namespace wis::core {

// Selected RTL_USER_PROCESS_PARAMETERS contents.
struct ProcessParametersInfo {
    std::uint64_t address = 0;      // VA of the parameters block
    std::wstring imagePathName;
    std::wstring commandLine;
    std::wstring currentDirectory;
};

// PEB_LDR_DATA header plus its module-list head addresses.
struct LoaderDataInfo {
    std::uint64_t address = 0;      // VA of PEB_LDR_DATA
    bool initialized = false;
    std::uint64_t inLoadOrderListHead = 0;
    std::uint64_t inMemoryOrderListHead = 0;
    std::uint64_t inInitOrderListHead = 0;
};

struct PebInfo {
    std::uint64_t address = 0;      // VA of the PEB itself
    bool beingDebugged = false;
    std::uint64_t imageBaseAddress = 0;
    std::uint64_t ldrAddress = 0;
    std::uint64_t processParametersAddress = 0;
    std::uint64_t processHeapAddress = 0;

    ProcessParametersInfo parameters;
    LoaderDataInfo loaderData;
};

}  // namespace wis::core

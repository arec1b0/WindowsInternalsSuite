#include "core/symbols/SymbolResolver.hpp"

#include <windows.h>

#include <dbghelp.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <iterator>
#include <utility>
#include <vector>

#include "common/text/Encoding.hpp"

namespace wis::core {
namespace {

// dbghelp needs to read the target's module list and memory.
constexpr ACCESS_MASK kSymbolAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;

// Upper bound for an undecorated symbol name.
constexpr std::size_t kMaxSymbolNameChars = 1024;

// True when a search path contains a component that would trigger network I/O.
bool containsRemoteComponent(const std::wstring& path) {
    std::wstring lower;
    lower.reserve(path.size());
    std::transform(path.begin(), path.end(), std::back_inserter(lower),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    return lower.find(L"srv*") != std::wstring::npos ||
           lower.find(L"http://") != std::wstring::npos ||
           lower.find(L"https://") != std::wstring::npos;
}

// Extracts the base file name from a full path.
std::wstring baseName(const std::wstring& fullPath) {
    const std::size_t slash = fullPath.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? fullPath : fullPath.substr(slash + 1);
}

}  // namespace

std::string formatSymbol(const SymbolLocation& location, std::uint64_t address) {
    char buffer[512] = {};

    if (location.hasSymbol) {
        const std::string module =
            location.hasModule ? encoding::wideToUtf8(location.moduleName) : std::string("?");
        if (location.displacement != 0) {
            std::snprintf(buffer, sizeof(buffer), "%s!%s+0x%llX", module.c_str(),
                          location.symbolName.c_str(),
                          static_cast<unsigned long long>(location.displacement));
        } else {
            std::snprintf(buffer, sizeof(buffer), "%s!%s", module.c_str(),
                          location.symbolName.c_str());
        }
        return std::string(buffer);
    }

    if (location.hasModule) {
        const std::string module = encoding::wideToUtf8(location.moduleName);
        const std::uint64_t offset = address - location.moduleBase;
        std::snprintf(buffer, sizeof(buffer), "%s+0x%llX", module.c_str(),
                      static_cast<unsigned long long>(offset));
        return std::string(buffer);
    }

    std::snprintf(buffer, sizeof(buffer), "0x%016llX",
                  static_cast<unsigned long long>(address));
    return std::string(buffer);
}

SymbolResolver::SymbolResolver(Handle process, bool initialized) noexcept
    : process_(std::move(process)), initialized_(initialized) {}

SymbolResolver::SymbolResolver(SymbolResolver&& other) noexcept
    : process_(std::move(other.process_)),
      initialized_(std::exchange(other.initialized_, false)) {}

SymbolResolver& SymbolResolver::operator=(SymbolResolver&& other) noexcept {
    if (this != &other) {
        release();
        process_ = std::move(other.process_);
        initialized_ = std::exchange(other.initialized_, false);
    }
    return *this;
}

SymbolResolver::~SymbolResolver() { release(); }

void SymbolResolver::release() noexcept {
    if (initialized_ && process_.valid()) {
        std::scoped_lock lock(mutex_);
        ::SymCleanup(process_.get());
        initialized_ = false;
    }
}

Result<SymbolResolver, ErrorCode> SymbolResolver::create(const ntapi::INativeApi& native,
                                                         std::uint32_t pid,
                                                         std::wstring symbolSearchPath,
                                                         bool allowSymbolServer) {
    if (!allowSymbolServer && !symbolSearchPath.empty() &&
        containsRemoteComponent(symbolSearchPath)) {
        return makeUnexpected(ErrorCode::application(
            "Symbol search path requests a symbol server but network lookups are disabled"));
    }

    auto processResult = native.openProcess(pid, kSymbolAccess);
    if (!processResult) {
        return makeUnexpected(std::move(processResult).error());
    }
    Handle process = std::move(processResult).value();

    // DEFERRED_LOADS keeps SymInitialize cheap: module symbols load only when an
    // address inside them is first resolved. UNDNAME yields readable C++ names.
    // FAIL_CRITICAL_ERRORS suppresses OS error dialogs for missing media.
    DWORD options = SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS |
                    SYMOPT_LOAD_LINES;
    if (!allowSymbolServer) {
        // Belt and braces: even if _NT_SYMBOL_PATH names a server, refuse to
        // fetch from it.
        options |= SYMOPT_NO_PROMPTS;
    }
    ::SymSetOptions(options);

    // fInvadeProcess == TRUE enumerates the target's loaded modules up front so
    // module lookups work without manual SymLoadModule calls.
    const BOOL ok = ::SymInitializeW(
        process.get(), symbolSearchPath.empty() ? nullptr : symbolSearchPath.c_str(), TRUE);
    if (ok == FALSE) {
        return makeUnexpected(ErrorCode::fromLastError());
    }

    return SymbolResolver(std::move(process), true);
}

SymbolLocation SymbolResolver::resolve(std::uint64_t address) const {
    SymbolLocation location;
    if (!initialized_ || address == 0) {
        return location;
    }

    // dbghelp is not thread-safe: serialize every call for this session.
    std::scoped_lock lock(mutex_);
    const HANDLE process = process_.get();

    // 1. Module containing the address.
    IMAGEHLP_MODULEW64 moduleInfo{};
    moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    if (::SymGetModuleInfoW64(process, address, &moduleInfo) != FALSE) {
        location.hasModule = true;
        location.moduleBase = moduleInfo.BaseOfImage;
        location.moduleName = moduleInfo.ModuleName[0] != L'\0'
                                  ? std::wstring(moduleInfo.ModuleName)
                                  : baseName(std::wstring(moduleInfo.ImageName));
    }

    // 2. Symbol containing the address. SYMBOL_INFO has a variable-length name
    //    buffer that must be allocated past the end of the struct.
    std::vector<std::byte> symbolBuffer(sizeof(SYMBOL_INFO) + kMaxSymbolNameChars);
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = static_cast<ULONG>(kMaxSymbolNameChars - 1);

    DWORD64 displacement = 0;
    if (::SymFromAddr(process, address, &displacement, symbol) != FALSE) {
        location.hasSymbol = true;
        location.symbolName = std::string(symbol->Name, symbol->NameLen);
        location.displacement = displacement;
    }

    // 3. Source line, when the module's PDB carries line information.
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);
    DWORD lineDisplacement = 0;
    if (::SymGetLineFromAddr64(process, address, &lineDisplacement, &line) != FALSE) {
        location.hasLine = true;
        location.fileName = (line.FileName != nullptr) ? std::string(line.FileName)
                                                       : std::string();
        location.lineNumber = line.LineNumber;
    }

    return location;
}

}  // namespace wis::core

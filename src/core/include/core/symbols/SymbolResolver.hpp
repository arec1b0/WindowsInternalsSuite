#pragma once

// Concrete ISymbolResolver backed by dbghelp.
//
// THREAD SAFETY: dbghelp is not thread-safe. Every Sym* call made through this
// class is serialized on an internal mutex, so a single resolver may be shared
// across threads. Separate resolver instances still contend inside dbghelp
// itself, so prefer one resolver per target process, shared.
//
// NETWORK: symbol-server lookups are disabled by default. They can block for
// tens of seconds; enable them explicitly only where that latency is acceptable.

#include <cstdint>
#include <mutex>
#include <string>

#include "common/raii/UniqueHandle.hpp"
#include "core/symbols/ISymbolResolver.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class SymbolResolver final : public ISymbolResolver {
public:
    // Creates a resolver bound to `pid`. `symbolSearchPath` may be empty (dbghelp
    // then searches the module directories and _NT_SYMBOL_PATH). When
    // `allowSymbolServer` is false, any "srv*"/"http" component of the effective
    // search path is rejected so no network access can occur.
    [[nodiscard]] static Result<SymbolResolver, ErrorCode> create(
        const ntapi::INativeApi& native, std::uint32_t pid,
        std::wstring symbolSearchPath = {}, bool allowSymbolServer = false);

    SymbolResolver(const SymbolResolver&) = delete;
    SymbolResolver& operator=(const SymbolResolver&) = delete;
    SymbolResolver(SymbolResolver&& other) noexcept;
    SymbolResolver& operator=(SymbolResolver&& other) noexcept;
    ~SymbolResolver() override;

    [[nodiscard]] SymbolLocation resolve(std::uint64_t address) const override;
    [[nodiscard]] bool ready() const noexcept override { return initialized_; }

private:
    SymbolResolver(Handle process, bool initialized) noexcept;
    void release() noexcept;

    Handle process_;
    bool initialized_ = false;
    mutable std::mutex mutex_;  // serializes all dbghelp access
};

}  // namespace wis::core

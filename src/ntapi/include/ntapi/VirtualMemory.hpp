#pragma once

// Wrappers over NtReadVirtualMemory / NtWriteVirtualMemory / NtQueryVirtualMemory.

#include <cstddef>
#include <cstdint>
#include <string>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/util/ByteSpan.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi::memory {

// Reads up to buffer.size() bytes from the target process at `address`.
// Returns the number of bytes actually read (a partial read with bytesRead > 0
// is reported as success; the caller inspects the count).
[[nodiscard]] Result<std::size_t, ErrorCode> read(HANDLE process, std::uint64_t address,
                                                   MutableByteSpan buffer);

// Writes buffer.size() bytes into the target process at `address`.
[[nodiscard]] Result<std::size_t, ErrorCode> write(HANDLE process, std::uint64_t address,
                                                    ByteSpan buffer);

// MEMORY_BASIC_INFORMATION for the region containing `address`. This is the
// native equivalent of VirtualQueryEx; the walk terminates when it returns
// STATUS_INVALID_PARAMETER (address past the user-mode boundary).
[[nodiscard]] Result<MEMORY_BASIC_INFORMATION, ErrorCode> queryRegion(HANDLE process,
                                                                      std::uint64_t address);

// Native device-path name of the file backing a mapped region, if any.
[[nodiscard]] Result<std::wstring, ErrorCode> queryMappedFilename(HANDLE process,
                                                                  std::uint64_t address);

}  // namespace wis::ntapi::memory
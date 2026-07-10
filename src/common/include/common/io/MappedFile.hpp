#pragma once

// Read-only memory-mapped view of a file. Owns three resources (file handle,
// mapping handle, view pointer) and releases them in the correct order on
// destruction. Move-only. Intended for parsing on-disk images (PE files).

#include <windows.h>

#include <cstddef>
#include <string_view>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/raii/UniqueHandle.hpp"
#include "common/util/ByteSpan.hpp"

namespace wis {

class MappedFile {
public:
    MappedFile() = default;

    // Opens `path` for reading and maps its entire contents. Fails for missing
    // files, access errors, or empty files (an empty file cannot be mapped).
    [[nodiscard]] static Result<MappedFile, ErrorCode> open(std::wstring_view path);

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;

    ~MappedFile();

    [[nodiscard]] const std::byte* data() const noexcept {
        return static_cast<const std::byte*>(view_);
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool valid() const noexcept { return view_ != nullptr; }
    [[nodiscard]] ByteSpan bytes() const noexcept { return ByteSpan(data(), size_); }

private:
    MappedFile(FileHandle file, Handle mapping, const void* view, std::size_t size) noexcept;

    void release() noexcept;

    FileHandle file_;
    Handle mapping_;
    const void* view_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace wis

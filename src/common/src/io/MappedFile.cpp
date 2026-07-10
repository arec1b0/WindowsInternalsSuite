#include "common/io/MappedFile.hpp"

#include <limits>
#include <string>
#include <utility>

namespace wis {

MappedFile::MappedFile(FileHandle file, Handle mapping, const void* view,
                       std::size_t size) noexcept
    : file_(std::move(file)), mapping_(std::move(mapping)), view_(view), size_(size) {}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : file_(std::move(other.file_)),
      mapping_(std::move(other.mapping_)),
      view_(std::exchange(other.view_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        release();
        file_ = std::move(other.file_);
        mapping_ = std::move(other.mapping_);
        view_ = std::exchange(other.view_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}

MappedFile::~MappedFile() { release(); }

void MappedFile::release() noexcept {
    if (view_ != nullptr) {
        ::UnmapViewOfFile(view_);
        view_ = nullptr;
    }
    // mapping_ and file_ close themselves via UniqueHandle destruction, but the
    // view must be unmapped first (it references the mapping).
    size_ = 0;
}

Result<MappedFile, ErrorCode> MappedFile::open(std::wstring_view path) {
    const std::wstring nullTerminated(path);

    FileHandle file(::CreateFileW(nullTerminated.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        return makeUnexpected(ErrorCode::fromLastError());
    }

    LARGE_INTEGER fileSize{};
    if (::GetFileSizeEx(file.get(), &fileSize) == FALSE) {
        return makeUnexpected(ErrorCode::fromLastError());
    }
    if (fileSize.QuadPart <= 0) {
        return makeUnexpected(ErrorCode::application("Cannot map an empty file"));
    }
    if (static_cast<std::uint64_t>(fileSize.QuadPart) >
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        return makeUnexpected(ErrorCode::application("File too large to map"));
    }

    Handle mapping(::CreateFileMappingW(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (!mapping) {
        return makeUnexpected(ErrorCode::fromLastError());
    }

    const void* view = ::MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) {
        return makeUnexpected(ErrorCode::fromLastError());
    }

    return MappedFile(std::move(file), std::move(mapping), view,
                      static_cast<std::size_t>(fileSize.QuadPart));
}

}  // namespace wis

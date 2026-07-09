#pragma once

// Policy-based RAII for Windows HANDLEs. Two invalid conventions exist:
//   - NULL           (e.g. OpenProcess, NtOpenProcess)
//   - INVALID_HANDLE (e.g. CreateFile, CreateToolhelp32Snapshot)
// The policy supplies the invalid sentinel and the close routine.

#include <windows.h>

#include <utility>

namespace wis {

// Invalid value == nullptr. CloseHandle on destruction.
struct NullHandlePolicy {
    [[nodiscard]] static HANDLE invalid() noexcept { return nullptr; }
    static void close(HANDLE handle) noexcept { ::CloseHandle(handle); }
};

// Invalid value == INVALID_HANDLE_VALUE. CloseHandle on destruction.
struct FileHandlePolicy {
    [[nodiscard]] static HANDLE invalid() noexcept { return INVALID_HANDLE_VALUE; }
    static void close(HANDLE handle) noexcept { ::CloseHandle(handle); }
};

template <typename Policy>
class UniqueHandle {
public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, Policy::invalid())) {}

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, Policy::invalid());
        }
        return *this;
    }

    ~UniqueHandle() { reset(); }

    [[nodiscard]] bool valid() const noexcept { return handle_ != Policy::invalid(); }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }

    // Releases ownership without closing.
    [[nodiscard]] HANDLE release() noexcept {
        return std::exchange(handle_, Policy::invalid());
    }

    void reset(HANDLE handle = Policy::invalid()) noexcept {
        if (valid()) {
            Policy::close(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = Policy::invalid();
};

using Handle = UniqueHandle<NullHandlePolicy>;
using FileHandle = UniqueHandle<FileHandlePolicy>;

}  // namespace wis
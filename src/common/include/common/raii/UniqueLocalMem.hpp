#pragma once

// RAII for LocalAlloc/FormatMessage-allocated buffers (freed via LocalFree).

#include <windows.h>

#include <utility>

namespace wis {

template <typename Pointer>
class UniqueLocalMem {
public:
    UniqueLocalMem() noexcept = default;
    explicit UniqueLocalMem(Pointer pointer) noexcept : pointer_(pointer) {}

    UniqueLocalMem(const UniqueLocalMem&) = delete;
    UniqueLocalMem& operator=(const UniqueLocalMem&) = delete;

    UniqueLocalMem(UniqueLocalMem&& other) noexcept
        : pointer_(std::exchange(other.pointer_, nullptr)) {}

    UniqueLocalMem& operator=(UniqueLocalMem&& other) noexcept {
        if (this != &other) {
            reset();
            pointer_ = std::exchange(other.pointer_, nullptr);
        }
        return *this;
    }

    ~UniqueLocalMem() { reset(); }

    [[nodiscard]] Pointer get() const noexcept { return pointer_; }
    [[nodiscard]] explicit operator bool() const noexcept { return pointer_ != nullptr; }

    void reset(Pointer pointer = nullptr) noexcept {
        if (pointer_ != nullptr) {
            ::LocalFree(reinterpret_cast<HLOCAL>(pointer_));
        }
        pointer_ = pointer;
    }

private:
    Pointer pointer_ = nullptr;
};

}  // namespace wis
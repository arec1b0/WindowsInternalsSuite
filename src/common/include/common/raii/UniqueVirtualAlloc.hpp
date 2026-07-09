src/common/include/common/raii/UniqueVirtualAlloc.hpp
#pragma once

// RAII for memory reserved in the current process via VirtualAlloc.
// Released with MEM_RELEASE on destruction.

#include <windows.h>

#include <cstddef>
#include <utility>

namespace wis {

class UniqueVirtualAlloc {
public:
    UniqueVirtualAlloc() noexcept = default;
    UniqueVirtualAlloc(void* address, std::size_t size) noexcept
        : address_(address), size_(size) {}

    UniqueVirtualAlloc(const UniqueVirtualAlloc&) = delete;
    UniqueVirtualAlloc& operator=(const UniqueVirtualAlloc&) = delete;

    UniqueVirtualAlloc(UniqueVirtualAlloc&& other) noexcept
        : address_(std::exchange(other.address_, nullptr)),
          size_(std::exchange(other.size_, 0)) {}

    UniqueVirtualAlloc& operator=(UniqueVirtualAlloc&& other) noexcept {
        if (this != &other) {
            reset();
            address_ = std::exchange(other.address_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    ~UniqueVirtualAlloc() { reset(); }

    [[nodiscard]] void* get() const noexcept { return address_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] explicit operator bool() const noexcept { return address_ != nullptr; }

    void reset() noexcept {
        if (address_ != nullptr) {
            ::VirtualFree(address_, 0, MEM_RELEASE);
            address_ = nullptr;
            size_ = 0;
        }
    }

private:
    void* address_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace wis
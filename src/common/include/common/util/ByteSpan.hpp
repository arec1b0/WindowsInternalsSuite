#pragma once

// Non-owning views over raw bytes, used across PE parsing and memory dumps.

#include <cstddef>
#include <span>
#include <type_traits>

namespace wis {

using ByteSpan = std::span<const std::byte>;
using MutableByteSpan = std::span<std::byte>;

// Reinterprets a trivially-copyable object as a read-only byte view.
template <typename T>
[[nodiscard]] ByteSpan asBytes(const T& value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "asBytes requires a trivially-copyable type");
    return ByteSpan(reinterpret_cast<const std::byte*>(&value), sizeof(T));
}

// Wraps a raw pointer + length as a read-only byte view.
[[nodiscard]] inline ByteSpan asBytes(const void* data, std::size_t size) noexcept {
    return ByteSpan(static_cast<const std::byte*>(data), size);
}

}  // namespace wis
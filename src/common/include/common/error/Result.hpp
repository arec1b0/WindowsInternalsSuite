#pragma once

// Result<T, E>: a std::expected-compatible value-or-error type for C++20.
// API mirrors std::expected (C++23) so migration is a header swap.
// Backed by std::variant to guarantee correct construction/destruction.

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace wis {

// Thrown when value()/error() is accessed on the wrong alternative.
class BadResultAccess : public std::logic_error {
public:
    explicit BadResultAccess(const char* what) : std::logic_error(what) {}
};

// Error-side wrapper, mirroring std::unexpected.
template <typename E>
class Unexpected {
public:
    explicit Unexpected(const E& error) : error_(error) {}
    explicit Unexpected(E&& error) : error_(std::move(error)) {}

    [[nodiscard]] const E& error() const& noexcept { return error_; }
    [[nodiscard]] E& error() & noexcept { return error_; }
    [[nodiscard]] E&& error() && noexcept { return std::move(error_); }

private:
    E error_;
};

template <typename E>
[[nodiscard]] Unexpected<std::decay_t<E>> makeUnexpected(E&& error) {
    return Unexpected<std::decay_t<E>>(std::forward<E>(error));
}

// -----------------------------------------------------------------------------
// Primary template: Result carrying a value of type T or an error of type E.
// -----------------------------------------------------------------------------
template <typename T, typename E>
class Result {
    static_assert(!std::is_void_v<T>, "Use Result<void, E> specialization for void.");

public:
    using value_type = T;
    using error_type = E;

    // Value construction.
    Result(const T& value) : storage_(std::in_place_index<0>, value) {}
    Result(T&& value) : storage_(std::in_place_index<0>, std::move(value)) {}

    // Error construction.
    Result(const Unexpected<E>& unexpected)
        : storage_(std::in_place_index<1>, unexpected.error()) {}
    Result(Unexpected<E>&& unexpected)
        : storage_(std::in_place_index<1>, std::move(unexpected).error()) {}

    [[nodiscard]] bool hasValue() const noexcept { return storage_.index() == 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }

    // Value access (throws BadResultAccess on error state).
    [[nodiscard]] const T& value() const& {
        if (!hasValue()) {
            throw BadResultAccess("Result::value() called on error state");
        }
        return std::get<0>(storage_);
    }
    [[nodiscard]] T& value() & {
        if (!hasValue()) {
            throw BadResultAccess("Result::value() called on error state");
        }
        return std::get<0>(storage_);
    }
    [[nodiscard]] T&& value() && {
        if (!hasValue()) {
            throw BadResultAccess("Result::value() called on error state");
        }
        return std::get<0>(std::move(storage_));
    }

    // Error access (throws BadResultAccess on value state).
    [[nodiscard]] const E& error() const& {
        if (hasValue()) {
            throw BadResultAccess("Result::error() called on value state");
        }
        return std::get<1>(storage_);
    }
    [[nodiscard]] E& error() & {
        if (hasValue()) {
            throw BadResultAccess("Result::error() called on value state");
        }
        return std::get<1>(storage_);
    }
    [[nodiscard]] E&& error() && {
        if (hasValue()) {
            throw BadResultAccess("Result::error() called on value state");
        }
        return std::get<1>(std::move(storage_));
    }

    template <typename U>
    [[nodiscard]] T valueOr(U&& fallback) const& {
        return hasValue() ? std::get<0>(storage_) : static_cast<T>(std::forward<U>(fallback));
    }
    template <typename U>
    [[nodiscard]] T valueOr(U&& fallback) && {
        return hasValue() ? std::get<0>(std::move(storage_))
                          : static_cast<T>(std::forward<U>(fallback));
    }

private:
    std::variant<T, E> storage_;
};

// -----------------------------------------------------------------------------
// void specialization: success carries no value.
// -----------------------------------------------------------------------------
template <typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    // Success.
    Result() noexcept = default;

    // Error construction.
    Result(const Unexpected<E>& unexpected) : error_(unexpected.error()) {}
    Result(Unexpected<E>&& unexpected) : error_(std::move(unexpected).error()) {}

    [[nodiscard]] bool hasValue() const noexcept { return !error_.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }

    void value() const {
        if (!hasValue()) {
            throw BadResultAccess("Result<void>::value() called on error state");
        }
    }

    [[nodiscard]] const E& error() const& {
        if (hasValue()) {
            throw BadResultAccess("Result<void>::error() called on value state");
        }
        return *error_;
    }
    [[nodiscard]] E&& error() && {
        if (hasValue()) {
            throw BadResultAccess("Result<void>::error() called on value state");
        }
        return std::move(*error_);
    }

private:
    std::optional<E> error_;
};

}  // namespace wis
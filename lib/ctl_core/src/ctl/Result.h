#pragma once

#include <new>
#include <utility>

#include "ctl/Error.h"

namespace ctl {

/// Either a value or an Error.
///
/// The firmware runs without exceptions and must not allocate on hot paths, so
/// fallible calls return this instead of throwing or using out-parameters. The
/// value is stored inline; there is no heap involvement at any point.
///
/// Reading value() on a failed Result is a programming error. Use ok() first,
/// or valueOr() when a fallback is acceptable.
template<typename T>
class Result {
public:
    Result(const T& value) : m_error(Error::None) { new (storage()) T(value); }

    Result(T&& value) : m_error(Error::None) { new (storage()) T(std::move(value)); }

    /// Constructing from Error::None would mean "failed with no reason", which
    /// is always a bug at the call site, so it is mapped to Unknown rather than
    /// producing a Result that claims to hold a value it never received.
    Result(Error error) : m_error(error == Error::None ? Error::Unknown : error) {}

    Result(const Result& other) : m_error(other.m_error) {
        if (other.ok()) {
            new (storage()) T(*other.storage());
        }
    }

    Result(Result&& other) : m_error(other.m_error) {
        if (other.ok()) {
            new (storage()) T(std::move(*other.storage()));
        }
    }

    Result& operator=(const Result& other) {
        if (this != &other) {
            destroy();
            m_error = other.m_error;
            if (other.ok()) {
                new (storage()) T(*other.storage());
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) {
        if (this != &other) {
            destroy();
            m_error = other.m_error;
            if (other.ok()) {
                new (storage()) T(std::move(*other.storage()));
            }
        }
        return *this;
    }

    ~Result() { destroy(); }

    bool ok() const { return m_error == Error::None; }
    explicit operator bool() const { return ok(); }

    Error error() const { return m_error; }

    T& value() { return *storage(); }
    const T& value() const { return *storage(); }

    /// The value if this Result succeeded, otherwise `fallback`.
    T valueOr(const T& fallback) const { return ok() ? *storage() : fallback; }

private:
    T* storage() { return reinterpret_cast<T*>(&m_storage[0]); }
    const T* storage() const { return reinterpret_cast<const T*>(&m_storage[0]); }

    void destroy() {
        if (ok()) {
            storage()->~T();
        }
    }

    alignas(T) unsigned char m_storage[sizeof(T)];
    Error m_error;
};

/// Result for calls that either succeed or fail without producing a value.
template<>
class Result<void> {
public:
    Result() : m_error(Error::None) {}
    Result(Error error) : m_error(error) {}

    bool ok() const { return m_error == Error::None; }
    explicit operator bool() const { return ok(); }

    Error error() const { return m_error; }

private:
    Error m_error;
};

/// Shorthand for a call that reports success or failure only.
using Status = Result<void>;

/// Evaluates `expr` and, if it failed, returns its error from the enclosing
/// function. The enclosing function must itself return a Result.
#define CTL_TRY(expr)                     \
    do {                                  \
        const auto ctlTryStatus = (expr); \
        if (!ctlTryStatus.ok()) {         \
            return ctlTryStatus.error();  \
        }                                 \
    } while (0)

}  // namespace ctl

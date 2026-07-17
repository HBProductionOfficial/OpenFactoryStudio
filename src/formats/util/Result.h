#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace OFS::Formats {

struct Error
{
    std::string message;
};

inline Error withContext(std::string_view context, const Error& error)
{
    std::string message;
    message.reserve(context.size() + 2 + error.message.size());
    message.append(context);
    message.append(": ");
    message.append(error.message);
    return Error{std::move(message)};
}

// A small stand-in for C++23's std::expected, so the library never throws. Reading the wrong
// side aborts.
template <typename T>
class [[nodiscard]] Result
{
public:
    Result(const T& value)
        : m_data(std::in_place_index<0>, value)
    {}
    Result(T&& value)
        : m_data(std::in_place_index<0>, std::move(value))
    {}
    Result(Error error)
        : m_data(std::in_place_index<1>, std::move(error))
    {}

    bool ok() const noexcept { return m_data.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    T& value() & { return *valuePtr(); }
    const T& value() const& { return *valuePtr(); }
    T&& value() && { return std::move(*valuePtr()); }

    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(*valuePtr()); }
    T* operator->() { return valuePtr(); }
    const T* operator->() const { return valuePtr(); }

    T valueOr(T fallback) const&
    {
        if (const T* v = std::get_if<0>(&m_data))
            return *v;
        return fallback;
    }

    const Error& error() const
    {
        const Error* e = std::get_if<1>(&m_data);
        if (!e)
            std::abort();
        return *e;
    }

private:
    T* valuePtr()
    {
        T* v = std::get_if<0>(&m_data);
        if (!v)
            std::abort();
        return v;
    }
    const T* valuePtr() const
    {
        const T* v = std::get_if<0>(&m_data);
        if (!v)
            std::abort();
        return v;
    }

    std::variant<T, Error> m_data;
};

template <>
class [[nodiscard]] Result<void>
{
public:
    Result() = default;
    Result(Error error)
        : m_error(std::move(error))
    {}

    bool ok() const noexcept { return !m_error.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    const Error& error() const
    {
        if (!m_error)
            std::abort();
        return *m_error;
    }

private:
    std::optional<Error> m_error;
};

using Status = Result<void>;

} // namespace OFS::Formats

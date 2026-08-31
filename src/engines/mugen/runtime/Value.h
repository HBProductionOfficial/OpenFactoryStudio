#pragma once

#include <cmath>
#include <cstdint>

namespace OFS::Mugen::Runtime {

// A value of the engine's expressions: a 32-bit integer, a single-precision float, or undefined
// ("bottom": division by zero, a missing redirection target). Undefined reads as 0 and is false.
struct Value
{
    enum class Type : std::uint8_t
    {
        Undefined,
        Int,
        Float,
    };

    Type type = Type::Int;
    std::int32_t i = 0;
    float f = 0;

    static Value integer(std::int32_t v) { return {Type::Int, v, 0}; }
    static Value real(float v) { return {Type::Float, 0, v}; }
    static Value boolean(bool v) { return integer(v ? 1 : 0); }
    static Value undefined() { return {Type::Undefined, 0, 0}; }

    bool isUndefined() const { return type == Type::Undefined; }
    bool isFloat() const { return type == Type::Float; }
    // Floats are truncated toward zero, as the engine converts them.
    std::int32_t toInt() const
    {
        if (type == Type::Float) {
            if (!std::isfinite(f))
                return 0;
            return static_cast<std::int32_t>(f);
        }
        return i;
    }
    float toFloat() const { return type == Type::Float ? f : static_cast<float>(i); }
    bool toBool() const { return type == Type::Float ? f != 0 : (type == Type::Int && i != 0); }
};

} // namespace OFS::Mugen::Runtime

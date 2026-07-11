#pragma once

#include "core/CoreExport.h"

#include <QString>

namespace OFS {

// Arithmetic behind View > Calculator. Binary operators run left to right as entered
// ("2 + 3 * 4 =" gives 20) and unary functions apply at once. Bitwise operators use the integer
// part. Trig functions use radians, like the MUGEN expression language.
class OFS_CORE_EXPORT Calculator
{
public:
    enum class Binary
    {
        None,
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Power, // **
        BitAnd,
        BitOr,
        BitXor,
    };

    enum class Unary
    {
        SquareRoot,
        Sine,
        Cosine,
        Tangent,
        Reciprocal,
        Factorial, // !n
        BitNot,
    };

    Calculator();

    QString display() const { return m_display; }
    bool hasError() const { return m_error; }
    Binary pendingOperator() const { return m_pending; }

    void inputDigit(int digit);
    void inputPoint();
    void toggleSign();
    void backspace();
    void clear();
    void inputPi();
    void applyBinary(Binary op);
    void applyUnary(Unary op);
    void equals();

    static QString format(double value);

    static QString errorText();

private:
    double displayValue() const;
    void setResult(double value);
    void setError();
    bool compute(double lhs, Binary op, double rhs, double* result) const;
    void startEntryIfNeeded();

    QString m_display;
    double m_result = 0.0; // full-precision value of a displayed result
    double m_accumulator = 0.0;
    Binary m_pending = Binary::None;
    bool m_newEntry = true;        // the next digit starts a new number
    bool m_operandEntered = false; // an operand was entered after the pending operator
    bool m_error = false;
};

} // namespace OFS

#include "core/tools/Calculator.h"

#include <QCoreApplication>

#include <cmath>
#include <numbers>

namespace OFS {

namespace {

constexpr int kMaxDigits = 32;
constexpr int kMaxFactorial = 170; // 171! overflows a double

// sin(pi) is 1.2e-16 in floating point, so show such rounding noise as 0.
double snapToZero(double value)
{
    return std::fabs(value) < 1e-12 ? 0.0 : value;
}

bool toInteger(double value, qint64* result)
{
    if (!std::isfinite(value) || std::fabs(value) >= 9.2e18)
        return false;
    *result = static_cast<qint64>(std::trunc(value));
    return true;
}

} // namespace

Calculator::Calculator()
{
    clear();
}

QString Calculator::errorText()
{
    return QCoreApplication::translate("OFS::Calculator", "Error");
}

QString Calculator::format(double value)
{
    if (value == 0.0)
        return QStringLiteral("0");
    return QString::number(value, 'g', 15);
}

double Calculator::displayValue() const
{
    // Results keep their full precision. The display only shows 15 significant digits.
    if (m_newEntry && !m_error)
        return m_result;
    return m_display.toDouble();
}

void Calculator::clear()
{
    m_display = QStringLiteral("0");
    m_result = 0.0;
    m_accumulator = 0.0;
    m_pending = Binary::None;
    m_newEntry = true;
    m_operandEntered = false;
    m_error = false;
}

void Calculator::startEntryIfNeeded()
{
    if (m_error)
        clear();
    if (m_newEntry) {
        m_display = QStringLiteral("0");
        m_newEntry = false;
    }
}

void Calculator::inputDigit(int digit)
{
    if (digit < 0 || digit > 9)
        return;
    startEntryIfNeeded();
    const QChar c = QChar(u'0' + digit);
    if (m_display == QLatin1String("0"))
        m_display = c;
    else if (m_display == QLatin1String("-0"))
        m_display = QStringLiteral("-") + c;
    else if (m_display.size() < kMaxDigits)
        m_display += c;
    m_operandEntered = true;
}

void Calculator::inputPoint()
{
    startEntryIfNeeded();
    if (!m_display.contains(QLatin1Char('.')) && m_display.size() < kMaxDigits)
        m_display += QLatin1Char('.');
    m_operandEntered = true;
}

void Calculator::toggleSign()
{
    if (m_error)
        return;
    if (m_newEntry) {
        setResult(-displayValue());
        return;
    }
    if (m_display.startsWith(QLatin1Char('-')))
        m_display.remove(0, 1);
    else if (m_display != QLatin1String("0"))
        m_display.prepend(QLatin1Char('-'));
    m_operandEntered = true;
}

void Calculator::backspace()
{
    if (m_error) {
        clear();
        return;
    }
    if (m_newEntry)
        return; // results cannot be edited
    m_display.chop(1);
    if (m_display.isEmpty() || m_display == QLatin1String("-"))
        m_display = QStringLiteral("0");
}

void Calculator::inputPi()
{
    if (m_error)
        clear();
    setResult(std::numbers::pi);
}

void Calculator::setResult(double value)
{
    if (!std::isfinite(value)) {
        setError();
        return;
    }
    m_display = format(value);
    m_result = value;
    m_newEntry = true;
    m_operandEntered = true;
}

void Calculator::setError()
{
    m_display = errorText();
    m_pending = Binary::None;
    m_accumulator = 0.0;
    m_newEntry = true;
    m_operandEntered = false;
    m_error = true;
}

bool Calculator::compute(double lhs, Binary op, double rhs, double* result) const
{
    qint64 a = 0;
    qint64 b = 0;
    switch (op) {
    case Binary::None:
        *result = rhs;
        return true;
    case Binary::Add:
        *result = lhs + rhs;
        break;
    case Binary::Subtract:
        *result = lhs - rhs;
        break;
    case Binary::Multiply:
        *result = lhs * rhs;
        break;
    case Binary::Divide:
        if (rhs == 0.0)
            return false;
        *result = lhs / rhs;
        break;
    case Binary::Modulo:
        if (rhs == 0.0)
            return false;
        *result = std::fmod(lhs, rhs);
        break;
    case Binary::Power:
        *result = std::pow(lhs, rhs);
        break;
    case Binary::BitAnd:
    case Binary::BitOr:
    case Binary::BitXor:
        if (!toInteger(lhs, &a) || !toInteger(rhs, &b))
            return false;
        if (op == Binary::BitAnd)
            *result = double(a & b);
        else if (op == Binary::BitOr)
            *result = double(a | b);
        else
            *result = double(a ^ b);
        break;
    }
    return std::isfinite(*result);
}

void Calculator::applyBinary(Binary op)
{
    if (m_error)
        return;
    if (m_pending != Binary::None && m_operandEntered) {
        double result = 0.0;
        if (!compute(m_accumulator, m_pending, displayValue(), &result)) {
            setError();
            return;
        }
        m_accumulator = result;
        m_display = format(result);
        m_result = result;
    } else if (m_pending == Binary::None) {
        m_accumulator = displayValue();
        m_result = m_accumulator;
    }
    // With an operator pending and no new operand, the new operator replaces it.
    m_pending = op;
    m_newEntry = true;
    m_operandEntered = false;
}

void Calculator::equals()
{
    if (m_error)
        return;
    if (m_pending == Binary::None) {
        m_result = displayValue();
        m_newEntry = true;
        return;
    }
    double result = 0.0;
    if (!compute(m_accumulator, m_pending, displayValue(), &result)) {
        setError();
        return;
    }
    m_pending = Binary::None;
    m_accumulator = result;
    setResult(result);
    m_operandEntered = false;
}

void Calculator::applyUnary(Unary op)
{
    if (m_error)
        return;
    const double x = displayValue();
    double result = 0.0;
    qint64 n = 0;
    switch (op) {
    case Unary::SquareRoot:
        if (x < 0.0) {
            setError();
            return;
        }
        result = std::sqrt(x);
        break;
    case Unary::Sine:
        result = snapToZero(std::sin(x));
        break;
    case Unary::Cosine:
        result = snapToZero(std::cos(x));
        break;
    case Unary::Tangent:
        result = snapToZero(std::tan(x));
        break;
    case Unary::Reciprocal:
        if (x == 0.0) {
            setError();
            return;
        }
        result = 1.0 / x;
        break;
    case Unary::Factorial:
        if (x < 0.0 || x > kMaxFactorial || x != std::floor(x)) {
            setError();
            return;
        }
        result = 1.0;
        for (int i = 2; i <= int(x); ++i)
            result *= i;
        break;
    case Unary::BitNot:
        if (!toInteger(x, &n)) {
            setError();
            return;
        }
        result = double(~n);
        break;
    }
    setResult(result);
}

} // namespace OFS

#include "engines/mugen/runtime/Expr.h"

#include "engines/mugen/MugenRedirects.h"

#include <QHash>

#include <cmath>
#include <numbers>

namespace OFS::Mugen::Runtime {

// Operators

namespace Ops {

namespace {

bool anyFloat(const Value& a, const Value& b)
{
    return a.isFloat() || b.isFloat();
}

} // namespace

Value negate(Value v)
{
    if (v.isFloat())
        return Value::real(-v.f);
    if (v.isUndefined())
        return v;
    return Value::integer(static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(v.i)));
}

Value logicalNot(Value v)
{
    if (v.isUndefined())
        return v;
    return Value::boolean(!v.toBool());
}

Value bitNot(Value v)
{
    return Value::integer(~v.toInt());
}

Value add(Value a, Value b)
{
    if (anyFloat(a, b))
        return Value::real(a.toFloat() + b.toFloat());
    return Value::integer(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(a.toInt()) + static_cast<std::uint32_t>(b.toInt())));
}

Value subtract(Value a, Value b)
{
    if (anyFloat(a, b))
        return Value::real(a.toFloat() - b.toFloat());
    return Value::integer(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(a.toInt()) - static_cast<std::uint32_t>(b.toInt())));
}

Value multiply(Value a, Value b)
{
    if (anyFloat(a, b))
        return Value::real(a.toFloat() * b.toFloat());
    return Value::integer(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(a.toInt()) * static_cast<std::uint32_t>(b.toInt())));
}

Value divide(Value a, Value b)
{
    if (b.toFloat() == 0)
        return Value::undefined();
    if (anyFloat(a, b))
        return Value::real(a.toFloat() / b.toFloat());
    if (a.toInt() == INT32_MIN && b.toInt() == -1)
        return Value::integer(INT32_MIN);
    return Value::integer(a.toInt() / b.toInt()); // truncated toward zero
}

Value modulo(Value a, Value b)
{
    if (b.toInt() == 0)
        return Value::undefined();
    if (a.toInt() == INT32_MIN && b.toInt() == -1)
        return Value::integer(0);
    return Value::integer(a.toInt() % b.toInt());
}

Value power(Value a, Value b)
{
    // A float operand or a negative exponent gives a float, else an integer power.
    if (anyFloat(a, b) || b.toFloat() < 0)
        return Value::real(std::pow(a.toFloat(), b.toFloat()));
    std::uint32_t base = static_cast<std::uint32_t>(a.toInt());
    std::uint32_t exponent = static_cast<std::uint32_t>(b.toInt());
    std::uint32_t result = 1;
    while (exponent) {
        if (exponent & 1)
            result *= base;
        base *= base;
        exponent >>= 1;
    }
    return Value::integer(static_cast<std::int32_t>(result));
}

Value compare(Value a, Value b, Compare op)
{
    if (anyFloat(a, b)) {
        const float x = a.toFloat();
        const float y = b.toFloat();
        switch (op) {
        case Compare::Equal:
            return Value::boolean(x == y);
        case Compare::NotEqual:
            return Value::boolean(x != y);
        case Compare::Less:
            return Value::boolean(x < y);
        case Compare::LessEqual:
            return Value::boolean(x <= y);
        case Compare::Greater:
            return Value::boolean(x > y);
        case Compare::GreaterEqual:
            return Value::boolean(x >= y);
        }
    }
    const std::int32_t x = a.toInt();
    const std::int32_t y = b.toInt();
    switch (op) {
    case Compare::Equal:
        return Value::boolean(x == y);
    case Compare::NotEqual:
        return Value::boolean(x != y);
    case Compare::Less:
        return Value::boolean(x < y);
    case Compare::LessEqual:
        return Value::boolean(x <= y);
    case Compare::Greater:
        return Value::boolean(x > y);
    case Compare::GreaterEqual:
        return Value::boolean(x >= y);
    }
    return Value::boolean(false);
}

Value interval(Value x, Value low, Value high, bool lowOpen, bool highOpen)
{
    if (x.isUndefined() || low.isUndefined() || high.isUndefined())
        return Value::undefined();
    if (x.isFloat() || low.isFloat() || high.isFloat()) {
        const float v = x.toFloat();
        return Value::boolean((lowOpen ? v > low.toFloat() : v >= low.toFloat()) &&
                              (highOpen ? v < high.toFloat() : v <= high.toFloat()));
    }
    const std::int32_t v = x.toInt();
    return Value::boolean((lowOpen ? v > low.toInt() : v >= low.toInt()) &&
                          (highOpen ? v < high.toInt() : v <= high.toInt()));
}

} // namespace Ops

// Nodes

struct Node
{
    enum class Kind : std::uint8_t
    {
        Constant,
        Negate,
        Not,
        BitNot,
        Binary,
        And,
        Or,
        Xor,
        Interval,
        Trigger,
        Math,
        Redirect,
        Assign,
    };
    enum class Op : std::uint8_t
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Power,
        BitAnd,
        BitOr,
        BitXor,
        Compare,
    };

    Kind kind = Kind::Constant;
    Op op = Op::Add;
    Compare compare = Compare::Equal;
    Value constant;
    Trigger trigger = Trigger::Time;
    Redirect redirect = Redirect::Parent;
    bool lowOpen = false;
    bool highOpen = false;
    bool negate = false;
    bool special = false;
    bool hasSecond = false;
    Compare second = Compare::Equal;
    QString keyword;
    QString text;
    QStringList list;
    std::vector<std::shared_ptr<const Node>> kids;
};

using NodePtr = std::shared_ptr<const Node>;

namespace {

Value evaluateNode(const Node& node, Context& context);

Value math(const Node& node, Context& context)
{
    auto arg = [&](int k) {
        return k < int(node.kids.size()) ? evaluateNode(*node.kids[k], context) : Value::undefined();
    };
    auto realOf = [](Value v, double (*f)(double)) {
        if (v.isUndefined())
            return v;
        return Value::real(static_cast<float>(f(v.toFloat())));
    };
    switch (node.trigger) {
    case Trigger::Abs: {
        const Value v = arg(0);
        if (v.isFloat())
            return Value::real(std::fabs(v.f));
        return v.isUndefined() ? v : Value::integer(v.i < 0 ? static_cast<std::int32_t>(0u - std::uint32_t(v.i)) : v.i);
    }
    case Trigger::ACos:
        return realOf(arg(0), [](double x) { return std::acos(x); });
    case Trigger::ASin:
        return realOf(arg(0), [](double x) { return std::asin(x); });
    case Trigger::ATan:
        return realOf(arg(0), [](double x) { return std::atan(x); });
    case Trigger::Cos:
        return realOf(arg(0), [](double x) { return std::cos(x); });
    case Trigger::Sin:
        return realOf(arg(0), [](double x) { return std::sin(x); });
    case Trigger::Tan:
        return realOf(arg(0), [](double x) { return std::tan(x); });
    case Trigger::Exp:
        return Value::real(static_cast<float>(std::exp(arg(0).toFloat())));
    case Trigger::Ln: {
        const Value v = arg(0);
        if (v.toFloat() <= 0)
            return Value::undefined();
        return Value::real(static_cast<float>(std::log(v.toFloat())));
    }
    case Trigger::Log: {
        const Value base = arg(0);
        const Value v = arg(1);
        if (base.toFloat() <= 0 || v.toFloat() <= 0)
            return Value::undefined();
        return Value::real(static_cast<float>(std::log(v.toFloat()) / std::log(base.toFloat())));
    }
    case Trigger::Floor:
    case Trigger::Ceil: {
        const Value v = arg(0);
        if (!v.isFloat())
            return v;
        const double r = node.trigger == Trigger::Floor ? std::floor(v.f) : std::ceil(v.f);
        if (std::isnan(r))
            return Value::undefined();
        return Value::integer(static_cast<std::int32_t>(r));
    }
    case Trigger::E:
        return Value::real(static_cast<float>(std::numbers::e));
    case Trigger::Pi:
        return Value::real(static_cast<float>(std::numbers::pi));
    case Trigger::IfElse: {
        const Value condition = arg(0);
        const Value a = arg(1);
        const Value b = arg(2);
        return condition.toBool() ? a : b;
    }
    case Trigger::Cond:
        return arg(0).toBool() ? arg(1) : arg(2);
    default:
        return Value::undefined();
    }
}

Value evaluateNode(const Node& node, Context& context)
{
    switch (node.kind) {
    case Node::Kind::Constant:
        return node.constant;
    case Node::Kind::Negate:
        return Ops::negate(evaluateNode(*node.kids[0], context));
    case Node::Kind::Not:
        return Ops::logicalNot(evaluateNode(*node.kids[0], context));
    case Node::Kind::BitNot:
        return Ops::bitNot(evaluateNode(*node.kids[0], context));
    case Node::Kind::And:
        return Value::boolean(evaluateNode(*node.kids[0], context).toBool() &&
                              evaluateNode(*node.kids[1], context).toBool());
    case Node::Kind::Or:
        return Value::boolean(evaluateNode(*node.kids[0], context).toBool() ||
                              evaluateNode(*node.kids[1], context).toBool());
    case Node::Kind::Xor:
        return Value::boolean(evaluateNode(*node.kids[0], context).toBool() !=
                              evaluateNode(*node.kids[1], context).toBool());
    case Node::Kind::Binary: {
        const Value a = evaluateNode(*node.kids[0], context);
        const Value b = evaluateNode(*node.kids[1], context);
        switch (node.op) {
        case Node::Op::Add:
            return Ops::add(a, b);
        case Node::Op::Subtract:
            return Ops::subtract(a, b);
        case Node::Op::Multiply:
            return Ops::multiply(a, b);
        case Node::Op::Divide:
            return Ops::divide(a, b);
        case Node::Op::Modulo:
            return Ops::modulo(a, b);
        case Node::Op::Power:
            return Ops::power(a, b);
        case Node::Op::BitAnd:
            return Value::integer(a.toInt() & b.toInt());
        case Node::Op::BitOr:
            return Value::integer(a.toInt() | b.toInt());
        case Node::Op::BitXor:
            return Value::integer(a.toInt() ^ b.toInt());
        case Node::Op::Compare:
            return Ops::compare(a, b, node.compare);
        }
        return Value::undefined();
    }
    case Node::Kind::Interval: {
        const Value inside = Ops::interval(evaluateNode(*node.kids[0], context), evaluateNode(*node.kids[1], context),
                                           evaluateNode(*node.kids[2], context), node.lowOpen, node.highOpen);
        return node.negate ? Ops::logicalNot(inside) : inside;
    }
    case Node::Kind::Math:
        return math(node, context);
    case Node::Kind::Trigger: {
        std::vector<Value> values;
        values.reserve(node.kids.size());
        for (const NodePtr& kid : node.kids)
            values.push_back(evaluateNode(*kid, context));
        TriggerCall call;
        call.trigger = node.trigger;
        call.args = values;
        call.keyword = node.keyword;
        call.text = node.text;
        call.list = node.list;
        call.special = node.special;
        call.negate = node.negate;
        call.hasSecond = node.hasSecond;
        call.second = node.second;
        return context.trigger(call);
    }
    case Node::Kind::Redirect: {
        const Value argument = node.kids.size() > 1 ? evaluateNode(*node.kids[0], context) : Value::undefined();
        Context* other = context.redirect(node.redirect, argument);
        if (!other)
            return Value::undefined();
        return evaluateNode(*node.kids.back(), *other);
    }
    case Node::Kind::Assign: {
        const Value index = evaluateNode(*node.kids[0], context);
        const Value value = evaluateNode(*node.kids[1], context);
        return context.assign(node.trigger, index.toInt(), value);
    }
    }
    return Value::undefined();
}

// Tokens

struct Token
{
    enum class Kind : std::uint8_t
    {
        Number,
        String,
        Name,
        Op,
        Comma,
        Open,
        Close,
        OpenBracket,
        CloseBracket,
        End,
    };
    Kind kind = Kind::End;
    QString text;
};

QList<Token> tokenize(const QString& s, QString* error)
{
    QList<Token> tokens;
    int i = 0;
    const int n = int(s.size());
    auto isNameChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.');
    };
    while (i < n) {
        const QChar c = s.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }
        if (c == QLatin1Char(';'))
            break;
        Token t;
        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && s.at(i + 1).isDigit())) {
            int j = i;
            while (j < n && (s.at(j).isDigit() || s.at(j) == QLatin1Char('.')))
                ++j;
            t.kind = Token::Kind::Number;
            t.text = s.mid(i, j - i);
            i = j;
        } else if (c == QLatin1Char('"')) {
            const int end = int(s.indexOf(QLatin1Char('"'), i + 1));
            if (end < 0) {
                if (error)
                    *error = QStringLiteral("unterminated string");
                return {};
            }
            t.kind = Token::Kind::String;
            t.text = s.mid(i + 1, end - i - 1);
            i = end + 1;
        } else if (c.isLetter() || c == QLatin1Char('_')) {
            int j = i;
            while (j < n && isNameChar(s.at(j)))
                ++j;
            t.kind = Token::Kind::Name;
            t.text = s.mid(i, j - i);
            i = j;
        } else if (c == QLatin1Char(',')) {
            t.kind = Token::Kind::Comma;
            ++i;
        } else if (c == QLatin1Char('(')) {
            t.kind = Token::Kind::Open;
            ++i;
        } else if (c == QLatin1Char(')')) {
            t.kind = Token::Kind::Close;
            ++i;
        } else if (c == QLatin1Char('[')) {
            t.kind = Token::Kind::OpenBracket;
            ++i;
        } else if (c == QLatin1Char(']')) {
            t.kind = Token::Kind::CloseBracket;
            ++i;
        } else {
            static const char* const twoChar[] = {"||", "&&", "^^", ":=", "!=", "<=", ">=", "**"};
            t.kind = Token::Kind::Op;
            const QString pair = s.mid(i, 2);
            bool matched = false;
            for (const char* op : twoChar) {
                if (pair == QLatin1String(op)) {
                    t.text = pair;
                    i += 2;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                if (!QStringLiteral("+-*/%=<>!~&|^").contains(c)) {
                    if (error)
                        *error = QStringLiteral("unexpected '%1'").arg(c);
                    return {};
                }
                t.text = c;
                ++i;
            }
        }
        tokens << t;
    }
    return tokens;
}

// The trigger table

enum class Shape : std::uint8_t
{
    None,      // Time
    Arg,       // Var(1)
    OptArg,    // NumHelper / NumHelper(1234)
    Args2,     // Log(a, b)
    Args3,     // IfElse(c, a, b)
    Component, // Pos X
    Keyword,   // Const(data.life)
    AnimElem,  // AnimElem = n[, [op] t]
    Proj,      // ProjContact[ID] = 0/1[, [op] t]
    TimeMod,   // TimeMod [op] div, rem
    String,    // Command = "x"
    Letters,   // StateType = S
    HitAttr,   // HitDefAttr = SCA, NA, SA
};

struct TriggerInfo
{
    Trigger trigger;
    Shape shape;
    bool math = false;
};

const QHash<QString, TriggerInfo>& triggerTable()
{
    static const QHash<QString, TriggerInfo> table = [] {
        QHash<QString, TriggerInfo> t;
        auto add = [&t](const char* name, Trigger trigger, Shape shape, bool isMath = false) {
            t.insert(QString::fromLatin1(name).toLower(), TriggerInfo{trigger, shape, isMath});
        };
        add("abs", Trigger::Abs, Shape::Arg, true);
        add("acos", Trigger::ACos, Shape::Arg, true);
        add("asin", Trigger::ASin, Shape::Arg, true);
        add("atan", Trigger::ATan, Shape::Arg, true);
        add("ceil", Trigger::Ceil, Shape::Arg, true);
        add("cos", Trigger::Cos, Shape::Arg, true);
        add("e", Trigger::E, Shape::None, true);
        add("exp", Trigger::Exp, Shape::Arg, true);
        add("floor", Trigger::Floor, Shape::Arg, true);
        add("ln", Trigger::Ln, Shape::Arg, true);
        add("log", Trigger::Log, Shape::Args2, true);
        add("pi", Trigger::Pi, Shape::None, true);
        add("sin", Trigger::Sin, Shape::Arg, true);
        add("tan", Trigger::Tan, Shape::Arg, true);
        add("ifelse", Trigger::IfElse, Shape::Args3, true);
        add("cond", Trigger::Cond, Shape::Args3, true);
        add("ailevel", Trigger::AILevel, Shape::None);
        add("alive", Trigger::Alive, Shape::None);
        add("anim", Trigger::Anim, Shape::None);
        add("animelem", Trigger::AnimElem, Shape::AnimElem);
        add("animelemno", Trigger::AnimElemNo, Shape::Arg);
        add("animelemtime", Trigger::AnimElemTime, Shape::Arg);
        add("animexist", Trigger::AnimExist, Shape::Arg);
        add("animtime", Trigger::AnimTime, Shape::None);
        add("authorname", Trigger::AuthorName, Shape::String);
        add("backedgebodydist", Trigger::BackEdgeBodyDist, Shape::None);
        add("backedgedist", Trigger::BackEdgeDist, Shape::None);
        add("canrecover", Trigger::CanRecover, Shape::None);
        add("command", Trigger::Command, Shape::String);
        add("const", Trigger::Const, Shape::Keyword);
        add("const240p", Trigger::Const240p, Shape::Arg);
        add("const480p", Trigger::Const480p, Shape::Arg);
        add("const720p", Trigger::Const720p, Shape::Arg);
        add("ctrl", Trigger::Ctrl, Shape::None);
        add("drawgame", Trigger::DrawGame, Shape::None);
        add("facing", Trigger::Facing, Shape::None);
        add("frontedgebodydist", Trigger::FrontEdgeBodyDist, Shape::None);
        add("frontedgedist", Trigger::FrontEdgeDist, Shape::None);
        add("fvar", Trigger::FVar, Shape::Arg);
        add("gameheight", Trigger::GameHeight, Shape::None);
        add("gametime", Trigger::GameTime, Shape::None);
        add("gamewidth", Trigger::GameWidth, Shape::None);
        add("gethitvar", Trigger::GetHitVar, Shape::Keyword);
        add("hitcount", Trigger::HitCount, Shape::None);
        add("hitdefattr", Trigger::HitDefAttr, Shape::HitAttr);
        add("hitfall", Trigger::HitFall, Shape::None);
        add("hitover", Trigger::HitOver, Shape::None);
        add("hitpausetime", Trigger::HitPauseTime, Shape::None);
        add("hitshakeover", Trigger::HitShakeOver, Shape::None);
        add("hitvel", Trigger::HitVel, Shape::Component);
        add("id", Trigger::ID, Shape::None);
        add("inguarddist", Trigger::InGuardDist, Shape::None);
        add("ishelper", Trigger::IsHelper, Shape::OptArg);
        add("ishometeam", Trigger::IsHomeTeam, Shape::None);
        add("life", Trigger::Life, Shape::None);
        add("lifemax", Trigger::LifeMax, Shape::None);
        add("lose", Trigger::Lose, Shape::None);
        add("loseko", Trigger::LoseKO, Shape::None);
        add("losetime", Trigger::LoseTime, Shape::None);
        add("matchno", Trigger::MatchNo, Shape::None);
        add("matchover", Trigger::MatchOver, Shape::None);
        add("movecontact", Trigger::MoveContact, Shape::None);
        add("moveguarded", Trigger::MoveGuarded, Shape::None);
        add("movehit", Trigger::MoveHit, Shape::None);
        add("movereversed", Trigger::MoveReversed, Shape::None);
        add("movetype", Trigger::MoveType, Shape::Letters);
        add("name", Trigger::Name, Shape::String);
        add("numenemy", Trigger::NumEnemy, Shape::None);
        add("numexplod", Trigger::NumExplod, Shape::OptArg);
        add("numhelper", Trigger::NumHelper, Shape::OptArg);
        add("numpartner", Trigger::NumPartner, Shape::None);
        add("numproj", Trigger::NumProj, Shape::None);
        add("numprojid", Trigger::NumProjID, Shape::Arg);
        add("numtarget", Trigger::NumTarget, Shape::OptArg);
        add("p1name", Trigger::P1Name, Shape::String);
        add("p2bodydist", Trigger::P2BodyDist, Shape::Component);
        add("p2dist", Trigger::P2Dist, Shape::Component);
        add("p2life", Trigger::P2Life, Shape::None);
        add("p2movetype", Trigger::P2MoveType, Shape::Letters);
        add("p2name", Trigger::P2Name, Shape::String);
        add("p2stateno", Trigger::P2StateNo, Shape::None);
        add("p2statetype", Trigger::P2StateType, Shape::Letters);
        add("p3name", Trigger::P3Name, Shape::String);
        add("p4name", Trigger::P4Name, Shape::String);
        add("palno", Trigger::PalNo, Shape::None);
        add("parentdist", Trigger::ParentDist, Shape::Component);
        add("playeridexist", Trigger::PlayerIDExist, Shape::Arg);
        add("pos", Trigger::Pos, Shape::Component);
        add("power", Trigger::Power, Shape::None);
        add("powermax", Trigger::PowerMax, Shape::None);
        add("prevstateno", Trigger::PrevStateNo, Shape::None);
        add("projcanceltime", Trigger::ProjCancelTime, Shape::Arg);
        add("projcontact", Trigger::ProjContact, Shape::Proj);
        add("projcontacttime", Trigger::ProjContactTime, Shape::Arg);
        add("projguarded", Trigger::ProjGuarded, Shape::Proj);
        add("projguardedtime", Trigger::ProjGuardedTime, Shape::Arg);
        add("projhit", Trigger::ProjHit, Shape::Proj);
        add("projhittime", Trigger::ProjHitTime, Shape::Arg);
        add("random", Trigger::Random, Shape::None);
        add("rootdist", Trigger::RootDist, Shape::Component);
        add("roundno", Trigger::RoundNo, Shape::None);
        add("roundsexisted", Trigger::RoundsExisted, Shape::None);
        add("roundstate", Trigger::RoundState, Shape::None);
        add("screenheight", Trigger::ScreenHeight, Shape::None);
        add("screenpos", Trigger::ScreenPos, Shape::Component);
        add("screenwidth", Trigger::ScreenWidth, Shape::None);
        add("selfanimexist", Trigger::SelfAnimExist, Shape::Arg);
        add("stagevar", Trigger::StageVar, Shape::Keyword);
        add("stateno", Trigger::StateNo, Shape::None);
        add("statetype", Trigger::StateType, Shape::Letters);
        add("sysfvar", Trigger::SysFVar, Shape::Arg);
        add("sysvar", Trigger::SysVar, Shape::Arg);
        add("teammode", Trigger::TeamMode, Shape::Letters);
        add("teamside", Trigger::TeamSide, Shape::None);
        add("tickspersecond", Trigger::TicksPerSecond, Shape::None);
        add("time", Trigger::Time, Shape::None);
        add("statetime", Trigger::Time, Shape::None); // the engine's other name for Time
        add("timemod", Trigger::TimeMod, Shape::TimeMod);
        add("uniqhitcount", Trigger::UniqHitCount, Shape::None);
        add("var", Trigger::Var, Shape::Arg);
        add("vel", Trigger::Vel, Shape::Component);
        add("win", Trigger::Win, Shape::None);
        add("winko", Trigger::WinKO, Shape::None);
        add("winperfect", Trigger::WinPerfect, Shape::None);
        add("wintime", Trigger::WinTime, Shape::None);
        return t;
    }();
    return table;
}

// Parser. Precedence from low to high: || ^^ && | ^ & := (= != intervals) (< <= > >=) (+ -)
// (* / %) ** then the unary ! ~ -

class Parser
{
public:
    explicit Parser(QList<Token> tokens)
        : m_tokens(std::move(tokens))
    {}

    NodePtr parseAll()
    {
        NodePtr root = parse(1);
        if (root && !atEnd())
            fail(QStringLiteral("unexpected '%1'").arg(peek().text));
        return m_error.isEmpty() ? root : nullptr;
    }
    std::vector<NodePtr> parseList()
    {
        std::vector<NodePtr> list;
        do {
            list.push_back(parse(1));
        } while (m_error.isEmpty() && accept(Token::Kind::Comma));
        if (m_error.isEmpty() && !atEnd())
            fail(QStringLiteral("unexpected '%1'").arg(peek().text));
        return list;
    }
    const QString& error() const { return m_error; }

private:
    static int precedence(const QString& op)
    {
        static const QHash<QString, int> table = {
            {QStringLiteral("||"), 1}, {QStringLiteral("^^"), 2}, {QStringLiteral("&&"), 3},  {QStringLiteral("|"), 4},
            {QStringLiteral("^"), 5},  {QStringLiteral("&"), 6},  {QStringLiteral(":="), 7},  {QStringLiteral("="), 8},
            {QStringLiteral("!="), 8}, {QStringLiteral("<"), 9},  {QStringLiteral("<="), 9},  {QStringLiteral(">"), 9},
            {QStringLiteral(">="), 9}, {QStringLiteral("+"), 10}, {QStringLiteral("-"), 10},  {QStringLiteral("*"), 11},
            {QStringLiteral("/"), 11}, {QStringLiteral("%"), 11}, {QStringLiteral("**"), 12},
        };
        return table.value(op, 0);
    }
    static Compare compareOf(const QString& op)
    {
        if (op == QLatin1String("!="))
            return Compare::NotEqual;
        if (op == QLatin1String("<"))
            return Compare::Less;
        if (op == QLatin1String("<="))
            return Compare::LessEqual;
        if (op == QLatin1String(">"))
            return Compare::Greater;
        if (op == QLatin1String(">="))
            return Compare::GreaterEqual;
        return Compare::Equal;
    }
    static bool isCompare(const Token& t)
    {
        return t.kind == Token::Kind::Op &&
               (t.text == QLatin1String("=") || t.text == QLatin1String("!=") || t.text == QLatin1String("<") ||
                t.text == QLatin1String("<=") || t.text == QLatin1String(">") || t.text == QLatin1String(">="));
    }

    const Token& peek(int ahead = 0) const
    {
        static const Token end;
        return m_pos + ahead < m_tokens.size() ? m_tokens[m_pos + ahead] : end;
    }
    Token next() { return m_pos < m_tokens.size() ? m_tokens[m_pos++] : Token(); }
    bool atEnd() const { return m_pos >= m_tokens.size(); }
    bool accept(Token::Kind kind)
    {
        if (peek().kind == kind) {
            ++m_pos;
            return true;
        }
        return false;
    }
    bool acceptOp(const char* op)
    {
        if (peek().kind == Token::Kind::Op && peek().text == QLatin1String(op)) {
            ++m_pos;
            return true;
        }
        return false;
    }
    NodePtr fail(const QString& message)
    {
        if (m_error.isEmpty())
            m_error = message;
        return nullptr;
    }

    static std::shared_ptr<Node> make(Node::Kind kind)
    {
        auto node = std::make_shared<Node>();
        node->kind = kind;
        return node;
    }

    NodePtr parse(int minPrecedence)
    {
        NodePtr left = parseUnary();
        while (left && m_error.isEmpty() && peek().kind == Token::Kind::Op) {
            const QString op = peek().text;
            const int prec = precedence(op);
            if (prec == 0 || prec < minPrecedence)
                break;
            next();
            if ((op == QLatin1String("=") || op == QLatin1String("!=")) &&
                (peek().kind == Token::Kind::OpenBracket || peek().kind == Token::Kind::Open) && intervalAhead()) {
                left = parseInterval(left, op == QLatin1String("!="));
                continue;
            }
            if (op == QLatin1String(":=")) {
                // Assignment to var(n), fvar(n), sysvar(n) or sysfvar(n). Right-associative.
                if (left->kind != Node::Kind::Trigger ||
                    (left->trigger != Trigger::Var && left->trigger != Trigger::FVar &&
                     left->trigger != Trigger::SysVar && left->trigger != Trigger::SysFVar))
                    return fail(QStringLiteral("':=' needs a variable on its left"));
                NodePtr value = parse(prec);
                if (!value)
                    return nullptr;
                auto node = make(Node::Kind::Assign);
                node->trigger = left->trigger;
                node->kids = {left->kids.front(), value};
                left = node;
                continue;
            }
            const bool rightAssociative = op == QLatin1String("**");
            NodePtr right = parse(rightAssociative ? prec : prec + 1);
            if (!right)
                return nullptr;
            std::shared_ptr<Node> node;
            if (op == QLatin1String("&&")) {
                node = make(Node::Kind::And);
            } else if (op == QLatin1String("||")) {
                node = make(Node::Kind::Or);
            } else if (op == QLatin1String("^^")) {
                node = make(Node::Kind::Xor);
            } else {
                node = make(Node::Kind::Binary);
                node->op = op == QLatin1String("+")    ? Node::Op::Add
                           : op == QLatin1String("-")  ? Node::Op::Subtract
                           : op == QLatin1String("*")  ? Node::Op::Multiply
                           : op == QLatin1String("/")  ? Node::Op::Divide
                           : op == QLatin1String("%")  ? Node::Op::Modulo
                           : op == QLatin1String("**") ? Node::Op::Power
                           : op == QLatin1String("&")  ? Node::Op::BitAnd
                           : op == QLatin1String("|")  ? Node::Op::BitOr
                           : op == QLatin1String("^")  ? Node::Op::BitXor
                                                       : Node::Op::Compare;
                node->compare = compareOf(op);
            }
            node->kids = {left, right};
            left = node;
        }
        return left;
    }

    bool intervalAhead() const
    {
        int depth = 0;
        for (int i = m_pos; i < m_tokens.size(); ++i) {
            const Token& t = m_tokens[i];
            if (t.kind == Token::Kind::Open || t.kind == Token::Kind::OpenBracket) {
                ++depth;
            } else if (t.kind == Token::Kind::Close || t.kind == Token::Kind::CloseBracket) {
                if (--depth == 0)
                    return false;
            } else if (t.kind == Token::Kind::Comma && depth == 1) {
                return true;
            }
        }
        return false;
    }

    NodePtr parseInterval(NodePtr value, bool negate)
    {
        const bool lowOpen = next().kind == Token::Kind::Open;
        NodePtr low = parse(1);
        if (!low || !accept(Token::Kind::Comma))
            return fail(QStringLiteral("',' expected in interval"));
        NodePtr high = parse(1);
        if (!high)
            return nullptr;
        const Token close = next();
        if (close.kind != Token::Kind::Close && close.kind != Token::Kind::CloseBracket)
            return fail(QStringLiteral("']' or ')' expected"));
        auto node = make(Node::Kind::Interval);
        node->lowOpen = lowOpen;
        node->highOpen = close.kind == Token::Kind::Close;
        node->negate = negate;
        node->kids = {value, low, high};
        return node;
    }

    NodePtr parseUnary()
    {
        if (peek().kind == Token::Kind::Op) {
            const QString op = peek().text;
            if (op == QLatin1String("!") || op == QLatin1String("~") || op == QLatin1String("-") ||
                op == QLatin1String("+")) {
                next();
                NodePtr operand = parseUnary();
                if (!operand)
                    return nullptr;
                if (op == QLatin1String("+"))
                    return operand;
                if (op == QLatin1String("-") && operand->kind == Node::Kind::Constant) {
                    auto node = make(Node::Kind::Constant);
                    node->constant = Ops::negate(operand->constant);
                    return node;
                }
                auto node = make(op == QLatin1String("!")   ? Node::Kind::Not
                                 : op == QLatin1String("~") ? Node::Kind::BitNot
                                                            : Node::Kind::Negate);
                node->kids = {operand};
                return node;
            }
        }
        return parsePrimary();
    }

    NodePtr parsePrimary()
    {
        if (atEnd())
            return fail(QStringLiteral("expression expected"));
        const Token t = next();
        switch (t.kind) {
        case Token::Kind::Number: {
            auto node = make(Node::Kind::Constant);
            if (t.text.contains(QLatin1Char('.')))
                node->constant = Value::real(t.text.toFloat());
            else
                node->constant = Value::integer(static_cast<std::int32_t>(t.text.toLongLong()));
            return node;
        }
        case Token::Kind::Open: {
            NodePtr inner = parse(1);
            if (!inner)
                return nullptr;
            if (!accept(Token::Kind::Close))
                return fail(QStringLiteral("unbalanced parentheses"));
            return inner;
        }
        case Token::Kind::Name:
            return parseName(t.text);
        default:
            return fail(QStringLiteral("unexpected '%1'").arg(t.text));
        }
    }

    NodePtr parseName(const QString& name)
    {
        const QString lower = name.toLower();
        static_assert(std::size(RedirectKeywords) == std::size_t(Redirect::PlayerID) + 1);
        if (const int r = redirectIndex(lower);
            r >= 0 && (peek().kind == Token::Kind::Comma || peek().kind == Token::Kind::Open)) {
            auto node = make(Node::Kind::Redirect);
            node->redirect = Redirect(r);
            if (accept(Token::Kind::Open)) {
                NodePtr argument = parse(1);
                if (!argument || !accept(Token::Kind::Close))
                    return fail(QStringLiteral("unbalanced parentheses"));
                node->kids.push_back(argument);
            }
            if (!accept(Token::Kind::Comma))
                return fail(QStringLiteral("',' expected after '%1'").arg(name));
            const Token target = next();
            if (target.kind != Token::Kind::Name)
                return fail(QStringLiteral("trigger expected after '%1'").arg(name));
            NodePtr inner = parseName(target.text);
            if (!inner)
                return nullptr;
            node->kids.push_back(inner);
            return node;
        }

        // ProjContact1234 = 1: a projectile ID glued to the name.
        QString key = lower;
        std::int32_t projectileId = -1;
        for (const char* proj : {"projcontact", "projguarded", "projhit"}) {
            if (lower.startsWith(QLatin1String(proj)) && lower.size() > int(qstrlen(proj))) {
                bool ok = false;
                const int id = lower.mid(int(qstrlen(proj))).toInt(&ok);
                if (ok) {
                    key = QLatin1String(proj);
                    projectileId = id;
                }
            }
        }
        const auto found = triggerTable().constFind(key);
        if (found == triggerTable().cend())
            return fail(QStringLiteral("unknown trigger '%1'").arg(name));
        const TriggerInfo info = *found;
        auto node = make(info.math ? Node::Kind::Math : Node::Kind::Trigger);
        node->trigger = info.trigger;

        auto arguments = [&](int minimum, int maximum) -> bool {
            if (!accept(Token::Kind::Open)) {
                if (minimum > 0)
                    fail(QStringLiteral("'%1' needs an argument").arg(name));
                return minimum == 0;
            }
            int count = 0;
            do {
                NodePtr argument = parse(1);
                if (!argument)
                    return false;
                node->kids.push_back(argument);
                ++count;
            } while (accept(Token::Kind::Comma));
            if (!accept(Token::Kind::Close)) {
                fail(QStringLiteral("unbalanced parentheses"));
                return false;
            }
            if (count < minimum || count > maximum) {
                fail(QStringLiteral("wrong number of arguments for '%1'").arg(name));
                return false;
            }
            return true;
        };
        auto equality = [&]() -> bool {
            if (acceptOp("="))
                return true;
            if (acceptOp("!=")) {
                node->negate = true;
                return true;
            }
            fail(QStringLiteral("'%1' must be compared with '=' or '!='").arg(name));
            return false;
        };
        auto secondPart = [&]() -> bool {
            if (!accept(Token::Kind::Comma))
                return true;
            node->hasSecond = true;
            if (isCompare(peek()))
                node->second = compareOf(next().text);
            NodePtr t = parse(10);
            if (!t)
                return false;
            node->kids.push_back(t);
            return true;
        };

        switch (info.shape) {
        case Shape::None:
            break;
        case Shape::Arg:
            if (!arguments(1, 1))
                return nullptr;
            break;
        case Shape::OptArg:
            if (!arguments(0, 1))
                return nullptr;
            break;
        case Shape::Args2:
            if (!arguments(2, 2))
                return nullptr;
            break;
        case Shape::Args3:
            if (!arguments(3, 3))
                return nullptr;
            break;
        case Shape::Component: {
            const Token component = next();
            if (component.kind != Token::Kind::Name)
                return fail(QStringLiteral("'%1' needs a component").arg(name));
            node->keyword = component.text.toLower();
            break;
        }
        case Shape::Keyword: {
            if (!accept(Token::Kind::Open))
                return fail(QStringLiteral("'%1' needs an argument").arg(name));
            const Token keyword = next();
            if (keyword.kind != Token::Kind::Name || !accept(Token::Kind::Close))
                return fail(QStringLiteral("'%1' needs a name in parentheses").arg(name));
            node->keyword = keyword.text.toLower();
            break;
        }
        case Shape::AnimElem:
            node->special = true;
            if (!equality())
                return nullptr;
            if (NodePtr value = parse(10); value)
                node->kids.push_back(value);
            else
                return nullptr;
            if (!secondPart())
                return nullptr;
            break;
        case Shape::Proj:
            node->special = true;
            if (projectileId >= 0)
                node->keyword = QString::number(projectileId);
            if (!equality())
                return nullptr;
            if (NodePtr value = parse(10); value)
                node->kids.push_back(value);
            else
                return nullptr;
            if (!secondPart())
                return nullptr;
            break;
        case Shape::TimeMod:
            node->special = true;
            if (!equality())
                return nullptr;
            for (int k = 0; k < 2; ++k) {
                NodePtr value = parse(10);
                if (!value)
                    return nullptr;
                node->kids.push_back(value);
                if (k == 0 && !accept(Token::Kind::Comma))
                    return fail(QStringLiteral("',' expected in TimeMod"));
            }
            break;
        case Shape::String: {
            node->special = true;
            if (!equality())
                return nullptr;
            const Token text = next();
            if (text.kind != Token::Kind::String)
                return fail(QStringLiteral("a string is expected after '%1 ='").arg(name));
            node->text = text.text;
            break;
        }
        case Shape::Letters: {
            node->special = true;
            if (!equality())
                return nullptr;
            const Token letters = next();
            if (letters.kind != Token::Kind::Name)
                return fail(QStringLiteral("a value is expected after '%1 ='").arg(name));
            node->keyword = letters.text.toUpper();
            break;
        }
        case Shape::HitAttr: {
            node->special = true;
            if (!equality())
                return nullptr;
            const Token letters = next();
            if (letters.kind != Token::Kind::Name)
                return fail(QStringLiteral("hit attributes expected after '%1 ='").arg(name));
            node->keyword = letters.text.toUpper();
            while (accept(Token::Kind::Comma)) {
                const Token cls = next();
                if (cls.kind != Token::Kind::Name)
                    return fail(QStringLiteral("attack class expected"));
                node->list << cls.text.toUpper();
            }
            break;
        }
        }
        return node;
    }

    QList<Token> m_tokens;
    int m_pos = 0;
    QString m_error;
};

} // namespace

// Expr

Expr::Expr() = default;
Expr::~Expr() = default;
Expr::Expr(const Expr&) = default;
Expr& Expr::operator=(const Expr&) = default;

Expr Expr::compile(const QString& text, QString* error)
{
    Expr expr;
    QString message;
    const QList<Token> tokens = tokenize(text, &message);
    if (message.isEmpty()) {
        Parser parser(tokens);
        expr.m_root = parser.parseAll();
        message = parser.error();
    }
    if (error)
        *error = message;
    return expr;
}

std::vector<Expr> Expr::compileList(const QString& text, QString* error)
{
    std::vector<Expr> list;
    QString message;
    const QList<Token> tokens = tokenize(text, &message);
    if (message.isEmpty()) {
        Parser parser(tokens);
        for (const NodePtr& node : parser.parseList()) {
            Expr expr;
            expr.m_root = node;
            list.push_back(expr);
        }
        message = parser.error();
        if (!message.isEmpty())
            list.clear();
    }
    if (error)
        *error = message;
    return list;
}

Expr Expr::constant(Value value)
{
    Expr expr;
    auto node = std::make_shared<Node>();
    node->kind = Node::Kind::Constant;
    node->constant = value;
    expr.m_root = node;
    return expr;
}

Value Expr::evaluate(Context& context) const
{
    return m_root ? evaluateNode(*m_root, context) : Value::undefined();
}

bool Expr::isConstant() const
{
    return m_root && m_root->kind == Node::Kind::Constant;
}

} // namespace OFS::Mugen::Runtime

#include "engines/mugen/runtime/Expr.h"

#include <QTest>

#include <map>

using namespace OFS::Mugen::Runtime;

namespace {

class FakeContext : public Context
{
public:
    Value trigger(const TriggerCall& call) override
    {
        calls.push_back(call.trigger);
        switch (call.trigger) {
        case Trigger::Time:
            return Value::integer(time);
        case Trigger::Life:
            return Value::integer(life);
        case Trigger::Var:
            return Value::integer(vars[call.args[0].toInt()]);
        case Trigger::FVar:
            return Value::real(fvars[call.args[0].toInt()]);
        case Trigger::Pos:
            return Value::real(call.keyword == QLatin1String("x") ? 10.5f : -20.f);
        case Trigger::Const:
            return call.keyword == QLatin1String("data.life") ? Value::integer(1000) : Value::undefined();
        case Trigger::StateType: {
            const bool match = call.keyword == stateType;
            return Value::boolean(match != call.negate);
        }
        case Trigger::Command: {
            const bool match = call.text.compare(command, Qt::CaseInsensitive) == 0;
            return Value::boolean(match != call.negate);
        }
        case Trigger::AnimElem: {
            // Element 3 started 2 ticks ago.
            const int element = call.args[0].toInt();
            if (element != 3)
                return Value::boolean(call.negate);
            if (!call.hasSecond)
                return Value::boolean((elementTime == 0) != call.negate);
            const Value result = Ops::compare(Value::integer(elementTime), call.args[1], call.second);
            return Value::boolean(result.toBool() != call.negate);
        }
        case Trigger::TimeMod: {
            const Value divisor = call.args[0];
            if (divisor.toInt() == 0)
                return Value::undefined();
            const bool match = time % divisor.toInt() == call.args[1].toInt();
            return Value::boolean(match != call.negate);
        }
        case Trigger::HitDefAttr:
            lastKeyword = call.keyword;
            lastList = call.list;
            return Value::boolean(!call.negate);
        case Trigger::ProjContact:
            lastKeyword = call.keyword;
            return Value::boolean(!call.negate);
        case Trigger::NumHelper:
            return Value::integer(call.args.empty() ? 2 : 1);
        default:
            return Value::undefined();
        }
    }

    Context* redirect(Redirect kind, const Value& argument) override
    {
        lastRedirect = kind;
        lastArgument = argument;
        return kind == Redirect::Parent ? parent : nullptr;
    }

    Value assign(Trigger kind, std::int32_t index, const Value& value) override
    {
        if (kind == Trigger::FVar) {
            fvars[index] = value.toFloat();
            return Value::real(fvars[index]);
        }
        vars[index] = value.toInt();
        return Value::integer(vars[index]);
    }

    int time = 5;
    int life = 800;
    int elementTime = 2;
    QString stateType = QStringLiteral("S");
    QString command = QStringLiteral("QCF_x");
    std::map<int, int> vars;
    std::map<int, float> fvars;
    std::vector<Trigger> calls;
    QString lastKeyword;
    QStringList lastList;
    Redirect lastRedirect = Redirect::Root;
    Value lastArgument;
    FakeContext* parent = nullptr;
};

Value eval(const QString& text, Context& context)
{
    QString error;
    const Expr expr = Expr::compile(text, &error);
    if (!error.isEmpty())
        qWarning("%s: %s", qPrintable(text), qPrintable(error));
    return expr.evaluate(context);
}

Value eval(const QString& text)
{
    FakeContext context;
    return eval(text, context);
}

QString errorOf(const QString& text)
{
    QString error;
    Expr::compile(text, &error);
    return error;
}

bool isInt(const Value& v, int expected)
{
    return v.type == Value::Type::Int && v.i == expected;
}

bool isFloat(const Value& v, float expected)
{
    return v.type == Value::Type::Float && qFuzzyCompare(v.f + 1, expected + 1);
}

} // namespace

class RuntimeExprTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void arithmetic()
    {
        QVERIFY(isInt(eval("1 + 2 * 3"), 7));
        QVERIFY(isInt(eval("(1 + 2) * 3"), 9));
        QVERIFY(isInt(eval("7 / 2"), 3));
        QVERIFY(isInt(eval("-7 / 2"), -3));
        QVERIFY(isInt(eval("-7 % 3"), -1));
        QVERIFY(isFloat(eval("7 / 2.0"), 3.5f));
        QVERIFY(isFloat(eval("1 + .5"), 1.5f));
        QVERIFY(isInt(eval("2 ** 10"), 1024));
        QVERIFY(isInt(eval("2 ** 3 ** 2"), 512));
        QVERIFY(isFloat(eval("2 ** -1"), 0.5f));
        QVERIFY(isInt(eval("-2 ** 2"), 4));
        QVERIFY(isInt(eval("2147483647 + 1"), INT32_MIN));
        QVERIFY(isInt(eval("~0"), -1));
        QVERIFY(isInt(eval("6 & 3 | 8 ^ 1"), (6 & 3) | (8 ^ 1)));
    }

    void divisionByZero()
    {
        QVERIFY(eval("1 / 0").isUndefined());
        QVERIFY(eval("1 % 0").isUndefined());
        QVERIFY(eval("1.0 / 0.0").isUndefined());
        QVERIFY(isInt(eval("1 / 0 + 2"), 2));
        QVERIFY(isInt(eval("1 / 0 || 1"), 1));
        QVERIFY(eval("!(1 / 0)").isUndefined());
        QVERIFY(eval("1 / 0 = [0, 2]").isUndefined());
    }

    void comparisonsAndLogic()
    {
        QVERIFY(isInt(eval("1 < 2"), 1));
        QVERIFY(isInt(eval("2 <= 1"), 0));
        QVERIFY(isInt(eval("1 = 1.0"), 1));
        QVERIFY(isInt(eval("1 != 2 && 3 > 2"), 1));
        QVERIFY(isInt(eval("1 ^^ 1"), 0));
        QVERIFY(isInt(eval("0 || 0 ^^ 1"), 1));
        QVERIFY(isInt(eval("!0"), 1));
        QVERIFY(isInt(eval("1 < 2 = 1"), 1));
        QVERIFY(isInt(eval("1 || 0 && 0"), 1));
    }

    void intervals()
    {
        QVERIFY(isInt(eval("5 = [1, 5]"), 1));
        QVERIFY(isInt(eval("5 = [1, 5)"), 0));
        QVERIFY(isInt(eval("1 = (1, 5]"), 0));
        QVERIFY(isInt(eval("3 != [1, 5]"), 0));
        QVERIFY(isInt(eval("2.5 = [2, 3]"), 1));
        QVERIFY(isInt(eval("time = [0, 10]"), 1));
        QVERIFY(isInt(eval("3 = (1 + 2)"), 1));
        QVERIFY(isInt(eval("3 = (ifelse(1, 3, 4))"), 1));
    }

    void triggers()
    {
        FakeContext context;
        QVERIFY(isInt(eval("Time", context), 5));
        QVERIFY(isInt(eval("life + 1", context), 801));
        QVERIFY(isFloat(eval("Pos X", context), 10.5f));
        QVERIFY(isFloat(eval("pos y * 2", context), -40.f));
        QVERIFY(isInt(eval("const(data.life)", context), 1000));
        QVERIFY(isInt(eval("Const(Data.Life) - life", context), 200));
        QVERIFY(isInt(eval("NumHelper", context), 2));
        QVERIFY(isInt(eval("NumHelper(1234)", context), 1));
        context.vars[3] = 42;
        QVERIFY(isInt(eval("var(3)", context), 42));
        QVERIFY(isInt(eval("var(1 + 2) = 42", context), 1));
    }

    void math()
    {
        QVERIFY(isInt(eval("abs(-3)"), 3));
        QVERIFY(isFloat(eval("abs(-2.5)"), 2.5f));
        QVERIFY(isInt(eval("floor(2.7)"), 2));
        QVERIFY(isInt(eval("ceil(2.1)"), 3));
        QVERIFY(isInt(eval("floor(-2.5)"), -3));
        QVERIFY(isFloat(eval("cos(0)"), 1.f));
        QVERIFY(isFloat(eval("pi"), 3.14159265f));
        QVERIFY(eval("ln(0)").isUndefined());
        QVERIFY(isFloat(eval("log(2, 8)"), 3.f));
        QVERIFY(isInt(eval("ifelse(1, 2, 3)"), 2));
        QVERIFY(isInt(eval("cond(0, 2, 3)"), 3));

        FakeContext context;
        eval("ifelse(1, 2, var(1) := 7)", context);
        QCOMPARE(context.vars[1], 7);
        eval("cond(1, 2, var(2) := 7)", context);
        QVERIFY(context.vars.find(2) == context.vars.end());
    }

    void specialForms()
    {
        FakeContext context;
        QVERIFY(isInt(eval("StateType = S", context), 1));
        QVERIFY(isInt(eval("statetype != s", context), 0));
        QVERIFY(isInt(eval("StateType = A || StateType = S", context), 1));
        QVERIFY(isInt(eval("command = \"QCF_x\"", context), 1));
        QVERIFY(isInt(eval("command != \"qcf_y\" && time > 1", context), 1));
        QVERIFY(isInt(eval("AnimElem = 3, >= 2", context), 1));
        QVERIFY(isInt(eval("AnimElem = 3, < 2", context), 0));
        QVERIFY(isInt(eval("AnimElem = 3, = 2", context), 1));
        QVERIFY(isInt(eval("AnimElem = 3", context), 0));
        QVERIFY(isInt(eval("AnimElem = 4 || AnimElem = 3, 2", context), 1));
        QVERIFY(isInt(eval("TimeMod = 5, 0", context), 1));
        QVERIFY(isInt(eval("TimeMod = 3, 1", context), 0));

        QVERIFY(isInt(eval("HitDefAttr = SCA, NA, SA", context), 1));
        QCOMPARE(context.lastKeyword, QStringLiteral("SCA"));
        QCOMPARE(context.lastList, QStringList({"NA", "SA"}));
        QVERIFY(isInt(eval("ProjContact1234 = 1", context), 1));
        QCOMPARE(context.lastKeyword, QStringLiteral("1234"));
    }

    void redirections()
    {
        FakeContext parent;
        parent.time = 99;
        FakeContext context;
        context.parent = &parent;
        QVERIFY(isInt(eval("parent, time", context), 99));
        QVERIFY(isInt(eval("parent, time + time", context), 104));
        QVERIFY(isInt(eval("parent, statetype = S", context), 1));
        QVERIFY(eval("root, time", context).isUndefined());
        QVERIFY(eval("helper(1234), life", context).isUndefined());
        QCOMPARE(context.lastRedirect, Redirect::Helper);
        QVERIFY(isInt(context.lastArgument, 1234));
        QVERIFY(isInt(eval("enemynear(1), life = 0 || 1", context), 1));
        QCOMPARE(context.lastRedirect, Redirect::EnemyNear);
    }

    void assignment()
    {
        FakeContext context;
        QVERIFY(isInt(eval("var(5) := 3 + 4", context), 7));
        QCOMPARE(context.vars[5], 7);
        QVERIFY(isFloat(eval("fvar(2) := 1.5", context), 1.5f));
        QCOMPARE(context.fvars[2], 1.5f);
        QVERIFY(isInt(eval("var(1) := 2 = 2", context), 1));
        QCOMPARE(context.vars[1], 1);
        eval("var(1) := var(2) := 9", context);
        QCOMPARE(context.vars[1], 9);
        QCOMPARE(context.vars[2], 9);
    }

    void shortCircuit()
    {
        FakeContext context;
        eval("0 && (var(1) := 1)", context);
        eval("1 || (var(2) := 1)", context);
        QVERIFY(context.vars.empty());
        eval("1 && (var(3) := 1)", context);
        QCOMPARE(context.vars[3], 1);
    }

    void lists()
    {
        FakeContext context;
        QString error;
        const std::vector<Expr> list = Expr::compileList("1, time * 2, pos x", &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(list.size(), std::size_t(3));
        QVERIFY(isInt(list[1].evaluate(context), 10));
        QVERIFY(isFloat(list[2].evaluate(context), 10.5f));
        QVERIFY(Expr::compile("-3").isConstant());
        QVERIFY(!Expr::compile("time").isConstant());
        QVERIFY(Expr::compile("").isEmpty());
        QVERIFY(Expr().evaluate(context).isUndefined());
        QVERIFY(isInt(Expr::constant(Value::integer(4)).evaluate(context), 4));
        QVERIFY(isInt(eval("1 + 1 ; + 5", context), 2));
    }

    void errors()
    {
        QVERIFY(!errorOf("1 +").isEmpty());
        QVERIFY(!errorOf("(1 + 2").isEmpty());
        QVERIFY(!errorOf("frobnicate").isEmpty());
        QVERIFY(!errorOf("var").isEmpty());
        QVERIFY(!errorOf("var(1, 2)").isEmpty());
        QVERIFY(!errorOf("pos").isEmpty());
        QVERIFY(!errorOf("command = x").isEmpty());
        QVERIFY(!errorOf("statetype > S").isEmpty());
        QVERIFY(!errorOf("time := 1").isEmpty());
        QVERIFY(!errorOf("\"open").isEmpty());
        QVERIFY(!errorOf("1 $ 2").isEmpty());
        QVERIFY(!errorOf("parent time").isEmpty());
        QVERIFY(Expr::compile("1 +").isEmpty());
    }
};

QTEST_GUILESS_MAIN(RuntimeExprTest)
#include "RuntimeExprTest.moc"

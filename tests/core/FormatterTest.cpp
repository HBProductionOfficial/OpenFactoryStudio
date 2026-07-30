#include "core/code/CodeFormatter.h"

#include <QTest>

using namespace OFS;
using namespace OFS::CodeFormatter;

namespace {

// Lines first to last are 1-based. 0 means every line.
QString run(const QString& text, Mode mode, int first = 0, int last = 0, const ReformatPredicate& reformat = {})
{
    QStringList lines = text.split(QLatin1Char('\n'));
    if (first == 0) {
        first = 1;
        last = int(lines.size());
    }
    const QStringList result = process(lines, first - 1, last - 1, mode, reformat);
    for (int i = 0; i < result.size(); ++i)
        lines[first - 1 + i] = result.at(i);
    return lines.join(QLatin1Char('\n'));
}

QString indent(const QString& text, int first = 0, int last = 0)
{
    return run(text, Mode::Indent, first, last);
}

bool exceptCommands(const QString& header, const QString&, const QString& key)
{
    static const QStringList constants = {"Data", "Size", "Velocity", "Movement", "Remap"};
    return !constants.contains(header, Qt::CaseInsensitive) && key.compare("command", Qt::CaseInsensitive) != 0 &&
           key.compare("name", Qt::CaseInsensitive) != 0;
}

QString format(const QString& text)
{
    return run(text, Mode::Format, 0, 0, exceptCommands);
}

} // namespace

class FormatterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void indentMainExample()
    {
        const QString input = "[Statedef 8999]\n  type = S\n\tctrl = 0\n\n; header comment col0\n  [Statedef 9000]\n"
                              "type = S\n  movetype = I\n\tphysics = S\n        anim = 0\n    ; indented comment\n"
                              ";comment col0\n\n\t[State 9000, 1]\ntype = ChangeState\n  trigger1 = Time = 0\n"
                              "\ttrigger2 = AnimTime = 0\n        value = 0\n   ;  comment in state\n \tctrl = 1\n\n"
                              "    [State 9000, 2]\ntype = VarSet\ntrigger1 = 1\nv = 1   \n\n[State 9000, 3]\n"
                              "    type = Null\n\ttrigger1 = 1";
        const QString expected = "[Statedef 8999]\n  type = S\n\tctrl = 0\n\n; header comment col0\n[Statedef 9000]\n"
                                 "type\t= S\nmovetype= I\nphysics\t= S\nanim\t= 0\n; indented comment\n"
                                 ";comment col0\n\n[State 9000, 1]\ntype\t= ChangeState\ntrigger1= Time = 0\n"
                                 "trigger2= AnimTime = 0\nvalue\t= 0\n;  comment in state\nctrl\t= 1\n\n"
                                 "[State 9000, 2]\ntype\t= VarSet\ntrigger1= 1\nv\t\t= 1\n\n[State 9000, 3]\n"
                                 "    type = Null\n\ttrigger1 = 1";
        QCOMPARE(indent(input, 5, 25), expected);
    }

    void indentColumns_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        const QString h = "[State 1, 1]\n";
        QTest::newRow("C1") << h + "ab = 1 ;c" << h + "ab\t= 1\t\t;c";
        QTest::newRow("C2") << h + "ab = 12 ;c" << h + "ab\t= 12\t\t;c";
        QTest::newRow("C3") << h + "ab = 12;c" << h + "ab\t= 12\t\t;c";
        QTest::newRow("C4") << h + "ab = 1  ;c" << h + "ab\t= 1\t\t;c";
        QTest::newRow("C5") << h + "abcd = 1\nv = 2" << h + "abcd= 1\nv\t= 2";
        QTest::newRow("C6") << h + "abcd=1\nvalue=2\nv= 3\nw =4" << h + "abcd\t=1\nvalue\t=2\nv\t\t= 3\nw\t\t=4";
        QTest::newRow("C7") << h + "abc = 1234567890123\nab = 1 ;c" << h + "abc\t= 1234567890123\nab\t= 1\t\t\t\t\t;c";
        QTest::newRow("C9") << "[State 1, 1] ; hdr\n  ; only comment\n  \t \nab = 1"
                            << "[State 1, 1] ; hdr\n; only comment\n\nab\t= 1";
        QTest::newRow("C14") << h + "name = \"a;b\"\nx = 1" << h + "name= \"a;b\"\nx\t= 1";
        QTest::newRow("C16") << h + "ab = 1\n[State 1, 2]\nverylongparametername = 1"
                             << h + "ab\t\t\t\t\t\t= 1\n[State 1, 2]\nverylongparametername\t= 1";
        QTest::newRow("C15")
            << "  verylongparametername = 1 ;x\n[State 1, 1]\nab = 1\n[State 1, 2]\ncdefgh = 2 ;y"
            << "verylongparametername = 1 ;x\n[State 1, 1]\nab\t\t= 1\n[State 1, 2]\ncdefgh\t= 2\t\t;y";
        QTest::newRow("P14") << "[Data]\nab = 1\n[Size]\nabc = 1\n[Statedef 1]\nabcd = 1\n[Movement]\nabcdefghijklm = 1"
                             << "[Data]\nab\t\t\t\t= 1\n[Size]\nabc\t\t\t\t= 1\n[Statedef 1]\nabcd= 1\n[Movement]\n"
                                "abcdefghijklm\t= 1";
        QTest::newRow("no equals") << h + "  noequalsline  " << h + "noequalsline";
    }

    void indentColumns()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(indent(input), expected);
    }

    void partialSelections()
    {
        const QString text = "[State 1, 1]\nignorehitpause = 1\n  ab = 1\n  cd = 2\n  ef = 3";
        QCOMPARE(indent(text, 3, 4), QStringLiteral("[State 1, 1]\nignorehitpause = 1\nab = 1\ncd = 2\n  ef = 3"));
        QCOMPARE(indent(text, 2, 4), QStringLiteral("[State 1, 1]\nignorehitpause = 1\nab = 1\ncd = 2\n  ef = 3"));
        QCOMPARE(indent(text, 1, 4),
                 QStringLiteral("[State 1, 1]\nignorehitpause\t= 1\nab\t\t\t\t= 1\ncd\t\t\t\t= 2\n  ef = 3"));
    }

    void formatValues()
    {
        const QString input = "  [Statedef 9200]   \ntype=S\nLIFE = 1000\nlife    =   1000\n  movetype = I   \n\n\n"
                              "[State 9200, 1]\ntype=ChangeState\nvalue = 1,2\nvalue = 1 , 2\nx = 1 ;comment\n"
                              "x = 1;comment\ntrigger1 = time=0&&animelem=3\ntrigger1 = Time = [0,10]\n"
                              "trigger2 = Time=[0 , 10] || ( var(1) >2 )\nTRIGGER3 = AnimElemTime(2)>=0\n"
                              "\tvalue = 1   \n     ;  indented comment   ";
        const QString expected = "[Statedef 9200]\ntype\t= S\nLIFE\t= 1000\nlife\t=  1000\nmovetype= I\n\n\n"
                                 "[State 9200, 1]\ntype\t= ChangeState\nvalue\t= 1, 2\nvalue\t= 1, 2\n"
                                 "x\t\t= 1\t\t\t\t\t\t\t\t\t\t;comment\nx\t\t= 1\t\t\t\t\t\t\t\t\t\t;comment\n"
                                 "trigger1= time = 0 && animelem = 3\ntrigger1= Time = [0, 10]\n"
                                 "trigger2= Time = [0, 10] || ( var(1) > 2 )\nTRIGGER3= AnimElemTime(2) >= 0\n"
                                 "value\t= 1\n;  indented comment";
        QCOMPARE(format(input), expected);

        QCOMPARE(format("[Command]\nname = \"QCF_x\"\ncommand = ~D, DF, F, x\ncommand = /F , a+b\n"
                        "command = >~F,  x ;c"),
                 QStringLiteral("[Command]\nname\t= \"QCF_x\"\ncommand\t= ~D, DF, F, x\ncommand\t= /F , a+b\n"
                                "command\t= >~F,  x\t\t\t;c"));
    }

    void expressions_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        const char* cases[][2] = {
            {"1,2", "1, 2"},
            {"1 , 2", "1, 2"},
            {"time=0&&animelem=3", "time = 0 && animelem = 3"},
            {"Time=[0 , 10] || ( var(1) >2 )", "Time = [0, 10] || ( var(1) > 2 )"},
            {"AnimElemTime(2)>=0", "AnimElemTime(2) >= 0"},
            {"(var(1)>2)", "(var(1) > 2)"},
            {"-1", "-1"},
            {"-2,-3", "-2, -3"},
            {"1-2", "1-2"},
            {"1 -2", "1 -2"},
            {"-var(1)*-1", "- var(1) * -1"},
            {"!time&&!(var(2)!=3)", "!time && !(var(2) != 3)"},
            {"var(3):=4", "var(3) := 4"},
            {"a<=b||a<b||a>=b", "a <= b || a < b || a >= b"},
            {"2**3%4/5", "2 ** 3 % 4 / 5"},
            {".5,-.5", ".5, - .5"},
            {"\"Kung,Fu  Man=1\"", "\"Kung,Fu  Man=1\""},
            {"SCA,NA,SA", "SCA, NA, SA"},
            {"ifelse(a,b,c)", "ifelse(a, b, c)"},
            {"Const(data.life)*2", "Const(data.life) * 2"},
            {"enemynear,pos x", "enemynear, pos x"},
            {"abs(-1)", "abs(-1)"},
            {"1  +   2", "1 + 2"},
            {"a&b|c^d", "a & b | c ^ d"},
            {"a^^b", "a ^^ b"},
            {"~a", "~a"},
            {"time=[1,2)", "time = [1, 2)"},
            {"3 , >=2", "3, >= 2"},
            {"time%2=0", "time % 2 = 0"},
            {"var(1):=var(1)+1", "var(1) := var(1) + 1"},
            {"pos y>=-10", "pos y >= -10"},
            {"vel x<-1.5", "vel x < -1.5"},
            {"1*(2+3)", "1 * (2 + 3)"},
            {"( 1 )", "( 1 )"},
            {"fvar(1)=1.5", "fvar(1) = 1.5"},
        };
        for (const auto& c : cases)
            QTest::newRow(c[0]) << QString::fromLatin1(c[0]) << QString::fromLatin1(c[1]);
    }

    void expressions()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(formatExpression(input), expected);
    }
};

QTEST_GUILESS_MAIN(FormatterTest)
#include "FormatterTest.moc"

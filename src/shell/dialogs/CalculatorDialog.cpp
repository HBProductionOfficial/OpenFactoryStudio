#include "shell/dialogs/CalculatorDialog.h"

#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/settings/SettingsImporter.h"

#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScreen>

namespace OFS {

namespace {

QPointer<CalculatorDialog> g_calculator;

const QString kX = QStringLiteral("calculator_x");
const QString kY = QStringLiteral("calculator_y");

} // namespace

CalculatorDialog::CalculatorDialog(Config* config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_display(new QLineEdit(this))
{
    if (!m_config && AppContext::instance())
        m_config = &AppContext::instance()->config();

    setObjectName(QStringLiteral("Calculator"));
    setWindowTitle(tr("Calculator"));

    m_display->setObjectName(QStringLiteral("display"));
    m_display->setReadOnly(true);
    m_display->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_display->setFocusPolicy(Qt::ClickFocus);
    m_display->installEventFilter(this);

    auto* grid = new QGridLayout(this);
    grid->setHorizontalSpacing(0);
    grid->setVerticalSpacing(2);
    grid->addWidget(m_display, 0, 0, 1, 7);

    using B = Calculator::Binary;
    using U = Calculator::Unary;
    auto digit = [this](int d) {
        return [this, d] {
            m_calculator.inputDigit(d);
        };
    };
    auto binary = [this](B op) {
        return [this, op] {
            m_calculator.applyBinary(op);
        };
    };
    auto unary = [this](U op) {
        return [this, op] {
            m_calculator.applyUnary(op);
        };
    };

    addButton(QStringLiteral("7"), QStringLiteral("digit7"), 1, 0, 1, digit(7));
    addButton(QStringLiteral("8"), QStringLiteral("digit8"), 1, 1, 1, digit(8));
    addButton(QStringLiteral("9"), QStringLiteral("digit9"), 1, 2, 1, digit(9));
    addButton(QStringLiteral("/"), QStringLiteral("divide"), 1, 3, 1, binary(B::Divide));
    addButton(QStringLiteral("%"), QStringLiteral("modulo"), 1, 4, 1, binary(B::Modulo));
    addButton(tr("sqrt"), QStringLiteral("sqrt"), 1, 5, 1, unary(U::SquareRoot));
    addButton(QStringLiteral("&&"), QStringLiteral("bitAnd"), 1, 6, 1, binary(B::BitAnd));

    addButton(QStringLiteral("4"), QStringLiteral("digit4"), 2, 0, 1, digit(4));
    addButton(QStringLiteral("5"), QStringLiteral("digit5"), 2, 1, 1, digit(5));
    addButton(QStringLiteral("6"), QStringLiteral("digit6"), 2, 2, 1, digit(6));
    addButton(QStringLiteral("*"), QStringLiteral("multiply"), 2, 3, 1, binary(B::Multiply));
    addButton(QStringLiteral("**"), QStringLiteral("power"), 2, 4, 1, binary(B::Power));
    addButton(tr("sin"), QStringLiteral("sin"), 2, 5, 1, unary(U::Sine));
    addButton(QStringLiteral("|"), QStringLiteral("bitOr"), 2, 6, 1, binary(B::BitOr));

    addButton(QStringLiteral("1"), QStringLiteral("digit1"), 3, 0, 1, digit(1));
    addButton(QStringLiteral("2"), QStringLiteral("digit2"), 3, 1, 1, digit(2));
    addButton(QStringLiteral("3"), QStringLiteral("digit3"), 3, 2, 1, digit(3));
    addButton(QStringLiteral("-"), QStringLiteral("subtract"), 3, 3, 1, binary(B::Subtract));
    addButton(QStringLiteral("1/x"), QStringLiteral("reciprocal"), 3, 4, 1, unary(U::Reciprocal));
    addButton(tr("cos"), QStringLiteral("cos"), 3, 5, 1, unary(U::Cosine));
    addButton(QStringLiteral("^"), QStringLiteral("bitXor"), 3, 6, 1, binary(B::BitXor));

    addButton(QStringLiteral("0"), QStringLiteral("digit0"), 4, 0, 1, digit(0));
    addButton(QStringLiteral("."), QStringLiteral("point"), 4, 1, 1, [this] { m_calculator.inputPoint(); });
    addButton(QStringLiteral("±"), QStringLiteral("sign"), 4, 2, 1, [this] { m_calculator.toggleSign(); });
    addButton(QStringLiteral("+"), QStringLiteral("add"), 4, 3, 1, binary(B::Add));
    addButton(QStringLiteral("!n"), QStringLiteral("factorial"), 4, 4, 1, unary(U::Factorial));
    addButton(tr("tan"), QStringLiteral("tan"), 4, 5, 1, unary(U::Tangent));
    addButton(QStringLiteral("~"), QStringLiteral("bitNot"), 4, 6, 1, unary(U::BitNot));

    addButton(tr("pi"), QStringLiteral("pi"), 5, 0, 1, [this] { m_calculator.inputPi(); });
    addButton(tr("clear"), QStringLiteral("clear"), 5, 1, 2, [this] { m_calculator.clear(); });
    addButton(tr("back"), QStringLiteral("back"), 5, 3, 2, [this] { m_calculator.backspace(); });
    addButton(QStringLiteral("="), QStringLiteral("equals"), 5, 5, 2, [this] { m_calculator.equals(); });

    for (int column = 0; column < 7; ++column)
        grid->setColumnStretch(column, 1);
    refresh();
    resize(547, 181);
}

CalculatorDialog::~CalculatorDialog() = default;

CalculatorDialog* CalculatorDialog::showCalculator(QWidget* parent)
{
    if (!g_calculator) {
        g_calculator = new CalculatorDialog(nullptr, parent);
        if (!parent)
            connect(qApp, &QCoreApplication::aboutToQuit, g_calculator.data(), &QObject::deleteLater);
    }
    g_calculator->show();
    g_calculator->raise();
    g_calculator->activateWindow();
    return g_calculator;
}

CalculatorDialog* CalculatorDialog::instance()
{
    return g_calculator;
}

QString CalculatorDialog::display() const
{
    return m_display->text();
}

QPushButton* CalculatorDialog::addButton(const QString& text, const QString& name, int row, int column, int columnSpan,
                                         const std::function<void()>& action)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName(name);
    button->setAutoDefault(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setMinimumWidth(0);
    connect(button, &QPushButton::clicked, this, [this, action] {
        action();
        refresh();
    });
    static_cast<QGridLayout*>(layout())->addWidget(button, row, column, 1, columnSpan);
    return button;
}

void CalculatorDialog::refresh()
{
    m_display->setText(m_calculator.display());
}

bool CalculatorDialog::handleKey(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy)) {
        QGuiApplication::clipboard()->setText(m_display->hasSelectedText() ? m_display->selectedText()
                                                                           : m_display->text());
        return true;
    }
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;

    using B = Calculator::Binary;
    using U = Calculator::Unary;
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Equal:
        m_calculator.equals();
        break;
    case Qt::Key_Backspace:
        m_calculator.backspace();
        break;
    case Qt::Key_Delete:
        m_calculator.clear();
        break;
    default: {
        const QString text = event->text();
        if (text.size() != 1)
            return false;
        const QChar c = text[0];
        if (c.isDigit())
            m_calculator.inputDigit(c.digitValue());
        else if (c == u'.' || c == u',')
            m_calculator.inputPoint();
        else if (c == u'+')
            m_calculator.applyBinary(B::Add);
        else if (c == u'-')
            m_calculator.applyBinary(B::Subtract);
        else if (c == u'*')
            m_calculator.applyBinary(B::Multiply);
        else if (c == u'/')
            m_calculator.applyBinary(B::Divide);
        else if (c == u'%')
            m_calculator.applyBinary(B::Modulo);
        else if (c == u'&')
            m_calculator.applyBinary(B::BitAnd);
        else if (c == u'|')
            m_calculator.applyBinary(B::BitOr);
        else if (c == u'^')
            m_calculator.applyBinary(B::BitXor);
        else if (c == u'~')
            m_calculator.applyUnary(U::BitNot);
        else if (c == u'!')
            m_calculator.applyUnary(U::Factorial);
        else
            return false;
        break;
    }
    }
    refresh();
    return true;
}

void CalculatorDialog::keyPressEvent(QKeyEvent* event)
{
    if (handleKey(event))
        return;
    QDialog::keyPressEvent(event);
}

bool CalculatorDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_display && event->type() == QEvent::KeyPress && handleKey(static_cast<QKeyEvent*>(event)))
        return true;
    return QDialog::eventFilter(watched, event);
}

void CalculatorDialog::showEvent(QShowEvent* event)
{
    if (!m_positionRestored) {
        restorePosition();
        m_positionRestored = true;
    }
    QDialog::showEvent(event);
    Q_EMIT visibilityChanged(true);
}

void CalculatorDialog::hideEvent(QHideEvent* event)
{
    storePosition();
    QDialog::hideEvent(event);
    Q_EMIT visibilityChanged(false);
}

void CalculatorDialog::moveEvent(QMoveEvent* event)
{
    QDialog::moveEvent(event);
    if (isVisible() && m_positionRestored)
        storePosition();
}

void CalculatorDialog::restorePosition()
{
    if (!m_config)
        return;
    bool okX = false;
    bool okY = false;
    const int x = m_config->value(kX).trimmed().toInt(&okX);
    const int y = m_config->value(kY).trimmed().toInt(&okY);
    if (!okX || !okY)
        return;
    QList<QRect> screens;
    const auto all = QGuiApplication::screens();
    for (const QScreen* screen : all)
        screens << screen->availableGeometry();
    move(SettingsImporter::fitToScreens(QPoint(x, y), screens, size()));
}

void CalculatorDialog::storePosition()
{
    if (!m_config || !m_positionRestored)
        return;
    m_config->setValue(kX, pos().x());
    m_config->setValue(kY, pos().y());
}

} // namespace OFS

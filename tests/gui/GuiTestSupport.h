#pragma once

// Helpers shared by the GUI tests that drive the main window.

#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTest>
#include <QTimer>
#include <QToolBar>

#include <functional>

namespace OFS::GuiTest {

inline void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

inline QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Key text as menus show it on this platform (Ctrl+O, or the command symbol on macOS).
inline QString keyText(const char* keys)
{
    return QKeySequence(QString::fromLatin1(keys)).toString(QKeySequence::NativeText);
}

// Message boxes on macOS have no title, as the platform guidelines require.
inline QString boxTitle(const QString& title)
{
#ifdef Q_OS_MACOS
    Q_UNUSED(title);
    return {};
#else
    return title;
#endif
}

inline QStringList visibleMenuTitles(QMainWindow& window)
{
    QStringList titles;
    for (QAction* a : window.menuBar()->actions()) {
        if (a->isVisible() && a->menu())
            titles << a->menu()->title();
    }
    return titles;
}

inline QStringList toolBarIds(QToolBar* bar)
{
    QStringList ids;
    for (QAction* a : bar->actions())
        ids << (a->isSeparator() ? QStringLiteral("|") : a->objectName());
    return ids;
}

inline QMenu* menu(QMainWindow& window, const QString& objectName)
{
    return window.findChild<QMenu*>(objectName);
}

// A menu's items as object names ("|" for a separator, "<title>" for a submenu without one).
inline QStringList menuIds(const QMenu* menu)
{
    QStringList ids;
    for (QAction* a : menu->actions()) {
        if (a->isSeparator())
            ids << QStringLiteral("|");
        else if (!a->objectName().isEmpty())
            ids << a->objectName();
        else
            ids << QLatin1Char('<') + a->text().remove(QLatin1Char('&')) + QLatin1Char('>');
    }
    return ids;
}

// Runs `answer` on the next modal window of type T once it is open. Another modal window may
// close first.
template <typename T>
void whenModal(std::function<void(T*)> answer)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, answer] {
        if (auto* window = qobject_cast<T*>(QApplication::activeModalWidget()); window && window->isVisible()) {
            timer->stop();
            timer->deleteLater();
            answer(window);
        }
    });
    timer->start();
}

inline void answerBoxes(QList<QMessageBox::StandardButton> buttons, QStringList* texts)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, buttons, texts]() mutable {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box || !box->isVisible())
            return;
        if (texts)
            *texts << box->text();
        box->button(buttons.takeFirst())->click();
        if (buttons.isEmpty()) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
}

inline void answerDiscard(QMessageBox::StandardButton button)
{
    // Waits for the box to show because a busy machine may fire the timer first.
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, button] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box || !box->isVisible())
            return;
        timer->stop();
        timer->deleteLater();
        QCOMPARE(box->windowTitle(), boxTitle("Discard changes?"));
        box->button(button)->click();
    });
    timer->start();
}

// App context for the main window tests, with an empty history and the quick search off.
inline AppContext* makeContext()
{
    qputenv("OFS_AUDIO_BACKEND", "null"); // playback without a sound card
    auto* context = new AppContext;
    context->loadSettings();
    context->history().clear(); // the history is kept on disk between runs
    // So are the settings a closed window saves.
    context->config().setValue("lastEditor", QString());
    context->config().setValue("show_quicksearch", false);
    context->initializeEngines();
    return context;
}

} // namespace OFS::GuiTest

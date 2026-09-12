#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"

#include <QAbstractButton>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

using namespace OFS;

// Translates every text lupdate finds to "~text" and lists what the main window still shows in English.
class TranslationCoverageTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QTranslator m_translator;
    QString m_lupdateLog;

    static void writeFile(const QString& path, const QByteArray& data)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

    static bool untranslated(const QString& text)
    {
        static const QRegularExpression letters(QStringLiteral("[A-Za-z]{2,}"));
        return !text.isEmpty() && !text.startsWith(QLatin1Char('~')) && letters.match(text).hasMatch();
    }

    static void collectMenu(QMenu* menu, const QString& path, QStringList& out)
    {
        if (untranslated(menu->title()))
            out << path + menu->title();
        for (QAction* a : menu->actions()) {
            if (a->menu())
                collectMenu(a->menu(), path + menu->title() + QStringLiteral(" > "), out);
        }
    }

private Q_SLOTS:
    void initTestCase()
    {
        const QString lupdate = QStringLiteral(OFS_LUPDATE);
        const QString lrelease = QStringLiteral(OFS_LRELEASE);
        if (lupdate.isEmpty() || lrelease.isEmpty() || !QFileInfo::exists(lupdate) || !QFileInfo::exists(lrelease))
            QSKIP("Qt Linguist tools are not installed");
        QVERIFY(m_dir.isValid());
        const QString sources = QStringLiteral(OFS_SOURCE_DIR "/src");
        const QString ts = m_dir.filePath(QStringLiteral("pseudo.ts"));
        const QString qm = m_dir.filePath(QStringLiteral("pseudo.qm"));

        QProcess update;
        update.setProcessChannelMode(QProcess::MergedChannels);
        update.start(lupdate, {QStringLiteral("-I"), sources, QStringLiteral("-locations"), QStringLiteral("none"),
                               sources, QStringLiteral("-ts"), ts});
        QVERIFY(update.waitForFinished(300000));
        m_lupdateLog = QString::fromLocal8Bit(update.readAll());
        QCOMPARE(update.exitCode(), 0);

        QFile file(ts);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString text = QString::fromUtf8(file.readAll());
        file.close();
        static const QRegularExpression message(
            QStringLiteral("<source>([^<]*)</source>((?:\\s*<(?:comment|extracomment)>[^<]*</(?:comment|"
                           "extracomment)>)*)(\\s*)<translation type=\"unfinished\"></translation>"));
        text.replace(message, QStringLiteral("<source>\\1</source>\\2\\3<translation>~\\1</translation>"));
        writeFile(ts, text.toUtf8());

        QProcess release;
        release.setProcessChannelMode(QProcess::MergedChannels);
        release.start(lrelease, {QStringLiteral("-silent"), ts, QStringLiteral("-qm"), qm});
        QVERIFY(release.waitForFinished(120000));
        QCOMPARE(release.exitCode(), 0);
        QVERIFY(m_translator.load(qm));
        QVERIFY(QCoreApplication::installTranslator(&m_translator));
    }

    // A lupdate warning means texts filed under a context the program does not use.
    void lupdateWarnings()
    {
        static const QRegularExpression warning(QStringLiteral("^.+:\\d+: .+$"), QRegularExpression::MultilineOption);
        QStringList warnings;
        for (auto it = warning.globalMatch(m_lupdateLog); it.hasNext();)
            warnings << it.next().captured(0);
        if (!warnings.isEmpty())
            qWarning().noquote() << warnings.join(QLatin1Char('\n'));
        QVERIFY(warnings.isEmpty());
    }

    void mainWindow()
    {
        qputenv("OFS_AUDIO_BACKEND", "null");
        AppContext context;
        context.loadSettings();
        context.history().clear();
        context.config().setValue("lastEditor", QString());
        context.initializeEngines();

        writeFile(m_dir.filePath("fighter.def"), "[Info]\nname = \"Test Fighter\"\nmugenversion = 1.0\n"
                                                 "[Files]\ncmd = fighter.cmd\ncns = fighter.cns\nst = fighter.cns\n"
                                                 "sprite = fighter.sff\nanim = fighter.air\nsound = fighter.snd\n");
        writeFile(m_dir.filePath("fighter.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(m_dir.filePath("fighter.cns"), "[Data]\nlife = 1000\n");
        writeFile(m_dir.filePath("fighter.air"), "[Begin Action 0]\n0,0, 0,0, 10\n");

        MainWindow window(context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        ProjectView* view = window.currentProjectView();
        QVERIFY(view);
        // Editors build their panels when first shown.
        for (const QString& id : view->editorIds())
            view->setCurrentEditor(id);

        ActionRegistry& actions = context.actions();
        QCOMPARE(actions.caption(QStringLiteral("file_new")), QStringLiteral("~New"));
        QCOMPARE(window.menuBar()->actions().first()->text(), QStringLiteral("~Project"));

        QStringList missing;
        int checked = 0;
        for (const QString& id : actions.ids()) {
            ++checked;
            if (untranslated(actions.caption(id)))
                missing << QStringLiteral("action %1: %2").arg(id, actions.caption(id));
        }
        for (QAction* a : window.menuBar()->actions()) {
            if (a->menu())
                collectMenu(a->menu(), QStringLiteral("menu "), missing);
        }
        const auto widgetText = [&missing, &checked](QWidget* w, const QString& text) {
            ++checked;
            if (untranslated(text))
                missing << QStringLiteral("%1 %2: %3")
                               .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName(), text);
        };
        for (QLabel* label : view->findChildren<QLabel*>())
            widgetText(label, label->text());
        for (QAbstractButton* button : view->findChildren<QAbstractButton*>())
            widgetText(button, button->text());
        for (QGroupBox* box : view->findChildren<QGroupBox*>())
            widgetText(box, box->title());

        missing.removeIf([](const QString& s) { return s.endsWith(QLatin1String("Test Fighter")); });
        missing.removeDuplicates();
        qInfo() << checked << "texts checked";
        if (!missing.isEmpty())
            qWarning().noquote() << missing.size() << "untranslated:\n" + missing.join(QLatin1Char('\n'));
        QVERIFY(missing.isEmpty());
    }

    void cleanupTestCase() { QCoreApplication::removeTranslator(&m_translator); }
};

QTEST_MAIN(TranslationCoverageTest)
#include "TranslationCoverageTest.moc"

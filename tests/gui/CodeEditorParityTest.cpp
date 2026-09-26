// Code editor, code toolbar and code tree tests.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/Paths.h"
#include "core/code/CodeCompletion.h"
#include "core/code/CodeEditor.h"
#include "core/code/CodeFormatter.h"
#include "core/code/CompletionModels.h"
#include "core/code/Snippet.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/project/ProjectFile.h"
#include "core/syntax/SyntaxDatabase.h"
#include "engines/mugen/MugenOutline.h"
#include "shell/AdvancedSearchPanel.h"
#include "shell/AnimationPage.h"
#include "shell/AnimationsEditor.h"
#include "shell/CodePage.h"
#include "shell/DocumentationPopup.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"
#include "shell/QuickFindBar.h"
#include "shell/dialogs/SnippetEditorDialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QToolButton>
#include <QTreeView>

using namespace OFS;
using namespace OFS::GuiTest;

class CodeEditorParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        QVERIFY(m_syntax.load(Paths::dataDir() + QStringLiteral("/engines/mugen/syntax.xml")));
        QVERIFY(m_models.load(Paths::dataDir() + QStringLiteral("/engines/mugen/completer.xml")));
        QVERIFY(m_dir.isValid());
        writeFile(m_dir.filePath("c.def"), "[Info]\nname = \"C\"\nmugenversion = 1.0\n"
                                           "[Files]\ncmd = c.cmd\ncns = c.cns\nst = c.cns\nst1 = c2.cns\n");
        writeFile(m_dir.filePath("c2.cns"), "[Statedef 900]\ntype = S\n\n[State 900, 1]\ntype = Null\n");
        writeFile(m_dir.filePath("q.def"),
                  "[Info]\nname = \"Q\"\nmugenversion = 1.0\n[Files]\nanim = q.air\ncns = q.cns\n");
        writeFile(m_dir.filePath("q.cns"), "[Data]\nlife = 1000\n");
        writeFile(m_dir.filePath("q.air"), "; Stand\n[Begin Action 0]\n-1,0, 0,0, 10\n\n; Walk\n[Begin Action 20]\n"
                                           "-1,0, 0,0, 10\n\n; Walk\n[Begin Action 21]\n-1,0, 0,0, 10\n\n"
                                           "; Jump\n[Begin Action 40]\n-1,0, 0,0, 10\n");
        // A snippet with one field of each type, in the user's snippet folder.
        Engine* mugen = m_context->engines().engine(QStringLiteral("mugen"));
        QVERIFY(mugen);
        QDir().mkpath(mugen->userSnippetDir());
        writeFile(QDir(mugen->userSnippetDir()).filePath("zz_test.xml"),
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<snippet>\r\n\r\n"
                  "\t<field name=\"num\" title=\"Num\" type=\"int\" />\r\n"
                  "\t<field name=\"txt\" title=\"Text\" type=\"string\" />\r\n"
                  "\t<field name=\"flt\" title=\"Flt\" type=\"float\" />\r\n"
                  "\t<field name=\"itm\" title=\"Pick\" type=\"items\" value=\"S,C,A\" "
                  "value_title=\"Stand,Crouch,Air\"/>\r\n\r\n"
                  "\t<code>\r\n\t\t    [State @num, zz]\r\n\t\t    type = Null\r\n"
                  "\t\t      trigger1 = @txt ; @flt\r\n\t\t    value = @itm\r\n"
                  "\t\t    x = @num@txt @zz @numx\r\n\t\t    v = CURSOR\r\n\t</code>\r\n\r\n</snippet>\r\n");
        mugen->reloadSnippets();
        writeFile(m_dir.filePath("c.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(m_dir.filePath("c.cns"), "[Data]\nlife = 1000\n\n[Statedef 0]\ntype = S\n\n"
                                           "[State 0, 1]\ntype = Null\ntrigger1 = 1\n");
    }

    void cleanupTestCase()
    {
        if (Engine* mugen = m_context->engines().engine(QStringLiteral("mugen")))
            QFile::remove(QDir(mugen->userSnippetDir()).filePath("zz_test.xml"));
        delete m_context;
    }

    // No smart editing. Tab and Shift+Tab type a tab even over a selection and Home always goes to column 1.
    void typing()
    {
        CodeEditor editor;
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        QCOMPARE(editor.tabStopDistance(), editor.fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4.0);
        const QString base = QStringLiteral("[State 9500, 1]\n    type = Null\n\ttrigger1 = 1\n        abc\nx = 1");
        auto line = [&editor](int n) {
            return editor.document()->findBlockByNumber(n).text();
        };
        auto caretAt = [&editor](int lineNumber, int column) {
            QTextCursor c(editor.document()->findBlockByNumber(lineNumber));
            c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
            editor.setTextCursor(c);
        };
        auto reset = [&] {
            editor.setPlainText(base);
        };

        reset();
        caretAt(1, int(line(1).size()));
        QTest::keyClick(&editor, Qt::Key_Return);
        QTest::keyClicks(&editor, "Q");
        QCOMPARE(line(2), QStringLiteral("Q"));
        reset();
        caretAt(1, 6);
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(line(1), QStringLiteral("    ty"));
        QCOMPARE(line(2), QStringLiteral("pe = Null"));

        reset();
        caretAt(4, 1);
        QTest::keyClick(&editor, Qt::Key_Tab);
        QCOMPARE(line(4), QStringLiteral("x\t = 1"));
        reset();
        caretAt(4, 0);
        QTest::keyClick(&editor, Qt::Key_Tab);
        QCOMPARE(line(4), QStringLiteral("\tx = 1"));
        reset();
        QTextCursor select(editor.document()->findBlockByNumber(4));
        select.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        editor.setTextCursor(select);
        QTest::keyClick(&editor, Qt::Key_Tab);
        QCOMPARE(line(4), QStringLiteral("\t = 1"));
        reset();
        auto selectLines = [&] {
            QTextCursor lines(editor.document()->findBlockByNumber(1));
            lines.setPosition(editor.document()->findBlockByNumber(3).position() + int(line(3).size()),
                              QTextCursor::KeepAnchor);
            editor.setTextCursor(lines);
        };
        selectLines();
        QTest::keyClick(&editor, Qt::Key_Tab);
        QCOMPARE(line(1), QStringLiteral("\t"));
        QCOMPARE(line(2), QStringLiteral("x = 1"));
        editor.undo();
        QCOMPARE(editor.toPlainText(), base);
        selectLines();
        QTest::keyClick(&editor, Qt::Key_Backtab);
        QCOMPARE(line(1), QStringLiteral("\t"));
        reset();
        caretAt(3, 9);
        QTest::keyClick(&editor, Qt::Key_Backtab);
        QCOMPARE(line(3), QStringLiteral("        a\tbc"));

        reset();
        caretAt(1, int(line(1).size()));
        QTest::keyClick(&editor, Qt::Key_Home);
        QCOMPARE(editor.textCursor().positionInBlock(), 0);
        QTest::keyClick(&editor, Qt::Key_Home);
        QCOMPARE(editor.textCursor().positionInBlock(), 0);
        caretAt(2, int(line(2).size()));
        QTest::keyClick(&editor, Qt::Key_Home);
        QCOMPARE(editor.textCursor().positionInBlock(), 0);

        reset();
        caretAt(4, 5);
        QTest::keyClicks(&editor, "[\"(");
        QCOMPARE(line(4), QStringLiteral("x = 1[\"("));

        reset();
        caretAt(3, 8);
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(line(3), QStringLiteral("       abc"));
        caretAt(2, 0);
        QTest::keyClick(&editor, Qt::Key_Delete);
        QCOMPARE(line(2), QStringLiteral("trigger1 = 1"));

        editor.setCompletionProvider(std::make_unique<SyntaxCompletionProvider>(&m_syntax, "state", &m_models));
        reset();
        caretAt(2, int(line(2).size()));
        QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
        QVERIFY(!editor.isCompleting());
        QCOMPARE(line(2), QStringLiteral("\ttrigger1 = 1"));
    }

    // No popup at the start of a line. Down picks the current item first and "(" closes the popup.
    void completion()
    {
        CodeEditor editor;
        editor.resize(600, 400);
        editor.setCompletionProvider(std::make_unique<SyntaxCompletionProvider>(&m_syntax, "state", &m_models));
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        auto start = [&editor](const QString& text) {
            editor.setPlainText(text);
            editor.moveCursor(QTextCursor::End);
        };
        auto last = [&editor] {
            return editor.document()->lastBlock().text();
        };

        start("[State 0, 1]\n");
        QTest::keyClicks(&editor, "tri");
        QVERIFY(!editor.isCompleting());

        start("[State 0, 1]\n");
        QTest::keyClicks(&editor, "type =");
        QVERIFY(editor.isCompleting());
        QAbstractItemView* popup = editor.completer()->popup();
        QCOMPARE(editor.completer()->maxVisibleItems(), 7);
        const QPoint caret = editor.viewport()->mapToGlobal(editor.cursorRect().bottomLeft());
        const QPoint corner = popup->mapToGlobal(QPoint(0, 0));
        QVERIFY2(corner.y() >= caret.y(), "under the line");
        QVERIFY2(qAbs(corner.x() - caret.x()) <= 2, "at the caret (Fighter Factory opened it 80 px to the left)");
        const QString first = popup->model()->index(0, 0).data().toString();
        QCOMPARE(first, QStringLiteral("AfterImage"));
        QTest::keyClick(popup, Qt::Key_Down);
        QTest::keyClick(popup, Qt::Key_Return);
        QCOMPARE(last(), QStringLiteral("type =AfterImage"));

        start("[State 0, 1]\n");
        QTest::keyClicks(&editor, "type =");
        popup = editor.completer()->popup();
        const QModelIndex third = popup->model()->index(2, 0);
        const QString thirdWord = third.data().toString();
        QTest::mouseClick(popup->viewport(), Qt::LeftButton, {}, popup->visualRect(third).center());
        QVERIFY(!editor.isCompleting());
        QCOMPARE(last(), QStringLiteral("type =") + thirdWord);

        start("[State 0, 1]\ntype = Null\n");
        QTest::keyClicks(&editor, "trigger1 = Ti");
        QVERIFY(editor.isCompleting());
        QTest::keyClicks(editor.completer()->popup(), "(");
        QVERIFY(!editor.isCompleting());
        QCOMPARE(last(), QStringLiteral("trigger1 = Ti("));
    }

    void formattingFollowsTheSyntax()
    {
        auto values = [](const QStringList& out) {
            QStringList v;
            for (const QString& l : out)
                v << (l.contains(QLatin1Char('=')) ? l.mid(l.indexOf(QLatin1Char('=')) + 1) : l);
            return v;
        };
        const QStringList cmd{"[Remap]",
                              "x=x",
                              "[Command]",
                              "name = \"QCF_x\"",
                              "command = /F , a+b",
                              "[State -1, run]",
                              "type = ChangeState",
                              "triggerall = command=\"FF\""};
        const QStringList cmdOut = CodeFormatter::process(cmd, 0, int(cmd.size()) - 1, CodeFormatter::Mode::Format,
                                                          CodeFormatter::syntaxPredicate(&m_syntax, "command"));
        QCOMPARE(values(cmdOut), (QStringList{"[Remap]", " x", "[Command]", " \"QCF_x\"", " /F , a+b",
                                              "[State -1, run]", " ChangeState", " command = \"FF\""}));

        const QStringList cns{
            "[Size]",         "head.pos = -5,-90", "[Statedef 200]", "velset = 0,0",  "poweradd = 10+var(1)",
            "[State 200, 1]", "type = HitDef",     "attr = S,NA",    "damage = 23,0", "p2stateno = 820+var(2)"};
        const QStringList cnsOut = CodeFormatter::process(cns, 0, int(cns.size()) - 1, CodeFormatter::Mode::Format,
                                                          CodeFormatter::syntaxPredicate(&m_syntax, "state"));
        QCOMPARE(values(cnsOut), (QStringList{"[Size]", " -5,-90", "[Statedef 200]", " 0, 0", " 10 + var(1)",
                                              "[State 200, 1]", " HitDef", " S, NA", " 23, 0", " 820 + var(2)"}));
    }

    // Auto-format writes tabs even when tab conversion is on.
    void formattingUndo()
    {
        CodeFile::setTabConversion(true, true);
        CodeEditor editor;
        editor.setSyntax(&m_syntax, QStringLiteral("state"));
        const QString text = QStringLiteral("[State 0, 1]\ntype=Null\ntrigger1 = time=0&&animelem=3\n");
        editor.setPlainText(text);
        editor.selectAll();
        editor.autoFormatSelection();
        const QString formatted = editor.toPlainText();
        QVERIFY(formatted != text);
        QVERIFY(formatted.contains(QLatin1Char('\t')));
        QVERIFY(formatted.contains(QStringLiteral("time = 0 && animelem = 3")));
        editor.undo();
        QCOMPARE(editor.toPlainText(), text);
        editor.redo();
        QCOMPARE(editor.toPlainText(), formatted);
        CodeFile::setTabConversion(false, false);
    }

    void codeMenu()
    {
        MainWindow window(*m_context);
        window.resize(1200, 800);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c.def")));
        ActionRegistry& actions = m_context->actions();
        ProjectView* view = window.currentProjectView();
        QMenu* code = menu(window, "menu_code");
        QVERIFY(code);
        QCOMPARE(menuIds(code), (QStringList{"code_goto", "code_prev_error", "code_next_error", "code_prev_deprecation",
                                             "code_next_deprecation", "|", "code_comment", "code_fold_all",
                                             "code_unfold_all", "|", "code_autoindent", "code_autoformat"}));
        QCOMPARE(actions.action("code_unfold_all")->text().section(QLatin1Char('\t'), 0, 0),
                 QStringLiteral("Unfold all code blocks"));
        for (const char* editor : {"editor_st", "editor_cmd", "editor_def"}) {
            actions.action(editor)->trigger();
            CodePage* page = view->currentCodePage();
            QVERIFY2(page, editor);
            CodeEditor* text = page->editor();
            text->setPlainText(QStringLiteral("[Info]\nx=1\n  y  = 2\n"));
            text->selectAll();
            actions.action("code_autoformat")->trigger();
            QVERIFY2(!text->toPlainText().contains(QStringLiteral("  y")), editor);
            QVERIFY2(text->toPlainText().contains(QStringLiteral("x\t= 1")), editor);
            text->undo();
        }
    }

    // Every field value is filled in, floats too, and the caret goes where CURSOR was.
    void snippetInsertion()
    {
        MainWindow window(*m_context);
        window.resize(1200, 800);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_st")->trigger();
        CodePage* page = window.currentProjectView()->currentCodePage();
        QVERIFY(page);
        auto* combo = page->findChild<QComboBox*>("codeSnippets");
        QVERIFY(combo && combo->findText("zz_test") >= 0);
        combo->setCurrentIndex(combo->findText("zz_test"));
        CodeEditor* editor = page->editor();
        const QString before = QStringLiteral("AAA\n    BBB CCC\nDDD");
        editor->setPlainText(before);
        auto caretBeforeCcc = [editor] {
            QTextCursor c(editor->document()->findBlockByNumber(1));
            c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 8);
            editor->setTextCursor(c);
        };

        caretBeforeCcc();
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("SnipFieldsDlg"));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
        });
        actions.action("code_snippet_insert")->trigger();
        QCOMPARE(editor->toPlainText(), before);

        caretBeforeCcc();
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->windowTitle(), QStringLiteral("zz_test"));
            QCOMPARE(dialog->width(), 300);
            auto* num = dialog->findChild<QSpinBox*>("num");
            auto* txt = dialog->findChild<QLineEdit*>("txt");
            auto* flt = dialog->findChild<QDoubleSpinBox*>("flt");
            auto* itm = dialog->findChild<QComboBox*>("itm");
            QVERIFY(num && txt && flt && itm);
            QCOMPARE(num->minimum(), -99999999);
            QCOMPARE(num->maximum(), 99999999);
            QCOMPARE(num->value(), 0);
            QCOMPARE(flt->decimals(), 2);
            QVERIFY(txt->text().isEmpty());
            QCOMPARE(itm->count(), 3);
            QCOMPARE(itm->currentText(), QStringLiteral("Stand"));
            num->setValue(42);
            txt->setText("hello world");
            flt->setValue(1.5);
            itm->setCurrentIndex(1);
            auto* ok = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            QVERIFY(ok->isDefault());
            ok->click();
        });
        actions.action("code_snippet_insert")->trigger();
        QCOMPARE(editor->toPlainText(), QStringLiteral("AAA\n    BBB [State 42, zz]\ntype = Null\n"
                                                       "trigger1 = hello world ; 1.50\nvalue = C\n"
                                                       "x = 42hello world @zz 42x\nv = \nCCC\nDDD"));
        QCOMPARE(editor->cursorLine(), 7);
        QCOMPARE(editor->textCursor().positionInBlock(), 4);
        editor->undo();
        QCOMPARE(editor->toPlainText(), before);

        QTextCursor select(editor->document()->findBlockByNumber(1));
        select.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 8);
        select.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 3);
        editor->setTextCursor(select);
        whenModal<QDialog>(
            [](QDialog* dialog) { dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click(); });
        actions.action("code_snippet_insert")->trigger();
        QVERIFY(!editor->toPlainText().contains(QStringLiteral("CCC")));
        QVERIFY(editor->toPlainText().startsWith(QStringLiteral("AAA\n    BBB [State 0, zz]")));
        editor->undo();

        editor->setPlainText(before);
        editor->moveCursor(QTextCursor::End);
        combo->setCurrentIndex(combo->findText("zz_test"));
        whenModal<QDialog>(
            [](QDialog* dialog) { dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click(); });
        combo->setFocus();
        combo->showPopup();
        QTRY_VERIFY(combo->view()->isVisible());
        QTest::keyClick(combo->view(), Qt::Key_Return);
        QTRY_VERIFY(editor->toPlainText().contains(QStringLiteral("[State 0, zz]")));
        editor->undo();
        editor->document()->setModified(false);
    }

    // The code shows left-trimmed. Removing a field and Esc do not ask first.
    void snippetEditor()
    {
        Snippet snippet;
        QVERIFY(snippet.load(QDir(m_context->engines().engine("mugen")->userSnippetDir()).filePath("zz_test.xml")));
        QTemporaryDir saveDir;
        SnippetEditorDialog dialog(snippet, saveDir.path(), &m_syntax);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        for (const char* name : {"saveBut", "previewBut", "saveTSBut", "insertBut", "add", "del", "up", "down"})
            QVERIFY2(dialog.findChild<QPushButton*>(name), name);
        QVERIFY(dialog.findChild<QPushButton*>("saveBut")->isDefault());
        auto* code = dialog.findChild<QPlainTextEdit*>("snipEdit");
        auto* fields = dialog.findChild<QListWidget*>("snipFields");
        QVERIFY(code && fields);
        QVERIFY(code->toPlainText().startsWith(QStringLiteral("[State @num, zz]\ntype = Null\ntrigger1 = @txt")));
        auto titles = [fields] {
            QStringList t;
            for (int row = 0; row < fields->count(); ++row)
                t << fields->item(row)->text();
            return t;
        };
        QCOMPARE(titles(), (QStringList{"Num", "Text", "Flt", "Pick"}));

        whenModal<QDialog>([](QDialog* add) {
            QCOMPARE(add->objectName(), QStringLiteral("SnipFieldDlg"));
            auto* type = add->findChild<QComboBox*>("type");
            auto* values = add->findChild<QPlainTextEdit*>("values");
            QVERIFY(type && values);
            type->setCurrentIndex(type->findText(QStringLiteral("items"), Qt::MatchFixedString));
            QVERIFY(values->isVisibleTo(add));
            add->findChild<QLineEdit*>("name")->setText("pos");
            add->findChild<QLineEdit*>("title")->setText("Position");
            values->setPlainText("1\n2");
            add->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        dialog.findChild<QPushButton*>("add")->click();
        QCOMPARE(titles().last(), QStringLiteral("Position"));
        fields->setCurrentRow(4);
        dialog.findChild<QPushButton*>("up")->click();
        QCOMPARE(titles(), (QStringList{"Num", "Text", "Flt", "Position", "Pick"}));
        dialog.findChild<QPushButton*>("down")->click();
        QCOMPARE(titles(), (QStringList{"Num", "Text", "Flt", "Pick", "Position"}));
        fields->setCurrentRow(4);
        dialog.findChild<QPushButton*>("del")->click();
        QCOMPARE(titles(), (QStringList{"Num", "Text", "Flt", "Pick"}));

        code->moveCursor(QTextCursor::End);
        fields->setCurrentRow(1);
        dialog.findChild<QPushButton*>("insertBut")->click();
        QVERIFY(code->toPlainText().endsWith(QStringLiteral("@txt")));

        bool previewed = false;
        whenModal<QDialog>([&previewed](QDialog* form) {
            previewed = form->objectName() == QLatin1String("SnipFieldsDlg");
            form->reject();
        });
        dialog.findChild<QPushButton*>("previewBut")->click();
        QVERIFY(previewed);

        QTest::keyClick(&dialog, Qt::Key_Escape);
        QVERIFY(!dialog.isVisible());
        QVERIFY(!QApplication::activeModalWidget());
    }

    // The help page opens at the click but stays on the screen, and closes when it loses focus.
    void documentation()
    {
        CodeEditor editor;
        editor.setSyntax(&m_syntax, QStringLiteral("state"));
        editor.setPlainText(QStringLiteral("[State 0, 1]\ntype = Null\ntrigger1 = Const(data.life) > 0\n"));
        const int at = int(editor.toPlainText().indexOf(QStringLiteral("Const")) + 2);
        const auto [kind, name] = editor.documentationTarget(at);
        QVERIFY(!kind.isEmpty());
        QCOMPARE(name.toLower(), QStringLiteral("const"));

        QTemporaryDir dir;
        writeFile(dir.filePath("page.html"), "<html><body><h1>Const</h1><p>text</p></body></html>");
        const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
        DocumentationPopup* popup =
            DocumentationPopup::showPage(dir.filePath("page.html"), screen.bottomRight() - QPoint(5, 5), &editor);
        QVERIFY(popup);
        QVERIFY(screen.contains(popup->frameGeometry().topLeft()));
        QVERIFY(screen.contains(popup->frameGeometry().bottomRight()));
        const QPoint near = screen.topLeft() + QPoint(10, 10);
        DocumentationPopup* second = DocumentationPopup::showPage(dir.filePath("page.html"), near, &editor);
        QVERIFY(second);
        QCOMPARE(second->pos(), near);
        QVERIFY(!(second->windowFlags() & Qt::WindowStaysOnTopHint));
        QCOMPARE(second->parentWidget(), static_cast<QWidget*>(&editor));
        Q_EMIT qApp->applicationStateChanged(Qt::ApplicationInactive);
        QVERIFY(!second->isVisible());
        QVERIFY(!popup->isVisible());
        second = DocumentationPopup::showPage(dir.filePath("page.html"), near, &editor);
        QVERIFY(second);
        QEvent deactivate(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(second, &deactivate);
        QVERIFY(!second->isVisible());
        popup->close();
    }

    void definitionsTree()
    {
        const QString def = QStringLiteral("[Info]\nname = \"KFM\"\ndisplayname = \"KFM\"\nversiondate = 09,23,2011\n"
                                           "mugenversion = 1.0\nauthor = \"E\"\npal.defaults = 1\n"
                                           "localcoord = 320,240\n[Files]\ncmd = kfm.cmd\ncns = kfm.cns\n"
                                           "sprite = kfm.sff\nanim = kfm.air\nsound = kfm.snd\n"
                                           "[Palette Keymap]\nx = 1\n[Arcade]\nintro.storyboard = intro.def\n");
        const Outline tree = Mugen::mugenOutline(def, &m_syntax, QStringLiteral("char"));
        QStringList blocks;
        for (const OutlineNode& node : tree)
            blocks << node.text;
        QCOMPARE(blocks, (QStringList{"Info", "Files", "Palette Keymap", "Arcade"}));
        QHash<QString, QString> icons;
        for (const OutlineNode& node : tree) {
            for (const OutlineNode& child : node.children)
                icons.insert(child.text, child.icon);
        }
        QCOMPARE(icons.value("name"), QStringLiteral("code_string"));
        QCOMPARE(icons.value("versiondate"), QStringLiteral("code_version"));
        QCOMPARE(icons.value("mugenversion"), QStringLiteral("code_version"));
        QCOMPARE(icons.value("pal.defaults"), QStringLiteral("code_int"));
        QCOMPARE(icons.value("localcoord"), QStringLiteral("code_coord"));
        QCOMPARE(icons.value("sprite"), QStringLiteral("code_file"));
        QCOMPARE(icons.value("intro.storyboard"), QStringLiteral("code_file"));
        QVERIFY(icons.contains("anim"));
        QVERIFY(!icons.contains("clsn1"));

        const QString cns = QStringLiteral("[State 200, 1]\ntype = HitDef\nattr = S, NA\nanimtype = Light\n"
                                           "ground.velocity = -4\nhitflag = MAF\n");
        const Outline states = Mugen::mugenOutline(cns, &m_syntax, QStringLiteral("state"));
        QHash<QString, QString> stateIcons;
        for (const OutlineNode& child : states.first().children)
            stateIcons.insert(child.text, child.icon);
        QCOMPARE(stateIcons.value("attr"), QStringLiteral("code_flags"));
        QCOMPARE(stateIcons.value("animtype"), QStringLiteral("code_keyword"));
        QCOMPARE(stateIcons.value("hitflag"), QStringLiteral("code_flags"));
        QCOMPARE(stateIcons.value("ground.velocity"), QStringLiteral("code_float"));
        const Outline commands =
            Mugen::mugenOutline(QStringLiteral("[Command]\nname = \"x\"\ncommand = x\n"), &m_syntax, "command");
        QCOMPARE(commands.first().children.first().children.at(1).icon, QStringLiteral("code_cmd"));
    }

    void definitionsTreesByType()
    {
        auto texts = [](const Outline& tree) {
            QStringList t;
            for (const OutlineNode& node : tree)
                t << node.text;
            return t;
        };
        const Outline stage = Mugen::mugenOutline(
            QStringLiteral("[Info]\nname = \"S\"\n[Camera]\nstartx = 0\n[PlayerInfo]\np1startx = -70\n[Bound]\n"
                           "screenleft = 15\n[StageInfo]\nzoffset = 190\n[Shadow]\nintensity = 128\n[Reflection]\n"
                           "intensity = 0\n[Music]\nbgmusic = \n[BGdef]\nspr = s.sff\n[BG Back]\ntype = normal\n"
                           "[BG Floor]\ntype = normal\n[BGCtrlDef Door]\nlooptime = 10\n[BGCtrl Open]\ntype = Null\n"),
            &m_syntax, QStringLiteral("stage"));
        QCOMPARE(texts(stage), (QStringList{"Info", "Camera", "PlayerInfo", "Bound", "StageInfo", "Shadow",
                                            "Reflection", "Music", "BG"}));
        QStringList elements;
        for (const OutlineNode& child : stage.last().children)
            elements << child.text;
        QVERIFY(elements.contains("Back") && elements.contains("Floor"));
        QVERIFY(elements.contains("BGCtrlDef Door"));

        const Outline motif = Mugen::mugenOutline(
            QStringLiteral(
                "[Info]\nname = \"M\"\n[Files]\nspr = system.sff\n[Music]\ntitle.bgm = \n[Title Info]\n"
                "fadein.time = 10\n[TitleBGdef]\nbgclearcolor = 0,0,0\n[TitleBG 0]\ntype = normal\n"
                "[Select Info]\nrows = 2\n[Begin Action 180]\n180,0, 0,0, -1\n[SelectBGdef]\nbgclearcolor = 0,0,0\n"
                "[SelectBG 0]\ntype = normal\n"),
            &m_syntax, QStringLiteral("motif"));
        QCOMPARE(texts(motif), (QStringList{"Info", "Files", "Music", "Title Info", "Title Backgrounds", "Select Info",
                                            "Animation 180", "Select Backgrounds"}));
        QCOMPARE(motif[4].children.last().text, QStringLiteral("0"));

        const Outline story = Mugen::mugenOutline(
            QStringLiteral("[Info]\nlocalcoord = 320,240\n[SceneDef]\nspr = intro.sff\n[Scene 0]\nbg.name = BG0\n"
                           "[BG0Def]\nspr = intro.sff\n[BG0 Back]\ntype = normal\n; Animation for Suave\n"
                           "[Begin Action 10]\n10,0, 0,0, 5\n"),
            &m_syntax, QStringLiteral("storyboard"));
        QVERIFY(texts(story).contains("BG0"));
        QVERIFY(texts(story).contains("10 - Animation for Suave"));

        const Outline font = Mugen::mugenOutline(
            QStringLiteral("[FNT v2]\nfntversion = 2,00\nname = \"F\"\n[Def]\nType = bitmap\nSize = 8,8\n"), &m_syntax,
            QStringLiteral("font"));
        QCOMPARE(texts(font), (QStringList{"FNT V2", "Bitmap"}));
    }

    // A double-click on a node also folds or unfolds it. Each state file tab has its own tree.
    void codeTrees()
    {
        MainWindow window(*m_context);
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.openProject(m_dir.filePath("c.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_st")->trigger();
        ProjectView* view = window.currentProjectView();
        QTreeView* tree = nullptr;
        for (QTreeView* t : view->findChildren<QTreeView*>("st_tree"))
            if (t->isVisible())
                tree = t;
        QVERIFY(tree);
        auto topTexts = [tree] {
            QStringList texts;
            for (int row = 0; row < tree->model()->rowCount(); ++row)
                texts << tree->model()->index(row, 0).data().toString();
            return texts;
        };
        QTRY_VERIFY(topTexts().contains(QStringLiteral("0"))); // filled a moment after the file is read
        auto stateDef = [&] {                                  // the tree may be refilled
            return tree->model()->index(int(topTexts().indexOf(QStringLiteral("0"))), 0);
        };
        QVERIFY(tree->model()->rowCount(stateDef()) > 0);
        const bool expanded = tree->isExpanded(stateDef());
        tree->scrollTo(stateDef());
        // Send the whole event sequence a real double-click makes.
        const QPoint at = tree->visualRect(stateDef()).center();
        const QPoint global = tree->viewport()->mapToGlobal(at);
        QWidget* top = window.childAt(tree->viewport()->mapTo(&window, at));
        QCOMPARE(top, tree->viewport());
        for (QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::MouseButtonDblClick,
                                  QEvent::MouseButtonRelease}) {
            const bool down = type != QEvent::MouseButtonRelease;
            QMouseEvent event(type, at, global, Qt::LeftButton, down ? Qt::LeftButton : Qt::NoButton, {});
            QCoreApplication::sendEvent(tree->viewport(), &event);
        }
        QCOMPARE(tree->isExpanded(stateDef()), !expanded);
        QCOMPARE(view->currentCodeEditor()->cursorLine(), 4); // the [Statedef 0] line

        QTabWidget* tabs = view->fileTabs(QStringLiteral("states"));
        QVERIFY(tabs && tabs->count() == 2);
        tabs->setCurrentIndex(1);
        QTRY_VERIFY(topTexts().contains(QStringLiteral("900")));
        QVERIFY(!topTexts().contains(QStringLiteral("0")));
        tabs->setCurrentIndex(0);
        QTRY_VERIFY(topTexts().contains(QStringLiteral("0")));
    }

    // In snippet code the longest field name wins and unknown @words stay as they are.
    void codeToolbar()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c.def")));
        m_context->actions().action("editor_st")->trigger();
        CodePage* page = window.currentProjectView()->currentCodePage();
        QWidget* bar = page->findChild<QWidget*>("codeToolBar");
        QVERIFY(bar);
        QStringList order;
        for (QWidget* w : bar->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            if (!w->objectName().isEmpty())
                order << w->objectName();
        std::sort(order.begin(), order.end(), [bar](const QString& a, const QString& b) {
            return bar->findChild<QWidget*>(a)->x() < bar->findChild<QWidget*>(b)->x();
        });
        QCOMPARE(order, (QStringList{"codeSnippets", "code_snippet_insert", "code_snippet_new", "code_snippet_edit",
                                     "codeIssues"}));

        Snippet snippet;
        snippet.fields = {SnippetField{"a", "A", SnippetField::Type::String, {}, {}},
                          SnippetField{"ab", "AB", SnippetField::Type::String, {}, {}}};
        snippet.code = QStringLiteral("x = @ab@a @abc @zz");
        QCOMPARE(snippet.expand({{"a", "1"}, {"ab", "2"}}), QStringLiteral("x = 21 2c @zz"));
    }

    void quickFindStartsAtCurrent()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("q.def")));
        window.switchEditor("animations");
        AnimationPage* page = window.currentProjectView()->animationsEditor()->page();
        QVERIFY(page);
        auto find = [&](const QString& text) {
            m_context->actions().action("edit_find_replace")->trigger();
            auto* bar = window.findChild<QuickFindBar*>();
            bar->setFindText(text);
            Q_EMIT bar->findRequested();
        };
        page->setCurrent(0);
        find("walk");
        QCOMPARE(page->current(), 1);
        find("walk");
        QCOMPARE(page->current(), 1);
        page->setCurrent(2);
        find("Walk");
        QCOMPARE(page->current(), 2);
        page->setCurrent(3);
        find("walk");
        QCOMPARE(page->current(), 1);
        window.findChild<QuickFindBar*>()->closeRequested();
    }

    // The file patterns only show for project and folder scopes. Clear keeps the search text.
    void advancedSearchDetails()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c.def")));
        auto* panel = window.findChild<AdvancedSearchPanel*>("advancedSearch");
        QVERIFY(panel);
        auto* scope = panel->findChild<QComboBox*>("advsearch_scope");
        auto* type = panel->findChild<QComboBox*>("advsearch_type");
        auto* text = panel->findChild<QLineEdit*>("advsearch_text");
        auto* pattern = panel->findChild<QComboBox*>("advsearch_pattern");
        auto* exclusion = panel->findChild<QComboBox*>("advsearch_exclusion");
        QVERIFY(scope && type && text && pattern && exclusion);
        for (int s : {int(AdvancedSearchPanel::AllProjects), int(AdvancedSearchPanel::CurrentProject),
                      int(AdvancedSearchPanel::FileSystem)}) {
            scope->setCurrentIndex(s);
            QVERIFY(pattern->isVisibleTo(panel) && exclusion->isVisibleTo(panel));
            QVERIFY(pattern->isEditable());
        }
        scope->setCurrentIndex(AdvancedSearchPanel::CurrentFile);
        QVERIFY(!pattern->isVisibleTo(panel) && !exclusion->isVisibleTo(panel));
        type->setCurrentIndex(1); // Lazy syntax fix
        QVERIFY(!text->isVisibleTo(panel));
        QVERIFY(!panel->findChild<QCheckBox*>("advsearch_case")->isVisibleTo(panel));
        type->setCurrentIndex(0);

        scope->setCurrentIndex(AdvancedSearchPanel::CurrentProject);
        text->setText("type");
        panel->search(false);
        QVERIFY(panel->isShowingResults());
        const QAbstractItemModel* model = panel->resultsView()->model();
        QVERIFY(model->rowCount() >= 1);
        const QModelIndex fileRow = model->index(0, 0);
        const QString fileHtml = fileRow.data(Qt::UserRole + 1).toString();
        QVERIFY2(fileHtml.contains(QStringLiteral("<b>")) && fileHtml.contains(QLatin1Char('/')) &&
                     fileHtml.contains(QStringLiteral(" (")),
                 qPrintable(fileHtml));
        const QModelIndex matchRow = model->index(0, 0, fileRow);
        QVERIFY(matchRow.data(Qt::UserRole + 2).toInt() > 0); // its line number
        QVERIFY(matchRow.data(Qt::UserRole + 1).toString().contains(QStringLiteral("#FFFF80")));
        const int found = int(panel->hits().size());

        panel->findChild<QPushButton*>("advsearch_clear")->click();
        QVERIFY(!panel->isShowingResults());
        QCOMPARE(text->text(), QStringLiteral("type"));
        panel->search(false);
        panel->findChild<QPushButton*>("advsearch_again")->click();
        QVERIFY(panel->isShowingResults());
        QCOMPARE(int(panel->hits().size()), found);
    }

    // A single click in the state controller list does not touch the code.
    void controllerListClick()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c.def")));
        m_context->actions().action("editor_st")->trigger();
        ProjectView* view = window.currentProjectView();
        QListView* list = nullptr;
        for (QListView* l : view->findChildren<QListView*>("controllersList"))
            if (l->isVisible())
                list = l;
        QVERIFY(list && list->model() && list->model()->rowCount() > 0);
        const QString before = view->currentCodeEditor()->toPlainText();
        Q_EMIT list->clicked(list->model()->index(0, 0));
        QCOMPARE(view->currentCodeEditor()->toPlainText(), before);
    }

private:
    AppContext* m_context = nullptr;
    SyntaxDatabase m_syntax;
    CompletionModels m_models;
    QTemporaryDir m_dir;
};

QTEST_MAIN(CodeEditorParityTest)
#include "CodeEditorParityTest.moc"

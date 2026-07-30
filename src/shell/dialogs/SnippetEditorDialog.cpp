#include "shell/dialogs/SnippetEditorDialog.h"

#include "core/code/CodeHighlighter.h"
#include "shell/FileDialogs.h"
#include "shell/dialogs/SnippetFieldsDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace OFS {

namespace {

QStringList nonEmptyLines(const QString& text)
{
    QStringList lines;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty())
            lines << line.trimmed();
    }
    return lines;
}

QPushButton* topButton(const QString& text, const char* objectName, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setAutoDefault(false);
    return button;
}

} // namespace

// ---------------------------------------------------------------------------------------------

SnippetFieldDialog::SnippetFieldDialog(QWidget* parent)
    : QDialog(parent)
    , m_name(new QLineEdit(this))
    , m_title(new QLineEdit(this))
    , m_type(new QComboBox(this))
    , m_itemsBox(new QWidget(this))
    , m_values(new QPlainTextEdit(m_itemsBox))
    , m_titles(new QPlainTextEdit(m_itemsBox))
{
    setObjectName(QStringLiteral("SnipFieldDlg"));
    setWindowTitle(tr("Add field"));
    m_name->setObjectName(QStringLiteral("name"));
    m_title->setObjectName(QStringLiteral("title"));
    m_type->setObjectName(QStringLiteral("type"));
    m_type->addItem(tr("String"), int(SnippetField::Type::String));
    m_type->addItem(tr("Integer"), int(SnippetField::Type::Int));
    m_type->addItem(tr("Float"), int(SnippetField::Type::Float));
    m_type->addItem(tr("Items"), int(SnippetField::Type::Items));
    m_values->setObjectName(QStringLiteral("values"));
    m_titles->setObjectName(QStringLiteral("titles"));

    auto* form = new QFormLayout;
    form->addRow(tr("Name:"), m_name);
    form->addRow(tr("Title:"), m_title);
    form->addRow(tr("Type:"), m_type);

    auto* items = new QHBoxLayout(m_itemsBox);
    items->setContentsMargins(0, 0, 0, 0);
    auto* valuesColumn = new QVBoxLayout;
    valuesColumn->addWidget(new QLabel(tr("Values:"), m_itemsBox));
    valuesColumn->addWidget(m_values);
    auto* titlesColumn = new QVBoxLayout;
    titlesColumn->addWidget(new QLabel(tr("Values (title):"), m_itemsBox));
    titlesColumn->addWidget(m_titles);
    items->addLayout(valuesColumn);
    items->addLayout(titlesColumn);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_name->text().trimmed().isEmpty()) {
            m_name->setFocus();
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_itemsBox, 1);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(m_type, &QComboBox::currentIndexChanged, this, &SnippetFieldDialog::updateItemsVisibility);
    updateItemsVisibility();
}

void SnippetFieldDialog::updateItemsVisibility()
{
    const bool items = m_type->currentData().toInt() == int(SnippetField::Type::Items);
    m_itemsBox->setVisible(items);
    resize(items ? QSize(555, 469) : QSize(226, 245));
}

SnippetField SnippetFieldDialog::field() const
{
    SnippetField field;
    field.name = m_name->text().trimmed();
    field.title = m_title->text().trimmed();
    field.type = SnippetField::Type(m_type->currentData().toInt());
    if (field.type == SnippetField::Type::Items) {
        field.values = nonEmptyLines(m_values->toPlainText());
        field.valueTitles = nonEmptyLines(m_titles->toPlainText());
    }
    return field;
}

// ---------------------------------------------------------------------------------------------

SnippetEditorDialog::SnippetEditorDialog(const Snippet& snippet, const QString& saveDir, const SyntaxDatabase* syntax,
                                         QWidget* parent)
    : QDialog(parent)
    , m_snippet(snippet)
    , m_saveDir(saveDir)
    , m_code(new QPlainTextEdit(this))
    , m_fields(new QListWidget(this))
{
    setObjectName(QStringLiteral("SnipDlg"));
    setWindowTitle(tr("Code snippet editor"));
    resize(605, 272);

    QPushButton* save = topButton(tr("Save"), "saveBut", this);
    save->setDefault(true);
    QPushButton* previewButton = topButton(tr("Preview"), "previewBut", this);
    QPushButton* translation = topButton(tr("Save for translation"), "saveTSBut", this);
    QPushButton* insert = topButton(tr("Insert selected"), "insertBut", this);
    QPushButton* add = topButton(QStringLiteral("+"), "add", this);
    QPushButton* remove = topButton(QStringLiteral("-"), "del", this);
    QPushButton* up = topButton(QStringLiteral("˄"), "up", this);
    QPushButton* down = topButton(QStringLiteral("˅"), "down", this);
    for (QPushButton* small : {add, remove, up, down})
        small->setFixedWidth(32);

    m_code->setObjectName(QStringLiteral("snipEdit"));
    m_code->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Courier New"));
    mono.setStyleHint(QFont::Monospace);
    m_code->setFont(mono);
    (new CodeHighlighter(m_code->document(), syntax, QStringLiteral("state")))->setParent(m_code);
    m_fields->setObjectName(QStringLiteral("snipFields"));
    m_fields->setFixedWidth(128);

    // The code shows the snippet lines left-trimmed, as they are inserted.
    QString code = snippet.code;
    code.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    QStringList lines = code.split(QLatin1Char('\n'));
    if (lines.size() > 1 && lines.first().trimmed().isEmpty())
        lines.removeFirst();
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    for (QString& line : lines) {
        qsizetype i = 0;
        while (i < line.size() && (line.at(i) == QLatin1Char(' ') || line.at(i) == QLatin1Char('\t')))
            ++i;
        line = line.mid(i);
    }
    m_code->setPlainText(lines.join(QLatin1Char('\n')));

    auto* top = new QHBoxLayout;
    for (QPushButton* b : {save, previewButton, translation, insert})
        top->addWidget(b);
    auto* fieldButtons = new QHBoxLayout;
    fieldButtons->setSpacing(0);
    for (QPushButton* b : {add, remove, up, down})
        fieldButtons->addWidget(b);
    top->addLayout(fieldButtons);
    top->addStretch();
    auto* body = new QHBoxLayout;
    body->addWidget(m_code, 1);
    body->addWidget(m_fields);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(body, 1);

    connect(save, &QPushButton::clicked, this, &SnippetEditorDialog::save);
    connect(previewButton, &QPushButton::clicked, this, &SnippetEditorDialog::preview);
    connect(translation, &QPushButton::clicked, this, &SnippetEditorDialog::saveForTranslation);
    connect(insert, &QPushButton::clicked, this, &SnippetEditorDialog::insertSelectedField);
    connect(add, &QPushButton::clicked, this, &SnippetEditorDialog::addField);
    connect(remove, &QPushButton::clicked, this, &SnippetEditorDialog::removeField);
    connect(up, &QPushButton::clicked, this, [this] { moveField(-1); });
    connect(down, &QPushButton::clicked, this, [this] { moveField(1); });
    refreshFields();
}

Snippet SnippetEditorDialog::snippet() const
{
    Snippet s = m_snippet;
    QString code = m_code->toPlainText();
    if (!code.endsWith(QLatin1Char('\n')))
        code += QLatin1Char('\n'); // inserted snippets end with a line break
    s.code = code;
    return s;
}

void SnippetEditorDialog::refreshFields()
{
    const int row = m_fields->currentRow();
    m_fields->clear();
    for (const SnippetField& field : std::as_const(m_snippet.fields))
        m_fields->addItem(field.title.isEmpty() ? field.name : field.title);
    if (row >= 0 && row < m_fields->count())
        m_fields->setCurrentRow(row);
}

bool SnippetEditorDialog::saveTo(const QString& filePath, QString* error)
{
    Snippet s = snippet();
    s.name = QFileInfo(filePath).completeBaseName();
    if (!s.save(filePath, error))
        return false;
    s.path = filePath;
    m_snippet = s;
    m_savedPath = filePath;
    Q_EMIT saved(filePath);
    return true;
}

void SnippetEditorDialog::save()
{
    QString path;
    if (!m_snippet.name.isEmpty()) {
        path = QDir(m_saveDir).filePath(m_snippet.name + QStringLiteral(".xml"));
    } else {
        QDir().mkpath(m_saveDir);
        path = FileDialogs::getSaveFileName(this, tr("Save code snippet"), m_saveDir,
                                            tr("Extensible Markup Language (*.xml)"));
        if (path.isEmpty())
            return;
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".xml");
    }
    QString error;
    if (!saveTo(path, &error))
        QMessageBox::warning(this, QApplication::applicationDisplayName(), error);
}

void SnippetEditorDialog::saveForTranslation()
{
    const QString path =
        FileDialogs::getSaveFileName(this, tr("Save for translation"), m_saveDir, tr("Qt translation sources (*.ts)"));
    if (path.isEmpty())
        return;
    QString error;
    Snippet s = snippet();
    if (s.name.isEmpty())
        s.name = QFileInfo(path).completeBaseName();
    if (!s.saveTranslationSource(path, &error))
        QMessageBox::warning(this, QApplication::applicationDisplayName(), error);
}

void SnippetEditorDialog::preview()
{
    Snippet s = snippet();
    if (s.name.isEmpty())
        s.name = tr("Code snippet");
    SnippetFieldsDialog dialog(s, this);
    dialog.exec();
}

void SnippetEditorDialog::insertSelectedField()
{
    const int row = m_fields->currentRow();
    if (row < 0 || row >= m_snippet.fields.size())
        return;
    m_code->insertPlainText(QLatin1Char('@') + m_snippet.fields.at(row).name);
    m_code->setFocus();
}

void SnippetEditorDialog::addField()
{
    SnippetFieldDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_snippet.fields << dialog.field();
    refreshFields();
    m_fields->setCurrentRow(m_fields->count() - 1);
}

void SnippetEditorDialog::removeField()
{
    const int row = m_fields->currentRow();
    if (row < 0 || row >= m_snippet.fields.size())
        return;
    m_snippet.fields.removeAt(row);
    refreshFields();
}

void SnippetEditorDialog::moveField(int delta)
{
    const int row = m_fields->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= m_snippet.fields.size())
        return;
    m_snippet.fields.move(row, target);
    refreshFields();
    m_fields->setCurrentRow(target);
}

} // namespace OFS

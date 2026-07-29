#pragma once

#include "core/code/Snippet.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;

namespace OFS {

class SyntaxDatabase;

class SnippetFieldDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SnippetFieldDialog(QWidget* parent = nullptr);

    SnippetField field() const;

private:
    void updateItemsVisibility();

    QLineEdit* m_name;
    QLineEdit* m_title;
    QComboBox* m_type;
    QWidget* m_itemsBox;
    QPlainTextEdit* m_values;
    QPlainTextEdit* m_titles;
};

// Code snippet editor (code toolbar > New/Edit code snippet).
class SnippetEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // New snippets are saved through a file dialog starting in `saveDir`. Edited ones go back there under their own
    // name.
    SnippetEditorDialog(const Snippet& snippet, const QString& saveDir, const SyntaxDatabase* syntax,
                        QWidget* parent = nullptr);

    Snippet snippet() const;
    QString savedPath() const { return m_savedPath; }

    bool saveTo(const QString& filePath, QString* error = nullptr);

Q_SIGNALS:
    void saved(const QString& filePath);

private:
    void save();
    void saveForTranslation();
    void preview();
    void insertSelectedField();
    void addField();
    void removeField();
    void moveField(int delta);
    void refreshFields();

    Snippet m_snippet;
    QString m_saveDir;
    QString m_savedPath;
    QPlainTextEdit* m_code;
    QListWidget* m_fields;
};

} // namespace OFS

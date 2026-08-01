#pragma once

#include "core/code/TextSearch.h"
#include "core/project/ProjectSearch.h"

#include <QWidget>

#include <functional>
#include <utility>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QStandardItemModel;
class QTreeView;

namespace OFS {

class Project;
class SyntaxDatabase;

// Advanced Search tab of the bottom panel: finds a string, or the Lazy syntax fix, in files and lists the matches.
class AdvancedSearchPanel : public QWidget
{
    Q_OBJECT

public:
    enum Scope
    {
        AllProjects,
        CurrentProject,
        CurrentFile,
        OpenedFiles,
        FileSystem,
    };

    struct Context
    {
        std::function<QList<Project*>()> projects;
        std::function<Project*()> currentProject;
        std::function<QString()> currentFile;
        std::function<std::pair<const SyntaxDatabase*, QString>(const QString& path)> syntaxFor;
        std::function<void(const QString& path, int line, int column)> openLocation;
    };

    explicit AdvancedSearchPanel(QWidget* parent = nullptr);

    void setContext(Context context) { m_context = std::move(context); }

    void search(bool replaceMode);
    int replaceChecked();
    void clear();

    bool isShowingResults() const;
    const QList<SearchHit>& hits() const { return m_hits; }
    QTreeView* resultsView() const { return m_results; }

Q_SIGNALS:
    void filesReplaced();

private:
    QWidget* createForm();
    QWidget* createResults();
    void updateForm();
    QStringList scopeFiles() const;
    void showResults(bool replaceMode);

    Context m_context;
    QStackedWidget* m_stack;
    QComboBox* m_scope;
    QComboBox* m_type;
    QLabel* m_textLabel;
    QLineEdit* m_text;
    QCheckBox* m_case;
    QCheckBox* m_words;
    QCheckBox* m_regExp;
    QLabel* m_directoryLabel;
    QLineEdit* m_directory;
    QPushButton* m_browse;
    QLabel* m_patternLabel;
    QComboBox* m_pattern;
    QLabel* m_exclusionLabel;
    QComboBox* m_exclusion;
    QPushButton* m_searchButton;
    QPushButton* m_replaceModeButton;
    QLabel* m_scopeCaption;
    QLabel* m_searchCaption;
    QLabel* m_count;
    QTreeView* m_results;
    QStandardItemModel* m_model;
    QWidget* m_replaceRow;
    QLineEdit* m_replacement;

    QList<SearchHit> m_hits;
    bool m_lazy = false;
    bool m_replaceMode = false;
    QString m_searchText;
    SearchOptions m_options;
};

} // namespace OFS

#include "shell/AdvancedSearchPanel.h"

#include "shell/FileDialogs.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCache>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

namespace OFS {

namespace {

enum Role
{
    HtmlRole = Qt::UserRole + 1,
    LineRole, // 1-based line number of a match row, 0 for file rows
    HitRole,
};

constexpr int kLineCellWidth = 60;

class ResultDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QString html = index.data(HtmlRole).toString();
        opt.text.clear();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        painter->save();
        const int line = index.data(LineRole).toInt();
        if (line > 0) {
            QFont bold = opt.font;
            bold.setBold(true);
            painter->setFont(bold);
            painter->setPen(opt.palette.color(QPalette::Text));
            painter->drawText(textRect.adjusted(2, 0, 0, 0)
                                  .intersected(QRect(textRect.topLeft(), QSize(kLineCellWidth, textRect.height()))),
                              Qt::AlignVCenter | Qt::AlignLeft, QString::number(line));
            textRect.setLeft(textRect.left() + kLineCellWidth);
        }
        const QTextDocument& doc = document(html, opt.font);
        painter->translate(textRect.left(), textRect.top() + (textRect.height() - doc.size().height()) / 2);
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = opt.palette;
        context.palette.setColor(QPalette::Text, opt.palette.color(QPalette::Text));
        context.clip = QRectF(0, 0, textRect.width(), textRect.height());
        doc.documentLayout()->draw(painter, context);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        const QTextDocument& doc = document(index.data(HtmlRole).toString(), option.font);
        size.setWidth(int(doc.idealWidth()) + (index.data(LineRole).toInt() > 0 ? kLineCellWidth : 0) + 8);
        size.setHeight(qMax(size.height(), int(doc.size().height()) + 2));
        return size;
    }

private:
    // Caches the laid-out text because a search can list thousands of rows.
    const QTextDocument& document(const QString& html, const QFont& font) const
    {
        const QString key = font.key() + QLatin1Char('\n') + html;
        if (const QTextDocument* known = m_documents.object(key))
            return *known;
        auto* doc = new QTextDocument;
        doc->setDefaultFont(font);
        doc->setDocumentMargin(0);
        doc->setHtml(html);
        m_documents.insert(key, doc);
        return *doc;
    }

    mutable QCache<QString, QTextDocument> m_documents{4000};
};

QString highlightedLine(const SearchHit& hit)
{
    QString line = hit.lineText;
    line.replace(QLatin1Char('\t'), QLatin1String("    "));
    const int column = int(hit.lineText.left(hit.column).count(QLatin1Char('\t'))) * 3 + hit.column;
    const QString before = line.left(column).toHtmlEscaped();
    const QString match = line.mid(column, hit.length).toHtmlEscaped();
    const QString after = line.mid(column + hit.length).toHtmlEscaped();
    return QStringLiteral("<span style=\"white-space:pre\">%1<span style=\"background-color:#FFFF80; color:black; "
                          "font-weight:bold\">%2</span>%3</span>")
        .arg(before, match, after);
}

bool looksBinary(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return true;
    return file.read(8192).contains('\0');
}

} // namespace

AdvancedSearchPanel::AdvancedSearchPanel(QWidget* parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
{
    setObjectName(QStringLiteral("advancedSearch"));
    m_stack->addWidget(createForm());
    m_stack->addWidget(createResults());
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);
    updateForm();
}

QWidget* AdvancedSearchPanel::createForm()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("advsearch_form"));
    m_scope = new QComboBox(page);
    m_scope->setObjectName(QStringLiteral("advsearch_scope"));
    m_scope->addItems({tr("All projects"), tr("Current project"), tr("Current file"), tr("Opened files"),
                       tr("Files in File system")});
    m_type = new QComboBox(page);
    m_type->setObjectName(QStringLiteral("advsearch_type"));
    m_type->addItems({tr("By string"), tr("Lazy syntax fix")});
    m_textLabel = new QLabel(tr("Search for:"), page);
    m_text = new QLineEdit(page);
    m_text->setObjectName(QStringLiteral("advsearch_text"));
    m_case = new QCheckBox(tr("Case sensitive"), page);
    m_case->setObjectName(QStringLiteral("advsearch_case"));
    m_words = new QCheckBox(tr("Whole words only"), page);
    m_words->setObjectName(QStringLiteral("advsearch_words"));
    m_regExp = new QCheckBox(tr("Use Regular expression"), page);
    m_regExp->setObjectName(QStringLiteral("advsearch_regexp"));
    m_directoryLabel = new QLabel(tr("Directory:"), page);
    m_directory = new QLineEdit(page);
    m_directory->setObjectName(QStringLiteral("advsearch_directory"));
    m_browse = new QPushButton(QStringLiteral("..."), page);
    m_browse->setObjectName(QStringLiteral("advsearch_browse"));
    m_patternLabel = new QLabel(tr("File pattern:"), page);
    m_pattern = new QComboBox(page);
    m_pattern->setObjectName(QStringLiteral("advsearch_pattern"));
    m_pattern->setEditable(true);
    m_pattern->addItems({QStringLiteral("*.def, *.cmd, *.cns, *.st"),
                         QStringLiteral("*.def *.sff *.snd *.air *.cns *.st *.* *.cmd *.*"), QStringLiteral("*")});
    m_exclusionLabel = new QLabel(tr("Exclusion pattern:"), page);
    m_exclusion = new QComboBox(page);
    m_exclusion->setObjectName(QStringLiteral("advsearch_exclusion"));
    m_exclusion->setEditable(true);
    m_searchButton = new QPushButton(tr("Search"), page);
    m_searchButton->setObjectName(QStringLiteral("advsearch_search"));
    m_replaceModeButton = new QPushButton(tr("Search && Replace"), page);
    m_replaceModeButton->setObjectName(QStringLiteral("advsearch_searchreplace"));

    auto* grid = new QGridLayout(page);
    grid->addWidget(new QLabel(tr("Scope:"), page), 0, 0, Qt::AlignRight);
    grid->addWidget(m_scope, 0, 1);
    grid->addWidget(new QLabel(tr("Type:"), page), 0, 2, Qt::AlignRight);
    grid->addWidget(m_type, 0, 3);
    grid->addWidget(m_textLabel, 1, 0, Qt::AlignRight);
    grid->addWidget(m_text, 1, 1, 1, 3);
    grid->addWidget(m_case, 2, 1);
    grid->addWidget(m_words, 2, 2);
    grid->addWidget(m_regExp, 2, 3);
    auto* directory = new QHBoxLayout;
    directory->addWidget(m_directory, 1);
    directory->addWidget(m_browse);
    grid->addWidget(m_directoryLabel, 3, 0, 1, 2, Qt::AlignRight);
    grid->addLayout(directory, 3, 2, 1, 2);
    grid->addWidget(m_patternLabel, 4, 0, 1, 2, Qt::AlignRight);
    grid->addWidget(m_pattern, 4, 2, 1, 2);
    grid->addWidget(m_exclusionLabel, 5, 0, 1, 2, Qt::AlignRight);
    grid->addWidget(m_exclusion, 5, 2, 1, 2);
    grid->setRowStretch(6, 1);
    grid->addWidget(m_searchButton, 7, 2);
    grid->addWidget(m_replaceModeButton, 7, 3);
    grid->setColumnStretch(4, 1);

    connect(m_scope, &QComboBox::currentIndexChanged, this, &AdvancedSearchPanel::updateForm);
    connect(m_type, &QComboBox::currentIndexChanged, this, &AdvancedSearchPanel::updateForm);
    connect(m_text, &QLineEdit::textChanged, this, &AdvancedSearchPanel::updateForm);
    connect(m_text, &QLineEdit::returnPressed, this, [this] {
        if (m_searchButton->isEnabled())
            search(false);
    });
    connect(m_searchButton, &QPushButton::clicked, this, [this] { search(false); });
    connect(m_replaceModeButton, &QPushButton::clicked, this, [this] { search(true); });
    connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString dir = FileDialogs::getExistingDirectory(this, tr("Directory"), m_directory->text());
        if (!dir.isEmpty())
            m_directory->setText(QDir::toNativeSeparators(dir));
    });
    return page;
}

QWidget* AdvancedSearchPanel::createResults()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("advsearch_results"));
    m_scopeCaption = new QLabel(page);
    m_searchCaption = new QLabel(page);
    QFont bold = m_searchCaption->font();
    bold.setBold(true);
    m_searchCaption->setFont(bold);
    auto* clearButton = new QPushButton(tr("Clear"), page);
    clearButton->setObjectName(QStringLiteral("advsearch_clear"));
    auto* againButton = new QPushButton(tr("Search again"), page);
    againButton->setObjectName(QStringLiteral("advsearch_again"));
    m_count = new QLabel(page);
    m_count->setObjectName(QStringLiteral("advsearch_count"));

    m_model = new QStandardItemModel(this);
    m_results = new QTreeView(page);
    m_results->setObjectName(QStringLiteral("advsearch_view"));
    m_results->setHeaderHidden(true);
    m_results->setModel(m_model);
    m_results->setItemDelegate(new ResultDelegate(m_results));
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setExpandsOnDoubleClick(false);

    m_replaceRow = new QWidget(page);
    m_replacement = new QLineEdit(m_replaceRow);
    m_replacement->setObjectName(QStringLiteral("advsearch_replacement"));
    auto* replaceButton = new QPushButton(tr("Replace"), m_replaceRow);
    replaceButton->setObjectName(QStringLiteral("advsearch_replace"));
    auto* replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->addWidget(new QLabel(tr("Replace with:"), m_replaceRow));
    replaceLayout->addWidget(m_replacement, 1);
    replaceLayout->addWidget(replaceButton);
    replaceLayout->addWidget(new QLabel(tr("This change cannot be undone on closed files."), m_replaceRow));

    auto* header = new QHBoxLayout;
    header->addWidget(m_scopeCaption);
    header->addWidget(m_searchCaption);
    header->addWidget(clearButton);
    header->addWidget(againButton);
    header->addStretch();
    header->addWidget(m_count);
    auto* layout = new QVBoxLayout(page);
    layout->addLayout(header);
    layout->addWidget(m_replaceRow);
    layout->addWidget(m_results, 1);

    // Checking a file row checks its matches. The file row shows partly checked when only some are.
    connect(m_model, &QStandardItemModel::itemChanged, this, [](QStandardItem* item) {
        static bool updating = false;
        if (updating || !item->isCheckable())
            return;
        updating = true;
        if (QStandardItem* file = item->parent()) {
            int checked = 0;
            for (int r = 0; r < file->rowCount(); ++r)
                checked += file->child(r)->checkState() == Qt::Checked ? 1 : 0;
            file->setCheckState(checked == 0                  ? Qt::Unchecked
                                : checked == file->rowCount() ? Qt::Checked
                                                              : Qt::PartiallyChecked);
        } else if (item->checkState() != Qt::PartiallyChecked) {
            for (int r = 0; r < item->rowCount(); ++r)
                item->child(r)->setCheckState(item->checkState());
        }
        updating = false;
    });
    connect(clearButton, &QPushButton::clicked, this, &AdvancedSearchPanel::clear);
    connect(againButton, &QPushButton::clicked, this, [this] { search(m_replaceMode); });
    connect(replaceButton, &QPushButton::clicked, this, [this] { replaceChecked(); });
    connect(m_results, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const int hit = index.data(HitRole).toInt();
        if (index.data(LineRole).toInt() <= 0 || hit < 0 || hit >= m_hits.size() || !m_context.openLocation)
            return;
        m_context.openLocation(m_hits.at(hit).filePath, m_hits.at(hit).line + 1, m_hits.at(hit).column);
    });
    return page;
}

void AdvancedSearchPanel::updateForm()
{
    const int scope = m_scope->currentIndex();
    const bool lazy = m_type->currentIndex() == 1;
    const bool patterns = scope == AllProjects || scope == CurrentProject || scope == FileSystem;
    for (QWidget* w : {static_cast<QWidget*>(m_textLabel), static_cast<QWidget*>(m_text), static_cast<QWidget*>(m_case),
                       static_cast<QWidget*>(m_words), static_cast<QWidget*>(m_regExp)})
        w->setVisible(!lazy);
    for (QWidget* w : {static_cast<QWidget*>(m_patternLabel), static_cast<QWidget*>(m_pattern),
                       static_cast<QWidget*>(m_exclusionLabel), static_cast<QWidget*>(m_exclusion)})
        w->setVisible(patterns);
    for (QWidget* w :
         {static_cast<QWidget*>(m_directoryLabel), static_cast<QWidget*>(m_directory), static_cast<QWidget*>(m_browse)})
        w->setVisible(scope == FileSystem);
    const bool canSearch = lazy || !m_text->text().isEmpty();
    m_searchButton->setEnabled(canSearch);
    m_replaceModeButton->setEnabled(canSearch);
}

bool AdvancedSearchPanel::isShowingResults() const
{
    return m_stack->currentIndex() == 1;
}

void AdvancedSearchPanel::clear()
{
    m_model->clear();
    m_hits.clear();
    m_stack->setCurrentIndex(0);
}

QStringList AdvancedSearchPanel::scopeFiles() const
{
    QStringList files;
    const QStringList include = ProjectSearch::splitPatterns(m_pattern->currentText());
    const QStringList exclude = ProjectSearch::splitPatterns(m_exclusion->currentText());
    auto addFiltered = [&](const QStringList& paths) {
        for (const QString& path : paths) {
            if (ProjectSearch::matchesPatterns(path, include, exclude) && !files.contains(path, Qt::CaseInsensitive))
                files << path;
        }
    };
    const QList<Project*> projects = m_context.projects ? m_context.projects() : QList<Project*>();
    switch (m_scope->currentIndex()) {
    case AllProjects:
        for (Project* project : projects)
            addFiltered(ProjectSearch::projectFiles(*project));
        break;
    case CurrentProject:
        if (Project* project = m_context.currentProject ? m_context.currentProject() : nullptr)
            addFiltered(ProjectSearch::projectFiles(*project));
        break;
    case CurrentFile:
        if (m_context.currentFile && !m_context.currentFile().isEmpty())
            files << m_context.currentFile();
        break;
    case OpenedFiles:
        for (Project* project : projects) {
            for (ProjectFile* file : project->files()) {
                if (qobject_cast<CodeFile*>(file) && !files.contains(file->path(), Qt::CaseInsensitive))
                    files << file->path();
            }
        }
        break;
    case FileSystem:
        files = ProjectSearch::directoryFiles(QDir::fromNativeSeparators(m_directory->text()), include, exclude);
        break;
    }
    return files;
}

void AdvancedSearchPanel::search(bool replaceMode)
{
    m_lazy = m_type->currentIndex() == 1;
    m_replaceMode = replaceMode;
    m_searchText = m_text->text();
    m_options.caseSensitive = m_case->isChecked();
    m_options.wholeWords = m_words->isChecked();
    m_options.regExp = m_regExp->isChecked();
    if (!m_lazy && m_searchText.isEmpty())
        return;
    const TextSearch textSearch(m_searchText, m_options);
    if (!m_lazy && !textSearch.isValid()) {
        QMessageBox::warning(this, QApplication::applicationDisplayName(), textSearch.errorString());
        return;
    }

    const QList<Project*> projects = m_context.projects ? m_context.projects() : QList<Project*>();
    m_hits.clear();
    for (const QString& path : scopeFiles()) {
        bool open = false;
        for (Project* project : projects)
            open = open || qobject_cast<CodeFile*>(project->file(path));
        if (!open && looksBinary(path))
            continue;
        bool ok = false;
        const QString text = ProjectSearch::fileText(projects, path, &ok);
        if (!ok)
            continue;
        if (m_lazy) {
            const auto [syntax, fileType] =
                m_context.syntaxFor ? m_context.syntaxFor(path) : std::pair<const SyntaxDatabase*, QString>();
            m_hits += ProjectSearch::lazySyntaxFixes(path, text, syntax, fileType);
        } else {
            m_hits += ProjectSearch::find(path, text, textSearch);
        }
    }
    showResults(replaceMode);
}

void AdvancedSearchPanel::showResults(bool replaceMode)
{
    m_model->clear();
    m_scopeCaption->setText(m_scope->currentText() + QStringLiteral(": "));
    m_searchCaption->setText(m_lazy ? m_type->currentText() : m_searchText);
    m_count->setText(tr("%1 matches found.").arg(m_hits.size()));
    m_replaceRow->setVisible(replaceMode);

    QStandardItem* fileItem = nullptr;
    QString currentPath;
    int fileCount = 0;
    auto finishFile = [&] {
        if (fileItem)
            fileItem->setData(QStringLiteral("<b>%1 (%2)</b>").arg(currentPath.toHtmlEscaped()).arg(fileCount),
                              HtmlRole);
    };
    for (int i = 0; i < m_hits.size(); ++i) {
        const SearchHit& hit = m_hits.at(i);
        if (!fileItem || hit.filePath != currentPath) {
            finishFile();
            currentPath = hit.filePath;
            fileCount = 0;
            fileItem = new QStandardItem(QDir::fromNativeSeparators(currentPath));
            fileItem->setData(0, LineRole);
            fileItem->setData(-1, HitRole);
            fileItem->setEditable(false);
            if (replaceMode) {
                fileItem->setCheckable(true);
                fileItem->setUserTristate(false);
                fileItem->setCheckState(Qt::Checked);
            }
            m_model->appendRow(fileItem);
        }
        ++fileCount;
        auto* item = new QStandardItem(QStringLiteral("%1: %2").arg(hit.line + 1).arg(hit.lineText));
        item->setData(highlightedLine(hit), HtmlRole);
        item->setData(hit.line + 1, LineRole);
        item->setData(i, HitRole);
        item->setEditable(false);
        if (replaceMode) {
            item->setCheckable(true);
            item->setCheckState(Qt::Checked);
        }
        fileItem->appendRow(item);
    }
    finishFile();
    m_results->expandAll();
    m_stack->setCurrentIndex(1);
}

int AdvancedSearchPanel::replaceChecked()
{
    const QList<Project*> projects = m_context.projects ? m_context.projects() : QList<Project*>();
    const TextSearch textSearch(m_searchText, m_options);
    int total = 0;
    QStringList errors;
    for (int f = 0; f < m_model->rowCount(); ++f) {
        const QStandardItem* fileItem = m_model->item(f);
        QList<SearchHit> chosen;
        QString path;
        for (int r = 0; r < fileItem->rowCount(); ++r) {
            const QStandardItem* item = fileItem->child(r);
            if (item->checkState() != Qt::Checked)
                continue;
            SearchHit hit = m_hits.at(item->data(HitRole).toInt());
            if (!m_lazy) {
                TextSearch::Match match;
                match.start = hit.offset;
                match.length = hit.length;
                match.captures = hit.captures;
                hit.replacement = textSearch.replacement(match, m_replacement->text());
            }
            path = hit.filePath;
            chosen << hit;
        }
        if (chosen.isEmpty())
            continue;
        QString error;
        const int count = ProjectSearch::replaceHits(projects, path, chosen, &error);
        if (count < 0)
            errors << QDir::toNativeSeparators(path) + QStringLiteral(": ") + error;
        else
            total += count;
    }
    if (!errors.isEmpty())
        QMessageBox::warning(this, QApplication::applicationDisplayName(), errors.join(QLatin1Char('\n')));
    clear();
    Q_EMIT filesReplaced();
    return total;
}

} // namespace OFS

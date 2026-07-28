#include "shell/ControllerTemplatesWidget.h"

#include "core/actions/ActionRegistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QListWidget>
#include <QSaveFile>
#include <QToolBar>
#include <QVBoxLayout>

namespace OFS {

namespace {

const QString kSuffix = QStringLiteral(".sctrl");

} // namespace

ControllerTemplatesWidget::ControllerTemplatesWidget(ActionRegistry& actions, const QStringList& dirs, QWidget* parent)
    : QWidget(parent)
    , m_dirs(dirs)
    , m_toolBar(new QToolBar(this))
    , m_list(new QListWidget(this))
{
    setObjectName(QStringLiteral("controllersWidget"));
    m_toolBar->setObjectName(QStringLiteral("controllersToolBar"));
    m_toolBar->setIconSize(QSize(22, 22));
    for (const char* id : {"st_sctrl_new", "st_sctrl_edit", "st_sctrl_delete"}) {
        if (QAction* a = actions.action(QString::fromLatin1(id)))
            m_toolBar->addAction(a);
    }
    m_list->setObjectName(QStringLiteral("controllersList"));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_toolBar);
    layout->addWidget(m_list, 1);
    setFixedWidth(125);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const QString text = templateText(item->text());
        if (!text.isEmpty())
            Q_EMIT insertRequested(text);
    });
    reload();
}

void ControllerTemplatesWidget::reload()
{
    const QString selected = selectedName();
    QStringList names;
    for (const QString& dir : std::as_const(m_dirs)) {
        for (const QString& file : QDir(dir).entryList({QStringLiteral("*") + kSuffix}, QDir::Files)) {
            const QString name = QFileInfo(file).completeBaseName();
            if (!names.contains(name, Qt::CaseInsensitive))
                names << name;
        }
    }
    names.sort(Qt::CaseInsensitive);
    m_list->clear();
    m_list->addItems(names);
    selectName(selected);
}

QStringList ControllerTemplatesWidget::names() const
{
    QStringList result;
    for (int i = 0; i < m_list->count(); ++i)
        result << m_list->item(i)->text();
    return result;
}

QString ControllerTemplatesWidget::selectedName() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item ? item->text() : QString();
}

void ControllerTemplatesWidget::selectName(const QString& name)
{
    if (name.isEmpty())
        return;
    const QList<QListWidgetItem*> items = m_list->findItems(name, Qt::MatchFixedString);
    if (!items.isEmpty())
        m_list->setCurrentItem(items.first());
}

QString ControllerTemplatesWidget::templatePath(const QString& name) const
{
    // Later folders (the user's) win.
    for (auto it = m_dirs.crbegin(); it != m_dirs.crend(); ++it) {
        const QDir dir(*it);
        for (const QString& file : dir.entryList({QStringLiteral("*") + kSuffix}, QDir::Files)) {
            if (QFileInfo(file).completeBaseName().compare(name, Qt::CaseInsensitive) == 0)
                return dir.filePath(file);
        }
    }
    return {};
}

QString ControllerTemplatesWidget::templateText(const QString& name) const
{
    QFile file(templatePath(name));
    if (name.isEmpty() || !file.open(QIODevice::ReadOnly))
        return {};
    QString text = QString::fromUtf8(file.readAll());
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    return text;
}

bool ControllerTemplatesWidget::saveTemplate(const QString& name, const QString& text, QString* error)
{
    if (m_dirs.isEmpty() || name.trimmed().isEmpty())
        return false;
    const QString dir = m_dirs.last();
    QDir().mkpath(dir);
    QSaveFile file(QDir(dir).filePath(name.trimmed() + kSuffix));
    QString body = text;
    body.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    body.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
    if (!file.open(QIODevice::WriteOnly) || file.write(body.toUtf8()) < 0 || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    reload();
    selectName(name.trimmed());
    return true;
}

bool ControllerTemplatesWidget::deleteTemplate(const QString& name, QString* error)
{
    const QString path = templatePath(name);
    if (path.isEmpty())
        return false;
    QFile file(path);
    if (!file.remove()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    reload();
    return true;
}

} // namespace OFS

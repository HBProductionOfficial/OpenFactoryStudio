#include "shell/SpriteExplorer.h"

#include "core/settings/Config.h"
#include "core/sprites/QtImages.h"
#include "core/sprites/SpriteDocument.h"

#include <QComboBox>
#include <QListView>
#include <QStandardItemModel>

#include <algorithm>
#include <utility>

namespace OFS {

SpriteExplorer::SpriteExplorer(const Config& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_model(new QStandardItemModel(this))
{}

void SpriteExplorer::bind(QWidget* panel)
{
    if (m_groups)
        disconnect(m_groups, nullptr, this, nullptr);
    m_groups = panel ? panel->findChild<QComboBox*>(QStringLiteral("thumb_grp_list")) : nullptr;
    m_view = panel ? panel->findChild<QListView*>(QStringLiteral("thumb_view")) : nullptr;
    if (m_groups)
        connect(m_groups, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_updating)
                Q_EMIT groupChosen();
        });
}

void SpriteExplorer::show(const SpriteDocument* doc)
{
    const bool updating = std::exchange(m_updating, true);
    if (m_view) {
        m_view->setModel(m_model);
        m_view->setViewMode(QListView::IconMode);
        m_view->setMovement(QListView::Static);
        m_view->setResizeMode(QListView::Adjust);
        m_view->setWrapping(true);
        m_view->setSpacing(4);
        m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
    QList<int> groups;
    if (doc) {
        for (const SpriteEntry& entry : doc->sprites()) {
            if (!groups.contains(entry.group()))
                groups << entry.group();
        }
    }
    std::sort(groups.begin(), groups.end());
    int group = groups.isEmpty() ? -1 : groups.first();
    if (m_groups) {
        if (m_groups->currentIndex() >= 0 && groups.contains(m_groups->currentText().toInt()))
            group = m_groups->currentText().toInt();
        m_groups->clear();
        for (int g : std::as_const(groups))
            m_groups->addItem(QString::number(g));
        m_groups->setCurrentIndex(int(groups.indexOf(group)));
    }
    const int size = std::max(16, m_config.intValue(QStringLiteral("thumb_size"), 64));
    if (m_view)
        m_view->setIconSize(QSize(size, size));
    m_model->clear();
    for (int i = 0; doc && i < doc->spriteCount(); ++i) {
        const SpriteEntry& entry = doc->sprites()[i];
        if (entry.group() != group)
            continue;
        const Formats::Image rgba = doc->imageRgba(i);
        QPixmap thumbnail;
        if (!rgba.pixelBuffer().empty())
            thumbnail =
                QPixmap::fromImage(toQImage(rgba).scaled(size, size, Qt::KeepAspectRatio, Qt::FastTransformation));
        auto* item = new QStandardItem(QIcon(thumbnail), QString::number(entry.number()));
        item->setData(i, Qt::UserRole);
        m_model->appendRow(item);
    }
    m_updating = updating;
}

QList<int> SpriteExplorer::sprites(bool all) const
{
    QList<int> sprites;
    if (!m_view || m_view->model() != m_model || !m_view->selectionModel())
        return sprites;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QModelIndex index = m_model->index(row, 0);
        if (all || m_view->selectionModel()->isSelected(index))
            sprites << index.data(Qt::UserRole).toInt();
    }
    return sprites;
}

void SpriteExplorer::clearSelection()
{
    if (m_view && m_view->model() == m_model)
        m_view->clearSelection();
}

} // namespace OFS

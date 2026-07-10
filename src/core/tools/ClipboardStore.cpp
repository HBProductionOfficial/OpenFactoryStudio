#include "core/tools/ClipboardStore.h"

#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QUrl>

namespace OFS {

namespace {

constexpr qsizetype kMaxNameLength = 40;

const QString kImageFormat = QStringLiteral("application/x-qt-image");
const QString kColorFormat = QStringLiteral("application/x-color");

QString shorten(QString text)
{
    text = text.simplified();
    if (text.size() > kMaxNameLength)
        text = text.left(kMaxNameLength - 3) + QStringLiteral("...");
    return text;
}

} // namespace

ClipboardStore::ClipboardStore(QObject* parent)
    : QAbstractListModel(parent)
{}

ClipboardStore::~ClipboardStore() = default;

QMimeData* ClipboardStore::copyMimeData(const QMimeData* data)
{
    auto* copy = new QMimeData;
    if (!data)
        return copy;
    // Images and colors are kept as variants, the other formats as raw bytes.
    if (data->hasImage())
        copy->setImageData(data->imageData());
    if (data->hasColor())
        copy->setColorData(data->colorData());
    const QStringList formats = data->formats();
    for (const QString& format : formats) {
        if (format == kImageFormat || format == kColorFormat)
            continue;
        copy->setData(format, data->data(format));
    }
    return copy;
}

QString ClipboardStore::describe(const QMimeData* data)
{
    if (!data)
        return {};
    if (data->hasImage()) {
        const QImage image = qvariant_cast<QImage>(data->imageData());
        return tr("Image %1x%2").arg(image.width()).arg(image.height());
    }
    if (data->hasUrls() && !data->urls().isEmpty()) {
        const QList<QUrl> urls = data->urls();
        const QString first =
            urls.first().isLocalFile() ? QFileInfo(urls.first().toLocalFile()).fileName() : urls.first().toString();
        return urls.size() == 1 ? tr("File: %1").arg(shorten(first))
                                : tr("%1 files: %2, ...").arg(urls.size()).arg(shorten(first));
    }
    if (data->hasText()) {
        const QString text = data->text();
        return tr("Text: %1").arg(shorten(text.section(QLatin1Char('\n'), 0, 0)));
    }
    if (data->hasColor())
        return tr("Color %1").arg(qvariant_cast<QColor>(data->colorData()).name());
    const QStringList formats = data->formats();
    return formats.isEmpty() ? QString() : shorten(formats.first());
}

int ClipboardStore::add(const QMimeData* data, const QString& name)
{
    if (!data || data->formats().isEmpty())
        return -1;
    const int row = count();
    beginInsertRows({}, row, row);
    Entry entry;
    entry.data.reset(copyMimeData(data));
    entry.name = name.trimmed().isEmpty() ? describe(data) : name.trimmed();
    m_entries.push_back(std::move(entry));
    endInsertRows();
    return row;
}

bool ClipboardStore::rename(int row, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (row < 0 || row >= count() || trimmed.isEmpty())
        return false;
    if (m_entries[size_t(row)].name != trimmed) {
        m_entries[size_t(row)].name = trimmed;
        const QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
    }
    return true;
}

bool ClipboardStore::remove(int row)
{
    if (row < 0 || row >= count())
        return false;
    beginRemoveRows({}, row, row);
    m_entries.erase(m_entries.begin() + row);
    endRemoveRows();
    return true;
}

void ClipboardStore::clear()
{
    if (m_entries.empty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

QString ClipboardStore::name(int row) const
{
    return row >= 0 && row < count() ? m_entries[size_t(row)].name : QString();
}

const QMimeData* ClipboardStore::entryData(int row) const
{
    return row >= 0 && row < count() ? m_entries[size_t(row)].data.get() : nullptr;
}

QMimeData* ClipboardStore::copy(int row) const
{
    const QMimeData* source = entryData(row);
    return source ? copyMimeData(source) : nullptr;
}

int ClipboardStore::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant ClipboardStore::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= count())
        return {};
    const Entry& entry = m_entries[size_t(index.row())];
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return entry.name;
    case Qt::ToolTipRole:
        return describe(entry.data.get());
    default:
        return {};
    }
}

bool ClipboardStore::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    return rename(index.row(), value.toString());
}

Qt::ItemFlags ClipboardStore::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

} // namespace OFS

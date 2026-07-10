#pragma once

#include "core/CoreExport.h"

#include <QAbstractListModel>
#include <QString>

#include <memory>
#include <vector>

class QMimeData;

namespace OFS {

// View > Clipboard container. Entries keep every format so they go back on the clipboard unchanged.
class OFS_CORE_EXPORT ClipboardStore : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ClipboardStore(QObject* parent = nullptr);
    ~ClipboardStore() override;

    // An empty name becomes a description of the content. Returns -1 when `data` holds nothing.
    int add(const QMimeData* data, const QString& name = {});
    bool rename(int row, const QString& name);
    bool remove(int row);
    void clear();

    int count() const { return int(m_entries.size()); }
    QString name(int row) const;
    const QMimeData* entryData(int row) const;
    QMimeData* copy(int row) const;

    // "Text: Hello", "Image 32x48", ...
    static QString describe(const QMimeData* data);
    static QMimeData* copyMimeData(const QMimeData* data);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    struct Entry
    {
        QString name;
        std::unique_ptr<QMimeData> data;
    };
    std::vector<Entry> m_entries;
};

} // namespace OFS

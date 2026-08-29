#pragma once

#include <QDialog>
#include <QList>

class QStandardItemModel;
class QTableView;

namespace OFS {

// Remap tool row: sprites drawn with palette source show with palette target instead (display only).
struct RemapRow
{
    int sourceGroup = 1;
    int sourceNumber = 1;
    int targetGroup = 1;
    int targetNumber = 1;
};

class RemapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemapDialog(QWidget* parent = nullptr);

    QList<RemapRow> rows() const;
    void setRows(const QList<RemapRow>& rows);
    QTableView* table() const { return m_table; }

Q_SIGNALS:
    void rowsChanged();

private:
    void addRow(const RemapRow& row);

    QStandardItemModel* m_model;
    QTableView* m_table;
};

} // namespace OFS

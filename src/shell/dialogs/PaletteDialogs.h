#pragma once

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QListWidget;
class QStandardItemModel;
class QTableView;

namespace OFS {

QString paletteFileFilters();

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

// Advanced palette editor > Commit sprite changes. Ignore commits only the pixels and keeps the sprite's palette.
class CommitSpriteDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        ImagePalette,   // the image's palette, if the sprite file does not hold it already
        ForcePalette,   // the palette chosen in the list (the image adapted to it)
        ExchangePalette // the palette chosen in the list gets the image's colors
    };
    static constexpr int Ignored = 2; // exec() result of Ignore

    explicit CommitSpriteDialog(const QStringList& palettes, QWidget* parent = nullptr);

    Mode mode() const;
    bool reversed() const;
    QList<int> palettes() const;

private:
    QCheckBox* m_reversed;
    QComboBox* m_mode;
    QListWidget* m_list;
};

} // namespace OFS

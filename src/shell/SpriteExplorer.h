#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class QComboBox;
class QListView;
class QStandardItemModel;
class QWidget;

namespace OFS {

class Config;
class SpriteDocument;

// Sprite explorer of the Thumbnails panel, shared by the Sprites editor's image mode and the Animations editor.
class SpriteExplorer : public QObject
{
    Q_OBJECT

public:
    explicit SpriteExplorer(const Config& config, QObject* parent = nullptr);

    void bind(QWidget* panel);
    void show(const SpriteDocument* doc);
    QList<int> sprites(bool all) const;
    void clearSelection();

Q_SIGNALS:
    void groupChosen();

private:
    const Config& m_config;
    QPointer<QComboBox> m_groups;
    QPointer<QListView> m_view;
    QStandardItemModel* m_model;
    bool m_updating = false;
};

} // namespace OFS

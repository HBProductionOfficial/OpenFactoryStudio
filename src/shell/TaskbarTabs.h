#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QWidget;

namespace OFS {

// Windows only: each project tab gets its own thumbnail in the taskbar preview, showing the character's big portrait or
// the window. Does nothing on other platforms.
class TaskbarTabs : public QObject
{
    Q_OBJECT

public:
    struct Tab
    {
        QObject* key = nullptr;
        QString title;
        std::function<QImage()> portrait;
    };

    explicit TaskbarTabs(QWidget* window);
    ~TaskbarTabs() override;

    // Whether the platform has tabbed thumbnails (Windows, except the offscreen platform).
    static bool isSupported();

    void setTabs(const QList<Tab>& tabs, QObject* current);
    int count() const;

Q_SIGNALS:
    void activated(QObject* key);
    void closeRequested(QObject* key);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace OFS

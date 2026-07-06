#pragma once

#include "core/CoreExport.h"

#include <QKeySequence>
#include <QMap>
#include <QObject>
#include <QString>

namespace OFS {

// Keyboard shortcuts (shortcuts.xml):
//     <shortcut action="file_open" keys="Ctrl+O"/>
// Defaults come from data/shortcuts.xml and the user file stores the whole edited table. An empty
// "keys" attribute means the user removed the shortcut.
class OFS_CORE_EXPORT ShortcutSettings : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutSettings(QObject* parent = nullptr);

    void load(const QString& defaultsPath, const QString& userPath);
    bool save(QString* error = nullptr) const;
    bool saveAs(const QString& path, QString* error = nullptr) const;

    QKeySequence keys(const QString& action) const;
    QKeySequence defaultKeys(const QString& action) const;
    bool hasEntry(const QString& action) const;
    void setKeys(const QString& action, const QKeySequence& keys);
    void resetAll();

    QStringList actions() const;

    static QString toText(const QKeySequence& keys);
    static QKeySequence fromText(const QString& text);

Q_SIGNALS:
    void changed(const QString& action);

private:
    QMap<QString, QString> m_defaults;
    QMap<QString, QString> m_values;
    QString m_userPath;
};

} // namespace OFS

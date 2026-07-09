#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace OFS::Mugen {

// Small reader for M.U.G.E.N definition files, used to identify projects and the files they name.
// The code editors use the full code model.
class DefFile
{
public:
    struct Section
    {
        QString name;
        QList<QPair<QString, QString>> values;
    };

    bool load(const QString& path);
    void parse(const QString& text);

    const QList<Section>& sections() const { return m_sections; }
    bool hasSection(const QString& name) const;
    bool hasSectionStartingWith(const QString& prefix) const;
    QString value(const QString& section, const QString& key, const QString& fallback = {}) const;
    QStringList values(const QString& section, const QString& keyPattern) const;

    static QString unquote(const QString& value);

private:
    const Section* find(const QString& name) const;
    QList<Section> m_sections;
};

} // namespace OFS::Mugen

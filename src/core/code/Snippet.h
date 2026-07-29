#pragma once

#include "core/CoreExport.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace OFS {

// Asked for before a snippet is inserted. @name in the code is replaced with the value.
struct OFS_CORE_EXPORT SnippetField
{
    enum class Type
    {
        Int,
        Float,
        String,
        Items
    };

    QString name;
    QString title;
    Type type = Type::String;
    QStringList values;
    QStringList valueTitles;

    static Type typeFromString(const QString& type);
    static QString typeToString(Type type);
};

// A code snippet: one XML file in an engine's snippets folder, named after the snippet.
//   <snippet>
//     <field name="sdef" title="StateDef" type="int"/>
//     <field name="kind" title="Type" type="items" value="S,C,A" value_title="Standing,Crouching,Air"/>
//     <code>[StateDef @sdef] ...</code>
//   </snippet>
class OFS_CORE_EXPORT Snippet
{
public:
    QString name;
    QList<SnippetField> fields;
    QString code;
    QString path;

    bool load(const QString& filePath, QString* error = nullptr);
    // UTF-8, CRLF, tab indentation, code lines indented by two tabs.
    bool save(const QString& filePath, QString* error = nullptr) const;
    bool saveTranslationSource(const QString& filePath, QString* error = nullptr) const;

    // Drops the empty first line, left-trims every line, replaces @name and removes the CURSOR
    // marker. *cursor gets the marker's offset, or -1.
    QString expand(const QHash<QString, QString>& values, int* cursor = nullptr) const;
};

// Later folders override snippets of the same name, so the user's folder comes last.
class OFS_CORE_EXPORT SnippetLibrary
{
public:
    void load(const QStringList& dirs);
    QStringList names() const;
    const Snippet* find(const QString& name) const;
    void insert(const Snippet& snippet);

private:
    QHash<QString, Snippet> m_snippets;
};

} // namespace OFS

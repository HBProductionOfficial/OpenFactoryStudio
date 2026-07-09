#pragma once

#include "core/CoreExport.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>

namespace OFS {

// syntax.xml <param>.
struct SyntaxParam
{
    QString name;
    QString type;
    bool required = false;
    QStringList optional; // per-component requirement: req, def, prev
    int max = 0;
    QStringList list;
    QString prefixes; // letters allowed before a number ("S", "F", "SF")
    bool indexed = false;
    bool nil = false; // value may be omitted or -1
    QString minver;
    QString deprecated;
};

// syntax.xml <block type="Section>Type:Base">.
struct SyntaxBlock
{
    QString name;
    QString section;
    QString typeName;
    QString base; // inherited block ("Pause", "state.HitDef")
    QString minver;
    QString deprecated;
    bool required = false;
    QList<SyntaxParam> params;
};

struct SyntaxFileType
{
    QString type;
    QList<SyntaxBlock> blocks;
};

struct SyntaxTrigger
{
    QString name;
    QString type;
    QStringList list;
    QStringList context;
    QString minver;
    QString deprecated;
};

// The syntax.xml of an engine, used by the highlighter, the validator and the completer.
class OFS_CORE_EXPORT SyntaxDatabase
{
public:
    SyntaxDatabase();
    ~SyntaxDatabase();
    SyntaxDatabase(const SyntaxDatabase&) = delete;
    SyntaxDatabase& operator=(const SyntaxDatabase&) = delete;

    bool load(const QString& path, QString* error = nullptr);
    bool loadFromData(const QByteArray& xml, QString* error = nullptr);
    bool isEmpty() const { return m_files.isEmpty(); }

    const SyntaxFileType* fileType(const QString& type) const;
    QStringList fileTypes() const;

    // Outside state controllers a unique abbreviation of the type works too ("norm").
    const SyntaxBlock* findBlock(const QString& fileType, const QString& header, const QString& typeValue) const;
    bool isTypedSection(const QString& fileType, const QString& header) const;
    QStringList sectionTypes(const QString& fileType, const QString& header) const;

    QList<SyntaxParam> params(const QString& fileType, const SyntaxBlock& block) const;
    const SyntaxParam* param(const QString& fileType, const SyntaxBlock& block, const QString& name) const;

    const SyntaxTrigger* trigger(const QString& name) const;
    QStringList triggerNames() const;
    bool isConst(const QString& group, const QString& name) const;
    QStringList constNames(const QString& group) const;

    // "" (any) < dosbeta < winbeta < 1.0 < 1.1 < ext, case-insensitive.
    static int versionRank(const QString& version);
    static bool availableIn(const QString& minver, const QString& projectVersion);
    static bool deprecatedIn(const QString& deprecated, const QString& projectVersion);

    static bool wildcardMatch(const QString& pattern, const QString& text);
    static bool headerMatchesSection(const QString& header, const QString& section);

private:
    const SyntaxBlock* resolveBase(const QString& fileType, const QString& base, QString* baseFileType) const;
    void collectParams(const QString& fileType, const SyntaxBlock& block, QList<SyntaxParam>& out, QSet<QString>& seen,
                       int depth) const;

    QHash<QString, SyntaxFileType> m_files;
    QStringList m_fileOrder;
    QList<SyntaxTrigger> m_triggers;
    QHash<QString, QStringList> m_consts;
    mutable QHash<QString, QList<SyntaxParam>> m_paramCache;
};

} // namespace OFS

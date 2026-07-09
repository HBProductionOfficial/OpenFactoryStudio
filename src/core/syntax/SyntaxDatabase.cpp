#include "core/syntax/SyntaxDatabase.h"

#include "core/settings/Config.h"

#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

namespace OFS {

namespace {

bool boolAttr(const QXmlStreamAttributes& a, const char* name)
{
    return Config::decodeBool(a.value(QLatin1String(name)).toString(), false);
}

QStringList listAttr(const QXmlStreamAttributes& a, const char* name)
{
    QStringList out;
    for (const QString& part : a.value(QLatin1String(name)).toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
        out << part.trimmed();
    return out;
}

} // namespace

SyntaxDatabase::SyntaxDatabase() = default;
SyntaxDatabase::~SyntaxDatabase() = default;

bool SyntaxDatabase::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return loadFromData(file.readAll(), error);
}

bool SyntaxDatabase::loadFromData(const QByteArray& xml, QString* error)
{
    m_files.clear();
    m_fileOrder.clear();
    m_triggers.clear();
    m_consts.clear();
    m_paramCache.clear();

    QXmlStreamReader r(xml);
    if (!r.readNextStartElement() || r.name() != QLatin1String("syntax")) {
        if (error)
            *error = QStringLiteral("not a syntax database");
        return false;
    }
    while (r.readNextStartElement()) {
        const auto attrs = r.attributes();
        if (r.name() == QLatin1String("file")) {
            SyntaxFileType ft;
            ft.type = attrs.value(QLatin1String("type")).toString();
            while (r.readNextStartElement()) {
                if (r.name() != QLatin1String("block")) {
                    r.skipCurrentElement();
                    continue;
                }
                const auto ba = r.attributes();
                SyntaxBlock block;
                QString type = ba.value(QLatin1String("type")).toString();
                type.replace(QLatin1String("::"), QLatin1String(":"));
                const qsizetype colon = type.indexOf(QLatin1Char(':'));
                if (colon >= 0) {
                    block.base = type.mid(colon + 1).trimmed();
                    type = type.left(colon);
                }
                block.name = type.trimmed();
                const qsizetype gt = block.name.indexOf(QLatin1Char('>'));
                block.section = (gt >= 0 ? block.name.left(gt) : block.name).trimmed();
                block.typeName = gt >= 0 ? block.name.mid(gt + 1).trimmed() : QString();
                block.minver = ba.value(QLatin1String("minver")).toString();
                block.deprecated = ba.value(QLatin1String("deprecated")).toString();
                block.required = boolAttr(ba, "required");
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("param")) {
                        const auto pa = r.attributes();
                        SyntaxParam p;
                        p.name = pa.value(QLatin1String("name")).toString();
                        p.type = pa.value(QLatin1String("type")).toString();
                        p.required = boolAttr(pa, "required");
                        p.optional = listAttr(pa, "optional");
                        p.max = pa.value(QLatin1String("max")).toInt();
                        p.list = listAttr(pa, "list");
                        p.prefixes = pa.value(QLatin1String("char")).toString();
                        p.indexed = pa.value(QLatin1String("format")) == QLatin1String("indexed");
                        p.nil = boolAttr(pa, "nil");
                        p.minver = pa.value(QLatin1String("minver")).toString();
                        p.deprecated = pa.value(QLatin1String("deprecated")).toString();
                        block.params << p;
                    }
                    r.skipCurrentElement();
                }
                ft.blocks << block;
            }
            const QString key = ft.type.toLower();
            if (!m_files.contains(key))
                m_fileOrder << ft.type;
            m_files.insert(key, ft);
        } else if (r.name() == QLatin1String("consts")) {
            QStringList names;
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("const"))
                    names << r.attributes().value(QLatin1String("name")).toString();
                r.skipCurrentElement();
            }
            m_consts[attrs.value(QLatin1String("name")).toString().toLower()] += names;
        } else if (r.name() == QLatin1String("trigger")) {
            SyntaxTrigger t;
            t.name = attrs.value(QLatin1String("name")).toString();
            t.type = attrs.value(QLatin1String("type")).toString();
            t.list = listAttr(attrs, "list");
            t.context = listAttr(attrs, "context");
            t.minver = attrs.value(QLatin1String("minver")).toString();
            t.deprecated = attrs.value(QLatin1String("deprecated")).toString();
            m_triggers << t;
            r.skipCurrentElement();
        } else {
            r.skipCurrentElement();
        }
    }
    if (r.hasError()) {
        if (error)
            *error = QStringLiteral("line %1: %2").arg(r.lineNumber()).arg(r.errorString());
        return false;
    }
    return true;
}

const SyntaxFileType* SyntaxDatabase::fileType(const QString& type) const
{
    auto it = m_files.constFind(type.toLower());
    return it != m_files.cend() ? &it.value() : nullptr;
}

QStringList SyntaxDatabase::fileTypes() const
{
    return m_fileOrder;
}

bool SyntaxDatabase::wildcardMatch(const QString& pattern, const QString& text)
{
    if (!pattern.contains(QLatin1Char('*')))
        return pattern.compare(text, Qt::CaseInsensitive) == 0;
    // Patterns are few and reused all the time, so cache them per thread.
    thread_local QHash<QString, QRegularExpression> cache;
    auto it = cache.find(pattern);
    if (it == cache.end()) {
        QString escaped = QRegularExpression::escape(pattern);
        escaped.replace(QLatin1String("\\*"), QLatin1String(".*"));
        it = cache.insert(pattern, QRegularExpression(QRegularExpression::anchoredPattern(escaped),
                                                      QRegularExpression::CaseInsensitiveOption));
    }
    return it->match(text).hasMatch();
}

bool SyntaxDatabase::headerMatchesSection(const QString& header, const QString& section)
{
    const QString h = header.trimmed();
    if (section.contains(QLatin1Char('*'))) {
        // "BG*Def" matches "BGDef" and "BG1Def". Trailing text after a space is allowed ("Scene 3").
        if (wildcardMatch(section, h))
            return true;
        const qsizetype space = h.indexOf(QLatin1Char(' '));
        return space > 0 && wildcardMatch(section, h.left(space));
    }
    if (h.compare(section, Qt::CaseInsensitive) == 0)
        return true;
    if (h.size() > section.size() && h.startsWith(section, Qt::CaseInsensitive)) {
        const QChar next = h.at(section.size());
        return next == QLatin1Char(' ') || next == QLatin1Char('\t') || next.isDigit() || next == QLatin1Char('-');
    }
    return false;
}

const SyntaxBlock* SyntaxDatabase::findBlock(const QString& fileTypeName, const QString& header,
                                             const QString& typeValue) const
{
    const SyntaxFileType* ft = fileType(fileTypeName);
    if (!ft)
        return nullptr;
    const SyntaxBlock* best = nullptr;
    qsizetype bestLength = -1;
    for (const SyntaxBlock& b : ft->blocks) {
        if (!headerMatchesSection(header, b.section))
            continue;
        if (!b.typeName.isEmpty()) {
            if (typeValue.isEmpty() || !wildcardMatch(b.typeName, typeValue.trimmed()))
                continue;
        }
        const qsizetype length = b.section.size() * 2 + (b.typeName.isEmpty() ? 0 : 1);
        if (length > bestLength) {
            best = &b;
            bestLength = length;
        }
    }
    return best;
}

bool SyntaxDatabase::isTypedSection(const QString& fileTypeName, const QString& header) const
{
    const SyntaxFileType* ft = fileType(fileTypeName);
    if (!ft)
        return false;
    for (const SyntaxBlock& b : ft->blocks) {
        if (!b.typeName.isEmpty() && headerMatchesSection(header, b.section))
            return true;
    }
    return false;
}

QStringList SyntaxDatabase::sectionTypes(const QString& fileTypeName, const QString& header) const
{
    QStringList types;
    if (const SyntaxFileType* ft = fileType(fileTypeName)) {
        for (const SyntaxBlock& b : ft->blocks) {
            if (!b.typeName.isEmpty() && headerMatchesSection(header, b.section) && !types.contains(b.typeName))
                types << b.typeName;
        }
    }
    return types;
}

const SyntaxBlock* SyntaxDatabase::resolveBase(const QString& fileTypeName, const QString& base,
                                               QString* baseFileType) const
{
    QString file = fileTypeName;
    QString name = base;
    // "file.Block" refers to another file type when the prefix is a known file type.
    const qsizetype dot = base.indexOf(QLatin1Char('.'));
    if (dot > 0 && fileType(base.left(dot))) {
        file = base.left(dot);
        name = base.mid(dot + 1);
    }
    *baseFileType = file;
    const SyntaxFileType* ft = fileType(file);
    if (!ft)
        return nullptr;
    for (const SyntaxBlock& b : ft->blocks) {
        if (b.name.compare(name, Qt::CaseInsensitive) == 0)
            return &b;
    }
    QString typed = name;
    typed.replace(QLatin1Char('.'), QLatin1Char('>'));
    for (const SyntaxBlock& b : ft->blocks) {
        if (b.name.compare(typed, Qt::CaseInsensitive) == 0 || b.typeName.compare(name, Qt::CaseInsensitive) == 0)
            return &b;
    }
    return nullptr;
}

void SyntaxDatabase::collectParams(const QString& fileTypeName, const SyntaxBlock& block, QList<SyntaxParam>& out,
                                   QSet<QString>& seen, int depth) const
{
    for (const SyntaxParam& p : block.params) {
        const QString key = p.name.toLower();
        if (!seen.contains(key)) {
            seen.insert(key);
            out << p;
        }
    }
    if (block.base.isEmpty() || depth > 16)
        return;
    QString baseFile;
    if (const SyntaxBlock* base = resolveBase(fileTypeName, block.base, &baseFile))
        collectParams(baseFile, *base, out, seen, depth + 1);
}

QList<SyntaxParam> SyntaxDatabase::params(const QString& fileTypeName, const SyntaxBlock& block) const
{
    const QString key = fileTypeName.toLower() + QLatin1Char('/') + block.name.toLower();
    auto it = m_paramCache.constFind(key);
    if (it != m_paramCache.cend())
        return *it;
    QList<SyntaxParam> out;
    QSet<QString> seen;
    collectParams(fileTypeName, block, out, seen, 0);
    m_paramCache.insert(key, out);
    return out;
}

const SyntaxParam* SyntaxDatabase::param(const QString& fileTypeName, const SyntaxBlock& block,
                                         const QString& name) const
{
    const QString key = fileTypeName.toLower() + QLatin1Char('/') + block.name.toLower();
    params(fileTypeName, block);
    const QList<SyntaxParam>& list = m_paramCache[key];
    for (const SyntaxParam& p : list) {
        if (wildcardMatch(p.name, name))
            return &p;
    }
    return nullptr;
}

const SyntaxTrigger* SyntaxDatabase::trigger(const QString& name) const
{
    for (const SyntaxTrigger& t : m_triggers) {
        if (t.name.compare(name, Qt::CaseInsensitive) == 0)
            return &t;
    }
    for (const SyntaxTrigger& t : m_triggers) {
        if (t.name.contains(QLatin1Char('*')) && wildcardMatch(t.name, name))
            return &t;
    }
    return nullptr;
}

QStringList SyntaxDatabase::triggerNames() const
{
    QStringList names;
    for (const SyntaxTrigger& t : m_triggers)
        names << t.name;
    return names;
}

bool SyntaxDatabase::isConst(const QString& group, const QString& name) const
{
    const auto it = m_consts.constFind(group.toLower());
    if (it == m_consts.cend())
        return false;
    for (const QString& c : *it) {
        if (c.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QStringList SyntaxDatabase::constNames(const QString& group) const
{
    return m_consts.value(group.toLower());
}

int SyntaxDatabase::versionRank(const QString& version)
{
    const QString v = version.trimmed().toLower();
    if (v.isEmpty())
        return 0;
    if (v == QLatin1String("dosbeta") || v == QLatin1String("dos"))
        return 1;
    if (v == QLatin1String("winbeta") || v == QLatin1String("win"))
        return 2;
    if (v == QLatin1String("1.0"))
        return 3;
    if (v == QLatin1String("1.1"))
        return 4;
    if (v == QLatin1String("ext"))
        return 5;
    return 0;
}

bool SyntaxDatabase::availableIn(const QString& minver, const QString& projectVersion)
{
    if (minver.isEmpty())
        return true;
    return versionRank(projectVersion) >= versionRank(minver);
}

bool SyntaxDatabase::deprecatedIn(const QString& deprecated, const QString& projectVersion)
{
    if (deprecated.isEmpty())
        return false;
    return versionRank(projectVersion) >= versionRank(deprecated);
}

} // namespace OFS

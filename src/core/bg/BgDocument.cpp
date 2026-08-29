#include "core/bg/BgDocument.h"

#include <QRegularExpression>

#include <cmath>

namespace OFS {

namespace {

QString code(const QString& line)
{
    QString raw = line;
    if (raw.endsWith(QLatin1Char('\r')))
        raw.chop(1);
    const qsizetype comment = raw.indexOf(QLatin1Char(';'));
    return (comment >= 0 ? raw.left(comment) : raw).trimmed();
}

QList<int> ids(const QString& text)
{
    QList<int> list;
    for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int id = part.trimmed().toInt(&ok);
        if (ok)
            list << id;
    }
    return list;
}

bool headed(const QString& header, const QString& prefix, const QString& word, QString* rest)
{
    const QString start = prefix + word;
    if (!header.startsWith(start, Qt::CaseInsensitive))
        return false;
    const QString after = header.mid(start.size());
    if (!after.isEmpty() && !after.front().isSpace())
        return false;
    *rest = after.trimmed();
    return true;
}

BgElement element(const BgSection& section)
{
    BgElement e;
    e.section = section;
    if (const auto type = section.value(QStringLiteral("type")))
        e.type = BgDocument::parseType(*type);
    QList<double> n = section.numbers(QStringLiteral("spriteno"), 2);
    e.spriteGroup = int(n[0]);
    e.spriteNumber = int(n[1]);
    if (const auto action = section.value(QStringLiteral("actionno")))
        e.action = action->toInt();
    e.layer = int(section.numbers(QStringLiteral("layerno"), 1)[0]);
    n = section.numbers(QStringLiteral("start"), 2);
    e.start = QPointF(n[0], n[1]);
    n = section.numbers(QStringLiteral("delta"), 2, 1);
    e.delta = QPointF(n[0], n[1]);
    n = section.numbers(QStringLiteral("velocity"), 2);
    e.velocity = QPointF(n[0], n[1]);
    const QString trans = section.value(QStringLiteral("trans")).value_or(QString()).toLower();
    e.trans = trans == QLatin1String("add")        ? BgElement::Trans::Add
              : trans == QLatin1String("add1")     ? BgElement::Trans::Add1
              : trans == QLatin1String("addalpha") ? BgElement::Trans::AddAlpha
              : trans == QLatin1String("sub")      ? BgElement::Trans::Sub
                                                   : BgElement::Trans::None;
    n = section.numbers(QStringLiteral("alpha"), 2);
    if (section.value(QStringLiteral("alpha"))) {
        e.alphaSource = int(n[0]);
        e.alphaDest = int(n[1]);
    }
    e.mask = section.numbers(QStringLiteral("mask"), 1)[0] != 0 || e.type == BgElement::Type::Animation;
    n = section.numbers(QStringLiteral("tile"), 2);
    e.tileX = int(n[0]);
    e.tileY = int(n[1]);
    n = section.numbers(QStringLiteral("tilespacing"), 2);
    e.tileSpacingX = int(n[0]);
    e.tileSpacingY = int(n[1]); // one value leaves y at 0, like the engine
    for (const char* key : {"window", "maskwindow"}) {
        if (section.value(QLatin1String(key))) {
            n = section.numbers(QLatin1String(key), 4);
            e.hasWindow = true;
            e.window = QRect(QPoint(int(n[0]), int(n[1])), QPoint(int(n[2]), int(n[3])));
        }
    }
    n = section.numbers(QStringLiteral("windowdelta"), 2);
    e.windowDelta = QPointF(n[0], n[1]);
    if (section.value(QStringLiteral("width"))) {
        n = section.numbers(QStringLiteral("width"), 2);
        e.hasWidth = true;
        e.widthTop = n[0];
        e.widthBottom = n[1];
    }
    if (section.value(QStringLiteral("xscale"))) {
        n = section.numbers(QStringLiteral("xscale"), 2, 1);
        e.hasXScale = true;
        e.xScaleTop = n[0];
        e.xScaleBottom = n[1];
    }
    e.yScaleStart = section.numbers(QStringLiteral("yscalestart"), 1, 100)[0];
    e.yScaleDelta = section.numbers(QStringLiteral("yscaledelta"), 1)[0];
    n = section.numbers(QStringLiteral("scalestart"), 2, 1);
    e.scaleStart = QPointF(n[0], n[1]);
    n = section.numbers(QStringLiteral("scaledelta"), 2);
    e.scaleDelta = QPointF(n[0], n[1]);
    n = section.numbers(QStringLiteral("sin.x"), 3);
    std::copy(n.begin(), n.end(), e.sinX);
    n = section.numbers(QStringLiteral("sin.y"), 3);
    std::copy(n.begin(), n.end(), e.sinY);
    e.id = int(section.numbers(QStringLiteral("id"), 1)[0]);
    e.positionLink = section.numbers(QStringLiteral("positionlink"), 1)[0] != 0;
    return e;
}

BgController controller(const BgSection& section)
{
    BgController c;
    c.section = section;
    c.type = BgDocument::parseControllerType(section.value(QStringLiteral("type")).value_or(QString()));
    if (const auto time = section.value(QStringLiteral("time"))) {
        const QStringList parts = time->split(QLatin1Char(','));
        c.startTime = parts.value(0).trimmed().toInt();
        c.endTime = parts.size() > 1 ? parts[1].trimmed().toInt() : c.startTime;
        if (parts.size() > 2)
            c.loopTime = parts[2].trimmed().toInt();
    }
    if (const auto value = section.value(QStringLiteral("value"))) {
        for (const QString& part : value->split(QLatin1Char(',')))
            c.value << part.trimmed().toDouble();
    }
    if (const auto x = section.value(QStringLiteral("x")))
        c.x = x->toDouble();
    if (const auto y = section.value(QStringLiteral("y")))
        c.y = y->toDouble();
    if (const auto list = section.value(QStringLiteral("ctrlid")))
        c.ids = ids(*list);
    return c;
}

} // namespace

std::optional<QString> BgSection::value(const QString& key) const
{
    for (auto it = params.crbegin(); it != params.crend(); ++it) {
        if (it->key == key)
            return it->value;
    }
    return std::nullopt;
}

QList<double> BgSection::numbers(const QString& key, int count, double fallback) const
{
    QList<double> list(count, fallback);
    const std::optional<QString> text = value(key);
    if (!text)
        return list;
    const QStringList parts = text->split(QLatin1Char(','));
    for (int i = 0; i < count && i < parts.size(); ++i) {
        bool ok = false;
        const double v = parts[i].trimmed().toDouble(&ok);
        if (ok)
            list[i] = v;
    }
    return list;
}

BgElement::Type BgDocument::parseType(const QString& text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("parallax"))
        return BgElement::Type::Parallax;
    if (t == QLatin1String("anim"))
        return BgElement::Type::Animation;
    if (t == QLatin1String("dummy"))
        return BgElement::Type::Dummy;
    if (t == QLatin1String("null"))
        return BgElement::Type::Null;
    return BgElement::Type::Normal;
}

BgController::Type BgDocument::parseControllerType(const QString& text)
{
    const QString t = text.trimmed().toLower();
    static const QList<QPair<QString, BgController::Type>> names = {
        {QStringLiteral("visible"), BgController::Type::Visible},
        {QStringLiteral("enable"), BgController::Type::Enable},
        {QStringLiteral("velset"), BgController::Type::VelSet},
        {QStringLiteral("veladd"), BgController::Type::VelAdd},
        {QStringLiteral("posset"), BgController::Type::PosSet},
        {QStringLiteral("posadd"), BgController::Type::PosAdd},
        {QStringLiteral("anim"), BgController::Type::Anim},
        {QStringLiteral("sinx"), BgController::Type::SinX},
        {QStringLiteral("siny"), BgController::Type::SinY},
    };
    for (const auto& [name, type] : names) {
        if (t == name)
            return type;
    }
    return BgController::Type::Null;
}

BgDocument BgDocument::parse(const QStringList& lines)
{
    BgDocument doc;
    BgSection* current = nullptr;
    for (int n = 0; n < lines.size(); ++n) {
        const QString text = code(lines.at(n));
        if (text.startsWith(QLatin1Char('['))) {
            if (current)
                current->endLine = n;
            const qsizetype end = text.indexOf(QLatin1Char(']'));
            BgSection section;
            section.header = text.mid(1, end > 0 ? end - 1 : -1).trimmed();
            section.headerLine = n;
            section.lastLine = n;
            doc.m_sections << section;
            current = &doc.m_sections.last();
            continue;
        }
        const qsizetype equals = text.indexOf(QLatin1Char('='));
        if (!current || equals <= 0)
            continue;
        current->params << BgParam{text.left(equals).trimmed().toLower(), text.mid(equals + 1).trimmed(), n};
        current->lastLine = n;
    }
    if (current)
        current->endLine = int(lines.size());

    // Groups: "[<prefix>Def]" with no name and "BG" in the prefix. "[BGCtrlDef]" is not one.
    static const QRegularExpression groupDef(QStringLiteral("^(\\S*BG\\S*)def$"),
                                             QRegularExpression::CaseInsensitiveOption);
    for (const BgSection& section : std::as_const(doc.m_sections)) {
        const QRegularExpressionMatch match = groupDef.match(section.header);
        if (!match.hasMatch() || match.captured(1).endsWith(QLatin1String("Ctrl"), Qt::CaseInsensitive))
            continue;
        BgGroup group;
        group.prefix = match.captured(1);
        group.def = section;
        doc.m_groups << group;
    }
    for (BgGroup& group : doc.m_groups) {
        for (const BgSection& section : std::as_const(doc.m_sections)) {
            QString rest;
            BgSection named = section;
            if (headed(section.header, group.prefix, QStringLiteral("CtrlDef"), &rest)) {
                named.name = rest;
                BgControllerDef def;
                def.section = named;
                def.loopTime = int(named.numbers(QStringLiteral("looptime"), 1, -1)[0]);
                if (const auto list = named.value(QStringLiteral("ctrlid")))
                    def.ids = ids(*list);
                group.controllerDefs << def;
            } else if (headed(section.header, group.prefix, QStringLiteral("Ctrl"), &rest)) {
                named.name = rest;
                if (!group.controllerDefs.isEmpty())
                    group.controllerDefs.last().controllers << controller(named);
            } else if (headed(section.header, group.prefix, QString(), &rest) && !rest.isEmpty()) {
                named.name = rest;
                group.elements << element(named);
            }
        }
    }
    return doc;
}

int BgDocument::indexOf(const QString& prefix) const
{
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].prefix.compare(prefix, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

const BgSection* BgDocument::section(const QString& header) const
{
    for (const BgSection& s : m_sections) {
        if (s.header.compare(header, Qt::CaseInsensitive) == 0)
            return &s;
    }
    return nullptr;
}

} // namespace OFS

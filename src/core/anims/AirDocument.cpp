#include "core/anims/AirDocument.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <cmath>

namespace OFS {

namespace {

QString shortNumber(double value)
{
    QString text = QString::number(value, 'f', 4);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    if (text == QLatin1String("-0"))
        text = QStringLiteral("0");
    return text;
}

// A comment line names an animation when it has a letter or digit.
QString commentName(const QString& line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1Char(';')))
        return {};
    QString name = trimmed;
    while (name.startsWith(QLatin1Char(';')))
        name.remove(0, 1);
    name = name.trimmed();
    for (QChar c : name) {
        if (c.isLetterOrNumber())
            return name;
    }
    return {};
}

bool parseInt(const QString& text, int* value)
{
    bool ok = false;
    *value = text.trimmed().toInt(&ok);
    return ok;
}

} // namespace

AirBox AirBox::normalized() const
{
    AirBox box = *this;
    if (box.left > box.right)
        std::swap(box.left, box.right);
    if (box.top > box.bottom)
        std::swap(box.top, box.bottom);
    return box;
}

QString AirTransparency::text() const
{
    switch (mode) {
    case Mode::None:
        return {};
    case Mode::Subtraction:
        return QStringLiteral("S");
    case Mode::Addition:
    case Mode::Addition50:
        if (source == 256 && dest == 256)
            return QStringLiteral("A");
        if (source == 256 && dest == 128)
            return QStringLiteral("A1");
        return QStringLiteral("AS%1D%2").arg(source).arg(dest);
    }
    return {};
}

AirTransparency AirTransparency::parse(const QString& field)
{
    AirTransparency result;
    const QString text = field.trimmed().toUpper();
    if (text.isEmpty())
        return result;
    if (text == QLatin1String("S")) {
        result.mode = Mode::Subtraction;
    } else if (text == QLatin1String("A1")) {
        result.mode = Mode::Addition50;
        result.dest = 128;
    } else if (text.startsWith(QLatin1Char('A'))) {
        result.mode = Mode::Addition;
        static const QRegularExpression alpha(QStringLiteral("^AS(\\d+)D(\\d+)$"));
        const QRegularExpressionMatch match = alpha.match(text);
        if (match.hasMatch()) {
            result.source = std::clamp(match.captured(1).toInt(), 0, 256);
            result.dest = std::clamp(match.captured(2).toInt(), 0, 256);
        }
    }
    return result;
}

QString AirFrame::text() const
{
    QString result = indent + QStringLiteral("%1,%2, %3,%4, %5").arg(group).arg(number).arg(x).arg(y).arg(time);
    const QString flip = flipH && flipV ? QStringLiteral("HV")
                         : flipH        ? QStringLiteral("H")
                         : flipV        ? QStringLiteral("V")
                                        : QString();
    const QString fields[] = {flip, transparency.text(), shortNumber(xScale) + QLatin1Char(',') + shortNumber(yScale),
                              shortNumber(angle)};
    const int last = angle != 0.0                       ? 4
                     : (xScale != 1.0 || yScale != 1.0) ? 3
                     : !fields[1].isEmpty()             ? 2
                     : !fields[0].isEmpty()             ? 1
                                                        : 0;
    for (int i = 0; i < last; ++i)
        result += QStringLiteral(", ") + fields[i];
    if (!comment.isEmpty())
        result += QLatin1Char(' ') + comment;
    return result;
}

bool AirFrame::hasBoxes(int type) const
{
    for (const AirBoxBlock& block : blocks) {
        if (block.type == type)
            return true;
    }
    return false;
}

QString AirAnimation::caption() const
{
    return name.isEmpty() ? QString::number(number) : QString::number(number) + QStringLiteral(" - ") + name;
}

QList<AirBox> AirAnimation::boxes(int frame, int type, bool* isDefault, bool* inherited) const
{
    if (isDefault)
        *isDefault = false;
    if (inherited)
        *inherited = false;
    if (frame < 0 || frame >= frames.size())
        return {};
    QList<AirBox> regular;
    const AirBoxBlock* ownDefault = nullptr;
    for (const AirBoxBlock& block : frames.at(frame).blocks) {
        if (block.type != type)
            continue;
        if (block.isDefault)
            ownDefault = &block;
        else
            regular += block.boxes;
    }
    if (!regular.isEmpty() || (frames.at(frame).hasBoxes(type) && !ownDefault))
        return regular;
    if (ownDefault) {
        if (isDefault)
            *isDefault = true;
        return ownDefault->boxes;
    }
    for (int f = frame - 1; f >= 0; --f) {
        for (int b = int(frames.at(f).blocks.size()) - 1; b >= 0; --b) {
            const AirBoxBlock& block = frames.at(f).blocks.at(b);
            if (block.type == type && block.isDefault) {
                if (isDefault)
                    *isDefault = true;
                if (inherited)
                    *inherited = true;
                return block.boxes;
            }
        }
    }
    return {};
}

int AirAnimation::endTick(int frame) const
{
    if (frame < 0 || frame >= frames.size() || frames.at(frame).time < 0)
        return -1;
    int tick = 0;
    for (int f = 0; f <= frame; ++f) {
        if (frames.at(f).time < 0)
            return -1;
        tick += frames.at(f).time;
    }
    return tick;
}

int AirAnimation::loopStartFrame() const
{
    for (int f = 0; f < frames.size(); ++f) {
        if (frames.at(f).loopStart)
            return f;
    }
    return 0;
}

AirDocument AirDocument::parse(const QString& text)
{
    return parse(text.split(QLatin1Char('\n')));
}

AirDocument AirDocument::parse(const QStringList& lines)
{
    static const QRegularExpression header(QStringLiteral("^clsn([12])(default)?\\s*:\\s*(-?\\d+)?\\s*$"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression box(QStringLiteral("^clsn([12])\\s*\\[\\s*(-?\\d+)\\s*\\]\\s*=\\s*(-?\\d+)\\s*,\\s*"
                                                       "(-?\\d+)\\s*,\\s*(-?\\d+)\\s*,\\s*(-?\\d+)\\s*$"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression interpolate(QStringLiteral("^interpolate\\s+(offset|blend|scale|angle)$"),
                                                QRegularExpression::CaseInsensitiveOption);
    AirDocument doc;
    AirAnimation* current = nullptr;
    QList<AirBoxBlock> pendingBlocks;
    QList<int> pendingLoop;
    QList<int> pendingInterpolate;
    int pendingFlags = 0;
    int sectionStart = -1;
    auto close = [&](int end) {
        if (!current)
            return;
        current->endLine = end;
        current = nullptr;
    };
    for (int n = 0; n < lines.size(); ++n) {
        QString raw = lines.at(n);
        if (raw.endsWith(QLatin1Char('\r')))
            raw.chop(1);
        const qsizetype commentAt = raw.indexOf(QLatin1Char(';'));
        const QString code = (commentAt >= 0 ? raw.left(commentAt) : raw).trimmed();
        if (code.startsWith(QLatin1Char('['))) {
            // A name comment right above the header belongs to the new animation.
            const bool named = n > 0 && !commentName(lines.at(n - 1)).isEmpty();
            close(named ? n - 1 : n);
            const qsizetype endBracket = code.indexOf(QLatin1Char(']'));
            const QStringList words =
                code.mid(1, endBracket > 0 ? endBracket - 1 : -1).simplified().split(QLatin1Char(' '));
            int number = 0;
            if (words.size() == 3 && words[0].compare(QLatin1String("Begin"), Qt::CaseInsensitive) == 0 &&
                words[1].compare(QLatin1String("Action"), Qt::CaseInsensitive) == 0 && parseInt(words[2], &number)) {
                AirAnimation animation;
                animation.number = number;
                if (named) {
                    animation.name = commentName(lines.at(n - 1));
                    animation.nameLine = n - 1;
                }
                animation.headerLine = n;
                animation.lastLine = n;
                doc.m_animations << animation;
                current = &doc.m_animations.last();
                pendingBlocks.clear();
                pendingLoop.clear();
                pendingInterpolate.clear();
                pendingFlags = 0;
                sectionStart = n + 1;
            }
            continue;
        }
        if (!current || code.isEmpty())
            continue;
        current->lastLine = n;
        QRegularExpressionMatch match = header.match(code);
        if (match.hasMatch()) {
            AirBoxBlock block;
            block.type = match.captured(1).toInt();
            block.isDefault = !match.captured(2).isEmpty();
            block.line = n;
            pendingBlocks << block;
            continue;
        }
        match = box.match(code);
        if (match.hasMatch()) {
            const int type = match.captured(1).toInt();
            if (pendingBlocks.isEmpty() || pendingBlocks.last().type != type) {
                doc.m_problems << Problem{
                    n,
                    QCoreApplication::translate("OFS::AirDocument", "Contact box without its 'Clsn%1' line").arg(type)};
                continue;
            }
            AirBox b;
            b.index = match.captured(2).toInt();
            b.left = match.captured(3).toInt();
            b.top = match.captured(4).toInt();
            b.right = match.captured(5).toInt();
            b.bottom = match.captured(6).toInt();
            b.line = n;
            pendingBlocks.last().boxes << b;
            continue;
        }
        if (code.compare(QLatin1String("loopstart"), Qt::CaseInsensitive) == 0) {
            pendingLoop << n;
            continue;
        }
        match = interpolate.match(code.simplified());
        if (match.hasMatch()) {
            const QString kind = match.captured(1).toLower();
            pendingFlags |= kind == QLatin1String("offset")  ? AirFrame::InterpolateOffset
                            : kind == QLatin1String("blend") ? AirFrame::InterpolateBlend
                            : kind == QLatin1String("scale") ? AirFrame::InterpolateScale
                                                             : AirFrame::InterpolateAngle;
            pendingInterpolate << n;
            continue;
        }
        // An element: "group, number, x, y, time[, flip[, transparency[, xscale, yscale[, angle]]]]".
        const QStringList fields = code.split(QLatin1Char(','));
        AirFrame frame;
        if (fields.size() < 5 || !parseInt(fields[0], &frame.group) || !parseInt(fields[1], &frame.number) ||
            !parseInt(fields[2], &frame.x) || !parseInt(fields[3], &frame.y) || !parseInt(fields[4], &frame.time)) {
            doc.m_problems << Problem{
                n, QCoreApplication::translate("OFS::AirDocument", "Unknown identifier '%1' for animation")
                       .arg(code.section(QRegularExpression(QStringLiteral("[\\s,]")), 0, 0))};
            continue;
        }
        if (fields.size() > 5) {
            const QString flip = fields[5].trimmed().toUpper();
            frame.flipH = flip.contains(QLatin1Char('H'));
            frame.flipV = flip.contains(QLatin1Char('V'));
        }
        if (fields.size() > 6)
            frame.transparency = AirTransparency::parse(fields[6]);
        auto real = [&fields](int i, double fallback) {
            if (i >= fields.size() || fields[i].trimmed().isEmpty())
                return fallback;
            bool ok = false;
            const double value = fields[i].trimmed().toDouble(&ok);
            return ok && std::isfinite(value) ? value : fallback;
        };
        frame.xScale = real(7, 1.0);
        frame.yScale = real(8, 1.0);
        frame.angle = real(9, 0.0);
        for (QChar c : raw) {
            if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
                break;
            frame.indent += c;
        }
        if (commentAt >= 0)
            frame.comment = raw.mid(commentAt).trimmed();
        frame.line = n;
        frame.sectionStart = sectionStart;
        frame.blocks = pendingBlocks;
        frame.loopStart = !pendingLoop.isEmpty();
        frame.loopStartLines = pendingLoop;
        frame.interpolate = pendingFlags;
        frame.interpolateLines = pendingInterpolate;
        pendingBlocks.clear();
        pendingLoop.clear();
        pendingInterpolate.clear();
        pendingFlags = 0;
        sectionStart = n + 1;
        current->frames << frame;
    }
    close(int(lines.size()));
    return doc;
}

int AirDocument::indexOf(int number) const
{
    for (int i = 0; i < count(); ++i) {
        if (m_animations.at(i).number == number)
            return i;
    }
    return -1;
}

bool AirDocument::locate(int line, int* animation, int* frame) const
{
    for (int a = 0; a < count(); ++a) {
        const AirAnimation& anim = m_animations.at(a);
        const int first = anim.nameLine >= 0 ? anim.nameLine : anim.headerLine;
        if (line < first || line >= anim.endLine)
            continue;
        if (animation)
            *animation = a;
        if (frame) {
            *frame = -1;
            for (int f = 0; f < anim.frames.size(); ++f) {
                if (anim.frames.at(f).line == line)
                    *frame = f;
            }
        }
        return true;
    }
    return false;
}

} // namespace OFS

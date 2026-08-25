#include "core/anims/AirEdits.h"

#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <cmath>
#include <map>

namespace OFS::AirEdits {

namespace {

const AirAnimation* animationAt(const AirDocument& doc, int animation)
{
    return animation >= 0 && animation < doc.count() ? &doc.animations().at(animation) : nullptr;
}

const AirFrame* frameAt(const AirDocument& doc, int animation, int frame)
{
    const AirAnimation* anim = animationAt(doc, animation);
    return anim && frame >= 0 && frame < anim->frames.size() ? &anim->frames.at(frame) : nullptr;
}

void removeLines(QStringList& lines, QList<int> numbers)
{
    std::sort(numbers.begin(), numbers.end(), std::greater<>());
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
    for (int n : std::as_const(numbers)) {
        if (n >= 0 && n < lines.size())
            lines.removeAt(n);
    }
}

QList<int> blockLines(const AirFrame& frame)
{
    QList<int> numbers;
    for (const AirBoxBlock& block : frame.blocks) {
        numbers << block.line;
        for (const AirBox& box : block.boxes)
            numbers << box.line;
    }
    return numbers;
}

QStringList blockText(const QStringList& lines, const AirFrame& frame)
{
    QStringList text;
    for (const AirBoxBlock& block : frame.blocks) {
        text << lines.at(block.line);
        for (const AirBox& box : block.boxes)
            text << lines.at(box.line);
    }
    return text;
}

QString leadingBlanks(const QString& line)
{
    QString blanks;
    for (QChar c : line) {
        if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
            break;
        blanks += c;
    }
    return blanks;
}

QString renumbered(const QString& line, int index)
{
    static const QRegularExpression brackets(QStringLiteral("\\[\\s*-?\\d+\\s*\\]"));
    QString result = line;
    const QRegularExpressionMatch match = brackets.match(result);
    if (match.hasMatch())
        result.replace(match.capturedStart(), match.capturedLength(), QStringLiteral("[%1]").arg(index));
    return result;
}

const char* const InterpolationNames[] = {"Offset", "Blend", "Scale", "Angle"};

QString interpolationName(int flag)
{
    switch (flag) {
    case AirFrame::InterpolateOffset:
        return QLatin1String(InterpolationNames[0]);
    case AirFrame::InterpolateBlend:
        return QLatin1String(InterpolationNames[1]);
    case AirFrame::InterpolateScale:
        return QLatin1String(InterpolationNames[2]);
    default:
        return QLatin1String(InterpolationNames[3]);
    }
}

bool isCommentedCode(const QString& line)
{
    QString text = line.trimmed();
    if (!text.startsWith(QLatin1Char(';')))
        return false;
    while (text.startsWith(QLatin1Char(';')))
        text.remove(0, 1);
    text = text.trimmed();
    if (text.isEmpty())
        return false;
    const AirDocument probe = AirDocument::parse(QStringList{QStringLiteral("[Begin Action 0]"), text});
    return probe.problems().isEmpty() &&
           (probe.animations().first().lastLine == 1 || !probe.animations().first().frames.isEmpty());
}

} // namespace

QString boxText(int type, int index, const AirBox& box)
{
    const AirBox b = box.normalized();
    return QStringLiteral("  Clsn%1[%2] = %3, %4, %5, %6")
        .arg(type)
        .arg(index)
        .arg(b.left)
        .arg(b.top)
        .arg(b.right)
        .arg(b.bottom);
}

QString blockHeaderText(int type, bool isDefault, int count)
{
    return QStringLiteral("Clsn%1%2: %3").arg(type).arg(isDefault ? QStringLiteral("Default") : QString()).arg(count);
}

QStringList lines(const QTextDocument* document)
{
    return document->toPlainText().split(QLatin1Char('\n'));
}

bool apply(QTextDocument* document, const QStringList& after)
{
    const QStringList before = lines(document);
    if (before == after)
        return false;
    const int n = int(before.size());
    const int m = int(after.size());
    int k = 0;
    while (k < n && k < m && before.at(k) == after.at(k))
        ++k;
    int j = 0;
    while (j < n - k && j < m - k && before.at(n - 1 - j) == after.at(m - 1 - j))
        ++j;
    // before[k, n - j) becomes after[k, m - j).
    auto offset = [&before](int line) {
        int position = 0;
        for (int i = 0; i < line; ++i)
            position += int(before.at(i).size()) + 1;
        return position;
    };
    const QStringList inserted = after.mid(k, m - j - k);
    const int removed = n - j - k;
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    if (removed > 0 && !inserted.isEmpty()) {
        cursor.setPosition(offset(k));
        cursor.setPosition(offset(n - j) - 1, QTextCursor::KeepAnchor);
        cursor.insertText(inserted.join(QLatin1Char('\n')));
    } else if (removed > 0) {
        // Whole lines go with one line break: the one after them, or before them at the end.
        const bool atEnd = n - j == n;
        const int start = atEnd ? std::max(0, offset(k) - 1) : offset(k);
        const int end = atEnd ? offset(n) - 1 : offset(n - j);
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else if (k < n) {
        cursor.setPosition(offset(k));
        cursor.insertText(inserted.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    } else {
        cursor.setPosition(offset(n) - 1);
        cursor.insertText(QLatin1Char('\n') + inserted.join(QLatin1Char('\n')));
    }
    cursor.endEditBlock();
    return true;
}

void setName(QStringList& lines, int animation, const QString& name)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim)
        return;
    const QString text = name.trimmed();
    if (anim->nameLine >= 0) {
        if (text.isEmpty())
            lines.removeAt(anim->nameLine);
        else
            lines[anim->nameLine] = QStringLiteral("; ") + text;
    } else if (!text.isEmpty()) {
        lines.insert(anim->headerLine, QStringLiteral("; ") + text);
    }
}

void setNumber(QStringList& lines, int animation, int number)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim)
        return;
    const QString old = lines.at(anim->headerLine);
    const qsizetype commentAt = old.indexOf(QLatin1Char(';'));
    lines[anim->headerLine] = leadingBlanks(old) + QStringLiteral("[Begin Action %1]").arg(number) +
                              (commentAt >= 0 ? QLatin1Char(' ') + old.mid(commentAt).trimmed() : QString());
}

void setFrame(QStringList& lines, int animation, int frame, const AirFrame& value)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* current = frameAt(doc, animation, frame);
    if (!current)
        return;
    AirFrame f = value;
    f.indent = current->indent;
    f.comment = current->comment;
    lines[current->line] = f.text();
}

void setLoopStart(QStringList& lines, int animation, int frame, bool on)
{
    AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim || frame < 0 || frame >= anim->frames.size())
        return;
    if (!on) {
        removeLines(lines, anim->frames.at(frame).loopStartLines);
        return;
    }
    QList<int> others;
    for (int f = 0; f < anim->frames.size(); ++f) {
        if (f != frame || anim->frames.at(f).loopStartLines.size() > 1)
            others += anim->frames.at(f).loopStartLines;
    }
    if (anim->frames.at(frame).loopStartLines.size() == 1 && others.isEmpty())
        return;
    removeLines(lines, others);
    doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (f && f->loopStartLines.isEmpty())
        lines.insert(f->line, QStringLiteral("LoopStart"));
}

void setInterpolation(QStringList& lines, int animation, int frame, int flag, bool on)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f)
        return;
    const QString name = interpolationName(flag);
    QList<int> existing;
    for (int line : f->interpolateLines) {
        if (lines.at(line).simplified().section(QLatin1Char(';'), 0, 0).trimmed().endsWith(name, Qt::CaseInsensitive))
            existing << line;
    }
    if (!on) {
        removeLines(lines, existing);
        return;
    }
    if (!existing.isEmpty())
        return;
    int at = f->line;
    for (int line : f->loopStartLines)
        at = std::min(at, line);
    lines.insert(at, QStringLiteral("Interpolate ") + name);
}

void applyToNext(QStringList& lines, int animation, int frame, Field field)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim || frame < 0 || frame >= anim->frames.size())
        return;
    const AirFrame& source = anim->frames.at(frame);
    for (int i = frame + 1; i < anim->frames.size(); ++i) {
        AirFrame f = anim->frames.at(i);
        switch (field) {
        case Field::Offset:
            f.x = source.x;
            f.y = source.y;
            break;
        case Field::Time:
            f.time = source.time;
            break;
        case Field::Flip:
            f.flipH = source.flipH;
            f.flipV = source.flipV;
            break;
        case Field::Transparency:
            f.transparency = source.transparency;
            break;
        case Field::Scale:
            f.xScale = source.xScale;
            f.yScale = source.yScale;
            break;
        }
        const QString text = f.text();
        if (text != lines.at(f.line))
            lines[f.line] = text;
    }
}

int addAnimation(QStringList& lines, int after, int number, const QString& name, const QList<AirFrame>& frames)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, after);
    int at = anim ? anim->lastLine + 1 : int(lines.size());
    // A final line break stays at the end.
    if (at == lines.size() && !lines.isEmpty() && lines.last().isEmpty())
        at = int(lines.size()) - 1;
    QStringList block;
    if (at > 0)
        block << QString();
    if (!name.trimmed().isEmpty())
        block << QStringLiteral("; ") + name.trimmed();
    block << QStringLiteral("[Begin Action %1]").arg(number);
    for (AirFrame f : frames) {
        f.indent.clear();
        f.comment.clear();
        block << f.text();
    }
    for (int i = 0; i < block.size(); ++i)
        lines.insert(at + i, block.at(i));
    return anim ? after + 1 : doc.count();
}

void deleteAnimation(QStringList& lines, int animation)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim)
        return;
    const int first = anim->nameLine >= 0 ? anim->nameLine : anim->headerLine;
    int last = anim->lastLine;
    if (last + 1 < lines.size() && lines.at(last + 1).trimmed().isEmpty())
        ++last;
    lines.erase(lines.begin() + first, lines.begin() + last + 1);
}

int duplicateAnimation(QStringList& lines, int animation, int number)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim)
        return -1;
    QStringList block;
    if (!anim->name.isEmpty())
        block << QStringLiteral("; ") + anim->name;
    block << QStringLiteral("[Begin Action %1]").arg(number);
    block += lines.mid(anim->headerLine + 1, anim->lastLine - anim->headerLine);
    int at = int(lines.size());
    if (const AirAnimation* next = animationAt(doc, animation + 1)) {
        at = next->nameLine >= 0 ? next->nameLine : next->headerLine;
        block << QString();
    } else {
        if (!lines.isEmpty() && lines.last().isEmpty())
            at = int(lines.size()) - 1;
        block.prepend(QString());
    }
    for (int i = 0; i < block.size(); ++i)
        lines.insert(at + i, block.at(i));
    return animation + 1;
}

QStringList animationText(const QStringList& lines, int animation)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim)
        return {};
    const int first = anim->nameLine >= 0 ? anim->nameLine : anim->headerLine;
    return lines.mid(first, anim->lastLine - first + 1);
}

void replaceAnimation(QStringList& lines, int animation, const QStringList& text)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim)
        return;
    const int first = anim->nameLine >= 0 ? anim->nameLine : anim->headerLine;
    lines.erase(lines.begin() + first, lines.begin() + anim->lastLine + 1);
    for (int i = 0; i < text.size(); ++i)
        lines.insert(first + i, text.at(i));
}

AirFrame blankFrame(int time)
{
    AirFrame frame;
    frame.group = -1;
    frame.number = 0;
    frame.time = time;
    return frame;
}

int insertFrames(QStringList& lines, int animation, int after, const QList<AirFrame>& frames)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim || frames.isEmpty())
        return -1;
    after = std::clamp(after, -1, int(anim->frames.size()) - 1);
    const int at = after >= 0 ? anim->frames.at(after).line + 1 : anim->headerLine + 1;
    for (int i = 0; i < frames.size(); ++i) {
        AirFrame f = frames.at(i);
        f.indent.clear();
        f.comment.clear();
        lines.insert(at + i, f.text());
    }
    return after + int(frames.size());
}

void deleteFrame(QStringList& lines, int animation, int frame)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f)
        return;
    QList<int> numbers = blockLines(*f);
    numbers << f->line;
    numbers += f->loopStartLines;
    numbers += f->interpolateLines;
    removeLines(lines, numbers);
}

BoxRef addBox(QStringList& lines, int animation, int frame, int type, const AirBox& box)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f)
        return {};
    int target = -1;
    for (int b = 0; b < f->blocks.size() && target < 0; ++b) {
        if (f->blocks.at(b).type == type && !f->blocks.at(b).isDefault)
            target = b;
    }
    for (int b = 0; b < f->blocks.size() && target < 0; ++b) {
        if (f->blocks.at(b).type == type)
            target = b;
    }
    if (target < 0) {
        lines.insert(f->sectionStart, blockHeaderText(type, false, 1));
        lines.insert(f->sectionStart + 1, boxText(type, 0, box));
        return {0, 0};
    }
    const AirBoxBlock& block = f->blocks.at(target);
    int index = 0;
    for (const AirBox& b : block.boxes)
        index = std::max(index, b.index + 1);
    const int at = block.boxes.isEmpty() ? block.line + 1 : block.boxes.last().line + 1;
    lines.insert(at, boxText(type, index, box));
    lines[block.line] = blockHeaderText(type, block.isDefault, int(block.boxes.size()) + 1);
    return {target, int(block.boxes.size())};
}

void setBox(QStringList& lines, int animation, int frame, BoxRef ref, const AirBox& box)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f || ref.block < 0 || ref.block >= f->blocks.size() || ref.box < 0 ||
        ref.box >= f->blocks.at(ref.block).boxes.size())
        return;
    const AirBoxBlock& block = f->blocks.at(ref.block);
    const AirBox& old = block.boxes.at(ref.box);
    lines[old.line] = boxText(block.type, old.index, box);
}

void deleteBoxes(QStringList& lines, int animation, int frame, const QList<BoxRef>& refs)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f)
        return;
    QList<int> removed;
    for (int b = 0; b < f->blocks.size(); ++b) {
        const AirBoxBlock& block = f->blocks.at(b);
        QList<AirBox> kept;
        for (int i = 0; i < block.boxes.size(); ++i) {
            if (refs.contains(BoxRef{b, i}))
                removed << block.boxes.at(i).line;
            else
                kept << block.boxes.at(i);
        }
        if (kept.size() == block.boxes.size())
            continue;
        if (kept.isEmpty()) {
            removed << block.line;
            continue;
        }
        for (int i = 0; i < kept.size(); ++i) {
            if (kept.at(i).index != i)
                lines[kept.at(i).line] = renumbered(lines.at(kept.at(i).line), i);
        }
        lines[block.line] = blockHeaderText(block.type, block.isDefault, int(kept.size()));
    }
    removeLines(lines, removed);
}

void deleteFrameBoxes(QStringList& lines, int animation, int frame)
{
    const AirDocument doc = AirDocument::parse(lines);
    if (const AirFrame* f = frameAt(doc, animation, frame))
        removeLines(lines, blockLines(*f));
}

void deleteAnimationBoxes(QStringList& lines, int animation)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim)
        return;
    QList<int> numbers;
    for (const AirFrame& f : anim->frames)
        numbers += blockLines(f);
    removeLines(lines, numbers);
}

BoxRef switchBoxType(QStringList& lines, int animation, int frame, BoxRef ref)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f || ref.block < 0 || ref.block >= f->blocks.size() || ref.box < 0 ||
        ref.box >= f->blocks.at(ref.block).boxes.size())
        return {};
    const int type = f->blocks.at(ref.block).type == 1 ? 2 : 1;
    const AirBox box = f->blocks.at(ref.block).boxes.at(ref.box);
    deleteBoxes(lines, animation, frame, {ref});
    return addBox(lines, animation, frame, type, box);
}

void switchBoxDefault(QStringList& lines, int animation, int frame, BoxRef ref)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f || ref.block < 0 || ref.block >= f->blocks.size())
        return;
    const AirBoxBlock& block = f->blocks.at(ref.block);
    lines[block.line] = blockHeaderText(block.type, !block.isDefault, int(block.boxes.size()));
}

void copyPreviousBoxes(QStringList& lines, int animation, int frame)
{
    AirDocument doc = AirDocument::parse(lines);
    const AirAnimation* anim = animationAt(doc, animation);
    if (!anim || frame <= 0 || frame >= anim->frames.size())
        return;
    const AirFrame& previous = anim->frames.at(frame - 1);
    QStringList text = blockText(lines, previous);
    if (previous.blocks.isEmpty()) {
        for (int type : {1, 2}) {
            const QList<AirBox> boxes = anim->boxes(frame - 1, type);
            if (boxes.isEmpty())
                continue;
            text << blockHeaderText(type, false, int(boxes.size()));
            for (int i = 0; i < boxes.size(); ++i)
                text << boxText(type, i, boxes.at(i));
        }
    }
    removeLines(lines, blockLines(anim->frames.at(frame)));
    doc = AirDocument::parse(lines);
    const AirFrame* f = frameAt(doc, animation, frame);
    if (!f)
        return;
    for (int i = 0; i < text.size(); ++i)
        lines.insert(f->sectionStart + i, text.at(i));
}

int replicateBoxes(QStringList& lines, int animation, int frame)
{
    const AirDocument doc = AirDocument::parse(lines);
    const AirFrame* source = frameAt(doc, animation, frame);
    if (!source)
        return 0;
    const QStringList text = blockText(lines, *source);
    // Bottom to top, so the lines above each change keep their numbers.
    QList<const AirFrame*> targets;
    for (int a = 0; a < doc.count(); ++a) {
        for (int f = 0; f < doc.animations().at(a).frames.size(); ++f) {
            const AirFrame& candidate = doc.animations().at(a).frames.at(f);
            if ((a != animation || f != frame) && candidate.group == source->group &&
                candidate.number == source->number)
                targets << &candidate;
        }
    }
    std::sort(targets.begin(), targets.end(),
              [](const AirFrame* x, const AirFrame* y) { return x->sectionStart > y->sectionStart; });
    for (const AirFrame* target : std::as_const(targets)) {
        removeLines(lines, blockLines(*target));
        for (int i = 0; i < text.size(); ++i)
            lines.insert(target->sectionStart + i, text.at(i));
    }
    return int(targets.size());
}

void pasteBoxes(QStringList& lines, int animation, int frame, int type, const QList<AirBox>& boxes)
{
    for (const AirBox& box : boxes)
        addBox(lines, animation, frame, type, box);
}

void align(QStringList& lines, const QList<int>& animations, AlignOperation operation, int x, int y)
{
    const AirDocument doc = AirDocument::parse(lines);
    for (int a : animations) {
        const AirAnimation* anim = animationAt(doc, a);
        if (!anim)
            continue;
        for (const AirFrame& frame : anim->frames) {
            if (operation == AlignOperation::OffsetAxis) {
                AirFrame f = frame;
                f.x += x;
                f.y += y;
                lines[f.line] = f.text();
                continue;
            }
            for (const AirBoxBlock& block : frame.blocks) {
                for (const AirBox& old : block.boxes) {
                    AirBox b = old.normalized();
                    switch (operation) {
                    case AlignOperation::OffsetBoxes:
                        b.left += x;
                        b.right += x;
                        b.top += y;
                        b.bottom += y;
                        break;
                    case AlignOperation::ResizeTopLeft:
                        b.left -= x;
                        b.top -= y;
                        break;
                    case AlignOperation::ResizeBottomRight:
                        b.right += x;
                        b.bottom += y;
                        break;
                    case AlignOperation::ScaleBoxes: {
                        const int width = std::max(1, int(std::lround((b.right - b.left + 1) * x / 100.0)));
                        const int height = std::max(1, int(std::lround((b.bottom - b.top + 1) * y / 100.0)));
                        b.left = int(std::lround(b.left * x / 100.0));
                        b.top = int(std::lround(b.top * y / 100.0));
                        b.right = b.left + width - 1;
                        b.bottom = b.top + height - 1;
                        break;
                    }
                    case AlignOperation::OffsetAxis:
                        break;
                    }
                    lines[old.line] = boxText(block.type, old.index, b);
                }
            }
        }
    }
}

int optimize(QStringList& lines)
{
    const qsizetype before = lines.size();
    // 1. Commented-out animations: the header, the name above it and their commented lines.
    static const QRegularExpression commented(QStringLiteral("^\\s*;+\\s*\\[\\s*begin\\s+action\\s+-?\\d+\\s*\\]"),
                                              QRegularExpression::CaseInsensitiveOption);
    for (int i = int(lines.size()) - 1; i >= 0; --i) {
        if (!commented.match(lines.at(i)).hasMatch())
            continue;
        int first = i;
        if (i > 0 && lines.at(i - 1).trimmed().startsWith(QLatin1Char(';')) && !isCommentedCode(lines.at(i - 1)) &&
            !commented.match(lines.at(i - 1)).hasMatch())
            first = i - 1;
        int last = i;
        while (last + 1 < lines.size() && isCommentedCode(lines.at(last + 1)))
            ++last;
        lines.erase(lines.begin() + first, lines.begin() + last + 1);
        if (first > 0 && first < lines.size() && lines.at(first - 1).trimmed().isEmpty() &&
            lines.at(first).trimmed().isEmpty())
            lines.removeAt(first);
        i = first;
    }
    // 2. Runs of identical regular blocks of one type -> one default block, where safe.
    const AirDocument doc = AirDocument::parse(lines);
    std::map<int, QString> headers;
    QList<int> removed;
    for (const AirAnimation& anim : doc.animations()) {
        const int count = int(anim.frames.size());
        for (int type : {1, 2}) {
            auto single = [&anim, type](int f) -> const AirBoxBlock* {
                const AirBoxBlock* found = nullptr;
                for (const AirBoxBlock& block : anim.frames.at(f).blocks) {
                    if (block.type != type)
                        continue;
                    if (found || block.isDefault)
                        return nullptr;
                    found = &block;
                }
                return found;
            };
            // Safe when each later frame has its own boxes of this type up to the next own default.
            auto safeAfter = [&anim, count, type](int last) {
                for (int k = last + 1; k < count; ++k) {
                    bool own = false;
                    bool ownDefault = false;
                    for (const AirBoxBlock& block : anim.frames.at(k).blocks) {
                        if (block.type == type) {
                            own = true;
                            ownDefault = ownDefault || block.isDefault;
                        }
                    }
                    if (ownDefault)
                        return true;
                    if (!own)
                        return false;
                }
                return true;
            };
            int f = 0;
            while (f < count) {
                const AirBoxBlock* first = single(f);
                if (!first || first->boxes.isEmpty()) {
                    ++f;
                    continue;
                }
                int g = f;
                while (g + 1 < count) {
                    const AirBoxBlock* next = single(g + 1);
                    if (!next || next->boxes != first->boxes)
                        break;
                    ++g;
                }
                if (g > f && safeAfter(g)) {
                    headers[first->line] = blockHeaderText(type, true, int(first->boxes.size()));
                    for (int k = f + 1; k <= g; ++k) {
                        const AirBoxBlock* block = single(k);
                        removed << block->line;
                        for (const AirBox& box : block->boxes)
                            removed << box.line;
                    }
                }
                f = g + 1;
            }
        }
    }
    for (const auto& [line, text] : headers)
        lines[line] = text;
    removeLines(lines, removed);
    return int(before - lines.size());
}

} // namespace OFS::AirEdits

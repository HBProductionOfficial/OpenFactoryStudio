#include "core/editors/Organize.h"

#include <algorithm>

namespace OFS::Organize {

int deltaOf(const QString& actionId, int count)
{
    if (actionId == QLatin1String("org_up"))
        return -1;
    if (actionId == QLatin1String("org_down"))
        return 1;
    if (actionId == QLatin1String("org_up_10"))
        return -10;
    if (actionId == QLatin1String("org_down_10"))
        return 10;
    if (actionId == QLatin1String("org_top"))
        return -count;
    if (actionId == QLatin1String("org_bottom"))
        return count;
    return 0;
}

QList<int> moveOrder(int count, QList<int> rows, int delta, QList<int>* moved)
{
    QList<int> order;
    for (int i = 0; i < count; ++i)
        order << i;
    rows.removeIf([count](int r) { return r < 0 || r >= count; });
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    QList<int> positions;
    if (delta < 0) {
        int limit = 0;
        for (int r : std::as_const(rows)) {
            const int target = std::max(r + delta, limit);
            order.move(r, target);
            positions << target;
            limit = target + 1;
        }
    } else if (delta > 0) {
        int limit = count - 1;
        for (auto it = rows.crbegin(); it != rows.crend(); ++it) {
            const int target = std::min(*it + delta, limit);
            order.move(*it, target);
            positions.prepend(target);
            limit = target - 1;
        }
    } else {
        positions = rows;
    }
    if (moved)
        *moved = positions;
    return order;
}

namespace {

bool isHeader(const QString& line)
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith(QLatin1Char('[')) && trimmed.contains(QLatin1Char(']'));
}

bool isComment(const QString& line)
{
    return line.trimmed().startsWith(QLatin1Char(';'));
}

// A section starts at its header or at the comment lines right above it.
int sectionStart(const QStringList& lines, int header, int floor)
{
    int start = header;
    while (start - 1 >= floor && isComment(lines.at(start - 1)))
        --start;
    return start;
}

} // namespace

QList<Block> sectionBlocks(const QStringList& lines, const QRegularExpression& header)
{
    QList<int> headers;
    for (int i = 0; i < lines.size(); ++i) {
        if (isHeader(lines.at(i)))
            headers << i;
    }
    QList<int> starts;
    int floor = 0;
    for (int h : std::as_const(headers)) {
        starts << sectionStart(lines, h, floor);
        floor = h + 1;
    }
    QList<Block> blocks;
    for (int k = 0; k < headers.size(); ++k) {
        if (!header.match(lines.at(headers[k])).hasMatch())
            continue;
        Block block;
        block.start = starts[k];
        block.end = k + 1 < headers.size() ? starts[k + 1] : int(lines.size());
        block.contentEnd = block.end;
        while (block.contentEnd > headers[k] + 1 && lines.at(block.contentEnd - 1).trimmed().isEmpty())
            --block.contentEnd;
        blocks << block;
    }
    return blocks;
}

void reorderBlocks(QStringList& lines, const QList<Block>& blocks, const QList<int>& order)
{
    if (blocks.size() != order.size())
        return;
    QStringList result;
    int position = 0;
    for (int k = 0; k < blocks.size(); ++k) {
        const Block& slot = blocks[k];
        const Block& source = blocks[order[k]];
        result << lines.mid(position, slot.start - position);
        result << lines.mid(source.start, source.contentEnd - source.start);
        result << lines.mid(slot.contentEnd, slot.end - slot.contentEnd); // the slot's blank lines
        position = slot.end;
    }
    result << lines.mid(position);
    lines = result;
}

} // namespace OFS::Organize

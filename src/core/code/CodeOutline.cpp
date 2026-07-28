#include "core/code/CodeOutline.h"

#include "core/code/CodeLine.h"

#include <QStringList>

namespace OFS {

Outline sectionOutline(const QString& text)
{
    Outline outline;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int n = 0; n < lines.size(); ++n) {
        const CodeLine line = CodeLine::parse(lines[n]);
        if (line.kind == CodeLine::Kind::Section) {
            OutlineNode node;
            node.text = line.header;
            node.line = n;
            outline << node;
        } else if (line.kind == CodeLine::Kind::KeyValue && !outline.isEmpty()) {
            OutlineNode child;
            child.text = line.key + QStringLiteral(" = ") + line.value;
            child.line = n;
            outline.last().children << child;
        }
    }
    return outline;
}

} // namespace OFS

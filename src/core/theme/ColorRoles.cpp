#include "core/theme/ColorRoles.h"

#include "core/theme/ColorSet.h"

#include <QCoreApplication>

namespace OFS {

QString ColorRole::displayName() const
{
    return QCoreApplication::translate("OFS::ColorRole", caption);
}

const QList<ColorRole>& ColorRole::all()
{
    // Fallback colors are those of the Dark color set.
    static const QList<ColorRole> roles = {
        {QStringLiteral("background"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Text background"), false,
         QColor(34, 34, 34)},
        {QStringLiteral("foreground"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Text foreground"), true,
         QColor(204, 204, 204)},
        {QStringLiteral("comment"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Comments"), true, QColor(128, 128, 128)},
        {QStringLiteral("block"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Block header"), true, QColor(0, 255, 0)},
        {QStringLiteral("number"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Number"), true, QColor(255, 0, 0)},
        {QStringLiteral("string"), QT_TRANSLATE_NOOP("OFS::ColorRole", "String"), true, QColor(0, 255, 255)},
        {QStringLiteral("keyword"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Keyword"), true, QColor(255, 255, 255)},
        {QStringLiteral("keyword_value"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Value keyword"), true,
         QColor(255, 0, 255)},
        {QStringLiteral("block_type"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Block type"), true, QColor(255, 85, 0)},
        {QStringLiteral("function"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Function"), true, QColor(255, 255, 0)},
        {QStringLiteral("const"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Constant"), true, QColor(255, 0, 128)},
        {QStringLiteral("paren"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Matched parenthesis"), true, QColor(255, 0, 0)},
        {QStringLiteral("paren_range"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Matched parenthesis range"), false,
         QColor(25, 25, 25)},
        {QStringLiteral("block_range"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Block range"), false, QColor(25, 25, 25)},
        {QStringLiteral("line"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Line highlight"), false, QColor(10, 10, 10)},
        {QStringLiteral("back_start"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Background start (graphic workspace)"),
         false, QColor(85, 85, 85)},
        {QStringLiteral("back_end"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Background end (graphic workspace)"), false,
         QColor(51, 51, 51)},
        {QStringLiteral("attackbox"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Attack contact box"), false,
         QColor(255, 0, 0)},
        {QStringLiteral("collisionbox"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Collision contact box"), false,
         QColor(0, 0, 255)},
        {QStringLiteral("sound_wave"), QT_TRANSLATE_NOOP("OFS::ColorRole", "Waveform"), false, QColor(185, 175, 55)},
    };
    return roles;
}

const ColorRole* ColorRole::find(const QString& key)
{
    for (const ColorRole& role : all()) {
        if (role.key == key)
            return &role;
    }
    return nullptr;
}

ColorSet ColorRole::complete(const ColorSet& set)
{
    ColorSet result = set;
    for (const ColorRole& role : all()) {
        if (!result.contains(role.key)) {
            ColorSet::Entry entry;
            entry.color = role.fallback;
            result.set(role.key, entry);
        }
    }
    return result;
}

} // namespace OFS

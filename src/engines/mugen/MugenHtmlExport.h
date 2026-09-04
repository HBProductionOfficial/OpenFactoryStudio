#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QPair>
#include <QSize>
#include <QString>
#include <QStringList>

namespace OFS {
class Project;
}

namespace OFS::Mugen {

// Tools > Export to HTML: a page made from a layout folder whose "!ff <key>!" placeholders are filled
// in. Layouts live in <data>/layouts/<kind>/<name>/.

struct HtmlCommand
{
    QString name;
    QString command;
    bool aiOnly = false; // left unticked at first: named for the AI ("AI_jump", "cpu1")
};

QList<HtmlCommand> htmlCommands(const QString& text);
QString commandHtml(const QString& command);

// A later folder's layout replaces an earlier one of the same name.
QList<QPair<QString, QString>> htmlLayouts(const QStringList& roots, const QString& kind);
QString htmlLayoutKind(const QString& projectType);

struct HtmlExportOptions
{
    QString layout;
    QString email;
    QString homepage;
    QString title;
    QString text;
    bool horizontalLines = false;
    bool boldNames = false;
    QStringList commands;
    // Rendered previews (stages).
    int frameStart = 0;
    int duration = 5; // seconds
    int frameRate = 60;
    bool sharedPalette = true;
    QSize gifSize{320, 240};
    QSize staticSize{320, 240};
    QColor background = Qt::black;
};

bool exportHtml(Project& project, const HtmlExportOptions& options, const QString& htmlPath, QString* error = nullptr);

// For layouts that bring no images of their own ("a", "x", "f", "b2", "df"...).
QImage commandGlyph(const QString& key);

} // namespace OFS::Mugen

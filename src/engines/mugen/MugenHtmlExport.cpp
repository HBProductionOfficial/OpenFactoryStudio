#include "engines/mugen/MugenHtmlExport.h"

#include "core/GameTime.h"
#include "core/anims/AirDocument.h"
#include "core/anims/AirGif.h"
#include "core/bg/BgDocument.h"
#include "core/bg/BgRenderer.h"
#include "core/bg/BgSimulator.h"
#include "core/editors/EditorInfo.h"
#include "core/palettes/PaletteOps.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteExport.h"
#include "core/sprites/SpriteFile.h"
#include "engines/mugen/DefFile.h"
#include "engines/mugen/MugenTypes.h"
#include "formats/codecs/Gif.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextDocument>

#include <cmath>
#include <cstring>
#include <tuple>

namespace OFS::Mugen {

namespace {

struct HtmlExportTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::HtmlExportTexts)
};

bool fail(QString* error, const QString& message)
{
    if (error)
        *error = message;
    return false;
}

QString projectText(Project& project, const QString& path)
{
    if (path.isEmpty())
        return {};
    if (auto* code = qobject_cast<CodeFile*>(project.file(path)))
        return code->document()->toPlainText();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QString encoding;
    QByteArray bom;
    return CodeFile::decode(file.readAll(), &encoding, &bom).replace(QLatin1String("\r\n"), QLatin1String("\n"));
}

const SpriteDocument* projectSprites(Project& project)
{
    SpriteFile* sprites = project.mainSpriteFile();
    return sprites ? &sprites->document() : nullptr;
}

QString escape(const QString& text)
{
    return text.toHtmlEscaped();
}

QString imageTag(const QString& file, const QSize& size = {})
{
    if (size.isValid())
        return QStringLiteral("<img src=\"images/%1\" width=\"%2\" height=\"%3\"/>")
            .arg(file)
            .arg(size.width())
            .arg(size.height());
    return QStringLiteral("<img src=\"images/%1\"/>").arg(file);
}

bool writeBytes(const QString& path, const QByteArray& bytes, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error,
                    HtmlExportTexts::tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
    return true;
}

// The palettes a character's previews are shown in: group 1, numbers 1 to 12.
QList<int> characterPalettes(const SpriteDocument& sprites)
{
    QList<int> palettes;
    for (int number = 1; number <= 12; ++number) {
        const int index = sprites.findPalette(1, number);
        if (index >= 0)
            palettes << index;
    }
    return palettes;
}

SpriteDocument withPalette(const SpriteDocument& sprites, int paletteIndex)
{
    SpriteDocument copy = sprites;
    const int first = sprites.findPalette(1, 1);
    if (first < 0 || paletteIndex < 0 || paletteIndex >= int(copy.palettes().size()))
        return copy;
    const SpritePalettePtr from = sprites.palettes()[first];
    const SpritePalettePtr to = copy.palettes()[paletteIndex];
    for (SpriteEntry& entry : copy.sprites()) {
        if (entry.palette == from)
            entry.palette = to;
    }
    return copy;
}

Formats::Image toFormatsImage(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_RGBA8888);
    std::vector<std::uint8_t> data(std::size_t(image.width()) * std::size_t(image.height()) * 4);
    for (int y = 0; y < image.height(); ++y)
        std::memcpy(data.data() + std::size_t(y) * std::size_t(image.width()) * 4, image.constScanLine(y),
                    std::size_t(image.width()) * 4);
    return Formats::Image::rgba(image.width(), image.height(), std::move(data));
}

QImage renderStage(const BgDocument& doc, const SpriteDocument* sprites, const AirDocument& animations,
                   const QSizeF& screen, int tick, const QColor& background)
{
    const int group = doc.indexOf(QStringLiteral("BG"));
    QImage image(screen.toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    if (group < 0)
        return image;
    BgSimulator simulator(doc.groups()[group]);
    simulator.runTo(tick);
    BgRenderer renderer(sprites, &animations);
    BgView view;
    view.screen = screen;
    QPainter painter(&image);
    for (const BgDrawItem& item : renderer.render(simulator, view)) {
        painter.save();
        if (item.clip)
            painter.setClipRect(*item.clip);
        painter.setOpacity(item.opacity);
        painter.setCompositionMode(item.mode);
        painter.drawImage(item.topLeft, item.image);
        painter.restore();
    }
    return image;
}

QByteArray framesGif(const QList<QImage>& frames, const QList<int>& delays, bool sharedPalette, QString* error)
{
    Formats::GifDocument gif;
    if (frames.isEmpty())
        return {};
    gif.width = frames.first().width();
    gif.height = frames.first().height();
    gif.loopCount = frames.size() > 1 ? 0 : -1;
    QList<Formats::Image> images;
    for (const QImage& frame : frames)
        images << toFormatsImage(frame);
    Formats::Palette shared;
    if (sharedPalette)
        shared = PaletteOps::paletteFromImages(images);
    for (int i = 0; i < images.size(); ++i) {
        const Formats::Palette palette = sharedPalette ? shared : PaletteOps::paletteFromImages({images[i]});
        Formats::GifFrame frame;
        frame.image = PaletteOps::adapt(images[i], palette);
        frame.image.setPalette(palette);
        frame.delay = delays.value(i);
        frame.localPalette = !sharedPalette;
        gif.frames.push_back(std::move(frame));
    }
    if (sharedPalette)
        gif.globalPalette = shared;
    const auto bytes = Formats::writeGif(gif);
    if (!bytes) {
        fail(error, QString::fromStdString(bytes.error().message));
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()));
}

const QStringList& glyphKeys()
{
    static const QStringList keys = {
        QStringLiteral("a"), QStringLiteral("b"),  QStringLiteral("c"),  QStringLiteral("x"),  QStringLiteral("y"),
        QStringLiteral("z"), QStringLiteral("s"),  QStringLiteral("f"),  QStringLiteral("b2"), QStringLiteral("d"),
        QStringLiteral("u"), QStringLiteral("df"), QStringLiteral("db"), QStringLiteral("uf"), QStringLiteral("ub")};
    return keys;
}

} // namespace

QList<HtmlCommand> htmlCommands(const QString& text)
{
    static const QRegularExpression section(QStringLiteral("^\\s*\\[\\s*([^\\]]*?)\\s*\\]"));
    static const QRegularExpression key(QStringLiteral("^\\s*(name|command)\\s*=\\s*([^;]*)"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ai(QStringLiteral("(^|[_\\s])(ai|cpu)([_\\s\\d]|$)"),
                                       QRegularExpression::CaseInsensitiveOption);
    QList<HtmlCommand> commands;
    bool inCommand = false;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (const QRegularExpressionMatch s = section.match(line); s.hasMatch()) {
            inCommand = s.captured(1).compare(QLatin1String("command"), Qt::CaseInsensitive) == 0;
            if (inCommand)
                commands << HtmlCommand();
            continue;
        }
        if (!inCommand)
            continue;
        const QRegularExpressionMatch m = key.match(line);
        if (!m.hasMatch())
            continue;
        const QString value = m.captured(2).trimmed();
        if (m.captured(1).compare(QLatin1String("name"), Qt::CaseInsensitive) == 0) {
            commands.last().name = QString(value).remove(QLatin1Char('"'));
            commands.last().aiOnly = ai.match(commands.last().name).hasMatch();
        } else {
            commands.last().command = value;
        }
    }
    return commands;
}

QString commandHtml(const QString& command)
{
    static const QHash<QString, QString> images = {
        {QStringLiteral("F"), QStringLiteral("f")},   {QStringLiteral("B"), QStringLiteral("b2")},
        {QStringLiteral("D"), QStringLiteral("d")},   {QStringLiteral("U"), QStringLiteral("u")},
        {QStringLiteral("DF"), QStringLiteral("df")}, {QStringLiteral("DB"), QStringLiteral("db")},
        {QStringLiteral("UF"), QStringLiteral("uf")}, {QStringLiteral("UB"), QStringLiteral("ub")},
        {QStringLiteral("a"), QStringLiteral("a")},   {QStringLiteral("b"), QStringLiteral("b")},
        {QStringLiteral("c"), QStringLiteral("c")},   {QStringLiteral("x"), QStringLiteral("x")},
        {QStringLiteral("y"), QStringLiteral("y")},   {QStringLiteral("z"), QStringLiteral("z")},
        {QStringLiteral("s"), QStringLiteral("s")},
    };
    QStringList parts;
    for (const QString& rawPart : command.split(QLatin1Char(','))) {
        QString part = rawPart.trimmed();
        // Modifiers first: release (with its hold time), hold, any direction near, nothing between.
        QString prefix;
        static const QRegularExpression modifiers(QStringLiteral("^([~/$>]\\d*)+"));
        if (const QRegularExpressionMatch m = modifiers.match(part); m.hasMatch()) {
            prefix = m.captured(0);
            part = part.mid(prefix.size());
        }
        QStringList symbols;
        for (const QString& symbol : part.split(QLatin1Char('+'))) {
            const QString trimmed = symbol.trimmed();
            const auto found = images.constFind(trimmed);
            symbols << (found != images.cend()
                            ? QStringLiteral("<img src=\"images/%1.png\" alt=\"%2\"/>").arg(*found, escape(trimmed))
                            : escape(trimmed));
        }
        parts << escape(prefix) + symbols.join(QLatin1Char('+'));
    }
    return parts.join(QStringLiteral(", "));
}

QList<QPair<QString, QString>> htmlLayouts(const QStringList& roots, const QString& kind)
{
    QMap<QString, QString> layouts;
    for (const QString& root : roots) {
        const QDir dir(root + QStringLiteral("/layouts/") + kind);
        for (const QString& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (!QDir(dir.filePath(name)).entryList({QStringLiteral("*.html"), QStringLiteral("*.htm")}).isEmpty())
                layouts.insert(name, dir.filePath(name));
        }
    }
    QList<QPair<QString, QString>> list;
    for (auto it = layouts.cbegin(); it != layouts.cend(); ++it)
        list << qMakePair(it.key(), it.value());
    return list;
}

QString htmlLayoutKind(const QString& projectType)
{
    if (projectType == MugenType::Character)
        return QStringLiteral("char");
    if (projectType == MugenType::Stage)
        return QStringLiteral("stage");
    if (projectType == MugenType::Storyboard)
        return QStringLiteral("scene");
    return {};
}

QImage commandGlyph(const QString& key)
{
    QImage image(24, 24, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    static const QHash<QString, int> angles = {
        {QStringLiteral("f"), 0},    {QStringLiteral("df"), 45},  {QStringLiteral("d"), 90},
        {QStringLiteral("db"), 135}, {QStringLiteral("b2"), 180}, {QStringLiteral("ub"), 225},
        {QStringLiteral("u"), 270},  {QStringLiteral("uf"), 315},
    };
    if (const auto angle = angles.constFind(key); angle != angles.cend()) {
        p.setPen(QPen(QColor(90, 100, 110), 1.2));
        p.setBrush(QColor(236, 240, 243));
        p.drawEllipse(QRectF(1.5, 1.5, 21, 21));
        p.translate(12, 12);
        p.rotate(*angle);
        QPainterPath arrow;
        arrow.moveTo(8, 0);
        arrow.lineTo(1, -6);
        arrow.lineTo(1, -2.5);
        arrow.lineTo(-7, -2.5);
        arrow.lineTo(-7, 2.5);
        arrow.lineTo(1, 2.5);
        arrow.lineTo(1, 6);
        arrow.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 48, 56));
        p.drawPath(arrow);
        return image;
    }
    // Kicks a, b, c. Punches x, y, z. Start s.
    static const QHash<QString, QColor> colors = {
        {QStringLiteral("a"), QColor(52, 120, 200)},  {QStringLiteral("b"), QColor(46, 160, 90)},
        {QStringLiteral("c"), QColor(210, 70, 60)},   {QStringLiteral("x"), QColor(110, 150, 220)},
        {QStringLiteral("y"), QColor(110, 190, 120)}, {QStringLiteral("z"), QColor(230, 120, 100)},
        {QStringLiteral("s"), QColor(120, 120, 130)},
    };
    p.setPen(Qt::NoPen);
    p.setBrush(colors.value(key, QColor(120, 120, 130)));
    p.drawEllipse(QRectF(1, 1, 22, 22));
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(14);
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(QRectF(0, 0, 24, 23), Qt::AlignCenter, key.toUpper());
    return image;
}

bool exportHtml(Project& project, const HtmlExportOptions& options, const QString& htmlPath, QString* error)
{
    const QDir layout(options.layout);
    QStringList pages = layout.entryList({QStringLiteral("layout.html")}, QDir::Files);
    if (pages.isEmpty())
        pages = layout.entryList({QStringLiteral("*.html"), QStringLiteral("*.htm")}, QDir::Files, QDir::Name);
    if (pages.isEmpty())
        return fail(error,
                    HtmlExportTexts::tr("The layout %1 has no page.").arg(QDir::toNativeSeparators(options.layout)));
    QFile templateFile(layout.filePath(pages.first()));
    if (!templateFile.open(QIODevice::ReadOnly))
        return fail(error, templateFile.errorString());
    QString encoding;
    QByteArray bom;
    QString page =
        CodeFile::decode(templateFile.readAll(), &encoding, &bom).replace(QLatin1String("\r\n"), QLatin1String("\n"));

    const QFileInfo target(htmlPath);
    const QDir images(target.absolutePath() + QStringLiteral("/images"));
    if (!QDir().mkpath(images.path()))
        return fail(error,
                    HtmlExportTexts::tr("Cannot create the folder %1.").arg(QDir::toNativeSeparators(images.path())));
    for (const QFileInfo& file : layout.entryInfoList(QDir::Files)) {
        const QString suffix = file.suffix().toLower();
        if (suffix == QLatin1String("html") || suffix == QLatin1String("htm"))
            continue;
        QFile::remove(images.filePath(file.fileName()));
        QFile::copy(file.absoluteFilePath(), images.filePath(file.fileName()));
    }
    for (const QString& key : glyphKeys()) {
        const QString file = images.filePath(key + QStringLiteral(".png"));
        if (!QFileInfo::exists(file))
            commandGlyph(key).save(file, "PNG");
    }

    DefFile def;
    def.parse(projectText(project, project.filePath()));
    const QString info = QStringLiteral("Info");
    QString name = def.value(info, QStringLiteral("displayname"));
    if (name.isEmpty())
        name = def.value(info, QStringLiteral("name"));
    if (name.isEmpty())
        name = project.name();

    QHash<QString, QString> values;
    values.insert(QStringLiteral("displayname"), escape(name));
    values.insert(QStringLiteral("charname"), escape(name));
    values.insert(QStringLiteral("chardate"), escape(def.value(info, QStringLiteral("versiondate"))));
    values.insert(QStringLiteral("mugenversion"), escape(def.value(info, QStringLiteral("mugenversion"))));
    values.insert(QStringLiteral("author"), escape(def.value(info, QStringLiteral("author"))));
    values.insert(QStringLiteral("email"), escape(options.email));
    values.insert(QStringLiteral("homepage"), escape(options.homepage));
    values.insert(QStringLiteral("titletext"), escape(options.title));
    values.insert(QStringLiteral("text"), escape(options.text).replace(QLatin1Char('\n'), QLatin1String("<br/>")));
    values.insert(QStringLiteral("titlename"), HtmlExportTexts::tr("Name: "));
    values.insert(QStringLiteral("titledate"), HtmlExportTexts::tr("Date: "));
    values.insert(QStringLiteral("titleversion"), HtmlExportTexts::tr("Version: "));
    values.insert(QStringLiteral("titleauthor"), HtmlExportTexts::tr("Author: "));
    values.insert(QStringLiteral("titleemail"), HtmlExportTexts::tr("E-mail: "));
    values.insert(QStringLiteral("titlehomepage"), HtmlExportTexts::tr("Home-page: "));
    values.insert(QStringLiteral("titleanim"), HtmlExportTexts::tr("Palettes: "));
    values.insert(QStringLiteral("titlecomm"), HtmlExportTexts::tr("Commands:"));

    const QString base = target.completeBaseName();
    const SpriteDocument* sprites = projectSprites(project);
    const QString type = project.typeId();
    if (type == MugenType::Character) {
        if (sprites) {
            const QList<std::tuple<int, QString, QString>> portraits = {
                {1, QStringLiteral("big.png"), QStringLiteral("bigportrait")},
                {0, QStringLiteral("small.png"), QStringLiteral("smallportrait")}};
            for (const auto& [number, file, key] : portraits) {
                const int index = sprites->findSprite(9000, number);
                if (index < 0)
                    continue;
                const Formats::Image image = sprites->image(index);
                if (writeImageFile(images.filePath(file), image, error))
                    values.insert(key, imageTag(file, QSize(image.width(), image.height())));
            }
        }
        const QStringList animFiles = project.editorFiles(EditorId::Animations);
        const AirDocument air = AirDocument::parse(projectText(project, animFiles.value(0)).split(QLatin1Char('\n')));
        const int standing = air.indexOf(0);
        if (sprites && standing >= 0) {
            QList<int> palettes = characterPalettes(*sprites);
            if (palettes.isEmpty())
                palettes << -1;
            for (int i = 0; i < palettes.size(); ++i) {
                const SpriteDocument shown = palettes[i] >= 0 ? withPalette(*sprites, palettes[i]) : *sprites;
                const QByteArray gif = animationGif(air.animations()[standing], shown, 1.0, 1.0, error);
                const QString file = QStringLiteral("%1_anim_%2.gif").arg(base).arg(i + 1);
                if (!gif.isEmpty() && writeBytes(images.filePath(file), gif, error))
                    values.insert(QStringLiteral("anim%1").arg(i + 1), imageTag(file));
            }
        }
        const QStringList commandFiles = project.editorFiles(EditorId::Commands);
        QString list;
        for (const HtmlCommand& command : htmlCommands(projectText(project, commandFiles.value(0)))) {
            if (!options.commands.contains(command.name))
                continue;
            const QString title = options.boldNames ? QStringLiteral("<strong>%1</strong>").arg(escape(command.name))
                                                    : escape(command.name);
            list += title + QStringLiteral("<br/>") + commandHtml(command.command) +
                    (options.horizontalLines ? QStringLiteral("<hr/>") : QStringLiteral("<br/>")) + QLatin1Char('\n');
        }
        values.insert(QStringLiteral("commands"), list);
    } else if (type == MugenType::Stage) {
        const QStringList lines = projectText(project, project.filePath()).split(QLatin1Char('\n'));
        const BgDocument bg = BgDocument::parse(lines);
        const AirDocument air = AirDocument::parse(lines);
        QSizeF screen(320, 240);
        const QStringList localcoord = def.value(QStringLiteral("StageInfo"), QStringLiteral("localcoord"))
                                           .split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (localcoord.size() == 2 && localcoord[0].toDouble() > 0 && localcoord[1].toDouble() > 0)
            screen = QSizeF(localcoord[0].toDouble(), localcoord[1].toDouble());
        const QImage still = renderStage(bg, sprites, air, screen, options.frameStart, options.background)
                                 .scaled(options.staticSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (still.save(images.filePath(QStringLiteral("stage.png")), "PNG"))
            values.insert(QStringLiteral("stage"), imageTag(QStringLiteral("stage.png")));
        const int rate = qBound(1, options.frameRate, TicksPerSecond);
        const int count = qMax(1, options.duration * rate);
        QList<QImage> frames;
        QList<int> delays;
        int shown = 0; // hundredths of a second already given
        for (int i = 0; i < count; ++i) {
            const int tick = options.frameStart + int(std::lround(i * double(TicksPerSecond) / rate));
            const QImage frame = renderStage(bg, sprites, air, screen, tick, options.background)
                                     .scaled(options.gifSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            const int end = int(std::lround((i + 1) * 100.0 / rate));
            if (!frames.isEmpty() && frame == frames.last()) {
                delays.last() += end - shown;
            } else {
                frames << frame;
                delays << end - shown;
            }
            shown = end;
        }
        const QByteArray gif = framesGif(frames, delays, options.sharedPalette, error);
        if (!gif.isEmpty() && writeBytes(images.filePath(QStringLiteral("stage.gif")), gif, error))
            values.insert(QStringLiteral("stage_anim"), imageTag(QStringLiteral("stage.gif")));
    }

    static const QRegularExpression placeholder(QStringLiteral("!ff\\s+([a-z0-9_]+)\\s*!"),
                                                QRegularExpression::CaseInsensitiveOption);
    QString result;
    qsizetype last = 0;
    for (QRegularExpressionMatchIterator it = placeholder.globalMatch(page); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        result += page.mid(last, m.capturedStart() - last) + values.value(m.captured(1).toLower());
        last = m.capturedEnd();
    }
    result += page.mid(last);
    static const QRegularExpression emptyTitle(QStringLiteral("<title>\\s*</title>"),
                                               QRegularExpression::CaseInsensitiveOption);
    result.replace(emptyTitle, QStringLiteral("<title>%1</title>").arg(escape(name)));

    // Written in the character set the layout declares, with CR LF line ends.
    static const QRegularExpression charset(QStringLiteral("charset\\s*=\\s*[\"']?([A-Za-z0-9_\\-]+)"),
                                            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch declared = charset.match(result);
    const QString outputEncoding = declared.hasMatch() ? declared.captured(1) : QStringLiteral("UTF-8");
    result.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
    return writeBytes(htmlPath, CodeFile::encode(result, outputEncoding), error);
}

} // namespace OFS::Mugen

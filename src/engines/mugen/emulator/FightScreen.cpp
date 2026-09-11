#include "engines/mugen/emulator/FightScreen.h"

#include "core/project/ProjectFile.h"
#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/IniText.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

// The engine waits this long after the Round call starts before it calls the fight.
constexpr int CallFightTime = 60;
// It shows the KO call this long after the round ends.
constexpr int KoDelay = 10;

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QString encoding;
    QByteArray bom;
    return CodeFile::decode(file.readAll(), &encoding, &bom).replace(QLatin1String("\r\n"), QLatin1String("\n"));
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

Formats::ByteSpan span(const QByteArray& data)
{
    return Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size()));
}

QList<double> numbers(const QString& value)
{
    QList<double> out;
    for (const QString& part : value.split(QLatin1Char(','))) {
        bool ok = false;
        const double v = part.trimmed().toDouble(&ok);
        out << (ok ? v : 0);
    }
    return out;
}

QPointF point(const IniSection* section, const QString& key, QPointF fallback = QPointF())
{
    if (!section || !section->has(key))
        return fallback;
    const QList<double> v = numbers(section->value(key));
    return QPointF(v.value(0), v.value(1));
}

int integer(const IniSection* section, const QString& key, int fallback)
{
    if (!section || !section->has(key))
        return fallback;
    return int(numbers(section->value(key)).value(0));
}

} // namespace

int FightScreen::Font::textWidth(const QString& text) const
{
    if (kind == Kind::TrueType) {
        QFont font(family);
        font.setPixelSize(std::max(1, info.height));
        return QFontMetrics(font).horizontalAdvance(text);
    }
    if (kind == Kind::Bitmap) {
        // Characters without a glyph take no room. A space is the font's width.
        int width = 0;
        for (int i = 0; i < text.size(); ++i) {
            int w = 0;
            if (text[i] == QLatin1Char(' ')) {
                w = info.width;
            } else if (sprites) {
                const int index = sprites->findSprite(0, text[i].unicode());
                w = index >= 0 ? sprites->sprites()[index].width() : 0;
            }
            width += w;
            if (w > 0 && i < text.size() - 1)
                width += info.spacingX;
        }
        return width;
    }
    int width = 0;
    for (int i = 0; i < text.size(); ++i) {
        const auto glyph = glyphs.constFind(text[i].unicode());
        const int w = text[i] == QLatin1Char(' ') || glyph == glyphs.cend() ? info.width : glyph->second;
        if (w + info.spacingX > 0) {
            width += w;
            if (i < text.size() - 1)
                width += info.spacingX;
        }
    }
    return width;
}

Formats::Palette FightScreen::Font::palette(int bank) const
{
    Formats::Palette base = image.palette();
    base.resize(256);
    if (bank <= 0 || bank >= banks || bankColors <= 0)
        return base;
    // A bank takes the font's colours from further down the palette.
    Formats::Palette out = base;
    const int c = bankColors;
    for (int i = 0; i < c; ++i)
        out[std::size_t(256 - c + i)] = base[std::size_t(256 - c * (bank + 1) + i)];
    return out;
}

std::optional<FightScreen::Font> FightScreen::Font::load(const QString& path)
{
    // FNT v2: a definition naming the glyphs' sprite file or a TrueType file.
    if (path.endsWith(QLatin1String(".def"), Qt::CaseInsensitive)) {
        const IniText ini = IniText::parse(readText(path));
        const IniSection* def = ini.section(QStringLiteral("Def"));
        if (!def)
            return std::nullopt;
        Font font;
        const QList<double> size = numbers(def->value(QStringLiteral("size")));
        const QList<double> spacing = numbers(def->value(QStringLiteral("spacing")));
        const QList<double> offset = numbers(def->value(QStringLiteral("offset")));
        font.info.width = int(size.value(0));
        font.info.height = int(size.value(1));
        font.info.spacingX = int(spacing.value(0));
        font.info.spacingY = int(spacing.value(1));
        font.info.offsetX = int(offset.value(0));
        font.info.offsetY = int(offset.value(1));
        const QString file = QDir(QFileInfo(path).absolutePath()).filePath(unquote(def->value(QStringLiteral("file"))));
        if (def->value(QStringLiteral("type")).trimmed().compare(QLatin1String("truetype"), Qt::CaseInsensitive) == 0) {
            font.kind = Kind::TrueType;
            const int id = QFontDatabase::addApplicationFont(file);
            const QStringList families = id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList();
            font.family = families.isEmpty() ? QFileInfo(file).completeBaseName() : families.first();
            return font;
        }
        const QByteArray sff = readBytes(file);
        auto doc = Formats::SffDocument::read(span(sff));
        if (!doc)
            return std::nullopt;
        font.kind = Kind::Bitmap;
        font.sprites = std::make_shared<const SpriteDocument>(SpriteDocument::fromSff(doc.value()));
        return font;
    }
    const QByteArray data = readBytes(path);
    auto doc = Formats::FntDocument::read(span(data));
    if (!doc)
        return std::nullopt;
    auto image = doc->image();
    if (!image || !image->isIndexed())
        return std::nullopt;
    Font font;
    font.image = std::move(image.value());
    font.info = doc->info();
    for (const Formats::FntInfo::Glyph& g : font.info.glyphs) {
        if (font.info.type == Formats::FntInfo::Type::Fixed)
            font.glyphs.insert(g.code, {g.x * font.info.width, font.info.width});
        else
            font.glyphs.insert(g.code, {g.x, g.width});
    }
    if (font.info.colors > 0) {
        font.bankColors = std::min(255, (font.info.colors + 15) / 16 * 16);
        font.banks = std::max(1, 255 / font.bankColors);
    }
    return font;
}

QImage FightScreen::Font::render(int bank) const
{
    if (kind != Kind::Fnt)
        return QImage();
    QImage indexed(image.width(), image.height(), QImage::Format_Indexed8);
    for (int y = 0; y < image.height(); ++y) {
        std::copy_n(image.pixels().data() + std::size_t(y) * std::size_t(image.width()), image.width(),
                    indexed.scanLine(y));
    }
    QList<QRgb> colors;
    const Formats::Palette colours = palette(bank);
    for (std::size_t i = 0; i < colours.size(); ++i)
        colors << (i == 0 ? qRgba(0, 0, 0, 0) : qRgb(colours[i].r, colours[i].g, colours[i].b));
    indexed.setColorTable(colors);
    return indexed.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

void FightScreen::Font::draw(QPainter& painter, const QImage& glyphImage, QPointF pos, int align, QPointF scale,
                             const QString& text, int bank) const
{
    const double sx = scale.x();
    const double sy = scale.y();
    if (kind == Kind::TrueType) {
        QFont font(family);
        font.setPixelSize(std::max(1, info.height));
        painter.save();
        painter.setFont(font);
        painter.setPen(Qt::white);
        const int width = textWidth(text);
        double x = pos.x() + info.offsetX;
        if (align == 0)
            x -= width / 2.0;
        else if (align < 0)
            x -= width;
        painter.translate(x, pos.y() + info.offsetY);
        painter.scale(sx, sy);
        painter.drawText(QPointF(), text);
        painter.restore();
        return;
    }
    if (kind == Kind::Bitmap) {
        if (!sprites)
            return;
        const int bankPalette = sprites->findPalette(0, std::max(0, bank));
        double x = pos.x() + info.offsetX * sx;
        const double y = pos.y() + (info.offsetY - info.height + 1) * sy;
        const int width = textWidth(text);
        if (align == 0)
            x -= width * sx / 2;
        else if (align < 0)
            x -= width * sx;
        for (const QChar c : text) {
            if (c == QLatin1Char(' ')) {
                x += (info.width + info.spacingX) * sx;
                continue;
            }
            const int index = sprites->findSprite(0, c.unicode());
            if (index < 0)
                continue;
            const SpriteEntry& entry = sprites->sprites()[index];
            const Formats::Palette* palette =
                bankPalette >= 0 ? &sprites->palettes()[bankPalette]->palette.colors : nullptr;
            const Formats::Image rgba = sprites->imageRgba(index, palette);
            if (!rgba.pixelBuffer().empty()) {
                const QImage glyph =
                    QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888)
                        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
                painter.drawImage(QRectF(x - entry.header.axisX * sx, y - entry.header.axisY * sy, glyph.width() * sx,
                                         glyph.height() * sy),
                                  glyph);
            }
            x += (entry.width() + info.spacingX) * sx;
        }
        return;
    }
    double x = pos.x() + info.offsetX * sx;
    const double y = pos.y() + (info.offsetY - info.height + 1) * sy;
    const int width = textWidth(text);
    if (align == 0)
        x -= width * sx / 2;
    else if (align < 0)
        x -= width * sx;
    for (const QChar c : text) {
        const auto glyph = glyphs.constFind(c.unicode());
        if (c == QLatin1Char(' ') || glyph == glyphs.cend()) {
            x += (info.width + info.spacingX) * sx;
            continue;
        }
        painter.drawImage(QRectF(x, y, glyph->second * sx, info.height * sy), glyphImage,
                          QRectF(glyph->first, 0, glyph->second, info.height));
        x += (glyph->second + info.spacingX) * sx;
    }
}

FightScreen::FightScreen() = default;
FightScreen::~FightScreen() = default;

const FightScreen::Font* FightScreen::font(int number) const
{
    auto found = m_fonts.constFind(number);
    return found == m_fonts.cend() ? nullptr : &found.value();
}

int FightScreen::callTime() const
{
    return m_loaded && !m_skipCalls ? m_roundTime + CallFightTime : 0;
}

FightScreen::Element FightScreen::readElement(const IniSection* section, const QString& prefix) const
{
    Element e;
    if (!section)
        return e;
    auto has = [&](const char* key) {
        return section->has(prefix + QLatin1String(key));
    };
    auto value = [&](const char* key) {
        return section->value(prefix + QLatin1String(key));
    };
    if (has("spr")) {
        const QList<double> v = numbers(value("spr"));
        e.sprGroup = int(v.value(0, -1));
        e.sprNumber = int(v.value(1));
    }
    if (has("anim")) {
        const int anim = int(numbers(value("anim")).value(0, -1));
        if (m_animations.indexOf(anim) >= 0)
            e.anim = anim;
    }
    if (has("font")) {
        const QList<double> v = numbers(value("font"));
        e.font = int(v.value(0, -1));
        e.bank = int(v.value(1));
        if (v.size() > 2)
            e.align = v[2] < 0 ? -1 : v[2] > 0 ? 1 : 0;
    }
    if (has("text"))
        e.text = unquote(value("text"));
    e.offset = point(section, prefix + QLatin1String("offset"));
    if (has("facing"))
        e.facing = numbers(value("facing")).value(0) < 0 ? -1 : 1;
    if (has("vfacing"))
        e.vfacing = numbers(value("vfacing")).value(0) < 0 ? -1 : 1;
    e.scale = point(section, prefix + QLatin1String("scale"), QPointF(1, 1));
    e.layer = integer(section, prefix + QLatin1String("layerno"), 0);
    e.displayTime = integer(section, prefix + QLatin1String("displaytime"), -2);
    if (has("snd")) {
        const QList<double> v = numbers(value("snd"));
        e.sndGroup = int(v.value(0, -1));
        e.sndNumber = int(v.value(1));
    }
    return e;
}

FightScreen::Bar FightScreen::readBar(const IniSection* section, const QString& prefix) const
{
    Bar bar;
    bar.pos = point(section, prefix + QLatin1String("pos"));
    bar.bg0 = readElement(section, prefix + QLatin1String("bg0."));
    bar.bg1 = readElement(section, prefix + QLatin1String("bg1."));
    bar.mid = readElement(section, prefix + QLatin1String("mid."));
    bar.front = readElement(section, prefix + QLatin1String("front."));
    bar.counter = readElement(section, prefix + QLatin1String("counter."));
    const QPointF range = point(section, prefix + QLatin1String("range.x"));
    bar.range[0] = int(range.x());
    bar.range[1] = int(range.y());
    for (Element* e : {&bar.bg0, &bar.bg1, &bar.mid, &bar.front})
        start(*e);
    return bar;
}

bool FightScreen::load(const QString& engineDir, const QString& fightDef,
                       const std::function<QString(const QString&)>& source)
{
    *this = FightScreen();
    const QString def = fightDef.isEmpty() ? QDir(engineDir).filePath(QStringLiteral("data/fight.def")) : fightDef;
    if (!QFileInfo(def).isFile())
        return false;
    const QString dataDir = QFileInfo(def).path();
    auto resolve = [&](const QString& name) -> QString {
        const QString cleaned = QDir::fromNativeSeparators(unquote(name).trimmed());
        if (cleaned.isEmpty())
            return QString();
        for (const QString& candidate : {QDir(dataDir).filePath(cleaned), QDir(engineDir).filePath(cleaned),
                                         QDir(engineDir).filePath(QStringLiteral("font/") + cleaned)}) {
            if (QFileInfo(candidate).isFile())
                return QDir::cleanPath(candidate);
        }
        return QString();
    };

    const QString text = source ? source(def) : readText(def);
    const IniText ini = IniText::parse(text);
    m_animations = AirDocument::parse(text);
    if (const IniSection* files = ini.section(QStringLiteral("Files"))) {
        if (const QByteArray sff = readBytes(resolve(files->value(QStringLiteral("sff")))); !sff.isEmpty()) {
            if (auto doc = Formats::SffDocument::read(span(sff)))
                m_sprites = std::make_shared<SpriteDocument>(SpriteDocument::fromSff(doc.value()));
        }
        if (const QByteArray snd = readBytes(resolve(files->value(QStringLiteral("snd")))); !snd.isEmpty()) {
            if (auto doc = Formats::SndDocument::read(span(snd)))
                m_soundFile = std::make_shared<SoundDocument>(SoundDocument::fromSnd(doc.value()));
        }
        for (const IniEntry& entry : files->entries) {
            const QString key = entry.key.toLower();
            bool ok = false;
            const int number = key.startsWith(QLatin1String("font")) ? key.mid(4).toInt(&ok) : 0;
            if (!ok)
                continue;
            if (std::optional<Font> font = Font::load(resolve(entry.value)))
                m_fonts.insert(number, std::move(*font));
        }
    }

    const IniSection* lifebar = ini.section(QStringLiteral("Lifebar"));
    const IniSection* powerbar = ini.section(QStringLiteral("Powerbar"));
    const IniSection* face = ini.section(QStringLiteral("Face"));
    const IniSection* name = ini.section(QStringLiteral("Name"));
    const IniSection* combo = ini.section(QStringLiteral("Combo"));
    const IniSection* winIcon = ini.section(QStringLiteral("WinIcon"));
    for (int side = 0; side < 2; ++side) {
        const QString p = QStringLiteral("p%1.").arg(side + 1);
        m_life[side] = readBar(lifebar, p);
        m_power[side] = readBar(powerbar, p);
        m_face[side].pos = point(face, p + QLatin1String("pos"));
        m_face[side].bg = readElement(face, p + QLatin1String("bg."));
        m_face[side].face = readElement(face, p + QLatin1String("face."));
        start(m_face[side].bg);
        m_namePos[side] = point(name, p + QLatin1String("pos"));
        m_name[side] = readElement(name, p + QLatin1String("name."));

        const QString team = QStringLiteral("team%1.").arg(side + 1);
        Combo& co = m_combo[side];
        co.pos = point(combo, team + QLatin1String("pos"));
        co.startX = combo && combo->has(team + QLatin1String("start.x"))
                        ? float(numbers(combo->value(team + QLatin1String("start.x"))).value(0))
                        : float(co.pos.x());
        co.counter = readElement(combo, team + QLatin1String("counter."));
        co.shake = integer(combo, team + QLatin1String("counter.shake"), 0) != 0;
        co.text = readElement(combo, team + QLatin1String("text."));
        co.displayTime = integer(combo, team + QLatin1String("displaytime"), 90);

        WinIcon& wi = m_winIcon[side];
        wi.pos = point(winIcon, p + QLatin1String("pos"));
        wi.iconOffset = point(winIcon, p + QLatin1String("iconoffset"));
        wi.counter = readElement(winIcon, p + QLatin1String("counter."));
        static const char* const kinds[] = {"n", "s", "h", "throw", "c", "t", "suicide", "perfect"};
        for (int i = 0; i < 8; ++i)
            wi.icons[std::size_t(i)] = readElement(winIcon, p + QLatin1String(kinds[i]) + QLatin1Char('.'));
    }
    for (int level = 1; level <= 3; ++level) {
        const QString key = QStringLiteral("level%1.snd").arg(level);
        if (powerbar && powerbar->has(key)) {
            const QList<double> v = numbers(powerbar->value(key));
            for (Bar& bar : m_power) {
                bar.levelSounds[std::size_t(level - 1)] = {int(v.value(0, -1)), int(v.value(1))};
            }
        }
    }
    m_useIconUpTo = integer(winIcon, QStringLiteral("useiconupto"), 4);

    const IniSection* time = ini.section(QStringLiteral("Time"));
    m_timePos = point(time, QStringLiteral("pos"));
    m_timeBg = readElement(time, QStringLiteral("bg."));
    m_timeCounter = readElement(time, QStringLiteral("counter."));
    m_framesPerCount = std::max(1, integer(time, QStringLiteral("framespercount"), 60));
    start(m_timeBg);

    const IniSection* round = ini.section(QStringLiteral("Round"));
    m_roundPos = point(round, QStringLiteral("pos"));
    m_roundTime = std::max(0, integer(round, QStringLiteral("round.time"), 0));
    m_roundSndTime = integer(round, QStringLiteral("round.sndtime"), m_roundTime);
    m_roundDefault = readElement(round, QStringLiteral("round.default."));
    for (int n = 1; n <= 9; ++n) {
        const QString prefix = QStringLiteral("round%1.").arg(n);
        const bool any = round && std::any_of(round->entries.cbegin(), round->entries.cend(), [&](const IniEntry& e) {
                             return e.key.startsWith(prefix, Qt::CaseInsensitive);
                         });
        if (!any)
            break;
        m_rounds << readElement(round, prefix);
    }
    m_fightTime = integer(round, QStringLiteral("fight.time"), 0);
    m_fightSndTime = integer(round, QStringLiteral("fight.sndtime"), m_fightTime);
    m_fight = readElement(round, QStringLiteral("fight."));
    m_koTime = integer(round, QStringLiteral("ko.time"), 0);
    m_koSndTime = integer(round, QStringLiteral("ko.sndtime"), m_koTime);
    m_ko = readElement(round, QStringLiteral("ko."));
    m_dko = readElement(round, QStringLiteral("dko."));
    m_to = readElement(round, QStringLiteral("to."));
    m_winTime = integer(round, QStringLiteral("win.time"), 0);
    m_winSndTime = integer(round, QStringLiteral("win.sndtime"), m_winTime);
    m_win = readElement(round, QStringLiteral("win."));
    m_win2 = readElement(round, QStringLiteral("win2."));
    m_draw = readElement(round, QStringLiteral("draw."));
    m_overWaitTime = integer(round, QStringLiteral("over.waittime"), 45);
    m_slowTime = std::max(0, integer(round, QStringLiteral("slow.time"), 60));
    if (round && round->has(QStringLiteral("slow.speed")))
        m_slowSpeed = std::clamp(float(numbers(round->value(QStringLiteral("slow.speed"))).value(0)), 0.01f, 1.0f);
    m_loaded = true;
    return true;
}

void FightScreen::start(Element& e) const
{
    e.count = 0;
    e.player = AnimPlayer();
    const int index = e.anim >= 0 ? m_animations.indexOf(e.anim) : -1;
    if (index < 0)
        return;
    const AirAnimation& animation = m_animations.animations()[index];
    QList<int> times;
    for (const AirFrame& frame : animation.frames)
        times << frame.time;
    e.player = AnimPlayer(times, animation.loopStartFrame());
}

bool FightScreen::over(const Element& e) const
{
    if (e.displayTime >= 0)
        return e.count >= e.displayTime;
    if (e.anim < 0)
        return e.sprGroup < 0; // a sprite stays; a text without a display time is not shown
    return e.player.ended() || (e.player.totalTime() < 0 && e.player.frame() == e.player.frameCount() - 1);
}

float FightScreen::gameSpeed(const Match& match) const
{
    const int ended = match.overClock();
    if (!m_loaded || ended < 0 || ended >= m_slowTime || match.roundEnd() == RoundEnd::TimeOver)
        return 1;
    const int left = m_slowTime - ended;
    const float fade = m_slowTime * 0.75f;
    float speed = m_slowSpeed;
    if (left < fade)
        speed += (1 - speed) * (fade - left) / fade;
    return speed;
}

void FightScreen::reset()
{
    for (int side = 0; side < 2; ++side) {
        for (Bar* bar : {&m_life[side], &m_power[side]}) {
            bar->top = bar->midFill = bar->midMin = bar->old = 1;
            bar->midTime = 0;
            bar->level = 0;
        }
        m_power[side].top = m_power[side].midFill = m_power[side].midMin = m_power[side].old = 0;
        Combo& co = m_combo[side];
        co.shown = 0;
        co.rest = 0;
        co.slide = co.startX - float(co.pos.x());
    }
    m_roundShown = m_koShown = m_winShown = nullptr;
    m_roundPhase = m_fightPhase = m_koPhase = m_winPhase = 0;
    m_sounds.clear();
}

void FightScreen::play(int group, int number)
{
    if (group >= 0)
        m_sounds << Sound{group, number};
}

void FightScreen::stepBar(Bar& bar, float fill, bool gettingHit, bool lagging)
{
    // The front part drops quickly. The mid part keeps what a combo took, then drains (lifebars) or
    // follows a little behind (power bars).
    fill = std::clamp(fill, 0.0f, 1.0f);
    if (bar.top > fill)
        bar.top += (fill - bar.top) / 2;
    else
        bar.top = fill;
    if (lagging && gettingHit) {
        if (bar.midTime < 30) {
            bar.midTime = 30;
            bar.midFill = bar.old;
            bar.midMin = bar.old;
        }
    } else {
        if (bar.midTime > 0)
            --bar.midTime;
        if (bar.midTime <= 0 && fill < bar.midMin) {
            bar.midMin += (fill - bar.midMin) * (1 / (12 - (fill - bar.midMin) * 144));
            bar.midMin = std::max(bar.midMin, fill);
        } else {
            bar.midMin = fill;
        }
        if (bar.midTime <= 0 && bar.midFill > bar.midMin)
            bar.midFill += (bar.midMin - bar.midFill) / 8;
        bar.old = fill;
    }
    const float least = std::max(bar.midMin, fill);
    if (bar.midFill < least)
        bar.midFill += (least - bar.midFill) / 2;
    for (Element* e : {&bar.bg0, &bar.bg1, &bar.mid, &bar.front})
        e->player.step();
}

void FightScreen::step(const Match& match, const std::array<Side, 2>& sides)
{
    m_sounds.clear();
    if (!m_loaded)
        return;
    for (int side = 0; side < 2; ++side) {
        const Character* c = sides[std::size_t(side)].character;
        if (!c)
            continue;
        const float lifeMax = std::max(1.0f, c->constant(QStringLiteral("data.life"), 1000));
        stepBar(m_life[side], float(c->life()) / lifeMax, c->moveType() == QLatin1Char('H'), true);
        const float powerMax = std::max(1.0f, c->constant(QStringLiteral("data.power"), 3000));
        Bar& power = m_power[side];
        stepBar(power, float(c->power()) / powerMax, false, false);
        const int level = c->power() / 1000;
        if (level > power.level && level >= 1 && level <= 3)
            play(power.levelSounds[std::size_t(level - 1)].first, power.levelSounds[std::size_t(level - 1)].second);
        power.level = level;
        m_face[side].bg.player.step();

        const Character* other = sides[std::size_t(1 - side)].character;
        const int hits = other && other->moveType() == QLatin1Char('H') ? other->getHitVars().hitCount : 0;
        Combo& co = m_combo[side];
        const float hidden = co.startX - float(co.pos.x());
        if (co.rest > 0) {
            co.slide -= co.slide / 8;
            if (std::abs(co.slide) < 1)
                co.slide = 0;
        } else if (hits < 2 && co.slide != hidden) {
            const float stepBy = hidden < 0 ? -4.0f : 4.0f;
            co.slide += stepBy;
            if ((hidden < 0 && co.slide < hidden) || (hidden >= 0 && co.slide > hidden))
                co.slide = hidden;
        }
        if (co.slide == 0 && co.rest > 0)
            --co.rest;
        if (hits >= 2 && hits != co.shown) {
            co.rest = co.displayTime;
            co.shown = hits;
            co.shakeTime = co.shake ? 10 : 0;
        }
        if (co.shakeTime > 0)
            --co.shakeTime;
    }
    m_timeBg.player.step();

    const int intro = match.introClock();
    if (intro < 0) {
        m_roundPhase = m_fightPhase = m_koPhase = m_winPhase = 0;
        return;
    }
    if (m_skipCalls)
        m_roundPhase = m_fightPhase = 2;
    if (m_roundPhase < 2) {
        const int round = match.roundNo();
        Element& shown = round >= 1 && round <= m_rounds.size() ? m_rounds[round - 1] : m_roundDefault;
        if (intro == m_roundSndTime)
            play(shown.sndGroup >= 0 ? shown : m_roundDefault);
        if (intro >= m_roundTime) {
            m_roundShown = shown.hasAnim() || shown.hasText() ? &shown : &m_roundDefault;
            if (m_roundPhase == 0) {
                start(*m_roundShown);
                m_roundPhase = 1;
            } else {
                m_roundShown->player.step();
            }
            ++m_roundShown->count;
            if (over(*m_roundShown))
                m_roundPhase = 2;
        }
    }
    const int fight = intro - callTime();
    if (fight >= 0 && m_fightPhase < 2) {
        if (fight == m_fightSndTime)
            play(m_fight);
        if (fight >= m_fightTime) {
            if (m_fightPhase == 0) {
                start(m_fight);
                m_fightPhase = 1;
            } else {
                m_fight.player.step();
            }
            ++m_fight.count;
            if (over(m_fight) && fight >= m_fightSndTime)
                m_fightPhase = 2;
        }
    }
    const int ended = match.overClock();
    if (ended >= 0 && m_koPhase < 2) {
        Element& shown = match.roundEnd() == RoundEnd::DoubleKO   ? m_dko
                         : match.roundEnd() == RoundEnd::TimeOver ? m_to
                                                                  : m_ko;
        if (ended == m_koSndTime + KoDelay)
            play(shown);
        if (ended >= m_koTime + KoDelay) {
            m_koShown = &shown;
            if (m_koPhase == 0) {
                start(shown);
                m_koPhase = 1;
            } else {
                shown.player.step();
            }
            ++shown.count;
            if (over(shown))
                m_koPhase = 2;
        }
    }
    const int winning = ended >= 0 ? ended - m_overWaitTime : -1;
    if (winning >= 0 && m_winPhase < 2) {
        Element& shown = match.roundWinner() == 0 || match.roundWinner() == 1 ? m_win : m_draw;
        if (winning == m_winSndTime)
            play(shown);
        if (winning >= m_winTime) {
            m_winShown = &shown;
            if (m_winPhase == 0) {
                start(shown);
                m_winPhase = 1;
            } else {
                shown.player.step();
            }
            ++shown.count;
            if (over(shown))
                m_winPhase = 2;
        }
    }
}

QRectF FightScreen::fillRect(const Bar& bar, float fill) const
{
    const int lo = std::min(bar.range[0], bar.range[1]);
    const int hi = std::max(bar.range[0], bar.range[1]);
    const double size = std::floor((hi - lo + 1) * double(fill) + 0.5);
    const double start = bar.range[0] > bar.range[1] ? bar.pos.x() + hi + 1 - size : bar.pos.x() + lo;
    return QRectF(start, -1000, size, 3000);
}

void FightScreen::addItems(QList<Item>& out, const Element& e, QPointF at, const SpriteDocument* sprites,
                           std::optional<QRectF> clip, const QString& text) const
{
    Item item;
    item.layer = e.layer;
    item.clip = clip;
    at += e.offset;
    if (e.anim >= 0) {
        const int index = m_animations.indexOf(e.anim);
        if (index < 0)
            return;
        const AirAnimation& animation = m_animations.animations()[index];
        const int frame = e.player.isEmpty() ? 0 : e.player.frame();
        if (frame >= animation.frames.size())
            return;
        const AirFrame& f = animation.frames[frame];
        if (f.group < 0)
            return;
        item.sprites = sprites ? sprites : m_sprites.get();
        item.group = f.group;
        item.number = f.number;
        item.pos = at + QPointF(f.x * e.facing * e.scale.x(), f.y * e.vfacing * e.scale.y());
        item.flipH = f.flipH != (e.facing < 0);
        item.flipV = f.flipV != (e.vfacing < 0);
        item.scale = QPointF(e.scale.x() * f.xScale, e.scale.y() * f.yScale);
        item.transparency = f.transparency;
        out << item;
    } else if (e.sprGroup >= 0) {
        item.sprites = sprites ? sprites : m_sprites.get();
        item.group = e.sprGroup;
        item.number = e.sprNumber;
        item.pos = at;
        item.flipH = e.facing < 0;
        item.flipV = e.vfacing < 0;
        item.scale = e.scale;
        out << item;
    } else if (const Font* f = font(e.font); f && !(text.isEmpty() ? e.text : text).isEmpty()) {
        item.font = f;
        item.bank = e.bank;
        item.align = e.align;
        item.text = text.isEmpty() ? e.text : text;
        item.pos = at;
        item.scale = e.scale;
        out << item;
    }
}

QList<FightScreen::Item> FightScreen::items(const Match& match, const std::array<Side, 2>& sides) const
{
    QList<Item> out;
    if (!m_loaded)
        return out;
    for (int side = 0; side < 2; ++side) {
        const Side& s = sides[std::size_t(side)];
        if (!s.character)
            continue;
        for (const Bar* bar : {&m_life[side], &m_power[side]}) {
            addItems(out, bar->bg0, bar->pos);
            addItems(out, bar->bg1, bar->pos);
            if (bar->range[0] != bar->range[1]) {
                const QRectF top = fillRect(*bar, bar->top);
                QRectF mid = fillRect(*bar, bar->midFill);
                if (bar->range[0] < bar->range[1])
                    mid.setLeft(top.right());
                else
                    mid.setRight(top.left());
                if (mid.width() > 0)
                    addItems(out, bar->mid, bar->pos, nullptr, mid);
                if (top.width() > 0)
                    addItems(out, bar->front, bar->pos, nullptr, top);
            } else {
                addItems(out, bar->front, bar->pos);
            }
        }
        const Bar& power = m_power[side];
        addItems(out, power.counter, power.pos, nullptr, std::nullopt, QString::number(s.character->power() / 1000));
        addItems(out, m_face[side].bg, m_face[side].pos);
        addItems(out, m_face[side].face, m_face[side].pos, s.sprites);
        addItems(out, m_name[side], m_namePos[side], nullptr, std::nullopt, s.name);
        const WinIcon& wi = m_winIcon[side];
        const QList<RoundWin>& won = match.roundWins(side);
        auto icon = [&](const RoundWin& w, QPointF at) {
            addItems(out, wi.icons[std::size_t(int(w.type))], at);
            if (w.perfect)
                addItems(out, wi.icons[7], at);
        };
        if (won.size() <= m_useIconUpTo) {
            for (int i = 0; i < won.size(); ++i)
                icon(won[i], wi.pos + wi.iconOffset * i);
        } else {
            icon(won.last(), wi.pos);
            addItems(out, wi.counter, wi.pos, nullptr, std::nullopt, QString::number(won.size()));
        }
    }
    // The timer ("o" without a limit).
    addItems(out, m_timeBg, m_timePos);
    const int left = match.roundTime();
    addItems(out, m_timeCounter, m_timePos, nullptr, std::nullopt,
             left < 0 ? QStringLiteral("o") : QString::number(left / m_framesPerCount));
    for (int side = 0; side < 2; ++side) {
        const Combo& co = m_combo[side];
        const float hidden = co.startX - float(co.pos.x());
        if (co.shown < 2 || co.slide == hidden)
            continue;
        const QString number = QString::number(co.shown);
        QString text = co.text.text;
        text.replace(QLatin1String("%i"), number);
        const Font* counterFont = font(co.counter.font);
        const Font* textFont = font(co.text.font);
        const double counterWidth = counterFont ? counterFont->textWidth(number) * co.counter.scale.x() : 0;
        const double textWidth = textFont ? textFont->textWidth(text) * co.text.scale.x() : 0;
        const double x = co.pos.x() + co.slide;
        const QPointF shake(0, co.shakeTime > 0 ? (co.shakeTime % 2 ? -1.0 : 1.0) * std::min(co.shakeTime, 4) / 2 : 0);
        if (side == 0) {
            addItems(out, co.counter, QPointF(x, co.pos.y()) + shake, nullptr, std::nullopt, number);
            addItems(out, co.text, QPointF(x + counterWidth, co.pos.y()), nullptr, std::nullopt, text);
        } else {
            addItems(out, co.text, QPointF(x, co.pos.y()), nullptr, std::nullopt, text);
            addItems(out, co.counter, QPointF(x - textWidth, co.pos.y()) + shake, nullptr, std::nullopt, number);
        }
    }
    if (m_roundPhase == 1 && m_roundShown) {
        QString text = m_roundShown->text;
        text.replace(QLatin1String("%i"), QString::number(match.roundNo()));
        addItems(out, *m_roundShown, m_roundPos, nullptr, std::nullopt, text);
    }
    if (m_fightPhase == 1)
        addItems(out, m_fight, m_roundPos);
    if (m_koPhase == 1 && m_koShown)
        addItems(out, *m_koShown, m_roundPos);
    if (m_winPhase == 1 && m_winShown) {
        QString text = m_winShown->text;
        const int winner = match.roundWinner();
        if (winner == 0 || winner == 1)
            text.replace(QLatin1String("%s"), sides[std::size_t(winner)].name);
        addItems(out, *m_winShown, m_roundPos, nullptr, std::nullopt, text);
    }
    std::stable_sort(out.begin(), out.end(), [](const Item& a, const Item& b) { return a.layer < b.layer; });
    return out;
}

} // namespace OFS::Mugen

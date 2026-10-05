#include "engines/mugen/emulator/MotifScreen.h"

#include "core/bg/BgRenderer.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/Blending.h"
#include "engines/mugen/emulator/EmulatorSession.h"
#include "engines/mugen/runtime/Files.h"
#include "engines/mugen/runtime/IniText.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QTextDocument>
#include <QtMath>

#include <algorithm>

namespace OFS::Mugen {

using namespace Runtime;

struct MotifScreen::Ini
{
    IniText text;
};

namespace {

// The title menu's items in the engine's order, with their default names.
const char* const MenuKeys[] = {"arcade",       "versus",   "teamarcade", "teamversus", "teamcoop", "survival",
                                "survivalcoop", "training", "watch",      "options",    "exit"};
const char* const MenuNames[] = {"ARCADE",         "VS MODE",  "TEAM ARCADE", "TEAM VS", "TEAM CO-OP", "SURVIVAL",
                                 "SURVIVAL CO-OP", "TRAINING", "WATCH",       "OPTIONS", "EXIT"};

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

QPointF pointOf(const IniSection* section, const QString& key, QPointF fallback = QPointF())
{
    if (!section || !section->has(key))
        return fallback;
    const QList<double> v = numbers(section->value(key));
    return QPointF(v.value(0), v.value(1));
}

int intOf(const IniSection* section, const QString& key, int fallback)
{
    if (!section || !section->has(key))
        return fallback;
    return int(numbers(section->value(key)).value(0));
}

std::shared_ptr<const SpriteDocument> spritesFrom(Project* project, const QString& path)
{
    if (project) {
        if (auto* file = qobject_cast<SpriteFile*>(project->file(path)))
            return std::shared_ptr<const SpriteDocument>(std::shared_ptr<const SpriteDocument>(), &file->document());
    }
    return readSprites(path);
}

} // namespace

MotifScreen::MotifScreen() = default;
MotifScreen::~MotifScreen() = default;
MotifScreen::MotifScreen(MotifScreen&&) noexcept = default;
MotifScreen& MotifScreen::operator=(MotifScreen&&) noexcept = default;

QString MotifScreen::textOf(const QString& path) const
{
    if (m_project) {
        if (auto* code = qobject_cast<CodeFile*>(m_project->file(path)))
            return code->document()->toPlainText();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QString encoding;
    QByteArray bom;
    return CodeFile::decode(file.readAll(), &encoding, &bom).replace(QLatin1String("\r\n"), QLatin1String("\n"));
}

const IniSection* MotifScreen::section(const QString& name) const
{
    return m_ini ? m_ini->text.section(name) : nullptr;
}

const FightScreen::Font* MotifScreen::font(int number) const
{
    auto found = m_fonts.constFind(number);
    return found == m_fonts.cend() ? nullptr : &found.value();
}

bool MotifScreen::start(Project& project, const QString& systemDef, const QString& engineDir, Screen screen,
                        QString* error)
{
    *this = MotifScreen();
    m_project = &project;
    m_screen = screen;
    m_text = textOf(systemDef);
    m_ini = std::make_unique<Ini>(Ini{IniText::parse(m_text)});
    const IniSection* files = section(QStringLiteral("Files"));
    if (!files) {
        if (error)
            *error = QStringLiteral("The motif has no [Files].");
        return false;
    }
    if (const IniSection* info = section(QStringLiteral("Info")); info && info->has(QStringLiteral("localcoord"))) {
        const QList<double> size = numbers(info->value(QStringLiteral("localcoord")));
        if (size.value(0) > 0 && size.value(1) > 0)
            m_size = QSize(int(size[0]), int(size[1]));
    }
    const QString folder = QFileInfo(systemDef).absolutePath();
    auto resolve = [&](const QString& name) -> QString {
        const QString cleaned = QDir::fromNativeSeparators(unquote(name).trimmed());
        if (cleaned.isEmpty())
            return QString();
        for (const QString& candidate :
             {QDir(folder).filePath(cleaned), QDir(engineDir).filePath(QStringLiteral("font/") + cleaned),
              QDir(engineDir).filePath(QStringLiteral("data/") + cleaned), QDir(engineDir).filePath(cleaned)}) {
            if (QFileInfo(candidate).isFile())
                return QDir::cleanPath(candidate);
        }
        return QString();
    };
    m_sprites = spritesFrom(&project, resolve(files->value(QStringLiteral("spr"))));
    const QString snd = resolve(files->value(QStringLiteral("snd")));
    if (auto* file = qobject_cast<SoundFile*>(project.file(snd))) {
        m_sounds = std::shared_ptr<const SoundDocument>(std::shared_ptr<const SoundDocument>(), &file->document());
    } else if (const QByteArray data = readBytes(snd); !data.isEmpty()) {
        if (auto doc = Formats::SndDocument::read(span(data)))
            m_sounds = std::make_shared<const SoundDocument>(SoundDocument::fromSnd(doc.value()));
    }
    for (const IniEntry& entry : files->entries) {
        const QString key = entry.key.toLower();
        bool ok = false;
        const int number = key.startsWith(QLatin1String("font")) ? key.mid(4).toInt(&ok) : 0;
        if (ok) {
            if (std::optional<FightScreen::Font> f = FightScreen::Font::load(resolve(entry.value)))
                m_fonts.insert(number, std::move(*f));
        }
    }
    m_animations = AirDocument::parse(m_text);
    m_bg = BgDocument::parse(m_text.split(QLatin1Char('\n')));
    m_bgRenderer = std::make_unique<BgRenderer>(m_sprites.get(), &m_animations);

    const QString character = EmulatorSession::defaultCharacter(engineDir);
    if (!character.isEmpty()) {
        const IniText def = IniText::parse(textOf(character));
        if (const IniSection* charInfo = def.section(QStringLiteral("Info")))
            m_characterName =
                unquote(charInfo->value(QStringLiteral("displayname"), charInfo->value(QStringLiteral("name"))));
        if (const IniSection* charFiles = def.section(QStringLiteral("Files")))
            m_character = spritesFrom(&project, QDir(QFileInfo(character).absolutePath())
                                                    .filePath(unquote(charFiles->value(QStringLiteral("sprite")))));
    }
    if (const QString stage = EmulatorSession::defaultStage(engineDir); !stage.isEmpty()) {
        const QString stageText = textOf(stage);
        m_stage = BgDocument::parse(stageText.split(QLatin1Char('\n')));
        m_stageAnimations = AirDocument::parse(stageText);
        const IniText stageIni = IniText::parse(stageText);
        m_stageZOffset = float(intOf(stageIni.section(QStringLiteral("StageInfo")), QStringLiteral("zoffset"), 200));
        if (const IniSection* bgdef = stageIni.section(QStringLiteral("BGdef")))
            m_stageSprites = spritesFrom(
                &project, QDir(QFileInfo(stage).absolutePath()).filePath(unquote(bgdef->value(QStringLiteral("spr")))));
        m_stageRenderer = std::make_unique<BgRenderer>(m_stageSprites.get(), &m_stageAnimations);
        if (const int group = m_stage.indexOf(QStringLiteral("BG")); group >= 0)
            m_stageSimulator = std::make_unique<BgSimulator>(m_stage.groups()[group]);
    }

    // [Music] files are in the engine folder (usually sound/) or the motif's, and loop unless told not to.
    const char* bgm = screen == Screen::Menu      ? "title.bgm"
                      : screen == Screen::Versus  ? "vs.bgm"
                      : screen == Screen::Victory ? "victory.bgm"
                                                  : nullptr;
    if (const IniSection* bgmSection = section(QStringLiteral("Music")); bgmSection && bgm) {
        const QString key = QLatin1String(bgm);
        BackgroundMusic::Track track;
        track.file = BackgroundMusic::find(bgmSection->value(key),
                                           {engineDir, folder, QDir(engineDir).filePath(QStringLiteral("data")),
                                            QDir(engineDir).filePath(QStringLiteral("sound"))});
        bool ok = false;
        const float percent = bgmSection->value(key + QLatin1String(".volume")).toFloat(&ok);
        track.volume = (ok ? std::max(0.0f, percent / 100) : 1.0f) * BackgroundMusic::engineVolume(engineDir);
        const QString loop = bgmSection->value(key + QLatin1String(".loop")).trimmed();
        track.loop = loop.isEmpty() || loop.toInt() != 0;
        track.loopStart = bgmSection->value(key + QLatin1String(".loopstart")).toLongLong();
        track.loopEnd = bgmSection->value(key + QLatin1String(".loopend")).toLongLong();
        m_music.play(track);
    }

    switch (screen) {
    case Screen::Menu: {
        startBackground(QStringLiteral("TitleBG"));
        const IniSection* title = section(QStringLiteral("Title Info"));
        for (int i = 0; i < 11; ++i) {
            const QString key = QStringLiteral("menu.itemname.") + QLatin1String(MenuKeys[i]);
            const QString name = title && title->has(key) ? unquote(title->value(key)) : QLatin1String(MenuNames[i]);
            if (name.isEmpty())
                continue;
            m_items << name;
            m_itemKeys << QLatin1String(MenuKeys[i]);
        }
        break;
    }
    case Screen::Versus:
        startBackground(QStringLiteral("VersusBG"));
        break;
    case Screen::Victory:
        startBackground(QStringLiteral("VictoryBG"));
        break;
    default:
        break;
    }
    m_started = true;
    return true;
}

void MotifScreen::startBackground(const QString& name)
{
    m_bgSimulator.reset();
    if (const int group = m_bg.indexOf(name); group >= 0)
        m_bgSimulator = std::make_unique<BgSimulator>(m_bg.groups()[group]);
}

void MotifScreen::playSound(const QString& spec)
{
    if (!spec.trimmed().isEmpty())
        m_pendingSounds << spec;
}

void MotifScreen::tick(const InputState& keys)
{
    if (!m_started)
        return;
    ++m_time;
    if (m_bgSimulator)
        m_bgSimulator->step();
    if (m_stageSimulator)
        m_stageSimulator->step();
    auto pressed = [this, &keys](bool InputState::*button) {
        return keys.*button && !(m_lastKeys.*button);
    };
    if (m_screen == Screen::Menu && !m_items.isEmpty()) {
        const IniSection* title = section(QStringLiteral("Title Info"));
        const int count = int(m_items.size());
        if (pressed(&InputState::up) || pressed(&InputState::down)) {
            m_cursor = (m_cursor + (pressed(&InputState::up) ? count - 1 : 1)) % count;
            playSound(title ? title->value(QStringLiteral("cursor.move.snd")) : QString());
        }
        if (pressed(&InputState::a) || pressed(&InputState::s))
            playSound(title ? title->value(QStringLiteral("cursor.done.snd")) : QString());
        const int visible = std::max(1, intOf(title, QStringLiteral("menu.window.visibleitems"), 5));
        if (m_cursor < m_scroll)
            m_scroll = m_cursor;
        if (m_cursor >= m_scroll + visible)
            m_scroll = m_cursor - visible + 1;
    }
    if (m_screen == Screen::Continue && (pressed(&InputState::left) || pressed(&InputState::right)))
        m_yes = !m_yes;
    m_lastKeys = keys;
}

void MotifScreen::playSounds(AudioMixer& mixer)
{
    for (const QString& spec : std::as_const(m_pendingSounds)) {
        auto cached = m_soundCache.constFind(spec);
        if (cached == m_soundCache.cend()) {
            std::shared_ptr<const AudioMixer::Sound> prepared;
            const QList<double> id = numbers(spec);
            if (m_sounds && id.size() >= 2) {
                if (const int at = m_sounds->find(int(id[0]), int(id[1])); at >= 0) {
                    if (auto decoded = SoundEdits::decode(m_sounds->sounds()[at]))
                        prepared = AudioMixer::prepare(decoded.value().buffer);
                }
            }
            cached = m_soundCache.insert(spec, prepared);
        }
        if (cached.value())
            mixer.play(cached.value(), Sounds, -1);
    }
    m_pendingSounds.clear();
    m_music.update(mixer);
}

void MotifScreen::drawText(QPainter& painter, const QString& fontSpec, QPointF at, const QString& text)
{
    const QList<double> spec = numbers(fontSpec);
    const FightScreen::Font* f = font(int(spec.value(0, -1)));
    if (!f || text.isEmpty())
        return;
    const int bank = int(spec.value(1));
    const int align = spec.size() > 2 ? (spec[2] < 0 ? -1 : spec[2] > 0 ? 1 : 0) : 0;
    const auto key = qMakePair(f, bank);
    auto image = m_fontImages.constFind(key);
    if (image == m_fontImages.cend())
        image = m_fontImages.insert(key, f->render(bank));
    f->draw(painter, image.value(), at, align, QPointF(1, 1), text, bank);
}

void MotifScreen::drawSprite(QPainter& painter, const SpriteDocument* sprites, int group, int number, QPointF at,
                             int facing, QPointF scale)
{
    if (!sprites)
        return;
    BgRenderer renderer(sprites, nullptr);
    QPoint axis;
    QImage image = renderer.sprite(group, number, true, &axis);
    if (image.isNull())
        return;
    if (facing < 0)
        image = image.mirrored(true, false);
    const double left = facing < 0 ? image.width() - axis.x() : axis.x();
    painter.save();
    painter.translate(at);
    painter.scale(scale.x(), scale.y());
    painter.drawImage(QPointF(-left, -axis.y()), image);
    painter.restore();
}

QImage MotifScreen::render()
{
    QImage frame(m_size, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::black);
    if (!m_started)
        return frame;
    QPainter p(&frame);
    auto drawBg = [&](BgRenderer* renderer, BgSimulator* simulator, int layer) {
        if (!renderer || !simulator)
            return;
        BgView view;
        view.screen = QSizeF(m_size);
        view.layer = layer;
        for (const BgDrawItem& item : renderer->render(*simulator, view)) {
            p.save();
            if (item.clip)
                p.setClipRect(*item.clip);
            drawBlended(p, QRectF(item.topLeft, item.image.deviceIndependentSize()), item.image, item.mode,
                        item.opacity);
            p.restore();
        }
    };
    // The end screens show over the stage, the character standing at its start.
    auto drawStage = [&] {
        drawBg(m_stageRenderer.get(), m_stageSimulator.get(), 0);
        drawSprite(p, m_character.get(), 0, 0, QPointF(m_size.width() / 2.0 - 70, m_stageZOffset), 1, QPointF(1, 1));
        drawBg(m_stageRenderer.get(), m_stageSimulator.get(), 1);
    };
    int fadeIn = 0;
    int fadeOut = 0;
    int length = 0;
    switch (m_screen) {
    case Screen::Menu: {
        const IniSection* title = section(QStringLiteral("Title Info"));
        drawBg(m_bgRenderer.get(), m_bgSimulator.get(), 0);
        const QPointF pos = pointOf(title, QStringLiteral("menu.pos"));
        const QPointF spacing = pointOf(title, QStringLiteral("menu.item.spacing"), QPointF(0, 13));
        const int visible = std::max(1, intOf(title, QStringLiteral("menu.window.visibleitems"), 5));
        const QPointF margins = pointOf(title, QStringLiteral("menu.window.margins.y"));
        p.save();
        p.setClipRect(
            QRectF(0, pos.y() - margins.x(), m_size.width(), spacing.y() * (visible - 1) + margins.x() + margins.y()));
        for (int i = 0; i < m_items.size(); ++i) {
            const QPointF at = pos + spacing * (i - m_scroll);
            if (i == m_cursor && intOf(title, QStringLiteral("menu.boxcursor.visible"), 1) != 0) {
                const QList<double> box =
                    numbers(title ? title->value(QStringLiteral("menu.boxcursor.coords")) : QString());
                if (box.size() >= 4) {
                    const int alpha = 48 + int(32 * std::sin(m_time * 0.15));
                    p.fillRect(QRectF(at + QPointF(box[0], box[1]), at + QPointF(box[2] + 1, box[3] + 1)),
                               QColor(255, 255, 255, alpha));
                }
            }
            const QString spec = !title          ? QString()
                                 : i == m_cursor ? title->value(QStringLiteral("menu.item.active.font"))
                                                 : title->value(QStringLiteral("menu.item.font"));
            drawText(p, spec, at, m_items[i]);
        }
        p.restore();
        drawBg(m_bgRenderer.get(), m_bgSimulator.get(), 1);
        fadeIn = intOf(title, QStringLiteral("fadein.time"), 0);
        break;
    }
    case Screen::Versus: {
        const IniSection* vs = section(QStringLiteral("VS Screen"));
        drawBg(m_bgRenderer.get(), m_bgSimulator.get(), 0);
        for (int side = 1; side <= 2; ++side) {
            const QString prefix = QStringLiteral("p%1.").arg(side);
            const QPointF spr = pointOf(vs, prefix + QLatin1String("spr"), QPointF(9000, 1));
            drawSprite(p, m_character.get(), int(spr.x()), int(spr.y()), pointOf(vs, prefix + QLatin1String("offset")),
                       intOf(vs, prefix + QLatin1String("facing"), side == 1 ? 1 : -1),
                       pointOf(vs, prefix + QLatin1String("scale"), QPointF(1, 1)));
            drawText(p, vs ? vs->value(prefix + QLatin1String("name.font")) : QString(),
                     pointOf(vs, prefix + QLatin1String("name.offset")), m_characterName);
        }
        QString match = vs ? unquote(vs->value(QStringLiteral("match.text"))) : QString();
        match.replace(QLatin1String("%i"), QStringLiteral("1"));
        drawText(p, vs ? vs->value(QStringLiteral("match.font")) : QString(),
                 pointOf(vs, QStringLiteral("match.offset")), match);
        drawBg(m_bgRenderer.get(), m_bgSimulator.get(), 1);
        fadeIn = intOf(vs, QStringLiteral("fadein.time"), 0);
        fadeOut = intOf(vs, QStringLiteral("fadeout.time"), 0);
        length = intOf(vs, QStringLiteral("time"), 0);
        break;
    }
    case Screen::Continue: {
        // The engine's own screen: "Continue?" with Yes and No over the fight.
        drawStage();
        int fontNumber = m_fonts.isEmpty() ? -1 : m_fonts.firstKey();
        for (auto it = m_fonts.cbegin(); it != m_fonts.cend(); ++it) {
            if (it.value().info.height >= 8 && it.value().info.height <= 10)
                fontNumber = it.key();
        }
        const QPointF pos(m_size.width() / 2.0, 40);
        const QString spec = QStringLiteral("%1,0,0").arg(fontNumber);
        drawText(p, spec, pos, QStringLiteral("Continue?"));
        drawText(p, spec, pos + QPointF(-17, 20), QStringLiteral("Yes"));
        drawText(p, spec, pos + QPointF(15, 20), QStringLiteral("No"));
        p.fillRect(QRectF(pos + QPointF(m_yes ? 5 : -30, 10), QSizeF(26, 14)), QColor(0, 0, 0, 64));
        break;
    }
    case Screen::Victory: {
        const IniSection* victory = section(QStringLiteral("Victory Screen"));
        drawBg(m_bgRenderer.get(), m_bgSimulator.get(), 0);
        const QPointF spr = pointOf(victory, QStringLiteral("p1.spr"), QPointF(9000, 2));
        const QList<double> window = numbers(victory ? victory->value(QStringLiteral("p1.window")) : QString());
        p.save();
        if (window.size() >= 4)
            p.setClipRect(QRectF(QPointF(window[0], window[1]), QPointF(window[2] + 1, window[3] + 1)));
        drawSprite(p, m_character.get(), int(spr.x()), int(spr.y()), pointOf(victory, QStringLiteral("p1.offset")),
                   intOf(victory, QStringLiteral("p1.facing"), 1),
                   pointOf(victory, QStringLiteral("p1.scale"), QPointF(1, 1)));
        p.restore();
        drawText(p, victory ? victory->value(QStringLiteral("p1.name.font")) : QString(),
                 pointOf(victory, QStringLiteral("p1.name.offset")), m_characterName);
        const QString quote = victory ? unquote(victory->value(QStringLiteral("winquote.text"))) : QString();
        const QString spec = victory ? victory->value(QStringLiteral("winquote.font")) : QString();
        const QPointF at = pointOf(victory, QStringLiteral("winquote.offset"));
        const QList<double> quoteWindow =
            numbers(victory ? victory->value(QStringLiteral("winquote.window")) : QString());
        const FightScreen::Font* f = font(int(numbers(spec).value(0, -1)));
        if (f) {
            const double width = quoteWindow.size() >= 4 ? quoteWindow[2] - at.x() : m_size.width() - at.x();
            QStringList lines;
            QString line;
            for (const QString& word : quote.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                const QString next = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
                if (!line.isEmpty() && f->textWidth(next) > width) {
                    lines << line;
                    line = word;
                } else {
                    line = next;
                }
            }
            if (!line.isEmpty())
                lines << line;
            for (int i = 0; i < lines.size(); ++i)
                drawText(p, spec, at + QPointF(0, i * (f->info.height + f->info.spacingY)), lines[i]);
        }
        drawBg(m_bgRenderer.get(), m_bgSimulator.get(), 1);
        fadeIn = intOf(victory, QStringLiteral("fadein.time"), 0);
        fadeOut = intOf(victory, QStringLiteral("fadeout.time"), 0);
        length = intOf(victory, QStringLiteral("time"), 0);
        break;
    }
    case Screen::Win:
    case Screen::SurvivalResults: {
        const bool win = m_screen == Screen::Win;
        const IniSection* s = section(win ? QStringLiteral("Win Screen") : QStringLiteral("Survival Results Screen"));
        const QString prefix = win ? QStringLiteral("wintext.") : QStringLiteral("winstext.");
        drawStage();
        QString text = s ? unquote(s->value(prefix + QLatin1String("text"))) : QString();
        text.replace(QLatin1String("%i"), QStringLiteral("0"));
        drawText(p, s ? s->value(prefix + QLatin1String("font")) : QString(),
                 pointOf(s, prefix + QLatin1String("offset")), text);
        fadeIn = intOf(s, QStringLiteral("fadein.time"), 0);
        fadeOut = intOf(s, QStringLiteral("fadeout.time"), 0);
        length = intOf(s, win ? QStringLiteral("pose.time") : QStringLiteral("show.time"), 0);
        break;
    }
    }
    double fade = 0;
    if (fadeIn > 0 && m_time < fadeIn)
        fade = 1.0 - double(m_time) / fadeIn;
    else if (length > 0 && fadeOut > 0 && m_time >= length - fadeOut)
        fade = std::min(1.0, double(m_time - (length - fadeOut)) / fadeOut);
    if (fade > 0) {
        p.setOpacity(fade);
        p.fillRect(frame.rect(), Qt::black);
    }
    return frame;
}

} // namespace OFS::Mugen

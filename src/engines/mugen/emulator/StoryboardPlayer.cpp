#include "engines/mugen/emulator/StoryboardPlayer.h"

#include "core/bg/BgRenderer.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"
#include "engines/mugen/runtime/Files.h"
#include "engines/mugen/runtime/IniText.h"
#include "formats/audio/AudioCodec.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QTextDocument>

#include <algorithm>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

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

QColor colorOf(const QString& value)
{
    const QList<double> v = numbers(value);
    return QColor(std::clamp(int(v.value(0)), 0, 255), std::clamp(int(v.value(1)), 0, 255),
                  std::clamp(int(v.value(2)), 0, 255));
}

} // namespace

StoryboardPlayer::StoryboardPlayer() = default;
StoryboardPlayer::~StoryboardPlayer() = default;
StoryboardPlayer::StoryboardPlayer(StoryboardPlayer&&) noexcept = default;
StoryboardPlayer& StoryboardPlayer::operator=(StoryboardPlayer&&) noexcept = default;

QString StoryboardPlayer::textOf(const QString& path) const
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

bool StoryboardPlayer::start(Project& project, const QString& defPath, const QString& engineDir, QString* error)
{
    *this = StoryboardPlayer();
    m_project = &project;
    m_path = defPath;
    const QString text = textOf(defPath);
    const IniText ini = IniText::parse(text);
    const IniSection* sceneDef = ini.section(QStringLiteral("SceneDef"));
    if (!sceneDef) {
        if (error)
            *error = QStringLiteral("The storyboard has no [SceneDef].");
        return false;
    }
    const QString folder = QFileInfo(defPath).absolutePath();
    auto resolve = [&](const QString& name) -> QString {
        const QString cleaned = QDir::fromNativeSeparators(unquote(name).trimmed());
        if (cleaned.isEmpty())
            return QString();
        for (const QString& candidate : {QDir(folder).filePath(cleaned), QDir(engineDir).filePath(cleaned),
                                         QDir(engineDir).filePath(QStringLiteral("font/") + cleaned)}) {
            if (QFileInfo(candidate).isFile())
                return QDir::cleanPath(candidate);
        }
        return QString();
    };
    if (const IniSection* info = ini.section(QStringLiteral("Info")); info && info->has(QStringLiteral("localcoord"))) {
        const QList<double> size = numbers(info->value(QStringLiteral("localcoord")));
        if (size.value(0) > 0 && size.value(1) > 0)
            m_size = QSize(int(size[0]), int(size[1]));
    }
    const QString spr = resolve(sceneDef->value(QStringLiteral("spr")));
    if (auto* file = qobject_cast<SpriteFile*>(project.file(spr))) {
        m_sprites = std::shared_ptr<const SpriteDocument>(std::shared_ptr<const SpriteDocument>(), &file->document());
    } else if (const QByteArray data = readBytes(spr); !data.isEmpty()) {
        if (auto doc = Formats::SffDocument::read(span(data)))
            m_sprites = std::make_shared<const SpriteDocument>(SpriteDocument::fromSff(doc.value()));
    }
    const QString snd = resolve(sceneDef->value(QStringLiteral("snd")));
    if (auto* file = qobject_cast<SoundFile*>(project.file(snd))) {
        m_sounds = std::shared_ptr<const SoundDocument>(std::shared_ptr<const SoundDocument>(), &file->document());
    } else if (const QByteArray data = readBytes(snd); !data.isEmpty()) {
        if (auto doc = Formats::SndDocument::read(span(data)))
            m_sounds = std::make_shared<const SoundDocument>(SoundDocument::fromSnd(doc.value()));
    }
    for (int n = 0; n <= 9; ++n) {
        const QString key = QStringLiteral("font%1").arg(n);
        if (sceneDef->has(key)) {
            if (std::optional<FightScreen::Font> font = FightScreen::Font::load(resolve(sceneDef->value(key))))
                m_fonts.insert(n, std::move(*font));
        }
    }
    m_animations = AirDocument::parse(text);
    m_bg = BgDocument::parse(text.split(QLatin1Char('\n')));
    m_bgRenderer = std::make_unique<BgRenderer>(m_sprites.get(), &m_animations);

    // Some values carry on from the scene before.
    std::optional<QColor> clear;
    QPointF layerAll;
    QRect window(QPoint(), m_size);
    for (const IniSection& section : ini.sections()) {
        if (!section.name.startsWith(QLatin1String("Scene "), Qt::CaseInsensitive))
            continue;
        Scene scene;
        scene.name = section.name.mid(6).trimmed();
        auto value = [&section](const QString& key) {
            return section.value(key);
        };
        auto has = [&section](const QString& key) {
            return section.has(key);
        };
        scene.endTime = int(numbers(value(QStringLiteral("end.time"))).value(0));
        scene.fadeInTime = int(numbers(value(QStringLiteral("fadein.time"))).value(0));
        if (has(QStringLiteral("fadein.col")))
            scene.fadeInColor = colorOf(value(QStringLiteral("fadein.col")));
        scene.fadeOutTime = int(numbers(value(QStringLiteral("fadeout.time"))).value(0));
        if (has(QStringLiteral("fadeout.col")))
            scene.fadeOutColor = colorOf(value(QStringLiteral("fadeout.col")));
        if (has(QStringLiteral("clearcolor"))) {
            const QList<double> c = numbers(value(QStringLiteral("clearcolor")));
            clear = c.value(0) < 0 ? std::nullopt : std::optional<QColor>(colorOf(value(QStringLiteral("clearcolor"))));
        }
        scene.clearColor = clear;
        if (has(QStringLiteral("layerall.pos"))) {
            const QList<double> p = numbers(value(QStringLiteral("layerall.pos")));
            layerAll = QPointF(p.value(0), p.value(1));
        }
        scene.layerAllPos = layerAll;
        if (has(QStringLiteral("window"))) {
            const QList<double> w = numbers(value(QStringLiteral("window")));
            window = QRect(QPoint(int(w.value(0)), int(w.value(1))), QPoint(int(w.value(2)), int(w.value(3))));
        }
        scene.window = window;
        for (const IniEntry& entry : section.entries) {
            const QString key = entry.key.toLower();
            if (key.startsWith(QLatin1String("layer")) && !key.startsWith(QLatin1String("layerall"))) {
                const int dot = key.indexOf(QLatin1Char('.'));
                bool ok = false;
                const int number = key.mid(5, dot - 5).toInt(&ok);
                if (!ok || dot < 0)
                    continue;
                Layer& layer = scene.layers[number];
                const QString field = key.mid(dot + 1);
                if (field == QLatin1String("anim")) {
                    layer.anim = int(numbers(entry.value).value(0, -1));
                } else if (field == QLatin1String("text")) {
                    layer.text = unquote(entry.value).replace(QLatin1String("\\n"), QLatin1String("\n"));
                } else if (field == QLatin1String("font")) {
                    const QList<double> f = numbers(entry.value);
                    layer.font = int(f.value(0, -1));
                    layer.bank = int(f.value(1));
                    layer.align = f.size() > 2 ? (f[2] < 0 ? -1 : f[2] > 0 ? 1 : 0) : 1;
                    if (f.size() > 5)
                        layer.color = QColor(std::clamp(int(f[3]), 0, 255), std::clamp(int(f[4]), 0, 255),
                                             std::clamp(int(f[5]), 0, 255));
                } else if (field == QLatin1String("textdelay")) {
                    layer.textDelay = int(numbers(entry.value).value(0));
                } else if (field == QLatin1String("offset")) {
                    const QList<double> o = numbers(entry.value);
                    layer.offset = QPointF(o.value(0), o.value(1));
                } else if (field == QLatin1String("starttime")) {
                    layer.startTime = int(numbers(entry.value).value(0));
                } else if (field == QLatin1String("endtime")) {
                    layer.endTime = int(numbers(entry.value).value(0));
                }
            } else if (key.startsWith(QLatin1String("sound"))) {
                const int dot = key.indexOf(QLatin1Char('.'));
                bool ok = false;
                const int number = key.mid(5, dot - 5).toInt(&ok);
                if (!ok || dot < 0)
                    continue;
                while (scene.sounds.size() <= number)
                    scene.sounds << Sound();
                Sound& sound = scene.sounds[number];
                const QString field = key.mid(dot + 1);
                const QList<double> v = numbers(entry.value);
                if (field == QLatin1String("value")) {
                    sound.group = int(v.value(0, -1));
                    sound.number = int(v.value(1));
                } else if (field == QLatin1String("starttime")) {
                    sound.startTime = int(v.value(0));
                } else if (field == QLatin1String("volumescale")) {
                    sound.volume = float(std::clamp(v.value(0), 0.0, 100.0) / 100);
                } else if (field == QLatin1String("pan")) {
                    sound.pan = std::clamp(int(v.value(0)), -127, 127);
                }
            } else if (key == QLatin1String("bgm")) {
                const QString file = unquote(entry.value).trimmed();
                scene.bgm = file.isEmpty() ? QString() : QDir(folder).filePath(file);
            } else if (key == QLatin1String("bgm.loop")) {
                scene.bgmLoop = numbers(entry.value).value(0) != 0;
            } else if (key == QLatin1String("bg.name")) {
                scene.bgName = unquote(entry.value).trimmed();
            }
        }
        m_scenes << scene;
    }
    if (m_scenes.isEmpty()) {
        if (error)
            *error = QStringLiteral("The storyboard has no scene.");
        return false;
    }
    m_canvas = QImage(m_size, QImage::Format_ARGB32_Premultiplied);
    m_canvas.fill(Qt::black);
    m_started = true;
    const int first = int(numbers(sceneDef->value(QStringLiteral("startscene"))).value(0));
    enterScene(std::clamp(first, 0, int(m_scenes.size()) - 1));
    return true;
}

void StoryboardPlayer::enterScene(int index)
{
    m_scene = index;
    m_time = 0;
    Scene& scene = m_scenes[index];
    for (Layer& layer : scene.layers) {
        layer.player = AnimPlayer();
        const int at = layer.anim >= 0 ? m_animations.indexOf(layer.anim) : -1;
        if (at < 0)
            continue;
        const AirAnimation& animation = m_animations.animations()[at];
        QList<int> times;
        for (const AirFrame& frame : animation.frames)
            times << frame.time;
        layer.player = AnimPlayer(times, animation.loopStartFrame());
    }
    m_bgSimulator.reset();
    if (!scene.bgName.isEmpty()) {
        if (const int group = m_bg.indexOf(scene.bgName); group >= 0)
            m_bgSimulator = std::make_unique<BgSimulator>(m_bg.groups()[group]);
    }
    if (scene.bgm) {
        m_pendingBgm = scene.bgm;
        m_pendingBgmLoop = scene.bgmLoop;
    }
    for (const Sound& sound : std::as_const(scene.sounds)) {
        if (sound.group >= 0 && sound.startTime == 0)
            m_pendingSounds << sound;
    }
}

void StoryboardPlayer::tick()
{
    if (!m_started || m_finished)
        return;
    Scene& scene = m_scenes[m_scene];
    ++m_time;
    if (m_time >= scene.endTime) {
        if (m_scene + 1 < m_scenes.size()) {
            enterScene(m_scene + 1);
        } else {
            m_finished = true;
            m_time = scene.endTime;
        }
        return;
    }
    for (Layer& layer : scene.layers) {
        if (m_time > layer.startTime)
            layer.player.step();
    }
    if (m_bgSimulator)
        m_bgSimulator->step();
    for (const Sound& sound : std::as_const(scene.sounds)) {
        if (sound.group >= 0 && sound.startTime == m_time)
            m_pendingSounds << sound;
    }
}

void StoryboardPlayer::playSounds(AudioMixer& mixer)
{
    for (const Sound& s : std::as_const(m_pendingSounds)) {
        const QPair<int, int> key(s.group, s.number);
        auto cached = m_soundCache.constFind(key);
        if (cached == m_soundCache.cend()) {
            std::shared_ptr<const AudioMixer::Sound> prepared;
            if (m_sounds) {
                if (const int at = m_sounds->find(s.group, s.number); at >= 0) {
                    if (auto decoded = SoundEdits::decode(m_sounds->sounds()[at]))
                        prepared = AudioMixer::prepare(decoded.value().buffer);
                }
            }
            cached = m_soundCache.insert(key, prepared);
        }
        if (cached.value())
            mixer.play(cached.value(), Sounds, -1, s.volume, s.pan / 127.0f);
    }
    m_pendingSounds.clear();
    if (m_pendingBgm) {
        mixer.stop(Music, -1);
        if (!m_pendingBgm->isEmpty()) {
            if (auto music = Formats::loadAudio(m_pendingBgm->toStdWString()))
                mixer.play(AudioMixer::prepare(music.value().buffer), Music, 0, 1, 0, m_pendingBgmLoop);
        }
        m_pendingBgm.reset();
    }
}

QImage StoryboardPlayer::render()
{
    if (!m_started)
        return m_canvas;
    const Scene& scene = m_scenes[m_scene];
    QPainter p(&m_canvas);
    if (scene.clearColor)
        p.fillRect(m_canvas.rect(), *scene.clearColor);
    auto drawBg = [&](int layer) {
        if (!m_bgSimulator)
            return;
        BgView view;
        view.screen = QSizeF(m_size);
        view.layer = layer;
        for (const BgDrawItem& item : m_bgRenderer->render(*m_bgSimulator, view)) {
            p.save();
            p.setOpacity(item.opacity);
            p.setCompositionMode(item.mode);
            if (item.clip)
                p.setClipRect(*item.clip);
            p.drawImage(item.topLeft, item.image);
            p.restore();
        }
    };
    drawBg(0);
    p.save();
    p.setClipRect(scene.window);
    for (const Layer& layer : scene.layers) {
        const int end = layer.endTime >= 0 ? layer.endTime : scene.endTime;
        if (m_time < layer.startTime || m_time >= end)
            continue;
        const QPointF at = scene.layerAllPos + layer.offset;
        if (layer.anim >= 0) {
            const int index = m_animations.indexOf(layer.anim);
            if (index < 0 || layer.player.isEmpty())
                continue;
            const AirAnimation& animation = m_animations.animations()[index];
            const AirFrame& f = animation.frames[std::min(layer.player.frame(), int(animation.frames.size()) - 1)];
            if (f.group < 0)
                continue;
            QPoint axis;
            QImage image = m_bgRenderer->sprite(f.group, f.number, true, &axis);
            if (image.isNull())
                continue;
            if (f.flipH || f.flipV)
                image = image.mirrored(f.flipH, f.flipV);
            const double left = f.x - (f.flipH ? image.width() - axis.x() : axis.x());
            const double top = f.y - (f.flipV ? image.height() - axis.y() : axis.y());
            p.save();
            if (f.transparency.mode != AirTransparency::Mode::None) {
                p.setCompositionMode(f.transparency.mode == AirTransparency::Mode::Subtraction
                                         ? QPainter::CompositionMode_Difference
                                         : QPainter::CompositionMode_Plus);
                if (f.transparency.mode == AirTransparency::Mode::Addition)
                    p.setOpacity(std::clamp(f.transparency.source / 256.0, 0.0, 1.0));
            }
            p.drawImage(at + QPointF(left, top), image);
            p.restore();
        } else if (const auto font = m_fonts.constFind(layer.font); font != m_fonts.cend() && !layer.text.isEmpty()) {
            // Text is typed one letter every textdelay ticks.
            QString shown = layer.text;
            if (layer.textDelay > 0)
                shown = shown.left((m_time - layer.startTime) / layer.textDelay + 1);
            const auto key = qMakePair(&font.value(), layer.bank);
            auto image = m_fontImages.constFind(key);
            if (image == m_fontImages.cend())
                image = m_fontImages.insert(key, font->render(layer.bank));
            QImage glyphs = image.value();
            if (layer.color != QColor(255, 255, 255)) {
                QPainter tint(&glyphs);
                tint.setCompositionMode(QPainter::CompositionMode_Multiply);
                tint.fillRect(glyphs.rect(), layer.color);
                tint.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                tint.drawImage(0, 0, image.value());
            }
            const QStringList lines = shown.split(QLatin1Char('\n'));
            const double lineHeight = font->info.height + font->info.spacingY;
            for (int i = 0; i < lines.size(); ++i)
                font->draw(p, glyphs, at + QPointF(0, i * lineHeight), layer.align, QPointF(1, 1), lines[i],
                           layer.bank);
        }
    }
    p.restore();
    drawBg(1);
    p.end();
    // The fades go over the frame only, because the canvas may be drawn on again without clearing.
    QImage frame = m_canvas;
    double fade = 0;
    QColor fadeColor;
    if (scene.fadeInTime > 0 && m_time < scene.fadeInTime) {
        fade = 1.0 - double(m_time) / scene.fadeInTime;
        fadeColor = scene.fadeInColor;
    } else if (scene.fadeOutTime > 0 && m_time >= scene.endTime - scene.fadeOutTime) {
        fade = double(m_time - (scene.endTime - scene.fadeOutTime)) / scene.fadeOutTime;
        fadeColor = scene.fadeOutColor;
    }
    if (fade > 0) {
        QPainter f(&frame);
        f.setOpacity(std::clamp(fade, 0.0, 1.0));
        f.fillRect(frame.rect(), fadeColor);
    }
    return frame;
}

} // namespace OFS::Mugen

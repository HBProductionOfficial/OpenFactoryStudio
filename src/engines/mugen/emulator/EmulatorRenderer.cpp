#include "engines/mugen/emulator/EmulatorRenderer.h"

#include "core/bg/BgRenderer.h"
#include "engines/mugen/emulator/EmulatorSession.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <functional>

namespace OFS::Mugen {

using namespace Runtime;

namespace {

struct Blend
{
    QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver;
    qreal opacity = 1;
};

Blend blendOf(const AirTransparency& frame, const DrawState* draw)
{
    Blend b;
    switch (frame.mode) {
    case AirTransparency::Mode::Addition:
        b.mode = QPainter::CompositionMode_Plus;
        b.opacity = std::clamp(frame.source / 256.0, 0.0, 1.0);
        break;
    case AirTransparency::Mode::Addition50:
        b.mode = QPainter::CompositionMode_Plus;
        b.opacity = 0.5;
        break;
    case AirTransparency::Mode::Subtraction:
        b.mode = QPainter::CompositionMode_Difference;
        break;
    default:
        break;
    }
    if (draw && draw->trans != QLatin1String("default")) {
        const QString& t = draw->trans;
        if (t == QLatin1String("none")) {
            b = Blend();
        } else if (t == QLatin1String("sub")) {
            b.mode = QPainter::CompositionMode_Difference;
            b.opacity = 1;
        } else if (t == QLatin1String("addalpha") && draw->alpha[1] < 256) {
            b.mode = QPainter::CompositionMode_SourceOver;
            b.opacity = std::clamp(draw->alpha[0] / 256.0, 0.0, 1.0);
        } else {
            b.mode = QPainter::CompositionMode_Plus;
            b.opacity = std::clamp(draw->alpha[0] / 256.0, 0.0, 1.0);
        }
    }
    return b;
}

} // namespace

EmulatorRenderer::EmulatorRenderer() = default;
EmulatorRenderer::~EmulatorRenderer() = default;

void EmulatorRenderer::clearCache()
{
    m_cache.clear();
    if (m_stage)
        m_stage->clearCache();
}

const EmulatorRenderer::Sprite* EmulatorRenderer::sprite(const SpriteDocument* sprites, int group, int number)
{
    if (!sprites)
        return nullptr;
    QHash<qint64, Sprite>& cache = m_cache[sprites];
    const qint64 key = (qint64(group) << 32) | quint32(number);
    auto found = cache.constFind(key);
    if (found == cache.cend()) {
        Sprite entry;
        const int index = sprites->findSprite(group, number);
        if (index >= 0) {
            const Formats::Image rgba = sprites->imageRgba(index);
            if (!rgba.pixelBuffer().empty())
                entry.image =
                    QImage(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888)
                        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
            const auto& header = sprites->sprites()[index].header;
            entry.axis = QPoint(header.axisX, header.axisY);
        }
        found = cache.insert(key, entry);
    }
    return &found.value();
}

QImage EmulatorRenderer::render(const EmulatorSession& session, bool debugInfo)
{
    QImage frame(Screen, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::black);
    if (!session.isStarted())
        return frame;
    const Match& match = session.match();
    const EmulatorSession::Stage& stage = session.stage();
    if (m_stageKey != &stage || !m_stage) {
        m_stage = std::make_unique<BgRenderer>(stage.sprites.get(), &stage.animations);
        m_stageKey = &stage;
    }

    QPainter p(&frame);
    const EnvShake& shake = match.envShake();
    if (shake.time > 0)
        p.translate(0, shake.ampl * std::sin(qDegreesToRadians(shake.phase + shake.freq * match.gameTime())));
    const float camera = match.cameraX();
    auto toScreen = [&](Vec2 pos) {
        return QPointF(pos.x - camera + Screen.width() / 2.0, pos.y + stage.zoffset);
    };

    auto drawStage = [&](int layer) {
        if (!stage.simulator)
            return;
        BgView view;
        view.screen = QSizeF(Screen);
        view.camera = QPointF(camera, 0);
        view.layer = layer;
        for (const BgDrawItem& item : m_stage->render(*stage.simulator, view)) {
            p.save();
            p.setOpacity(item.opacity);
            p.setCompositionMode(item.mode);
            if (item.clip)
                p.setClipRect(*item.clip);
            p.drawImage(item.topLeft, item.image);
            p.restore();
        }
    };

    auto drawFrame = [&](const SpriteDocument* sprites, const AirAnimation* animation, const AnimPlayer& player,
                         QPointF at, int facing, int vfacing, const DrawState* draw, QPointF scale) {
        if (!animation || player.isEmpty() || player.frame() >= animation->frames.size())
            return;
        const AirFrame& f = animation->frames[player.frame()];
        if (f.group < 0)
            return;
        const Sprite* s = sprite(sprites, f.group, f.number);
        if (!s || s->image.isNull())
            return;
        const bool flipH = f.flipH != (facing < 0);
        const bool flipV = f.flipV != (vfacing < 0);
        const QImage image = flipH || flipV ? s->image.mirrored(flipH, flipV) : s->image;
        if (draw)
            at += QPointF(draw->offset.x, draw->offset.y);
        const double left = f.x * facing - (flipH ? image.width() - s->axis.x() : s->axis.x());
        const double top = f.y - (flipV ? image.height() - s->axis.y() : s->axis.y());
        const Blend blend = blendOf(f.transparency, draw);
        p.save();
        p.setCompositionMode(blend.mode);
        p.setOpacity(blend.opacity);
        p.translate(at);
        double angle = f.angle;
        QPointF scaled(scale.x() * f.xScale, scale.y() * f.yScale);
        if (draw && draw->angleDraw) {
            angle += draw->angle;
            scaled = QPointF(scaled.x() * draw->angleScale.x, scaled.y() * draw->angleScale.y);
        }
        if (angle != 0)
            p.rotate(-angle * facing);
        p.scale(scaled.x(), scaled.y());
        p.drawImage(QPointF(left, top), image);
        p.restore();
    };

    drawStage(0);
    if (match.superPauseTime() > 0 && match.superDarken())
        p.fillRect(QRect(QPoint(), Screen), QColor(0, 0, 0, 128));
    const EnvColor& color = match.envColor();
    const QColor fill(color.color[0], color.color[1], color.color[2]);
    if (color.time != 0 && color.under)
        p.fillRect(QRect(QPoint(), Screen), fill);

    // Creation order at equal sprite priority.
    struct Item
    {
        int priority;
        int order;
        std::function<void()> draw;
    };
    QList<Item> items;
    int order = 0;
    for (const Character* c : match.characters()) {
        if (c->destroyed())
            continue;
        const SpriteDocument* sprites = session.spritesOf(*c);
        if (!c->isAsserted(QStringLiteral("invisible"))) {
            items << Item{c->sprPriority(), order++, [&, c, sprites] {
                              const AirAnimation* animation = c->animationData().animation(c->animNo());
                              drawFrame(sprites, animation, c->anim(),
                                        toScreen(Vec2{float(c->pos().x()), float(c->pos().y())}), c->facing(), 1,
                                        &c->draw(), QPointF(1, 1));
                          }};
        }
        for (const Explod& e : c->explods()) {
            const SpriteDocument* explodSprites = e.common ? session.fightFxSprites() : session.spritesOf(*c);
            const int priority = e.onTop ? 1000 : e.sprPriority;
            items << Item{priority, order++, [&, e, explodSprites] {
                              const AirAnimation* animation = e.animData ? e.animData->animation(e.animNo) : nullptr;
                              const QPointF at =
                                  e.screen ? QPointF(e.drawPos().x, e.drawPos().y) : toScreen(e.drawPos());
                              drawFrame(explodSprites, animation, e.anim, at, e.drawFacing(), e.vFacing, nullptr,
                                        QPointF(e.scale.x, e.scale.y));
                          }};
        }
        for (const Projectile& pr : c->projectiles()) {
            const SpriteDocument* projSprites = pr.shown.common ? session.fightFxSprites() : sprites;
            items << Item{pr.sprPriority, order++, [&, pr, projSprites] {
                              const AirAnimation* animation =
                                  pr.animData ? pr.animData->animation(pr.shown.number) : nullptr;
                              drawFrame(projSprites, animation, pr.player, toScreen(pr.pos), pr.facing, 1, nullptr,
                                        QPointF(pr.scale.x, pr.scale.y));
                          }};
        }
    }
    for (const EmulatorSession::Effect& effect : session.effects()) {
        const Character* owner = match.playerById(effect.playerId);
        const CharacterData* data = effect.common ? session.fightFx() : owner ? &owner->data() : nullptr;
        const SpriteDocument* sprites = effect.common ? session.fightFxSprites()
                                        : owner       ? session.spritesOf(*owner)
                                                      : nullptr;
        items << Item{effect.under ? -1000 : 500, order++, [&, effect, data, sprites] {
                          drawFrame(sprites, data ? data->animation(effect.anim) : nullptr, effect.player,
                                    toScreen(effect.pos), 1, 1, nullptr, QPointF(1, 1));
                      }};
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.priority < b.priority; });
    for (const Item& item : std::as_const(items))
        item.draw();

    drawStage(1);
    if (color.time != 0 && !color.under)
        p.fillRect(QRect(QPoint(), Screen), fill);

    if (debugInfo) {
        p.resetTransform();
        p.setPen(Qt::white);
        QFont font = p.font();
        font.setPixelSize(8);
        p.setFont(font);
        for (const Character* c : match.characters()) {
            if (c->destroyed())
                continue;
            const QPointF at = toScreen(Vec2{float(c->pos().x()), float(c->pos().y())});
            p.drawText(at + QPointF(-20, 10),
                       QStringLiteral("%1 S%2 A%3").arg(c->id()).arg(c->stateNo()).arg(c->animNo()));
        }
    }
    return frame;
}

} // namespace OFS::Mugen

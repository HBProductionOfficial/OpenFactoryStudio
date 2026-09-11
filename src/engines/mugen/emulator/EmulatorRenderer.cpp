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

// How the engine blends: drawn over, or added to or subtracted from what is there, each side weighted
// out of 256 ("AS256D128"). Drawn over with a source weight under 256, the image is see-through.
struct Blend
{
    enum class Kind
    {
        Normal,
        Add,
        Sub,
    };
    Kind kind = Kind::Normal;
    int src = 256;
    int dst = 256;
};

Blend blendOf(const AirTransparency& frame, const QString& trans = QStringLiteral("default"),
              const std::array<int, 2>& alpha = {256, 0})
{
    Blend b;
    switch (frame.mode) {
    case AirTransparency::Mode::Addition:
        b = {Blend::Kind::Add, frame.source, frame.dest};
        break;
    case AirTransparency::Mode::Addition50:
        b = {Blend::Kind::Add, 256, 128};
        break;
    case AirTransparency::Mode::Subtraction:
        b = {Blend::Kind::Sub, 256, 256};
        break;
    case AirTransparency::Mode::None:
        break;
    }
    if (trans == QLatin1String("default") || trans.isEmpty())
        return b;
    if (trans == QLatin1String("none"))
        return Blend();
    if (trans == QLatin1String("sub"))
        return {Blend::Kind::Sub, 256, 256};
    if (trans == QLatin1String("add1"))
        return {Blend::Kind::Add, 256, 128};
    if (trans == QLatin1String("addalpha") || trans == QLatin1String("add_alpha"))
        return {Blend::Kind::Add, std::clamp(alpha[0], 0, 256), std::clamp(alpha[1], 0, 256)};
    return {Blend::Kind::Add, 256, 256};
}

struct ColorFx
{
    float color = 1;
    bool invert = false;
    std::array<float, 3> mul{1, 1, 1};
    std::array<float, 3> add{0, 0, 0};
};

// PalFX: (colour + add + sinadd's wave) * mul / 256, after the colour level and the inversion.
void addFx(QList<ColorFx>& list, const PalFx& p)
{
    if (!p.active())
        return;
    ColorFx fx;
    fx.color = std::clamp(p.color / 256.0f, 0.0f, 1.0f);
    fx.invert = p.invertAll;
    const double wave = p.sinPeriod > 0 ? std::sin(2 * M_PI * p.elapsed / p.sinPeriod) : 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        fx.mul[i] = std::max(0, p.mul[i]) / 256.0f;
        fx.add[i] = float((p.add[i] + p.sinAdd[i] * wave) * fx.mul[i]);
    }
    list << fx;
}

// The k-th after-image: (colour + palbright) * palcontrast / 256 + palpostbright, then k times
// (colour + paladd) * palmul.
ColorFx afterImageFx(const AfterImage& a, int k)
{
    ColorFx fx;
    fx.color = std::clamp(a.palColor / 256.0f, 0.0f, 1.0f);
    fx.invert = a.palInvertAll;
    for (std::size_t i = 0; i < 3; ++i) {
        fx.mul[i] = std::max(0, a.palContrast[i]) / 256.0f;
        fx.add[i] = a.palBright[i] * fx.mul[i] + a.palPostBright[i];
        for (int n = 0; n < k; ++n) {
            fx.add[i] = (fx.add[i] + a.palAdd[i]) * a.palMul[i];
            fx.mul[i] *= a.palMul[i];
        }
    }
    return fx;
}

QImage withFx(const QImage& source, const QList<ColorFx>& effects)
{
    if (effects.isEmpty() || source.isNull())
        return source;
    QImage image = source.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb px = line[x];
            if (qAlpha(px) == 0)
                continue;
            float c[3] = {float(qRed(px)), float(qGreen(px)), float(qBlue(px))};
            for (const ColorFx& fx : effects) {
                if (fx.color < 1) {
                    const float grey = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
                    for (float& v : c)
                        v = grey + (v - grey) * fx.color;
                }
                for (std::size_t i = 0; i < 3; ++i) {
                    float v = fx.invert ? 255 - c[i] : c[i];
                    c[i] = std::clamp(v * fx.mul[i] + fx.add[i], 0.0f, 255.0f);
                }
            }
            line[x] = qRgba(int(c[0] + 0.5f), int(c[1] + 0.5f), int(c[2] + 0.5f), qAlpha(px));
        }
    }
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

// The engine's weighted additions and subtractions are drawn alone on a layer and mixed in by hand.
class Canvas
{
public:
    explicit Canvas(QImage& frame)
        : m_frame(frame)
        , m_painter(&frame)
    {}

    QPainter& painter() { return m_painter; }
    void setBase(const QTransform& base)
    {
        m_base = base;
        m_painter.setTransform(base);
    }

    void draw(const QImage& image, const QTransform& transform, const Blend& blend,
              const std::optional<QRectF>& clip = std::nullopt)
    {
        if (image.isNull())
            return;
        const QTransform full = transform * m_base;
        const bool byHand = blend.kind == Blend::Kind::Sub || (blend.kind == Blend::Kind::Add && blend.dst != 256);
        if (!byHand) {
            m_painter.save();
            if (clip)
                m_painter.setClipRect(*clip);
            m_painter.setTransform(full);
            if (blend.kind == Blend::Kind::Add) {
                m_painter.setCompositionMode(QPainter::CompositionMode_Plus);
                m_painter.setOpacity(std::clamp(blend.src / 256.0, 0.0, 1.0));
            }
            m_painter.drawImage(QPointF(), image);
            m_painter.restore();
            return;
        }
        QImage layer(m_frame.size(), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        {
            QPainter lp(&layer);
            lp.setTransform(m_base);
            if (clip)
                lp.setClipRect(*clip);
            lp.setTransform(full);
            lp.drawImage(QPointF(), image);
        }
        const QRect area = full.mapRect(QRectF(image.rect())).toAlignedRect().adjusted(-1, -1, 1, 1) & m_frame.rect();
        m_painter.end();
        const bool sub = blend.kind == Blend::Kind::Sub;
        for (int y = area.top(); y <= area.bottom(); ++y) {
            auto* d = reinterpret_cast<QRgb*>(m_frame.scanLine(y));
            const auto* s = reinterpret_cast<const QRgb*>(layer.constScanLine(y));
            for (int x = area.left(); x <= area.right(); ++x) {
                const int a = qAlpha(s[x]);
                if (a == 0)
                    continue;
                auto mix = [&](int dc, int sc) {
                    const int kept = dc - dc * (256 - blend.dst) * a / (256 * 255);
                    const int v = sub ? kept - sc * blend.src / 256 : kept + sc * blend.src / 256;
                    return std::clamp(v, 0, 255);
                };
                d[x] =
                    qRgb(mix(qRed(d[x]), qRed(s[x])), mix(qGreen(d[x]), qGreen(s[x])), mix(qBlue(d[x]), qBlue(s[x])));
            }
        }
        m_painter.begin(&m_frame);
        m_painter.setTransform(m_base);
    }

private:
    QImage& m_frame;
    QPainter m_painter;
    QTransform m_base;
};

} // namespace

EmulatorRenderer::EmulatorRenderer() = default;
EmulatorRenderer::~EmulatorRenderer() = default;

void EmulatorRenderer::clearCache()
{
    m_cache.clear();
    m_fonts.clear();
    if (m_stage)
        m_stage->clearCache();
}

const EmulatorRenderer::Sprite* EmulatorRenderer::sprite(const SpriteDocument* sprites, int group, int number,
                                                         const Formats::Palette* palette)
{
    if (!sprites)
        return nullptr;
    QHash<SpriteKey, Sprite>& cache = m_cache[sprites];
    const SpriteKey key{group, number, palette};
    auto found = cache.constFind(key);
    if (found == cache.cend()) {
        Sprite entry;
        const int index = sprites->findSprite(group, number);
        if (index >= 0) {
            const Formats::Image rgba = sprites->imageRgba(index, palette);
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

const QImage& EmulatorRenderer::fontImage(const FightScreen::Font& font, int bank)
{
    const auto key = qMakePair(&font, bank);
    auto found = m_fonts.constFind(key);
    if (found == m_fonts.cend())
        found = m_fonts.insert(key, font.render(bank));
    return found.value();
}

QImage EmulatorRenderer::render(const EmulatorSession& session, const Options& options)
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

    Canvas canvas(frame);
    QPainter& p = canvas.painter();
    const EnvShake& shake = match.envShake();
    if (shake.time > 0) {
        canvas.setBase(QTransform::fromTranslate(
            0, shake.ampl * std::sin(qDegreesToRadians(shake.phase + shake.freq * match.gameTime()))));
    }
    const float camera = match.cameraX();
    auto toScreen = [&](Vec2 pos) {
        return QPointF(pos.x - camera + Screen.width() / 2.0, pos.y + stage.zoffset);
    };
    // AllPalFX colours everything. BGPalFX colours the stage too.
    QList<ColorFx> allFx;
    addFx(allFx, match.allPalFx());
    QList<ColorFx> stageFx;
    addFx(stageFx, match.bgPalFx());
    stageFx << allFx;

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
            p.drawImage(item.topLeft, withFx(item.image, stageFx));
            p.restore();
        }
    };

    struct Look
    {
        int facing = 1;
        int vfacing = 1;
        QPointF scale{1, 1};
        double angle = 0;
        QPointF angleScale{1, 1};
        const Formats::Palette* palette = nullptr;
        QList<ColorFx> fx;
        std::optional<Blend> blend;
        QString trans = QStringLiteral("default");
        std::array<int, 2> alpha{256, 0};
    };
    auto drawFrame = [&](const SpriteDocument* sprites, const AirAnimation* animation, int frameIndex, QPointF at,
                         const Look& look) {
        if (!animation || frameIndex < 0 || frameIndex >= animation->frames.size())
            return;
        const AirFrame& f = animation->frames[frameIndex];
        if (f.group < 0)
            return;
        const Sprite* s = sprite(sprites, f.group, f.number, look.palette);
        if (!s || s->image.isNull())
            return;
        const bool flipH = f.flipH != (look.facing < 0);
        const bool flipV = f.flipV != (look.vfacing < 0);
        const QImage image = withFx(flipH || flipV ? s->image.mirrored(flipH, flipV) : s->image, look.fx);
        const double left = f.x * look.facing - (flipH ? image.width() - s->axis.x() : s->axis.x());
        const double top = f.y - (flipV ? image.height() - s->axis.y() : s->axis.y());
        QTransform t;
        t.translate(at.x(), at.y());
        const double angle = f.angle + look.angle;
        if (angle != 0)
            t.rotate(-angle * look.facing);
        t.scale(look.scale.x() * f.xScale * look.angleScale.x(), look.scale.y() * f.yScale * look.angleScale.y());
        t.translate(left, top);
        canvas.draw(image, t, look.blend ? *look.blend : blendOf(f.transparency, look.trans, look.alpha));
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
        const DrawState& draw = c->draw();
        QList<ColorFx> ownFx;
        addFx(ownFx, draw.palFx);
        ownFx << allFx;
        auto paletteOf = [&session, c](const AirAnimation* animation, int frame) -> const Formats::Palette* {
            if (!animation || frame < 0 || frame >= animation->frames.size())
                return nullptr;
            const AirFrame& f = animation->frames[frame];
            return session.paletteOf(*c, f.group, f.number);
        };
        const AfterImage& ai = draw.afterImage;
        const QList<AfterImageFrame>& history = c->afterImages();
        const int gap = std::max(1, ai.frameGap);
        for (int i = int((history.size() - 1) / gap) * gap; i >= 0 && !history.isEmpty(); i -= gap) {
            const AfterImageFrame h = history[i];
            const int k = i / gap;
            items << Item{c->sprPriority(), order++, [&, sprites, h, k, ai, paletteOf] {
                              const AirAnimation* animation = h.animData ? h.animData->animation(h.animNo) : nullptr;
                              Look look;
                              look.facing = h.facing;
                              if (h.angleDraw) {
                                  look.angle = h.angle;
                                  look.angleScale = QPointF(h.angleScale.x, h.angleScale.y);
                              }
                              look.palette = paletteOf(animation, h.frame);
                              look.fx = {afterImageFx(ai, k)};
                              look.fx << allFx;
                              look.blend = blendOf(AirTransparency(), ai.trans);
                              drawFrame(sprites, animation, h.frame, toScreen(h.pos), look);
                          }};
        }
        if (!c->isAsserted(QStringLiteral("invisible"))) {
            items << Item{c->sprPriority(), order++, [&, c, sprites, ownFx, paletteOf] {
                              const AirAnimation* animation = c->animationData().animation(c->animNo());
                              const DrawState& d = c->draw();
                              Look look;
                              look.facing = c->facing();
                              if (d.angleDraw) {
                                  look.angle = d.angle;
                                  look.angleScale = QPointF(d.angleScale.x, d.angleScale.y);
                              }
                              look.palette = paletteOf(animation, c->anim().frame());
                              look.fx = ownFx;
                              look.trans = d.trans;
                              look.alpha = d.alpha;
                              const QPointF at =
                                  toScreen(Vec2{float(c->pos().x()) + d.offset.x, float(c->pos().y()) + d.offset.y});
                              drawFrame(sprites, animation, c->anim().isEmpty() ? -1 : c->anim().frame(), at, look);
                          }};
        }
        for (const Explod& e : c->explods()) {
            const SpriteDocument* explodSprites = e.common ? session.fightFxSprites() : session.spritesOf(*c);
            const int priority = e.onTop ? 1000 : e.sprPriority;
            QList<ColorFx> explodFx;
            if (!e.ownPal)
                addFx(explodFx, draw.palFx);
            explodFx << allFx;
            items << Item{priority, order++, [&, c, e, explodSprites, explodFx] {
                              const AirAnimation* animation = e.animData ? e.animData->animation(e.animNo) : nullptr;
                              const QPointF at =
                                  e.screen ? QPointF(e.drawPos().x, e.drawPos().y) : toScreen(e.drawPos());
                              Look look;
                              look.facing = e.drawFacing();
                              look.vfacing = e.vFacing;
                              look.scale = QPointF(e.scale.x, e.scale.y);
                              const int frame = e.anim.isEmpty() ? -1 : e.anim.frame();
                              if (!e.common && animation && frame >= 0 && frame < animation->frames.size()) {
                                  const AirFrame& f = animation->frames[frame];
                                  look.palette = session.paletteOf(*c, f.group, f.number);
                              }
                              look.fx = explodFx;
                              look.trans = e.trans;
                              look.alpha = e.alpha;
                              drawFrame(explodSprites, animation, frame, at, look);
                          }};
        }
        for (const Projectile& pr : c->projectiles()) {
            const SpriteDocument* projSprites = pr.shown.common ? session.fightFxSprites() : sprites;
            items << Item{pr.sprPriority, order++, [&, c, pr, projSprites, ownFx] {
                              const AirAnimation* animation =
                                  pr.animData ? pr.animData->animation(pr.shown.number) : nullptr;
                              Look look;
                              look.facing = pr.facing;
                              look.scale = QPointF(pr.scale.x, pr.scale.y);
                              const int frame = pr.player.isEmpty() ? -1 : pr.player.frame();
                              if (!pr.shown.common && animation && frame >= 0 && frame < animation->frames.size()) {
                                  const AirFrame& f = animation->frames[frame];
                                  look.palette = session.paletteOf(*c, f.group, f.number);
                              }
                              look.fx = ownFx;
                              drawFrame(projSprites, animation, frame, toScreen(pr.pos), look);
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
                          Look look;
                          look.fx = allFx;
                          drawFrame(sprites, data ? data->animation(effect.anim) : nullptr,
                                    effect.player.isEmpty() ? -1 : effect.player.frame(), toScreen(effect.pos), look);
                      }};
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.priority < b.priority; });
    for (const Item& item : std::as_const(items))
        item.draw();

    drawStage(1);
    if (color.time != 0 && !color.under)
        p.fillRect(QRect(QPoint(), Screen), fill);

    canvas.setBase(QTransform());
    for (const FightScreen::Item& item : session.fightScreen().items(match, session.sides())) {
        if (item.font) {
            p.save();
            if (item.clip)
                p.setClipRect(*item.clip);
            item.font->draw(p, withFx(fontImage(*item.font, item.bank), allFx), item.pos, item.align, item.scale,
                            item.text, item.bank);
            p.restore();
        } else if (const Sprite* s = sprite(item.sprites, item.group, item.number); s && !s->image.isNull()) {
            const QImage image =
                withFx(item.flipH || item.flipV ? s->image.mirrored(item.flipH, item.flipV) : s->image, allFx);
            QTransform t;
            t.translate(item.pos.x(), item.pos.y());
            t.scale(item.scale.x(), item.scale.y());
            t.translate(-(item.flipH ? image.width() - s->axis.x() : s->axis.x()),
                        -(item.flipV ? image.height() - s->axis.y() : s->axis.y()));
            canvas.draw(image, t, blendOf(item.transparency), item.clip);
        }
    }

    if (options.attackBoxes || options.collisionBoxes) {
        p.save();
        canvas.setBase(QTransform());
        for (const Character* c : match.characters()) {
            if (c->destroyed())
                continue;
            for (int type : {2, 1}) {
                if ((type == 1 && !options.attackBoxes) || (type == 2 && !options.collisionBoxes))
                    continue;
                const QColor line = type == 1 ? options.attackColor : options.collisionColor;
                QColor area = line;
                area.setAlpha(64);
                for (const QRectF& box : c->boxes(type)) {
                    const QPointF topLeft = toScreen(Vec2{float(box.left()), float(box.top())});
                    const QRectF onScreen(topLeft, box.size());
                    p.fillRect(onScreen, area);
                    p.setPen(line);
                    p.drawRect(onScreen.adjusted(0, 0, -1, -1));
                }
            }
        }
        p.restore();
    }

    if (options.debugInfo) {
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

#include "engines/mugen/emulator/EmulatorRenderer.h"

#include "core/app/ImageCache.h"
#include "core/bg/BgRenderer.h"
#include "core/sprites/QtImages.h"
#include "core/widgets/Blending.h"
#include "engines/mugen/emulator/EmulatorSession.h"

#include <QPainter>
#include <QPolygonF>
#include <QtMath>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>

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

QImage silhouette(const QImage& image, const QColor& color)
{
    QImage shape = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&shape);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(shape.rect(), color);
    return shape;
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
            } else if (blend.src < 256) {
                m_painter.setOpacity(std::max(0.0, blend.src / 256.0));
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
    m_stage.reset();
    m_stageKey = nullptr;
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
                entry.image = toQImage(rgba, QImage::Format_ARGB32_Premultiplied);
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
    qsizetype cached = 0;
    for (const auto& sprites : std::as_const(m_cache))
        cached += sprites.size();
    if (ImageCache::full(cached))
        m_cache.clear();
    QImage frame(Screen, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::black);
    if (!session.isStarted())
        return frame;
    const Match& match = session.match();
    const EmulatorSession::Stage& stage = session.stage();
    // Keyed by the sprites because a restarted session keeps its Stage object but loads new documents
    // into it. Keying by the Stage left the renderer on freed sprites.
    if (m_stageKey != stage.sprites.get() || !m_stage) {
        m_stage = std::make_unique<BgRenderer>(stage.sprites.get(), &stage.animations);
        m_stageKey = stage.sprites.get();
    }

    Canvas canvas(frame);
    QPainter& p = canvas.painter();
    const EnvShake& shake = match.envShake();
    if (shake.time > 0) {
        canvas.setBase(QTransform::fromTranslate(
            0, shake.ampl * std::sin(qDegreesToRadians(shake.phase + shake.freq * match.gameTime()))));
    }
    const Match::View& view = match.view();
    const QTransform world = QTransform::fromTranslate(-view.x, -view.y) *
                             QTransform::fromScale(view.scale, view.scale) *
                             QTransform::fromTranslate(Screen.width() / 2.0, stage.zoffset);
    const QTransform toWorld = world.inverted();
    auto toScreen = [&world](Vec2 pos) {
        return world.map(QPointF(pos.x, pos.y));
    };
    // AllPalFX colours everything. BGPalFX colours the stage too.
    QList<ColorFx> allFx;
    addFx(allFx, match.allPalFx());
    QList<ColorFx> stageFx;
    addFx(stageFx, match.bgPalFx());
    stageFx << allFx;

    auto drawStage = [&](int layer) {
        if (!stage.simulator || match.isAsserted(layer == 0 ? QStringLiteral("nobg") : QStringLiteral("nofg")))
            return;
        BgView bgView;
        bgView.screen = QSizeF(Screen);
        bgView.camera = QPointF(view.x, view.y);
        bgView.zoom = view.scale;
        bgView.ground = stage.zoffset;
        bgView.layer = layer;
        for (const BgDrawItem& item : m_stage->render(*stage.simulator, bgView)) {
            p.save();
            if (item.clip)
                p.setClipRect(*item.clip);
            const QImage image = withFx(item.image, stageFx);
            const QSizeF size = image.deviceIndependentSize();
            drawBlended(p, QRectF(item.topLeft, QSizeF(size.width() * item.scale.x(), size.height() * item.scale.y())),
                        image, item.mode, item.opacity);
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
        double xAngle = 0;
        double yAngle = 0;
        Projection projection = Projection::Perspective;
        double focalLength = 2048;
        bool screen = false;
        const Formats::Palette* palette = nullptr;
        QList<ColorFx> fx;
        std::optional<Blend> blend;
        QString trans = QStringLiteral("default");
        std::array<int, 2> alpha{256, 0};
    };
    struct Placed
    {
        QImage image;
        QTransform transform;
        Blend blend;
    };
    auto place = [&](const SpriteDocument* sprites, const AirAnimation* animation, int frameIndex, QPointF at,
                     const Look& look) -> std::optional<Placed> {
        if (!animation || frameIndex < 0 || frameIndex >= animation->frames.size())
            return std::nullopt;
        const AirFrame& f = animation->frames[frameIndex];
        if (f.group < 0)
            return std::nullopt;
        const Sprite* s = sprite(sprites, f.group, f.number, look.palette);
        if (!s || s->image.isNull())
            return std::nullopt;
        const bool flipH = f.flipH != (look.facing < 0);
        const bool flipV = f.flipV != (look.vfacing < 0);
        Placed placed;
        placed.image = withFx(flipH || flipV ? s->image.mirrored(flipH, flipV) : s->image, look.fx);
        placed.blend = look.blend ? *look.blend : blendOf(f.transparency, look.trans, look.alpha);
        const double left = f.x * look.facing - (flipH ? placed.image.width() - s->axis.x() : s->axis.x());
        const double top = f.y - (flipV ? placed.image.height() - s->axis.y() : s->axis.y());
        const QTransform space = look.screen ? QTransform() : world;
        QTransform flat;
        const double angle = f.angle + look.angle;
        if (angle != 0)
            flat.rotate(-angle * look.facing);
        flat.scale(look.scale.x() * f.xScale * look.angleScale.x(), look.scale.y() * f.yScale * look.angleScale.y());
        flat.translate(left, top);
        if (look.xAngle == 0 && look.yAngle == 0) {
            placed.transform = flat * QTransform::fromTranslate(at.x(), at.y()) * space;
            return placed;
        }
        // The engine's fixed perspective looks from 2048 pixels in front of the screen's centre, so corners
        // turned nearer grow.
        const double yAngle = qDegreesToRadians(look.yAngle) * ((look.facing < 0) != (look.vfacing < 0) ? -1 : 1);
        const double xAngle = qDegreesToRadians(look.xAngle);
        const double zoom = look.screen ? 1.0 : double(view.scale);
        const double focal = look.focalLength * zoom;
        const QPointF eye = look.projection == Projection::Perspective2
                                ? space.map(at)
                                : QPointF(Screen.width() / 2.0, Screen.height() / 2.0);
        const QRectF rect(placed.image.rect());
        const QPolygonF square({rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()});
        QPolygonF corners;
        for (const QPointF& corner : square) {
            const QPointF r = flat.map(corner);
            const double x = r.x() * std::cos(yAngle);
            const double zy = -r.x() * std::sin(yAngle);
            const double y = r.y() * std::cos(xAngle) - zy * std::sin(xAngle);
            const double z = (r.y() * std::sin(xAngle) + zy * std::cos(xAngle)) * zoom;
            QPointF onScreen = space.map(at + QPointF(x, y));
            if (look.projection != Projection::Orthographic) {
                if (z >= focal * 0.99)
                    return std::nullopt;
                onScreen = eye + (onScreen - eye) * (focal / (focal - z));
            }
            corners << onScreen;
        }
        if (!QTransform::quadToQuad(square, corners, placed.transform))
            return std::nullopt;
        return placed;
    };

    // Shadows and reflections draw the sprites again on the ground: a shadow squashed by yscale, leaning
    // by xshear and darkened by its intensity, and a reflection mirrored in the floor.
    const EmulatorSession::Shadows& shading = stage.shadows;
    const bool shadows = !match.isAsserted(QStringLiteral("globalnoshadow"));
    struct Ground
    {
        Placed placed;
        double height = 0;
        double offset = 0;
        std::array<int, 3> color{0, 0, 0};
    };
    QList<Ground> grounds;
    auto castReflection = [&](const Ground& g) {
        if (shading.reflection <= 0)
            return;
        const int r = shading.reflection;
        Blend b = g.placed.blend;
        if (b.kind == Blend::Kind::Sub) {
            b.src = b.src * r / 256;
        } else {
            const int src = b.kind == Blend::Kind::Normal ? 256 : b.src;
            const int dst = b.kind == Blend::Kind::Normal ? 0 : b.dst;
            b = Blend{Blend::Kind::Add, src * r / 256, 256 - (256 - dst) * r / 256};
        }
        const QTransform mirror(1, 0, 0, -1, 0, g.offset);
        canvas.draw(g.placed.image, g.placed.transform * toWorld * mirror * world, b);
    };
    auto castShadow = [&](const Ground& g) {
        double strength = 1;
        if (shading.fade && g.height < shading.fadeBottom)
            strength = std::max(0.0, (g.height - shading.fadeTop) / (shading.fadeBottom - shading.fadeTop));
        if (g.placed.blend.kind != Blend::Kind::Normal)
            strength *= std::clamp(1 - g.placed.blend.dst / 256.0, 0.0, 1.0);
        if (strength <= 0)
            return;
        const std::array<int, 3>& rgb = g.color[0] < 0 ? shading.color : g.color;
        const QTransform squash(1, 0, shading.xShear, -shading.yScale, 0, g.offset);
        const QTransform onGround = g.placed.transform * toWorld * squash * world;
        if (shading.intensity > 0)
            canvas.draw(silhouette(g.placed.image, Qt::black), onGround,
                        Blend{Blend::Kind::Normal, int(std::lround(shading.intensity * strength)), 0});
        if (rgb != std::array<int, 3>{0, 0, 0}) {
            const QColor taken(std::clamp(rgb[0], 0, 255), std::clamp(rgb[1], 0, 255), std::clamp(rgb[2], 0, 255));
            canvas.draw(silhouette(g.placed.image, taken), onGround,
                        Blend{Blend::Kind::Sub, int(std::lround(256 * strength)), 256});
        }
    };

    // Creation order at equal sprite priority.
    struct Item
    {
        int priority;
        int order;
        std::function<void()> draw;
    };
    QList<Item> items;
    int order = 0;
    auto add = [&](int priority, const std::optional<Placed>& placed) {
        if (placed)
            items << Item{priority, order++, [&canvas, placed] {
                              canvas.draw(placed->image, placed->transform, placed->blend);
                          }};
    };
    auto afterImageLook = [&allFx](const AfterImage& ai, int k, const AfterImageFrame& h) {
        Look look;
        look.facing = h.facing;
        look.scale = QPointF(h.scale.x, h.scale.y);
        if (h.angleDraw) {
            look.angle = h.angle;
            look.angleScale = QPointF(h.angleScale.x, h.angleScale.y);
        }
        look.fx = {afterImageFx(ai, k)};
        look.fx << allFx;
        look.blend = blendOf(AirTransparency(), ai.trans);
        return look;
    };
    auto frameOf = [](const AirAnimation* animation, int index) -> const AirFrame* {
        return animation && index >= 0 && index < animation->frames.size() ? &animation->frames[index] : nullptr;
    };
    for (const Character* c : match.characters()) {
        if (c->destroyed())
            continue;
        const SpriteDocument* sprites = session.spritesOf(*c);
        const DrawState& draw = c->draw();
        QList<ColorFx> ownFx;
        addFx(ownFx, draw.palFx);
        // A helper without ownpal shares its parent's palette, and its PalFX.
        for (const Character* up = c; !up->ownPal() && up->parent(); up = up->parent())
            addFx(ownFx, up->parent()->draw().palFx);
        ownFx << allFx;
        const PaletteRemap remap = c->paletteRemap();
        auto paletteOf = [&](const AirAnimation* animation, int index, const PaletteRemap& shown) {
            const AirFrame* f = frameOf(animation, index);
            return f ? session.paletteOf(*c, f->group, f->number, shown) : nullptr;
        };
        const AfterImage& ai = draw.afterImage;
        const QList<AfterImageFrame>& history = c->afterImages();
        const int gap = std::max(1, ai.frameGap);
        for (int i = int((history.size() - 1) / gap) * gap; i >= 0 && !history.isEmpty(); i -= gap) {
            const AfterImageFrame& h = history[i];
            const AirAnimation* animation = h.animData ? h.animData->animation(h.animNo) : nullptr;
            Look look = afterImageLook(ai, i / gap, h);
            look.palette = paletteOf(animation, h.frame, remap);
            add(c->sprPriority(), place(sprites, animation, h.frame, QPointF(h.pos.x, h.pos.y), look));
        }
        if (!c->isAsserted(QStringLiteral("invisible"))) {
            const AirAnimation* animation = c->animationData().animation(c->animNo());
            Look look;
            look.facing = c->facing();
            if (draw.angleDraw) {
                look.angle = draw.angle;
                look.angleScale = QPointF(draw.angleScale.x, draw.angleScale.y);
            }
            look.palette = paletteOf(animation, c->anim().frame(), remap);
            look.fx = ownFx;
            look.trans = draw.trans;
            look.alpha = draw.alpha;
            const QPointF at(c->pos().x() + draw.offset.x, c->pos().y() + draw.offset.y);
            const std::optional<Placed> placed =
                place(sprites, animation, c->anim().isEmpty() ? -1 : c->anim().frame(), at, look);
            add(c->sprPriority(), placed);
            if (placed && shadows && !c->isAsserted(QStringLiteral("noshadow")))
                grounds << Ground{*placed, at.y(), c->constant(QStringLiteral("size.shadowoffset"), 0), {-1, 0, 0}};
        }
        for (const Explod& e : c->explods()) {
            const SpriteDocument* explodSprites = e.common ? session.fightFxSprites() : session.spritesOf(*c);
            const AirAnimation* animation = e.animData ? e.animData->animation(e.animNo) : nullptr;
            Look look;
            if (!e.ownPal)
                addFx(look.fx, draw.palFx);
            look.fx << allFx;
            look.facing = e.drawFacing();
            look.vfacing = e.vFacing;
            look.scale = QPointF(e.scale.x, e.scale.y);
            look.angle = e.angle;
            look.xAngle = e.xAngle;
            look.yAngle = e.yAngle;
            look.projection = e.projection;
            look.focalLength = e.focalLength;
            look.screen = e.screen;
            look.trans = e.trans;
            look.alpha = e.alpha;
            const int frameNo = e.anim.isEmpty() ? -1 : e.anim.frame();
            if (!e.common)
                look.palette = paletteOf(animation, frameNo, e.ownPal ? e.remap : remap);
            const QPointF at(e.drawPos().x, e.drawPos().y);
            const std::optional<Placed> placed = place(explodSprites, animation, frameNo, at, look);
            add(e.onTop ? 1000 : e.sprPriority, placed);
            if (placed && shadows && !e.screen && e.shadow != std::array<int, 3>{0, 0, 0})
                grounds << Ground{*placed, at.y(), 0, e.shadow};
        }
        for (const Projectile& pr : c->projectiles()) {
            const QList<ColorFx> projFx = pr.ownPal ? allFx : ownFx;
            const PaletteRemap& projRemap = pr.ownPal ? pr.remap : remap;
            const AfterImage& trail = pr.afterImage;
            const int every = std::max(1, trail.frameGap);
            for (int i = int((pr.afterImages.size() - 1) / every) * every; i >= 0 && !pr.afterImages.isEmpty();
                 i -= every) {
                const AfterImageFrame& h = pr.afterImages[i];
                const AirAnimation* animation = h.animData ? h.animData->animation(h.animNo) : nullptr;
                Look look = afterImageLook(trail, i / every, h);
                if (!h.common)
                    look.palette = paletteOf(animation, h.frame, projRemap);
                add(pr.sprPriority, place(h.common ? session.fightFxSprites() : sprites, animation, h.frame,
                                          QPointF(h.pos.x, h.pos.y), look));
            }
            const AirAnimation* animation = pr.animData ? pr.animData->animation(pr.shown.number) : nullptr;
            Look look;
            look.facing = pr.facing;
            look.scale = QPointF(pr.scale.x, pr.scale.y);
            const int frameNo = pr.player.isEmpty() ? -1 : pr.player.frame();
            if (!pr.shown.common)
                look.palette = paletteOf(animation, frameNo, projRemap);
            look.fx = projFx;
            const std::optional<Placed> placed = place(pr.shown.common ? session.fightFxSprites() : sprites, animation,
                                                       frameNo, QPointF(pr.pos.x, pr.pos.y), look);
            add(pr.sprPriority, placed);
            if (placed && shadows && pr.shadow != std::array<int, 3>{0, 0, 0})
                grounds << Ground{*placed, pr.pos.y, 0, pr.shadow};
        }
    }
    for (const EmulatorSession::Effect& effect : session.effects()) {
        const Character* owner = match.playerById(effect.playerId);
        const CharacterData* data = effect.common ? session.fightFx() : owner ? &owner->data() : nullptr;
        const SpriteDocument* sprites = effect.common ? session.fightFxSprites()
                                        : owner       ? session.spritesOf(*owner)
                                                      : nullptr;
        Look look;
        look.fx = allFx;
        add(effect.under ? -1000 : 500,
            place(sprites, data ? data->animation(effect.anim) : nullptr,
                  effect.player.isEmpty() ? -1 : effect.player.frame(), QPointF(effect.pos.x, effect.pos.y), look));
    }

    drawStage(0);
    for (const Ground& g : std::as_const(grounds))
        castReflection(g);
    for (const Ground& g : std::as_const(grounds))
        castShadow(g);
    if (match.superPauseTime() > 0 && match.superDarken())
        p.fillRect(QRect(QPoint(), Screen), QColor(0, 0, 0, 128));
    const EnvColor& color = match.envColor();
    const QColor fill(color.color[0], color.color[1], color.color[2]);
    if (color.time != 0 && color.under)
        p.fillRect(QRect(QPoint(), Screen), fill);

    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.priority < b.priority; });
    for (const Item& item : std::as_const(items))
        item.draw();

    drawStage(1);
    if (color.time != 0 && !color.under)
        p.fillRect(QRect(QPoint(), Screen), fill);

    // The fight screen ignores the shake and the zoom, but AllPalFX colours it too.
    canvas.setBase(QTransform());
    const QList<FightScreen::Item> screenItems = match.isAsserted(QStringLiteral("nobardisplay"))
                                                     ? QList<FightScreen::Item>()
                                                     : session.fightScreen().items(match, session.sides());
    for (const FightScreen::Item& item : screenItems) {
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
                    const QRectF onScreen = world.mapRect(box);
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

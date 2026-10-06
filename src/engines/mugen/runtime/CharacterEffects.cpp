#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"
#include "engines/mugen/runtime/IniText.h"

#include <algorithm>
#include <cmath>

namespace OFS::Mugen::Runtime {

using namespace Detail;

namespace {

// AA, AT and AP stand for all attacks, all throws and all projectiles.
QSet<QString> attackClasses(const QStringList& names)
{
    QSet<QString> classes;
    for (const QString& raw : names) {
        const QString name = raw.trimmed().toUpper();
        if (name.size() != 2)
            continue;
        if (name.at(0) == QLatin1Char('A')) {
            for (const char* kind : {"N", "S", "H"})
                classes.insert(QLatin1String(kind) + name.at(1));
        } else {
            classes.insert(name);
        }
    }
    return classes;
}

// %d and %i take a whole number, %f, %e and %g (any case) a float, and %% is a percent sign.
// \n, \t, \\ and \" are escapes.
QString formatText(const QString& format, const std::vector<Value>& args)
{
    QString out;
    std::size_t next = 0;
    for (int i = 0; i < format.size(); ++i) {
        const QChar ch = format.at(i);
        const QChar following = i + 1 < format.size() ? format.at(i + 1) : QChar();
        const char spec = following.toLatin1();
        if (ch == QLatin1Char('\\') && QByteArrayView("nt\\\"").contains(spec)) {
            out += spec == 'n' ? QLatin1Char('\n') : spec == 't' ? QLatin1Char('\t') : following;
            ++i;
        } else if (ch == QLatin1Char('%') && spec == '%') {
            out += QLatin1Char('%');
            ++i;
        } else if (ch == QLatin1Char('%') && (spec == 'd' || spec == 'i')) {
            out += QString::number(next < args.size() ? args[next++].toInt() : 0);
            ++i;
        } else if (ch == QLatin1Char('%') && QByteArrayView("fFeEgG").contains(spec)) {
            const double value = next < args.size() ? double(args[next++].toFloat()) : 0.0;
            const char kind = spec == 'F' ? 'f' : spec == 'E' ? 'e' : spec == 'G' ? 'g' : spec;
            QString text = QString::number(value, kind, 6);
            if (spec == 'E' || spec == 'G')
                text = text.toUpper();
            out += text;
            ++i;
        } else {
            out += ch;
        }
    }
    return out;
}

} // namespace

bool Character::hittableBy(const HitDef& hit) const
{
    // HitBy lets through only attacks of its states and classes. NotHitBy refuses them.
    for (const HitBySlot& slot : m_hitBy) {
        if (slot.time == 0)
            continue;
        const bool stateMatch = std::any_of(hit.attrStates.cbegin(), hit.attrStates.cend(),
                                            [&slot](QChar s) { return slot.states.contains(s); });
        const bool classMatch = std::any_of(hit.attrClasses.cbegin(), hit.attrClasses.cend(),
                                            [&slot](const QString& k) { return slot.classes.contains(k.toUpper()); });
        if (slot.notHitBy ? (stateMatch || classMatch) : !(stateMatch && classMatch))
            return false;
    }
    return true;
}

bool Character::executeEffect(const Controller& c)
{
    const QString& type = c.type;
    auto ints3 = [this, &c](const QString& name, std::array<int, 3>& out) {
        for (int i = 0; i < 3; ++i)
            out[std::size_t(i)] = intParam(c, name, out[std::size_t(i)], i);
    };
    auto readPalFx = [this, &c, &ints3](PalFx& fx) {
        fx = PalFx();
        fx.time = intParam(c, QStringLiteral("time"), 0);
        ints3(QStringLiteral("add"), fx.add);
        ints3(QStringLiteral("mul"), fx.mul);
        ints3(QStringLiteral("sinadd"), fx.sinAdd);
        fx.sinPeriod = intParam(c, QStringLiteral("sinadd"), 0, 3);
        fx.invertAll = intParam(c, QStringLiteral("invertall"), 0) != 0;
        fx.color = floatParam(c, QStringLiteral("color"), 256);
    };
    auto text = [&c](const QString& name) {
        const Param* p = c.param(name);
        return p ? unquote(p->text.trimmed()) : QString();
    };

    if (type == QLatin1String("assertspecial")) {
        for (const char* key : {"flag", "flag2", "flag3"}) {
            const QString flag = text(QLatin1String(key)).trimmed().toLower();
            if (!flag.isEmpty())
                m_asserted.insert(flag);
        }
        return true;
    }
    if (type == QLatin1String("hitby") || type == QLatin1String("nothitby")) {
        for (int slot = 0; slot < 2; ++slot) {
            const QString attr = text(slot == 0 ? QStringLiteral("value") : QStringLiteral("value2"));
            if (attr.isEmpty())
                continue;
            QStringList parts = attr.split(QLatin1Char(','));
            HitBySlot& s = m_hitBy[std::size_t(slot)];
            s.notHitBy = type == QLatin1String("nothitby");
            s.states = parts.takeFirst().trimmed().toUpper();
            s.classes = attackClasses(parts);
            s.time = intParam(c, QStringLiteral("time"), 1);
        }
        return true;
    }
    if (type == QLatin1String("hitoverride")) {
        const int slot = std::clamp(intParam(c, QStringLiteral("slot"), 0), 0, 7);
        HitOverrideSlot& s = m_overrides[std::size_t(slot)];
        QStringList parts = text(QStringLiteral("attr")).split(QLatin1Char(','));
        s.states = parts.takeFirst().trimmed().toUpper();
        s.classes = attackClasses(parts);
        s.stateNo = intParam(c, QStringLiteral("stateno"), -1);
        s.time = intParam(c, QStringLiteral("time"), 1);
        s.forceAir = intParam(c, QStringLiteral("forceair"), 0) != 0;
        return true;
    }
    if (type == QLatin1String("reversaldef")) {
        HitDef rd = hitDefFrom(c);
        QStringList parts = text(QStringLiteral("reversal.attr")).split(QLatin1Char(','));
        rd.reversal = true;
        rd.reversalStates = parts.takeFirst().trimmed().toUpper();
        rd.reversalClasses = QStringList(parts);
        m_hitDef = rd;
        return true;
    }
    if (type == QLatin1String("attackmulset")) {
        m_attackMul = floatParam(c, QStringLiteral("value"), 1);
        return true;
    }
    if (type == QLatin1String("defencemulset")) {
        m_defenceMul = std::max(0.0001f, floatParam(c, QStringLiteral("value"), 1));
        return true;
    }
    if (type == QLatin1String("attackdist")) {
        m_hitDef.guardDist = intParam(c, QStringLiteral("value"), m_hitDef.guardDist);
        return true;
    }
    if (type == QLatin1String("screenbound")) {
        m_screenBound = intParam(c, QStringLiteral("value"), 0) != 0;
        m_moveCamera[0] = intParam(c, QStringLiteral("movecamera"), 0, 0) != 0;
        m_moveCamera[1] = intParam(c, QStringLiteral("movecamera"), 0, 1) != 0;
        return true;
    }
    if (type == QLatin1String("displaytoclipboard") || type == QLatin1String("appendtoclipboard")) {
        std::vector<Value> args;
        if (const Param* params = c.param(QStringLiteral("params"))) {
            for (int i = 0; i < int(params->values.size()); ++i)
                args.push_back(eval(params, i));
        }
        const QString line = formatText(text(QStringLiteral("text")), args);
        if (type == QLatin1String("displaytoclipboard"))
            m_clipboard = line;
        else
            m_clipboard += (m_clipboard.isEmpty() ? QString() : QStringLiteral("\n")) + line;
        return true;
    }
    if (type == QLatin1String("clearclipboard")) {
        m_clipboard.clear();
        return true;
    }
    if (type == QLatin1String("playsnd") || type == QLatin1String("stopsnd") || type == QLatin1String("sndpan")) {
        if (!m_match)
            return true;
        SoundEvent event;
        event.playerId = m_id;
        event.channel = intParam(c, QStringLiteral("channel"), -1);
        if (type == QLatin1String("playsnd")) {
            const Param* value = c.param(QStringLiteral("value"));
            event.common = value && value->prefix == QLatin1Char('F');
            event.group = intParam(c, QStringLiteral("value"), 0, 0);
            event.index = intParam(c, QStringLiteral("value"), 0, 1);
            event.lowPriority = intParam(c, QStringLiteral("lowpriority"), 0) != 0;
            event.volume = intParam(c, QStringLiteral("volume"), 0);
            event.volumeScale = std::clamp(floatParam(c, QStringLiteral("volumescale"), 100), 0.f, 100.f);
            event.freqMul = floatParam(c, QStringLiteral("freqmul"), 1);
            event.loop = intParam(c, QStringLiteral("loop"), 0) != 0;
        } else {
            event.kind = type == QLatin1String("stopsnd") ? SoundEvent::Kind::Stop : SoundEvent::Kind::Pan;
        }
        if (has(c, QStringLiteral("abspan"))) {
            event.pan = floatParam(c, QStringLiteral("abspan"), 0);
            event.absolutePan = true;
        } else {
            event.pan = floatParam(c, QStringLiteral("pan"), 0);
        }
        m_match->addSound(event);
        return true;
    }
    if (type == QLatin1String("offset")) {
        if (has(c, QStringLiteral("x")))
            m_draw.offset.x = floatParam(c, QStringLiteral("x"), 0);
        if (has(c, QStringLiteral("y")))
            m_draw.offset.y = floatParam(c, QStringLiteral("y"), 0);
        return true;
    }
    if (type == QLatin1String("trans")) {
        // Each kind has its own alpha unless alpha gives one.
        const QString kind = text(QStringLiteral("trans")).trimmed().toLower();
        m_draw.trans = kind.isEmpty() ? QStringLiteral("default") : kind;
        std::array<int, 2> alpha{256, 0};
        if (kind == QLatin1String("add") || kind == QLatin1String("sub"))
            alpha = {256, 256};
        else if (kind == QLatin1String("add1"))
            alpha = {256, 128};
        alpha[0] = std::clamp(intParam(c, QStringLiteral("alpha"), alpha[0], 0), 0, 256);
        alpha[1] = std::clamp(intParam(c, QStringLiteral("alpha"), alpha[1], 1), 0, 256);
        m_draw.alpha = alpha;
        return true;
    }
    if (type == QLatin1String("angledraw")) {
        m_draw.angleDraw = true;
        if (has(c, QStringLiteral("value")))
            m_draw.angle = floatParam(c, QStringLiteral("value"), 0);
        m_draw.angleScale =
            Vec2{floatParam(c, QStringLiteral("scale"), 1, 0), floatParam(c, QStringLiteral("scale"), 1, 1)};
        return true;
    }
    if (type == QLatin1String("angleset")) {
        m_draw.angle = floatParam(c, QStringLiteral("value"), 0);
        return true;
    }
    if (type == QLatin1String("angleadd")) {
        m_draw.angle += floatParam(c, QStringLiteral("value"), 0);
        return true;
    }
    if (type == QLatin1String("anglemul")) {
        m_draw.angle *= floatParam(c, QStringLiteral("value"), 1);
        return true;
    }
    if (type == QLatin1String("palfx")) {
        readPalFx(m_draw.palFx);
        return true;
    }
    if (type == QLatin1String("allpalfx") || type == QLatin1String("bgpalfx")) {
        if (m_match)
            readPalFx(type == QLatin1String("allpalfx") ? m_match->allPalFx() : m_match->bgPalFx());
        return true;
    }
    if (type == QLatin1String("afterimage")) {
        m_draw.afterImage = readAfterImage(c, QString());
        return true;
    }
    if (type == QLatin1String("afterimagetime")) {
        if (m_draw.afterImage.time == 0)
            return true;
        const QString key = has(c, QStringLiteral("time")) ? QStringLiteral("time") : QStringLiteral("value");
        m_draw.afterImage.time = intParam(c, key, m_draw.afterImage.time);
        return true;
    }
    if (type == QLatin1String("envshake")) {
        if (m_match) {
            EnvShake& shake = m_match->envShake();
            shake.time = intParam(c, QStringLiteral("time"), 1);
            shake.freq = std::max(0.f, floatParam(c, QStringLiteral("freq"), 60));
            shake.ampl = int(std::lround(float(intParam(c, QStringLiteral("ampl"), -4)) * scale()));
            shake.phase = floatParam(c, QStringLiteral("phase"), shake.freq >= 90 ? 90 : 0);
        }
        return true;
    }
    if (type == QLatin1String("fallenvshake")) {
        if (m_match && m_ghv.fallEnvShakeTime > 0) {
            EnvShake& shake = m_match->envShake();
            shake.time = m_ghv.fallEnvShakeTime;
            shake.freq = m_ghv.fallEnvShakeFreq;
            shake.ampl = int(std::lround(float(m_ghv.fallEnvShakeAmpl) * scale()));
            shake.phase = m_ghv.fallEnvShakePhase;
            m_ghv.fallEnvShakeTime = 0;
        }
        return true;
    }
    if (type == QLatin1String("envcolor")) {
        if (m_match) {
            EnvColor& color = m_match->envColor();
            for (int i = 0; i < 3; ++i)
                color.color[std::size_t(i)] = intParam(c, QStringLiteral("value"), 255, i);
            color.time = intParam(c, QStringLiteral("time"), 1);
            color.under = intParam(c, QStringLiteral("under"), 0) != 0;
        }
        return true;
    }
    if (type == QLatin1String("remappal")) {
        const QPair<int, int> source(intParam(c, QStringLiteral("source"), -1, 0),
                                     intParam(c, QStringLiteral("source"), 0, 1));
        const QPair<int, int> dest(intParam(c, QStringLiteral("dest"), -1, 0),
                                   intParam(c, QStringLiteral("dest"), 0, 1));
        if (source.first < 0) {
            m_draw.remap.clear();
            if (dest.first >= 0)
                m_draw.remap.insert(DrawState::AllPalettes, dest);
        } else if (dest.first < 0 || dest == source) {
            m_draw.remap.remove(source);
        } else {
            m_draw.remap.insert(source, dest);
        }
        return true;
    }
    if (type == QLatin1String("makedust") || type == QLatin1String("gamemakeanim")) {
        if (!m_match)
            return true;
        SpawnEvent event;
        event.playerId = m_id;
        const bool dust = type == QLatin1String("makedust");
        if (dust) {
            const int spacing = std::max(1, intParam(c, QStringLiteral("spacing"), 3));
            if (m_match->gameTime() - m_lastDust < spacing)
                return true;
            m_lastDust = m_match->gameTime();
        }
        event.kind = dust ? SpawnEvent::Kind::Dust : SpawnEvent::Kind::GameAnim;
        event.anim = dust ? 120 : intParam(c, QStringLiteral("value"), 0);
        event.under = !dust && intParam(c, QStringLiteral("under"), 0) != 0;
        // GameMakeAnim's random moves it up to half its value each way.
        Vec2 shift;
        if (const int range = dust ? 0 : intParam(c, QStringLiteral("random"), 0); range > 0) {
            shift.x = float(m_match->random(range + 1) - range / 2);
            shift.y = float(m_match->random(range + 1) - range / 2);
        }
        auto at = [this, &c, &event, shift](const QString& name) {
            SpawnEvent e = event;
            e.pos = Vec2{(m_pos.x + (floatParam(c, name, 0, 0) + shift.x) * float(m_facing)) * scale(),
                         (m_pos.y + floatParam(c, name, 0, 1) + shift.y) * scale()};
            e.scale = e.common ? 1 : m_data.localScale();
            return e;
        };
        m_match->addSpawn(at(QStringLiteral("pos")));
        if (dust && has(c, QStringLiteral("pos2")))
            m_match->addSpawn(at(QStringLiteral("pos2")));
        return true;
    }
    if (type == QLatin1String("victoryquote")) {
        m_victoryQuote = intParam(c, QStringLiteral("value"), -1);
        return true;
    }
    if (type == QLatin1String("pause") || type == QLatin1String("superpause")) {
        if (!m_match)
            return true;
        const bool super = type == QLatin1String("superpause");
        const int time = intParam(c, QStringLiteral("time"), super ? 30 : 0);
        int moveTime = std::max(0, intParam(c, QStringLiteral("movetime"), 0));
        const bool running = super ? m_match->superPauseTime() > 0 : m_match->pauseTime() > 0;
        if (moveTime > time)
            moveTime = 0;
        else if (running && moveTime > 0)
            --moveTime;
        (super ? m_superMoveTime : m_pauseMoveTime) = moveTime;
        const bool pauseBg = intParam(c, QStringLiteral("pausebg"), 1) != 0;
        const int endCmdBufTime = intParam(c, QStringLiteral("endcmdbuftime"), 0);
        if (!super) {
            m_match->requestPause(false, m_id, time, pauseBg, false, endCmdBufTime);
            return true;
        }
        m_match->requestPause(true, m_id, time, pauseBg, intParam(c, QStringLiteral("darken"), 1) != 0, endCmdBufTime);
        powerOwner()->m_power =
            std::clamp(powerOwner()->m_power + intParam(c, QStringLiteral("poweradd"), 0), 0, int(m_data.powerMax()));
        // Out of reach for the pause. It starts next tick, so one tick more.
        if (intParam(c, QStringLiteral("unhittable"), 1) != 0)
            m_unhittableTime = time + (time > 0 ? 1 : 0);
        float defMul = floatParam(c, QStringLiteral("p2defmul"), 0);
        if (defMul <= 0)
            defMul = m_match->rules().superTargetDefenceMul;
        if (defMul != 1) {
            for (Character* other : m_match->characters()) {
                if (other != this)
                    other->m_superDefenceMulNext *= defMul;
            }
        }
        // The charging animation (fightfx 30 unless S) and a sound (fightfx's unless S).
        const Param* anim = c.param(QStringLiteral("anim"));
        const int animNo = intParam(c, QStringLiteral("anim"), 30);
        if (animNo >= 0) {
            SpawnEvent event;
            event.kind = SpawnEvent::Kind::GameAnim;
            event.playerId = m_id;
            event.anim = animNo;
            event.common = !(anim && anim->prefix == QLatin1Char('S'));
            event.scale = event.common ? 1 : m_data.localScale();
            event.pos = Vec2{(m_pos.x + floatParam(c, QStringLiteral("pos"), 0, 0) * float(m_facing)) * scale(),
                             (m_pos.y + floatParam(c, QStringLiteral("pos"), 0, 1)) * scale()};
            m_match->addSpawn(event);
        }
        if (const Param* sound = c.param(QStringLiteral("sound"));
            sound && intParam(c, QStringLiteral("sound"), -1, 0) >= 0) {
            SoundEvent event;
            event.playerId = m_id;
            event.common = sound->prefix != QLatin1Char('S');
            event.group = intParam(c, QStringLiteral("sound"), 0, 0);
            event.index = intParam(c, QStringLiteral("sound"), 0, 1);
            m_match->addSound(event);
        }
        return true;
    }
    if (type == QLatin1String("forcefeedback"))
        return true;
    if (type == QLatin1String("zoom")) {
        // A scale of 0 or less and a lag outside 0 to 1 are not taken.
        if (!m_match)
            return true;
        ZoomEffect& z = m_match->zoomEffect();
        z.pos = Vec2{floatParam(c, QStringLiteral("pos"), 0, 0) * scale(),
                     floatParam(c, QStringLiteral("pos"), 0, 1) * scale()};
        const float scale = floatParam(c, QStringLiteral("scale"), 1);
        z.scale = scale > 0 ? scale : 1;
        const float lag = floatParam(c, QStringLiteral("lag"), 0);
        z.lag = lag >= 0 && lag <= 1 ? lag : 0;
        z.time = intParam(c, QStringLiteral("time"), 1);
        z.active = true;
        return true;
    }
    return false;
}

AfterImage Character::readAfterImage(const Controller& c, const QString& prefix)
{
    auto name = [&prefix](const char* key) {
        return prefix + QLatin1String(key);
    };
    AfterImage a;
    a.time = intParam(c, name("time"), 1);
    a.length = intParam(c, name("length"), 20);
    a.timeGap = std::max(1, intParam(c, name("timegap"), 1));
    a.frameGap = std::max(1, intParam(c, name("framegap"), 4));
    if (const Param* trans = c.param(name("trans"))) {
        if (const QString t = unquote(trans->text.trimmed()).trimmed().toLower(); !t.isEmpty())
            a.trans = t;
    }
    a.palColor = intParam(c, name("palcolor"), 256);
    a.palInvertAll = intParam(c, name("palinvertall"), 0) != 0;
    for (int i = 0; i < 3; ++i) {
        const auto k = std::size_t(i);
        a.palBright[k] = intParam(c, name("palbright"), a.palBright[k], i);
        a.palContrast[k] = intParam(c, name("palcontrast"), a.palContrast[k], i);
        a.palPostBright[k] = intParam(c, name("palpostbright"), a.palPostBright[k], i);
        a.palAdd[k] = intParam(c, name("paladd"), a.palAdd[k], i);
        a.palMul[k] = floatParam(c, name("palmul"), a.palMul[k], i);
    }
    return a;
}

void Character::forceRemap(const Controller& c, PaletteRemap& remap)
{
    const int group = intParam(c, QStringLiteral("remappal"), -1, 0);
    if (group >= 0)
        remap = PaletteRemap{{DrawState::AllPalettes, {group, intParam(c, QStringLiteral("remappal"), 0, 1)}}};
}

PaletteRemap Character::paletteRemap() const
{
    PaletteRemap remap;
    for (const Character* c = this; c; c = c->ownPal() ? nullptr : c->m_parent) {
        for (auto it = c->m_draw.remap.cbegin(); it != c->m_draw.remap.cend(); ++it) {
            if (!remap.contains(it.key()))
                remap.insert(it.key(), it.value());
        }
        // A remap of every palette leaves nothing to the parent.
        if (c->m_draw.remap.contains(DrawState::AllPalettes))
            break;
    }
    return remap;
}

} // namespace OFS::Mugen::Runtime

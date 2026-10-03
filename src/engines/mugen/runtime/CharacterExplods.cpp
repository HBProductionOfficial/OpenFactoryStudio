#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"
#include "engines/mugen/runtime/IniText.h"

#include <algorithm>

namespace OFS::Mugen::Runtime {

using namespace Detail;

AnimPlayer Detail::animPlayerFor(const AirAnimation& animation)
{
    QList<int> times;
    for (const AirFrame& frame : animation.frames)
        times << frame.time;
    return AnimPlayer(times, animation.loopStartFrame());
}

void Character::readExplod(const Controller& c, Explod& e, bool create)
{
    auto sign = [this, &c](const QString& name, int current) {
        return has(c, name) ? (intParam(c, name, 1) < 0 ? -1 : 1) : current;
    };
    auto pair = [this, &c](const QString& name, Vec2 current) {
        return Vec2{floatParam(c, name, current.x, 0), floatParam(c, name, current.y, 1)};
    };
    if (create || has(c, QStringLiteral("id")))
        e.id = intParam(c, QStringLiteral("id"), e.id);
    e.relativeFacing = sign(QStringLiteral("facing"), e.relativeFacing);
    e.vFacing = sign(QStringLiteral("vfacing"), e.vFacing);
    e.vel = pair(has(c, QStringLiteral("vel")) ? QStringLiteral("vel") : QStringLiteral("velocity"), e.vel);
    e.accel = pair(QStringLiteral("accel"), e.accel);
    e.angle = floatParam(c, QStringLiteral("angle"), e.angle);
    e.bindTime = intParam(c, QStringLiteral("bindtime"), e.bindTime);
    e.removeTime = intParam(c, QStringLiteral("removetime"), e.removeTime);
    if (has(c, QStringLiteral("supermove")))
        e.superMoveTime = intParam(c, QStringLiteral("supermove"), 0) != 0 ? -1 : 0;
    e.superMoveTime = intParam(c, QStringLiteral("supermovetime"), e.superMoveTime);
    e.pauseMoveTime = intParam(c, QStringLiteral("pausemovetime"), e.pauseMoveTime);
    e.scale = pair(QStringLiteral("scale"), e.scale);
    e.sprPriority = intParam(c, QStringLiteral("sprpriority"), e.sprPriority);
    e.onTop = intParam(c, QStringLiteral("ontop"), e.onTop ? 1 : 0) != 0;
    e.ownPal = intParam(c, QStringLiteral("ownpal"), e.ownPal ? 1 : 0) != 0;
    e.removeOnGetHit = intParam(c, QStringLiteral("removeongethit"), e.removeOnGetHit ? 1 : 0) != 0;
    e.ignoreHitPause = intParam(c, QStringLiteral("ignorehitpause"), e.ignoreHitPause ? 1 : 0) != 0;
    if (const Param* trans = c.param(QStringLiteral("trans"))) {
        e.trans = unquote(trans->text.trimmed()).toLower();
        e.alpha = {256, e.trans == QLatin1String("add") || e.trans == QLatin1String("sub") ? 256 : 0};
    }
    e.alpha[0] = intParam(c, QStringLiteral("alpha"), e.alpha[0], 0);
    e.alpha[1] = intParam(c, QStringLiteral("alpha"), e.alpha[1], 1);

    // MUGEN 1.1 places explods where pos says unless a postype is given, following bindID.
    const bool rules11 = explodRules11();
    const bool moved = has(c, QStringLiteral("pos")) || has(c, QStringLiteral("postype")) ||
                       (rules11 && (has(c, QStringLiteral("space")) || has(c, QStringLiteral("bindid"))));
    if (!create && !moved)
        return;
    if (create)
        e.posType = rules11 ? Explod::PosType::None : Explod::PosType::P1;
    if (const Param* posType = c.param(QStringLiteral("postype"))) {
        const QString kind = posType->text.trimmed().toLower();
        e.posType = kind == QLatin1String("p2")                ? Explod::PosType::P2
                    : kind == QLatin1String("front")           ? Explod::PosType::Front
                    : kind == QLatin1String("back")            ? Explod::PosType::Back
                    : kind == QLatin1String("left")            ? Explod::PosType::Left
                    : kind == QLatin1String("right")           ? Explod::PosType::Right
                    : rules11 && kind == QLatin1String("none") ? Explod::PosType::None
                                                               : Explod::PosType::P1;
    }
    if (rules11) {
        if (const Param* space = c.param(QStringLiteral("space")))
            e.screenSpace = space->text.trimmed().compare(QLatin1String("screen"), Qt::CaseInsensitive) == 0;
        e.bindId = intParam(c, QStringLiteral("bindid"), e.bindId);
    }
    e.relative = pair(QStringLiteral("pos"), create ? Vec2() : e.relative);
    // random = rx, ry: anywhere from -r/2 to r/2 - 1.
    if (create && m_match) {
        for (int axis = 0; axis < 2; ++axis) {
            const int range = intParam(c, QStringLiteral("random"), 0, axis);
            if (range > 0)
                (axis == 0 ? e.relative.x : e.relative.y) += float(m_match->random(range) - range / 2);
        }
    }
    placeExplod(e);
}

void Character::placeExplod(Explod& e)
{
    e.screen = false;
    e.bindTo = nullptr;
    switch (e.posType) {
    case Explod::PosType::P1:
    case Explod::PosType::P2: {
        Character* at = this;
        if (e.posType == Explod::PosType::P2 && m_match && m_match->enemy(this))
            at = m_match->enemy(this);
        e.bindTo = at;
        e.facing = at->m_facing;
        e.relative.x *= float(at->m_facing);
        e.pos = at->m_pos;
        break;
    }
    case Explod::PosType::Front:
    case Explod::PosType::Back: {
        if (e.posType == Explod::PosType::Back)
            e.facing = m_facing;
        const bool right = (m_facing > 0) == (e.posType == Explod::PosType::Front);
        if (e.posType == Explod::PosType::Back && right)
            e.relative.x = -e.relative.x;
        e.posType = right ? Explod::PosType::Right : Explod::PosType::Left;
        e.screen = true;
        e.pos = Vec2{right && m_match ? m_match->screenWidth() : right ? 320.f : 0.f, 0};
        break;
    }
    case Explod::PosType::Left:
    case Explod::PosType::Right:
        e.screen = true;
        e.pos = Vec2{e.posType == Explod::PosType::Right ? (m_match ? m_match->screenWidth() : 320.f) : 0.f, 0};
        break;
    case Explod::PosType::None: {
        e.screen = e.screenSpace;
        e.facing = 1;
        e.pos = Vec2();
        Character* at = nullptr;
        if (!e.screen && e.bindId != -2)
            at = e.bindId == -1 ? this : m_match ? m_match->playerById(e.bindId) : nullptr;
        if (at) {
            e.bindTo = at;
            e.pos = at->m_pos;
        }
        break;
    }
    }
    e.next = e.pos;
}

bool Character::executeExplod(const Controller& c)
{
    const QString& type = c.type;
    // Only the character's own explods. A helper that went away left its explods to its root.
    auto withId = [this, &c](auto apply) {
        const int id = intParam(c, QStringLiteral("id"), -1);
        for (Explod& e : m_explods) {
            if (e.ownerId == m_id && (id < 0 || e.id == id))
                apply(e);
        }
    };
    if (type == QLatin1String("explod")) {
        Explod e;
        e.ownerId = m_id;
        const Param* anim = c.param(QStringLiteral("anim"));
        e.common = anim && anim->prefix == QLatin1Char('F');
        e.ownPal = e.common;
        e.animNo = intParam(c, QStringLiteral("anim"), 0);
        e.animData = e.common ? (m_match ? m_match->fightFx() : nullptr) : &m_data;
        const AirAnimation* animation = e.animData ? e.animData->animation(e.animNo) : nullptr;
        if (!animation) {
            if (!e.common)
                m_messages
                    << QStringLiteral("explod: no action %1 (%2, line %3)").arg(e.animNo).arg(c.file).arg(c.line + 1);
            return true;
        }
        e.anim = animPlayerFor(*animation);
        readExplod(c, e, true);
        m_explods << e;
        return true;
    }
    if (type == QLatin1String("modifyexplod")) {
        withId([this, &c](Explod& e) { readExplod(c, e, false); });
        return true;
    }
    if (type == QLatin1String("removeexplod")) {
        const int id = intParam(c, QStringLiteral("id"), -1);
        const int owner = m_id;
        m_explods.removeIf([id, owner](const Explod& e) { return e.ownerId == owner && (id < 0 || e.id == id); });
        return true;
    }
    if (type == QLatin1String("explodbindtime")) {
        const QString key = has(c, QStringLiteral("time")) ? QStringLiteral("time") : QStringLiteral("value");
        const int time = intParam(c, key, 1);
        withId([time](Explod& e) { e.bindTime = time; });
        return true;
    }
    return false;
}

void Character::updateExplods()
{
    for (int i = 0; i < m_explods.size();) {
        Explod& e = m_explods[i];
        if ((e.removeTime >= 0 && e.time >= e.removeTime) || (e.removeTime <= -2 && e.anim.ended()) ||
            (e.removeOnGetHit && m_wasHit)) {
            m_explods.removeAt(i);
            continue;
        }
        bool paused = false;
        if (m_match && m_match->superPauseTime() > 0)
            paused = (e.superMoveTime >= 0 && e.time >= e.superMoveTime) || e.superMoveTime < -2;
        else if (m_match && m_match->pauseTime() > 0)
            paused = (e.pauseMoveTime >= 0 && e.time >= e.pauseMoveTime) || e.pauseMoveTime < -2;
        if (!paused && !e.ignoreHitPause && m_hitPauseTime > 0)
            paused = true;
        e.paused = paused;
        if (!paused) {
            e.pos = e.bindTime != 0 && e.bindTo ? e.bindTo->m_pos : e.next;
            ++e.time;
            if (e.bindTime > 0)
                --e.bindTime;
            e.next = Vec2{e.pos.x + e.vel.x * float(e.facing), e.pos.y + e.vel.y};
            e.vel.x += e.accel.x;
            e.vel.y += e.accel.y;
            e.anim.step();
        }
        ++i;
    }
}

int Character::numExplods(int id) const
{
    const int owner = m_id;
    return int(std::count_if(m_explods.cbegin(), m_explods.cend(),
                             [id, owner](const Explod& e) { return e.ownerId == owner && (id < 0 || e.id == id); }));
}

} // namespace OFS::Mugen::Runtime

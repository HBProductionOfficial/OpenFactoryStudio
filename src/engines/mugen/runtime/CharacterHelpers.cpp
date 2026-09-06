#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"
#include "engines/mugen/runtime/IniText.h"

#include <algorithm>

namespace OFS::Mugen::Runtime {

using namespace Detail;

bool Character::executeHelper(const Controller& c)
{
    const QString& type = c.type;
    if (type == QLatin1String("helper")) {
        if (!m_match)
            return true;
        Character* h = m_match->spawnHelper(*this);
        if (!h) {
            m_messages << QStringLiteral("too many helpers (%1, line %2)").arg(c.file).arg(c.line + 1);
            return true;
        }
        const Param* helperType = c.param(QStringLiteral("helpertype"));
        h->m_helperPlayerType = helperType && unquote(helperType->text.trimmed()).toLower() == QLatin1String("player");
        h->m_helperId = std::max(0, intParam(c, QStringLiteral("id"), 0));
        h->m_keyCtrl = intParam(c, QStringLiteral("keyctrl"), 0) != 0;
        h->m_superMoveTime = intParam(c, QStringLiteral("supermovetime"), 0);
        h->m_pauseMoveTime = intParam(c, QStringLiteral("pausemovetime"), 0);
        h->m_constants = m_constants;
        for (const char* key : {"size.xscale", "size.yscale", "size.ground.back", "size.ground.front", "size.air.back",
                                "size.air.front", "size.height", "size.proj.doscale", "size.shadowoffset"}) {
            if (has(c, QLatin1String(key)))
                h->m_constants.insert(QLatin1String(key), floatParam(c, QLatin1String(key), 0));
        }
        for (const char* key : {"size.head.pos", "size.mid.pos"}) {
            if (!has(c, QLatin1String(key)))
                continue;
            h->m_constants.insert(QLatin1String(key) + QLatin1String(".x"), floatParam(c, QLatin1String(key), 0, 0));
            h->m_constants.insert(QLatin1String(key) + QLatin1String(".y"), floatParam(c, QLatin1String(key), 0, 1));
        }

        // facing -1 faces away from the way it is placed.
        const float x = floatParam(c, QStringLiteral("pos"), 0, 0);
        const float y = floatParam(c, QStringLiteral("pos"), 0, 1);
        const int turn = intParam(c, QStringLiteral("facing"), 1) < 0 ? -1 : 1;
        const QString posType = c.param(QStringLiteral("postype"))
                                    ? c.param(QStringLiteral("postype"))->text.trimmed().toLower()
                                    : QStringLiteral("p1");
        const float half = m_match->screenWidth() / 2;
        const float left = m_match->cameraX() - half;
        const float right = m_match->cameraX() + half;
        Vec2 at;
        int facing = turn;
        if (posType == QLatin1String("p2") && m_match->enemy(this)) {
            const Character* p2 = m_match->enemy(this);
            at = Vec2{p2->m_pos.x + x * float(p2->m_facing), p2->m_pos.y + y};
            facing *= p2->m_facing;
        } else if (posType == QLatin1String("front") || posType == QLatin1String("back")) {
            const bool onRight = (m_facing > 0) == (posType == QLatin1String("front"));
            at = Vec2{(onRight ? right : left) + x * float(m_facing), y};
            facing *= m_facing;
        } else if (posType == QLatin1String("left") || posType == QLatin1String("right")) {
            at = Vec2{(posType == QLatin1String("left") ? left : right) + x, y};
        } else {
            at = Vec2{m_pos.x + x * float(m_facing), m_pos.y + y};
            facing *= m_facing;
        }
        h->m_pos = at;
        h->m_vel = Vec2();
        h->m_facing = facing;

        // Its first state is entered when it first acts (later this tick), with animation 0 and control.
        if (m_data.animation(0))
            h->changeAnim(0);
        h->m_ctrl = true;
        h->m_minus = 3;
        h->changeState(intParam(c, QStringLiteral("stateno"), 0));
        return true;
    }
    if (type == QLatin1String("bindtoparent") || type == QLatin1String("bindtoroot")) {
        Character* to = type == QLatin1String("bindtoparent") ? m_parent : m_root;
        if (!m_isHelper || !to)
            return true;
        m_bindTo = to;
        m_bindIsTarget = false;
        m_bindTime = intParam(c, QStringLiteral("time"), 1);
        m_bindTurn = intParam(c, QStringLiteral("facing"), 0);
        m_bindPos = Vec2{floatParam(c, QStringLiteral("pos"), 0, 0), floatParam(c, QStringLiteral("pos"), 0, 1)};
        if (m_bindTime == 0)
            m_bindTo = nullptr;
        return true;
    }
    if (type == QLatin1String("bindtotarget")) {
        const QList<Character*> targets = targetsWithId(intParam(c, QStringLiteral("id"), -1));
        if (targets.isEmpty())
            return true;
        Character* t = targets.first();
        Vec2 offset{floatParam(c, QStringLiteral("pos"), 0, 0), floatParam(c, QStringLiteral("pos"), 0, 1)};
        const Param* pos = c.param(QStringLiteral("pos"));
        const QString part = pos ? pos->tail : QString();
        if (part == QLatin1String("mid") || part == QLatin1String("head")) {
            offset.x += t->constant(QStringLiteral("size.") + part + QStringLiteral(".pos.x"), 0);
            offset.y += t->constant(QStringLiteral("size.") + part + QStringLiteral(".pos.y"), 0);
        }
        m_bindTo = t;
        m_bindIsTarget = false;
        m_bindTime = intParam(c, QStringLiteral("time"), 1);
        m_bindTurn = 0;
        m_bindPos = offset;
        if (m_bindTime == 0)
            m_bindTo = nullptr;
        return true;
    }
    return false;
}

} // namespace OFS::Mugen::Runtime

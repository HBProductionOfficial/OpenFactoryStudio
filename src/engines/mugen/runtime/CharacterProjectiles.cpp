#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterDetail.h"

#include <algorithm>

namespace OFS::Mugen::Runtime {

using namespace Detail;

QList<QRectF> Detail::clsnBoxes(const AirAnimation* animation, const AnimPlayer& player, int type, Vec2 pos, int facing)
{
    QList<QRectF> rects;
    if (!animation || player.isEmpty())
        return rects;
    for (const AirBox& box : animation->boxes(player.frame(), type)) {
        float left = float(std::min(box.left, box.right));
        float right = float(std::max(box.left, box.right));
        if (facing < 0) {
            const float l = -right;
            right = -left;
            left = l;
        }
        const float top = float(std::min(box.top, box.bottom));
        const float bottom = float(std::max(box.top, box.bottom));
        rects << QRectF(pos.x + left, pos.y + top, right - left, bottom - top);
    }
    return rects;
}

bool Detail::boxesMeet(const QList<QRectF>& a, const QList<QRectF>& b)
{
    for (const QRectF& r : a) {
        for (const QRectF& s : b) {
            if (std::min(r.right(), s.right()) >= std::max(r.left(), s.left()) &&
                std::min(r.bottom(), s.bottom()) >= std::max(r.top(), s.top()))
                return true;
        }
    }
    return false;
}

const CharacterData* Character::projectileAnimData(const Projectile::Anim& anim) const
{
    if (anim.number < 0)
        return nullptr;
    return anim.common ? (m_match ? m_match->fightFx() : nullptr) : &m_data;
}

bool Character::showProjectileAnim(Projectile& p, const Projectile::Anim& anim)
{
    p.shown = anim;
    p.animData = projectileAnimData(anim);
    const AirAnimation* animation = p.animData ? p.animData->animation(anim.number) : nullptr;
    if (!animation) {
        p.animData = nullptr;
        p.player = AnimPlayer();
        return false;
    }
    p.player = animPlayerFor(*animation);
    return true;
}

QList<QRectF> Character::projectileBoxes(const Projectile& p, int type) const
{
    const AirAnimation* animation = p.animData ? p.animData->animation(p.shown.number) : nullptr;
    return clsnBoxes(animation, p.player, type, p.pos, p.facing);
}

bool Character::executeProjectile(const Controller& c)
{
    if (c.type != QLatin1String("projectile"))
        return false;
    Character* owner = m_root ? m_root : this;
    Projectile p;
    p.ownerId = owner->m_id;
    p.hitDef = hitDefFrom(c);
    p.id = intParam(c, QStringLiteral("projid"), 0);
    auto anim = [this, &c](const QString& name, Projectile::Anim fallback) {
        const Param* param = c.param(name);
        if (!param)
            return fallback;
        return Projectile::Anim{intParam(c, name, -1), param->prefix == QLatin1Char('F')};
    };
    p.anim = anim(QStringLiteral("projanim"), Projectile::Anim{0, false});
    p.hitAnim = anim(QStringLiteral("projhitanim"), Projectile::Anim{-1, false});
    p.remAnim = anim(QStringLiteral("projremanim"), p.hitAnim);
    p.cancelAnim = anim(QStringLiteral("projcancelanim"), p.remAnim);
    auto pair = [this, &c](const QString& name, Vec2 fallback) {
        return Vec2{floatParam(c, name, fallback.x, 0), floatParam(c, name, fallback.y, 1)};
    };
    p.scale = pair(QStringLiteral("projscale"), p.scale);
    p.remove = intParam(c, QStringLiteral("projremove"), 1) != 0;
    p.removeTime = intParam(c, QStringLiteral("projremovetime"), -1);
    p.vel = pair(QStringLiteral("velocity"), Vec2());
    p.remVel = pair(QStringLiteral("remvelocity"), Vec2());
    p.accel = pair(QStringLiteral("accel"), Vec2());
    p.velMul = pair(QStringLiteral("velmul"), p.velMul);
    p.hits = intParam(c, QStringLiteral("projhits"), 1);
    p.missTime = intParam(c, QStringLiteral("projmisstime"), 0);
    p.priority = p.priorityPoints = intParam(c, QStringLiteral("projpriority"), 1);
    p.sprPriority = intParam(c, QStringLiteral("projsprpriority"), 3);
    p.edgeBound = intParam(c, QStringLiteral("projedgebound"), 40);
    p.stageBound = intParam(c, QStringLiteral("projstagebound"), 40);
    p.heightBound[0] = intParam(c, QStringLiteral("projheightbound"), -240, 0);
    p.heightBound[1] = intParam(c, QStringLiteral("projheightbound"), 1, 1);
    p.superMoveTime = intParam(c, QStringLiteral("supermovetime"), 0);
    p.pauseMoveTime = intParam(c, QStringLiteral("pausemovetime"), 0);

    const Vec2 offset = pair(QStringLiteral("offset"), Vec2());
    const QString posType = c.param(QStringLiteral("postype"))
                                ? c.param(QStringLiteral("postype"))->text.trimmed().toLower()
                                : QStringLiteral("p1");
    const float half = m_match ? m_match->screenWidth() / 2 : 160;
    const float camera = m_match ? m_match->cameraX() : 0;
    p.facing = m_facing;
    if (posType == QLatin1String("p2") && m_match && m_match->enemy(this)) {
        const Character* p2 = m_match->enemy(this);
        p.pos = Vec2{p2->m_pos.x + offset.x * float(p2->m_facing), p2->m_pos.y + offset.y};
    } else if (posType == QLatin1String("front") || posType == QLatin1String("back")) {
        const bool right = (m_facing > 0) == (posType == QLatin1String("front"));
        p.pos = Vec2{camera + (right ? half : -half) + offset.x * float(m_facing), offset.y};
    } else if (posType == QLatin1String("left") || posType == QLatin1String("right")) {
        p.pos = Vec2{camera + (posType == QLatin1String("left") ? -half : half) + offset.x, offset.y};
    } else {
        p.pos = Vec2{m_pos.x + offset.x * float(m_facing), m_pos.y + offset.y};
    }
    if (!showProjectileAnim(p, p.anim))
        m_messages
            << QStringLiteral("projectile: no action %1 (%2, line %3)").arg(p.anim.number).arg(c.file).arg(c.line + 1);
    owner->m_projectiles << p;
    return true;
}

void Character::updateProjectiles()
{
    const float half = m_match ? m_match->screenWidth() / 2 : 160;
    const float camera = m_match ? m_match->cameraX() : 0;
    for (Projectile& p : m_projectiles) {
        if (p.gone)
            continue;
        p.paused = false;
        if (m_match && m_match->superPauseTime() > 0)
            p.paused = p.superMoveTime == 0 || p.superMoveTime < -1;
        else if (m_match && m_match->pauseTime() > 0)
            p.paused = p.pauseMoveTime == 0 || p.pauseMoveTime < -1;
        if (!p.paused && p.hitPause == 0) {
            if (p.status == Projectile::Status::Active) {
                const float left = camera - half;
                const float right = camera + half;
                const float stageLeft = m_match ? m_match->stageLeft() : -1000;
                const float stageRight = m_match ? m_match->stageRight() : 1000;
                if (p.removeTime == 0 || (p.removeTime <= -2 && p.player.ended()) ||
                    p.pos.x < left - float(p.edgeBound) || p.pos.x > right + float(p.edgeBound) ||
                    (p.vel.x * float(p.facing) < 0 && p.pos.x < stageLeft - float(p.stageBound)) ||
                    (p.vel.x * float(p.facing) > 0 && p.pos.x > stageRight + float(p.stageBound)) ||
                    (p.vel.y > 0 && p.pos.y > float(p.heightBound[1])) ||
                    (p.vel.y < 0 && p.pos.y < float(p.heightBound[0])))
                    p.status = Projectile::Status::Removed;
            }
            if (p.status != Projectile::Status::Active && !p.removeStarted) {
                p.removeStarted = true;
                const Projectile::Anim last = p.status == Projectile::Status::Hit         ? p.hitAnim
                                              : p.status == Projectile::Status::Cancelled ? p.cancelAnim
                                                                                          : p.remAnim;
                if (last.number != p.shown.number || last.common != p.shown.common)
                    showProjectileAnim(p, last);
                p.vel = p.remVel;
                p.accel = Vec2();
                p.velMul = Vec2{1, 1};
            }
            if (p.status != Projectile::Status::Active &&
                (!p.animData || p.player.totalTime() <= 0 || p.player.animTime() == 0))
                p.gone = true;
        }
        if (p.gone || p.paused || p.hitPause > 0 || p.frozen)
            continue;
        p.pos.x += p.vel.x * float(p.facing);
        p.pos.y += p.vel.y;
        p.vel.x = (p.vel.x + p.accel.x) * p.velMul.x;
        p.vel.y = (p.vel.y + p.accel.y) * p.velMul.y;
        // Moving backwards turns it around.
        if (p.vel.x < 0 && p.shown.number != -1) {
            p.facing = -p.facing;
            p.vel.x = -p.vel.x;
            p.accel.x = -p.accel.x;
        }
    }
}

void Character::cancelProjectiles(Character& other)
{
    // Projectiles that meet cancel each other. The one with more priority only loses a priority point.
    auto lose = [](Projectile& p, const Projectile& against, Character& owner) {
        if (p.priorityPoints > against.priorityPoints)
            --p.priorityPoints;
        else
            --p.hits;
        if (p.hits <= 0) {
            p.status = Projectile::Status::Cancelled;
            owner.m_projContact = ProjContactKind::Cancel;
            owner.m_projContactTime = 0;
            owner.m_projContactId = p.id;
        }
        p.hitPause = p.hits > 0 ? std::max(0, p.hitDef.pauseTime[0]) : 0;
    };
    for (Projectile& mine : m_projectiles) {
        for (Projectile& theirs : other.m_projectiles) {
            if (mine.gone || theirs.gone || mine.status != Projectile::Status::Active ||
                theirs.status != Projectile::Status::Active || mine.hits < 0 || theirs.hits < 0)
                continue;
            if (!boxesMeet(projectileBoxes(mine, 2), other.projectileBoxes(theirs, 2)))
                continue;
            const Projectile before = mine;
            lose(mine, theirs, *this);
            lose(theirs, before, other);
        }
    }
}

void Character::projectilesAttack(Character& target)
{
    for (Projectile& p : m_projectiles) {
        if (p.gone || p.status != Projectile::Status::Active || p.paused || p.hits <= 0 || p.curMissTime > 0 ||
            p.hitPause > 0 || p.contact)
            continue;
        if (!hitAllowed(p.hitDef, target))
            continue;
        if (!boxesMeet(projectileBoxes(p, 1), target.boxes(2)))
            continue;
        const bool guarded = deliverHit(target, p.hitDef, p.facing);
        p.contact = true;
        p.hitPause = std::max(0, guarded ? p.hitDef.guardPauseTime[0] : p.hitDef.pauseTime[0]);
        m_projContact = guarded ? ProjContactKind::Guarded : ProjContactKind::Hit;
        m_projContactTime = 0;
        m_projContactId = p.id;
    }
}

void Character::tickProjectiles()
{
    for (Projectile& p : m_projectiles) {
        if (p.gone)
            continue;
        if (p.contact) {
            p.contact = false;
            p.curMissTime = std::max(0, p.missTime);
            if (p.hits >= 0 && --p.hits <= 0 && p.remove)
                p.status = Projectile::Status::Hit;
        }
        if (p.paused)
            continue;
        if (p.hitPause <= 0) {
            ++p.time;
            if (p.removeTime > 0)
                --p.removeTime;
            if (p.curMissTime > 0)
                --p.curMissTime;
            if (p.superMoveTime > 0)
                --p.superMoveTime;
            if (p.pauseMoveTime > 0)
                --p.pauseMoveTime;
            p.frozen = false;
            p.player.step();
        } else {
            --p.hitPause;
            p.frozen = true;
        }
    }
    m_projectiles.removeIf([](const Projectile& p) { return p.gone; });
}

int Character::numProjectiles(int id) const
{
    return int(std::count_if(m_projectiles.cbegin(), m_projectiles.cend(),
                             [id](const Projectile& p) { return !p.gone && (id < 0 || p.id == id); }));
}

int Character::projContactTime(ProjContactKind kind, int id) const
{
    const Character* owner = m_root ? m_root : this;
    if (owner->m_projContactTime < 0 || (id > 0 && id != owner->m_projContactId))
        return -1;
    const ProjContactKind last = owner->m_projContact;
    const bool match = kind == ProjContactKind::Contact ? last != ProjContactKind::Cancel : last == kind;
    return match ? owner->m_projContactTime : -1;
}

} // namespace OFS::Mugen::Runtime

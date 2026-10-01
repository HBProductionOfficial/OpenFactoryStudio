#pragma once

#include "engines/mugen/runtime/Value.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <span>
#include <vector>

namespace OFS::Mugen::Runtime {

// The engine's triggers (M.U.G.E.N 1.1), as the expression compiler knows them.
enum class Trigger : std::uint16_t
{
    // Math
    Abs,
    ACos,
    ASin,
    ATan,
    Ceil,
    Cos,
    E,
    Exp,
    Floor,
    Ln,
    Log,
    Pi,
    Sin,
    Tan,
    IfElse,
    Cond,
    // Character state
    AILevel,
    Alive,
    Anim,
    AnimElem,
    AnimElemNo,
    AnimElemTime,
    AnimExist,
    AnimTime,
    AuthorName,
    BackEdge,
    BackEdgeBodyDist,
    BackEdgeDist,
    BottomEdge,
    CameraPos,
    CameraZoom,
    CanRecover,
    Command,
    Const,
    Const240p,
    Const480p,
    Const720p,
    Ctrl,
    DrawGame,
    Facing,
    FrontEdge,
    FrontEdgeBodyDist,
    FrontEdgeDist,
    FVar,
    GameHeight,
    GameTime,
    GameWidth,
    GetHitVar,
    HitCount,
    HitDefAttr,
    HitFall,
    HitOver,
    HitPauseTime,
    HitShakeOver,
    HitVel,
    ID,
    InGuardDist,
    IsHelper,
    IsHomeTeam,
    LeftEdge,
    Life,
    LifeMax,
    Lose,
    LoseKO,
    LoseTime,
    MatchNo,
    MatchOver,
    MoveContact,
    MoveGuarded,
    MoveHit,
    MoveReversed,
    MoveType,
    Name,
    NumEnemy,
    NumExplod,
    NumHelper,
    NumPartner,
    NumProj,
    NumProjID,
    NumTarget,
    P1Name,
    P2BodyDist,
    P2Dist,
    P2Life,
    P2MoveType,
    P2Name,
    P2StateNo,
    P2StateType,
    P3Name,
    P4Name,
    PalNo,
    ParentDist,
    PlayerIDExist,
    Pos,
    Power,
    PowerMax,
    PrevStateNo,
    ProjCancelTime,
    ProjContact,
    ProjContactTime,
    ProjGuarded,
    ProjGuardedTime,
    ProjHit,
    ProjHitTime,
    Random,
    RightEdge,
    RootDist,
    RoundNo,
    RoundsExisted,
    RoundState,
    ScreenHeight,
    ScreenPos,
    ScreenWidth,
    SelfAnimExist,
    StageVar,
    StateNo,
    StateType,
    SysFVar,
    SysVar,
    TeamMode,
    TeamSide,
    TicksPerSecond,
    Time,
    TimeMod,
    TopEdge,
    UniqHitCount,
    Var,
    Vel,
    Win,
    WinKO,
    WinPerfect,
    WinTime,
};

// Where a redirection points, in the order of Mugen::RedirectKeywords.
enum class Redirect : std::uint8_t
{
    Parent,
    Root,
    Helper,
    Target,
    Partner,
    Enemy,
    EnemyNear,
    PlayerID,
};

// The operator of a special form's optional second part ("AnimElem = 3, >= 2"), else Equal.
enum class Compare : std::uint8_t
{
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

struct TriggerCall
{
    Trigger trigger = Trigger::Time;
    std::span<const Value> args;
    QString keyword;
    QString text;
    QStringList list;
    bool special = false;
    bool negate = false;
    bool hasSecond = false;
    Compare second = Compare::Equal;
};

class Context
{
public:
    virtual ~Context() = default;
    virtual Value trigger(const TriggerCall& call) = 0;
    virtual Context* redirect(Redirect kind, const Value& argument) = 0;
    virtual Value assign(Trigger kind, std::int32_t index, const Value& value) = 0;
};

struct Node;

// Empty when it could not be compiled (error() tells why). An empty expression evaluates as undefined.
class Expr
{
public:
    Expr();
    ~Expr();
    Expr(const Expr&);
    Expr& operator=(const Expr&);

    static Expr compile(const QString& text, QString* error = nullptr);
    static std::vector<Expr> compileList(const QString& text, QString* error = nullptr);
    static Expr constant(Value value);

    bool isEmpty() const { return !m_root; }
    Value evaluate(Context& context) const;
    bool isConstant() const;

private:
    std::shared_ptr<const Node> m_root;
};

namespace Ops {
Value negate(Value v);
Value logicalNot(Value v);
Value bitNot(Value v);
Value add(Value a, Value b);
Value subtract(Value a, Value b);
Value multiply(Value a, Value b);
Value divide(Value a, Value b);
Value modulo(Value a, Value b);
Value power(Value a, Value b);
Value compare(Value a, Value b, Compare op);
Value interval(Value x, Value low, Value high, bool lowOpen, bool highOpen);
} // namespace Ops

} // namespace OFS::Mugen::Runtime

#pragma once

#include "core/anims/AirDocument.h"
#include "engines/mugen/runtime/Commands.h"
#include "engines/mugen/runtime/Expr.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

namespace OFS::Mugen::Runtime {

struct IniSection;

// A parameter's text, and its comma-separated values compiled as expressions (empty when the text is
// not an expression list, like "attr = S, NA").
struct Param
{
    QString text;
    std::vector<Expr> values;
    int line = 0;
    QChar prefix; // 'F' (the engine's common files) or 'S' (the character's own) before the first value
    QString tail;

    bool isExpression() const { return !values.empty(); }
};

struct Controller
{
    QString label;
    QString type;
    QString file;
    int line = 0;
    std::vector<Expr> triggerAll;
    // trigger1, trigger2...: each a group of lines that must all hold. The controller runs when every
    // triggerall line and one group hold. Numbering stops at the first missing number.
    std::vector<std::vector<Expr>> triggers;
    std::vector<int> triggerAllLines;
    std::vector<std::vector<int>> triggerLines;
    QHash<QString, Param> params;
    // persistent = 0: once each time the state is entered. 1: whenever triggered. n: every n times.
    int persistent = 1;
    bool ignoreHitPause = false;

    const Param* param(const QString& name) const;
};

struct StateDef
{
    int number = 0;
    QString file;
    int line = 0;
    QHash<QString, Param> params;
    std::vector<Controller> controllers;

    const Param* param(const QString& name) const;
};

struct LoadIssue
{
    QString file;
    int line = 0;
    QString message;
};

class CharacterData
{
public:
    CharacterData();

    // States come from st, st0 to st9, the command file, then the common states. The first definition
    // of a state wins.
    bool load(const QString& defPath, const QString& engineDir = QString(), QString* error = nullptr);
    using TextSource = std::function<std::optional<QString>(const QString& path)>;
    void setTextSource(TextSource source) { m_source = std::move(source); }
    QString name() const { return m_name; }
    QString displayName() const { return m_displayName; }
    QString mugenVersion() const { return m_mugenVersion; }
    void setMugenVersion(const QString& version) { m_mugenVersion = version; }
    bool isMugen11() const;
    int localCoordWidth() const { return m_localCoordWidth; }
    QString spriteFile() const { return m_spriteFile; }
    QString soundFile() const { return m_soundFile; }
    const QStringList& paletteFiles() const { return m_paletteFiles; }

    // Constants are named as Const() reads them: "data.life", "size.head.pos.x", "velocity.jump.neu.y"
    // (also "velocity.jump.y").
    void addConstants(const QString& text, const QString& file = QString());
    // The character's own files replace a state already loaded. The common states, loaded later, do not.
    void addStates(const QString& text, const QString& file = QString(), bool replace = true);

    float constant(const QString& name, float fallback = 0) const;
    bool hasConstant(const QString& name) const;
    void setConstant(const QString& name, float value);

    void setCommands(const QString& text, const QString& file = QString());
    const CommandList& commands() const { return m_commands; }

    void setAnimations(const QString& text);
    const AirAnimation* animation(int number) const;

    const QMap<int, StateDef>& states() const { return m_states; }
    const StateDef* state(int number) const;
    const QList<LoadIssue>& issues() const { return m_issues; }

private:
    void addSection(const IniSection& section, const QString& prefix);
    Param compileParam(const QString& text, int line, const QString& file, bool report);

    QString m_name;
    QString m_displayName;
    QString m_mugenVersion;
    int m_localCoordWidth = 320;
    QString m_spriteFile;
    QString m_soundFile;
    QStringList m_paletteFiles;
    QHash<QString, float> m_constants;
    AirDocument m_animations;
    CommandList m_commands;
    QMap<int, StateDef> m_states;
    QList<LoadIssue> m_issues;
    TextSource m_source;
};

} // namespace OFS::Mugen::Runtime

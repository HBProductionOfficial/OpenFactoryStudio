#include "engines/mugen/runtime/CharacterData.h"

#include "engines/mugen/runtime/IniText.h"

#include <QRegularExpression>

namespace OFS::Mugen::Runtime {

namespace {

const QList<QPair<const char*, float>>& defaults()
{
    static const QList<QPair<const char*, float>> values = {
        {"data.life", 1000},
        {"data.power", 3000},
        {"data.attack", 100},
        {"data.defence", 100},
        {"data.fall.defence_up", 50},
        {"data.liedown.time", 60},
        {"data.airjuggle", 15},
        {"data.sparkno", 2},
        {"data.guard.sparkno", 40},
        {"data.ko.echo", 0},
        {"data.volume", 0},
        {"data.intpersistindex", 60},
        {"data.floatpersistindex", 40},
        {"size.xscale", 1},
        {"size.yscale", 1},
        {"size.ground.back", 15},
        {"size.ground.front", 16},
        {"size.air.back", 12},
        {"size.air.front", 12},
        {"size.height", 60},
        {"size.attack.dist", 160},
        {"size.proj.attack.dist", 90},
        {"size.proj.doscale", 0},
        {"size.head.pos.x", -5},
        {"size.head.pos.y", -90},
        {"size.mid.pos.x", -5},
        {"size.mid.pos.y", -60},
        {"size.shadowoffset", 0},
        {"size.draw.offset.x", 0},
        {"size.draw.offset.y", 0},
        {"velocity.air.gethit.groundrecover.x", -0.15f},
        {"velocity.air.gethit.groundrecover.y", -3.5f},
        {"velocity.air.gethit.airrecover.mul.x", 0.5f},
        {"velocity.air.gethit.airrecover.mul.y", 0.2f},
        {"velocity.air.gethit.airrecover.add.x", 0},
        {"velocity.air.gethit.airrecover.add.y", -4.5f},
        {"velocity.air.gethit.airrecover.back", -1},
        {"velocity.air.gethit.airrecover.fwd", 0},
        {"velocity.air.gethit.airrecover.up", -2},
        {"velocity.air.gethit.airrecover.down", 1.5f},
        {"movement.airjump.num", 0},
        {"movement.airjump.height", 35},
        {"movement.yaccel", 0.44f},
        {"movement.stand.friction", 0.85f},
        {"movement.crouch.friction", 0.82f},
        {"movement.stand.friction.threshold", 2},
        {"movement.crouch.friction.threshold", 0},
        {"movement.air.gethit.groundlevel", 25},
        {"movement.air.gethit.groundrecover.ground.threshold", -20},
        {"movement.air.gethit.groundrecover.groundlevel", 10},
        {"movement.air.gethit.airrecover.threshold", -1},
        {"movement.air.gethit.airrecover.yaccel", 0.35f},
        {"movement.air.gethit.trip.groundlevel", 15},
        {"movement.down.bounce.offset.x", 0},
        {"movement.down.bounce.offset.y", 20},
        {"movement.down.bounce.yaccel", 0.4f},
        {"movement.down.bounce.groundlevel", 12},
        {"movement.down.friction.threshold", 0.05f},
    };
    return values;
}

// Other names Const() accepts for some constants.
const QHash<QString, QString>& aliases()
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("velocity.jump.y"), QStringLiteral("velocity.jump.neu.y")},
        {QStringLiteral("velocity.airjump.y"), QStringLiteral("velocity.airjump.neu.y")},
        {QStringLiteral("velocity.runjump.y"), QStringLiteral("velocity.runjump.fwd.y")},
        {QStringLiteral("size.attack.dist.width.front"), QStringLiteral("size.attack.dist")},
        {QStringLiteral("size.proj.attack.dist.width.front"), QStringLiteral("size.proj.attack.dist")},
    };
    return names;
}

} // namespace

const Param* Controller::param(const QString& name) const
{
    const auto it = params.constFind(name);
    return it != params.cend() ? &*it : nullptr;
}

const Param* StateDef::param(const QString& name) const
{
    const auto it = params.constFind(name);
    return it != params.cend() ? &*it : nullptr;
}

CharacterData::CharacterData()
{
    for (const auto& [name, value] : defaults())
        m_constants.insert(QString::fromLatin1(name), value);
}

void CharacterData::addSection(const IniSection& section, const QString& prefix)
{
    for (const IniEntry& entry : section.entries) {
        const QString key = prefix + QLatin1Char('.') + entry.key.toLower();
        const QStringList parts = entry.value.split(QLatin1Char(','));
        QList<float> numbers;
        bool numeric = true;
        for (const QString& part : parts) {
            bool ok = false;
            const float value = part.trimmed().toFloat(&ok);
            if (!ok && !part.trimmed().isEmpty()) {
                numeric = false;
                break;
            }
            numbers << (ok ? value : 0.0f);
        }
        if (!numeric || numbers.isEmpty())
            continue;
        // One value is read with or without an axis ("velocity.walk.fwd" and ".x"). Several values are read
        // by axis ("size.head.pos.x", ".y").
        static const char* const axes[] = {".x", ".y", ".z"};
        m_constants.insert(key, numbers.first());
        for (int i = 0; i < numbers.size() && i < 3; ++i)
            m_constants.insert(key + QLatin1String(axes[i]), numbers[i]);
    }
}

void CharacterData::addConstants(const QString& text, const QString& file)
{
    Q_UNUSED(file)
    const IniText ini = IniText::parse(text);
    for (const IniSection& section : ini.sections()) {
        const QString name = section.name.toLower();
        if (name == QLatin1String("data") || name == QLatin1String("size") || name == QLatin1String("velocity") ||
            name == QLatin1String("movement"))
            addSection(section, name);
    }
}

Param CharacterData::compileParam(const QString& text, int line, const QString& file, bool report)
{
    Param param;
    param.text = text;
    param.line = line;
    QString error;
    param.values = Expr::compileList(text, &error);
    if (!error.isEmpty()) {
        param.values.clear();
        if (report)
            m_issues << LoadIssue{file, line, error};
    }
    return param;
}

void CharacterData::addStates(const QString& text, const QString& file, bool replace)
{
    static const QRegularExpression statedef(QStringLiteral("^statedef\\s+(-?\\d+)"),
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression stateHeader(QStringLiteral("^state\\s+[^,]*(?:,(.*))?$"),
                                                QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression triggerKey(QStringLiteral("^trigger(\\d+|all)$"),
                                               QRegularExpression::CaseInsensitiveOption);
    const IniText ini = IniText::parse(text);
    StateDef* current = nullptr;
    QMap<int, StateDef> loaded;
    for (const IniSection& section : ini.sections()) {
        if (const QRegularExpressionMatch m = statedef.match(section.name); m.hasMatch()) {
            StateDef def;
            def.number = m.captured(1).toInt();
            def.file = file;
            def.line = section.line;
            for (const IniEntry& entry : section.entries) {
                const QString key = entry.key.toLower();
                const bool letters =
                    key == QLatin1String("type") || key == QLatin1String("movetype") || key == QLatin1String("physics");
                def.params.insert(key, letters ? Param{entry.value, {}, entry.line}
                                               : compileParam(entry.value, entry.line, file, true));
            }
            loaded.insert(def.number, def);
            current = &loaded[def.number];
            continue;
        }
        const QRegularExpressionMatch m = stateHeader.match(section.name);
        if (!m.hasMatch() || !current)
            continue; // [Data], [Size]... or controllers before any state
        Controller controller;
        controller.label = m.captured(1).trimmed();
        controller.file = file;
        controller.line = section.line;
        QMap<int, std::vector<Expr>> groups;
        for (const IniEntry& entry : section.entries) {
            const QString key = entry.key.toLower();
            if (key == QLatin1String("type")) {
                controller.type = entry.value.toLower().trimmed();
                continue;
            }
            if (const QRegularExpressionMatch t = triggerKey.match(key); t.hasMatch()) {
                QString error;
                Expr expr = Expr::compile(entry.value, &error);
                if (!error.isEmpty())
                    m_issues << LoadIssue{file, entry.line, error};
                if (t.captured(1).compare(QLatin1String("all"), Qt::CaseInsensitive) == 0)
                    controller.triggerAll.push_back(expr);
                else
                    groups[t.captured(1).toInt()].push_back(expr);
                continue;
            }
            if (key == QLatin1String("persistent")) {
                controller.persistent = entry.value.toInt();
                continue;
            }
            if (key == QLatin1String("ignorehitpause")) {
                controller.ignoreHitPause = entry.value.toInt() != 0;
                continue;
            }
            controller.params.insert(key, compileParam(entry.value, entry.line, file, false));
        }
        // Triggers count from 1 without gaps.
        for (int n = 1; groups.contains(n); ++n)
            controller.triggers.push_back(groups.value(n));
        if (controller.type.isEmpty())
            m_issues << LoadIssue{file, section.line, QStringLiteral("missing type for the controller")};
        else if (controller.triggers.empty())
            m_issues << LoadIssue{file, section.line, QStringLiteral("missing trigger1 for the controller")};
        current->controllers.push_back(controller);
    }
    for (auto it = loaded.cbegin(); it != loaded.cend(); ++it) {
        if (replace || !m_states.contains(it.key()))
            m_states.insert(it.key(), it.value());
    }
}

float CharacterData::constant(const QString& name, float fallback) const
{
    QString key = name.toLower();
    key = aliases().value(key, key);
    return m_constants.value(key, fallback);
}

bool CharacterData::hasConstant(const QString& name) const
{
    QString key = name.toLower();
    key = aliases().value(key, key);
    return m_constants.contains(key);
}

void CharacterData::setConstant(const QString& name, float value)
{
    m_constants.insert(name.toLower(), value);
}

void CharacterData::setCommands(const QString& text, const QString& file)
{
    m_commands = CommandList();
    QStringList errors;
    m_commands.load(text, &errors);
    for (const QString& error : std::as_const(errors)) {
        const int colon = int(error.indexOf(QLatin1Char(':')));
        m_issues << LoadIssue{file, error.left(colon).toInt() - 1, error.mid(colon + 2)};
    }
}

void CharacterData::setAnimations(const QString& text)
{
    m_animations = AirDocument::parse(text.split(QLatin1Char('\n')));
}

const AirAnimation* CharacterData::animation(int number) const
{
    const int index = m_animations.indexOf(number);
    return index >= 0 ? &m_animations.animations()[index] : nullptr;
}

const StateDef* CharacterData::state(int number) const
{
    const auto it = m_states.constFind(number);
    return it != m_states.cend() ? &*it : nullptr;
}

} // namespace OFS::Mugen::Runtime

#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QStringList>

#include <optional>

namespace OFS {

struct OFS_CORE_EXPORT BgParam
{
    QString key; // lower case
    QString value;
    int line = -1;
};

struct OFS_CORE_EXPORT BgSection
{
    QString header;
    QString name; // "[BG 0]" -> "0"
    int headerLine = -1;
    int lastLine = -1;
    int endLine = -1;
    QList<BgParam> params;

    // The last value written wins.
    std::optional<QString> value(const QString& key) const;
    QList<double> numbers(const QString& key, int count, double fallback = 0) const;
};

struct OFS_CORE_EXPORT BgElement
{
    enum class Type
    {
        Null,
        Normal,
        Animation,
        Parallax,
        Dummy,
    };
    enum class Trans
    {
        None,
        Add,
        Add1,
        AddAlpha,
        Sub,
    };

    BgSection section;
    Type type = Type::Normal;
    int spriteGroup = 0;
    int spriteNumber = 0;
    int action = -1;
    int layer = 0;
    QPointF start;
    QPointF delta{1, 1};
    QPointF velocity;
    Trans trans = Trans::None;
    int alphaSource = 256;
    int alphaDest = 0;
    bool mask = false;
    int tileX = 0;
    int tileY = 0;
    int tileSpacingX = 0;
    int tileSpacingY = 0;
    bool hasWindow = false;
    bool maskWindow = false; // "maskwindow" (1.1): from the top centre, x2,y2 exclusive
    QRect window;
    QPointF windowDelta;
    // Parallax: width (top, bottom) or xscale (top, bottom), whichever is written.
    bool hasWidth = false;
    double widthTop = 0;
    double widthBottom = 0;
    bool hasXScale = false;
    double xScaleTop = 1;
    double xScaleBottom = 1;
    double yScaleStart = 100; // percent
    double yScaleDelta = 0;
    QPointF scaleStart{1, 1};
    QPointF scaleDelta;
    // Sine: amplitude, period (ticks), phase (degrees).
    double sinX[3] = {0, 0, 0};
    double sinY[3] = {0, 0, 0};
    int id = 0;
    bool positionLink = false;
};

struct OFS_CORE_EXPORT BgController
{
    enum class Type
    {
        Null,
        Visible,
        Enable,
        VelSet,
        VelAdd,
        PosSet,
        PosAdd,
        Anim,
        SinX,
        SinY,
    };
    BgSection section;
    Type type = Type::Null;
    int startTime = 0;
    int endTime = 0;
    std::optional<int> loopTime; // none: the definition's
    QList<double> value;
    std::optional<double> x;
    std::optional<double> y;
    std::optional<QList<int>> ids; // overrides the definition's, -1 is every element
};

struct OFS_CORE_EXPORT BgControllerDef
{
    BgSection section;
    int loopTime = -1;
    std::optional<QList<int>> ids;
    QList<BgController> controllers;
};

// "[<prefix>Def]" (BGdef in stages, TitleBGDef... in motifs, BG<n>Def in storyboards) and the
// "[<prefix> name]", "[<prefix>CtrlDef name]" and "[<prefix>Ctrl name]" sections after it.
struct OFS_CORE_EXPORT BgGroup
{
    QString prefix;
    BgSection def;
    QList<BgElement> elements;
    QList<BgControllerDef> controllerDefs;
};

class OFS_CORE_EXPORT BgDocument
{
public:
    static BgDocument parse(const QStringList& lines);

    const QList<BgGroup>& groups() const { return m_groups; }
    int indexOf(const QString& prefix) const;
    const QList<BgSection>& sections() const { return m_sections; }
    const BgSection* section(const QString& header) const;

    static BgElement::Type parseType(const QString& text);
    static BgController::Type parseControllerType(const QString& text);

private:
    QList<BgGroup> m_groups;
    QList<BgSection> m_sections;
};

} // namespace OFS

#include "core/theme/StyleSheet.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <cmath>

namespace OFS {

QString StyleSheet::preprocess(const QString& source, double scale, const QString& baseDir)
{
    static const QRegularExpression dpiToken(QStringLiteral(R"(\[\s*(-?\d+(?:\.\d+)?)\s*\])"));
    static const QRegularExpression urlToken(QStringLiteral(R"(url\(\s*(['"]?)([^)'"]+)\1\s*\))"));

    QString out;
    out.reserve(source.size() + 64);

    qsizetype last = 0;
    for (auto it = dpiToken.globalMatch(source); it.hasNext();) {
        const auto m = it.next();
        out += QStringView(source).mid(last, m.capturedStart() - last);
        const double value = m.captured(1).toDouble() * scale;
        long scaled = std::lround(value);
        // Keep hairlines visible: a non-zero size never rounds down to zero.
        if (scaled == 0 && value != 0.0)
            scaled = value > 0 ? 1 : -1;
        out += QString::number(scaled);
        last = m.capturedEnd();
    }
    out += QStringView(source).mid(last);

    const QString base = QDir::fromNativeSeparators(baseDir);
    QString result;
    result.reserve(out.size() + 256);
    last = 0;
    for (auto it = urlToken.globalMatch(out); it.hasNext();) {
        const auto m = it.next();
        result += QStringView(out).mid(last, m.capturedStart() - last);
        QString path = m.captured(2).trimmed();
        // Drive-letter paths ("C:/...") count as absolute on every platform.
        const bool driveLetter = path.size() > 2 && path.at(0).isLetter() && path.at(1) == QLatin1Char(':') &&
                                 (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\'));
        const bool absolute = path.startsWith(QLatin1Char(':')) || path.startsWith(QLatin1Char('/')) || driveLetter ||
                              QDir::isAbsolutePath(path) || path.contains(QLatin1String("://"));
        if (!absolute && !base.isEmpty())
            path = base + QLatin1Char('/') + path;
        result += QStringLiteral("url(\"") + path + QStringLiteral("\")");
        last = m.capturedEnd();
    }
    result += QStringView(out).mid(last);
    return result;
}

QString StyleSheet::load(const QString& styleDir, double scale)
{
    QFile file(styleDir + QStringLiteral("/stylesheet.css"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return preprocess(QString::fromUtf8(file.readAll()), scale, styleDir);
}

} // namespace OFS

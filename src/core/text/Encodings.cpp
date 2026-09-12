#include "core/text/Encodings.h"

#include <QCoreApplication>
#include <QStringConverter>
#include <QTextCodec>

#include <optional>

namespace OFS {

QString TextEncoding::groupName() const
{
    return QCoreApplication::translate("OFS::TextEncoding", group);
}

QString TextEncoding::codecName() const
{
    return names.section(QLatin1Char(','), 0, 0).trimmed();
}

QString TextEncoding::displayText() const
{
    return groupName() + QStringLiteral(" - ") + names;
}

const QList<TextEncoding>& TextEncoding::all()
{
    static const QList<TextEncoding> list = {
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "System default"), QStringLiteral("System")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Baltic"), QStringLiteral("windows-1257, CP1257")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Central European"), QStringLiteral("windows-1250, CP1250")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Chinese (Simplified)"),
         QStringLiteral("GBK, CP936, MS936, windows-936")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Chinese (Simplified)"), QStringLiteral("GB18030")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Chinese (Simplified)"), QStringLiteral("GB2312")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Chinese (Traditional)"), QStringLiteral("Big5-HKSCS")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Chinese (Traditional)"), QStringLiteral("Big5, Big5-ETen, CP950")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Cyrillic"), QStringLiteral("windows-1251, CP1251")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Cyrillic"), QStringLiteral("IBM866, CP866, csIBM866")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Cyrillic"), QStringLiteral("KOI8-R, csKOI8R")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Cyrillic"), QStringLiteral("KOI8-U, KOI8-RU")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Hebrew"), QStringLiteral("windows-1255, CP1255")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Japanese"), QStringLiteral("Shift_JIS, SJIS, MS_Kanji")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Japanese"), QStringLiteral("ISO-2022-JP, JIS7")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Japanese"), QStringLiteral("EUC-JP")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Korean"), QStringLiteral("windows-949, CP949")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Korean"), QStringLiteral("EUC-KR")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Arabic"),
         QStringLiteral("ISO-8859-6, ISO-8859-6-I, ECMA-114, ASMO-708, arabic, iso-ir-127, csISOLatinArabic")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Arabic"), QStringLiteral("windows-1256, CP1256")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Baltic"), QStringLiteral("ISO-8859-13")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Bengali"), QStringLiteral("iscii-bng")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Celtic"),
         QStringLiteral("ISO-8859-14, iso-ir-199, latin8, iso-celtic")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Cyrillic"),
         QStringLiteral("ISO-8859-5, cyrillic, iso-ir-144, csISOLatinCyrillic")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Devanagari"), QStringLiteral("iscii-dev")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Eastern European"),
         QStringLiteral("ISO-8859-2, latin2, iso-ir-101, csISOLatin2")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Greek"),
         QStringLiteral("ISO-8859-7, ECMA-118, greek, iso-ir-126, csISOLatinGreek")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Gujarati"), QStringLiteral("iscii-gjr")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Gurmukhī"), QStringLiteral("iscii-pnj")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Hebrew"),
         QStringLiteral("ISO-8859-8, ISO 8859-8-I, iso-ir-138, hebrew, csISOLatinHebrew")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Kannada"), QStringLiteral("iscii-knd")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Malayalam"), QStringLiteral("iscii-mlm")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Nordic"),
         QStringLiteral("ISO-8859-10, iso-ir-157, latin6, ISO-8859-10:1992, csISOLatin6")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/North European"),
         QStringLiteral("ISO-8859-4, latin4, iso-ir-110, csISOLatin4")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Odia"), QStringLiteral("iscii-ori")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/South European"),
         QStringLiteral("ISO-8859-3, latin3, iso-ir-109, csISOLatin3")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/South-Eastern European"),
         QStringLiteral("ISO-8859-16, iso-ir-226, latin10")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Tamil"), QStringLiteral("iscii-tml")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Telugu"), QStringLiteral("iscii-tlg")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Thai"), QStringLiteral("TIS-620, ISO 8859-11")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Thai"), QStringLiteral("IBM874, CP874")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Turkish"), QStringLiteral("windows-1254, CP1254")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Turkish"),
         QStringLiteral("ISO-8859-9, iso-ir-148, latin5, csISOLatin5")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Western European"),
         QStringLiteral("ISO-8859-1, latin1, CP819, IBM819, iso-ir-100, csISOLatin1")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Western European"), QStringLiteral("ISO-8859-15, latin9")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Latin/Western European"), QStringLiteral("windows-1252, CP1252")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Modern Greek"), QStringLiteral("windows-1253, CP1253")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Tamil"), QStringLiteral("TSCII")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unicode"), QStringLiteral("UTF-16BE")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unicode"), QStringLiteral("UTF-16")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unicode"), QStringLiteral("UTF-16LE")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unicode"), QStringLiteral("UTF-8")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unicode"), QStringLiteral("UTF-32")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unicode"), QStringLiteral("UTF-32BE")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unicode"), QStringLiteral("UTF-32LE")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Vietnamese"), QStringLiteral("windows-1258, CP1258")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Western European"), QStringLiteral("hp-roman8, roman8, csHPRoman8")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Western European"),
         QStringLiteral("IBM850, CP850, csPC850Multilingual")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Western European"),
         QStringLiteral("macintosh, Apple Roman, MacRoman")},
        {QT_TRANSLATE_NOOP("OFS::TextEncoding", "Unknown"), QStringLiteral("WINSAMI2, WS2")},
    };
    return list;
}

int TextEncoding::indexOf(const QString& name)
{
    const QString wanted = name.trimmed();
    const QList<TextEncoding>& list = all();
    for (int i = 0; i < int(list.size()); ++i) {
        const QStringList aliases = list[i].names.split(QLatin1Char(','));
        for (const QString& alias : aliases) {
            if (alias.trimmed().compare(wanted, Qt::CaseInsensitive) == 0)
                return i;
        }
    }
    return -1;
}

namespace {

QStringList candidateNames(const QString& name)
{
    QStringList names{name.trimmed()};
    const int index = TextEncoding::indexOf(name);
    if (index >= 0) {
        for (const QString& alias : TextEncoding::all().at(index).names.split(QLatin1Char(',')))
            names << alias.trimmed();
    }
    names.removeDuplicates();
    return names;
}

std::optional<QStringConverter::Encoding> converterFor(const QString& name)
{
    if (name.compare(QLatin1String("System"), Qt::CaseInsensitive) == 0)
        return QStringConverter::System;
    for (const QString& candidate : candidateNames(name)) {
        if (auto e = QStringConverter::encodingForName(candidate.toLatin1()))
            return e;
    }
    return std::nullopt;
}

QTextCodec* codecFor(const QString& name)
{
    for (const QString& candidate : candidateNames(name)) {
        if (QTextCodec* codec = QTextCodec::codecForName(candidate.toLatin1()))
            return codec;
    }
    return nullptr;
}

} // namespace

QString TextEncoding::decode(const QByteArray& data, const QString& name, bool* ok)
{
    if (ok)
        *ok = true;
    if (auto e = converterFor(name)) {
        QStringDecoder decoder(*e);
        return decoder(data);
    }
    if (QTextCodec* codec = codecFor(name))
        return codec->toUnicode(data);
    if (ok)
        *ok = false;
    return QString::fromLocal8Bit(data);
}

QByteArray TextEncoding::encode(const QString& text, const QString& name, bool* ok)
{
    if (ok)
        *ok = true;
    if (auto e = converterFor(name)) {
        QStringEncoder encoder(*e);
        return encoder(text);
    }
    if (QTextCodec* codec = codecFor(name))
        return codec->fromUnicode(text);
    if (ok)
        *ok = false;
    return text.toLocal8Bit();
}

bool TextEncoding::isAvailable(const QString& name)
{
    return converterFor(name) || codecFor(name);
}

} // namespace OFS

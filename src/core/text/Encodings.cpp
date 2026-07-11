#include "core/text/Encodings.h"

#include <QCoreApplication>

#define OFS_GROUP(text) QT_TRANSLATE_NOOP("OFS::TextEncoding", text)

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
        {OFS_GROUP("System default"), QStringLiteral("System")},
        {OFS_GROUP("Baltic"), QStringLiteral("windows-1257, CP1257")},
        {OFS_GROUP("Central European"), QStringLiteral("windows-1250, CP1250")},
        {OFS_GROUP("Chinese (Simplified)"), QStringLiteral("GBK, CP936, MS936, windows-936")},
        {OFS_GROUP("Chinese (Simplified)"), QStringLiteral("GB18030")},
        {OFS_GROUP("Chinese (Simplified)"), QStringLiteral("GB2312")},
        {OFS_GROUP("Chinese (Traditional)"), QStringLiteral("Big5-HKSCS")},
        {OFS_GROUP("Chinese (Traditional)"), QStringLiteral("Big5, Big5-ETen, CP950")},
        {OFS_GROUP("Cyrillic"), QStringLiteral("windows-1251, CP1251")},
        {OFS_GROUP("Cyrillic"), QStringLiteral("IBM866, CP866, csIBM866")},
        {OFS_GROUP("Cyrillic"), QStringLiteral("KOI8-R, csKOI8R")},
        {OFS_GROUP("Cyrillic"), QStringLiteral("KOI8-U, KOI8-RU")},
        {OFS_GROUP("Hebrew"), QStringLiteral("windows-1255, CP1255")},
        {OFS_GROUP("Japanese"), QStringLiteral("Shift_JIS, SJIS, MS_Kanji")},
        {OFS_GROUP("Japanese"), QStringLiteral("ISO-2022-JP, JIS7")},
        {OFS_GROUP("Japanese"), QStringLiteral("EUC-JP")},
        {OFS_GROUP("Korean"), QStringLiteral("windows-949, CP949")},
        {OFS_GROUP("Korean"), QStringLiteral("EUC-KR")},
        {OFS_GROUP("Latin/Arabic"),
         QStringLiteral("ISO-8859-6, ISO-8859-6-I, ECMA-114, ASMO-708, arabic, iso-ir-127, csISOLatinArabic")},
        {OFS_GROUP("Latin/Arabic"), QStringLiteral("windows-1256, CP1256")},
        {OFS_GROUP("Latin/Baltic"), QStringLiteral("ISO-8859-13")},
        {OFS_GROUP("Latin/Bengali"), QStringLiteral("iscii-bng")},
        {OFS_GROUP("Latin/Celtic"), QStringLiteral("ISO-8859-14, iso-ir-199, latin8, iso-celtic")},
        {OFS_GROUP("Latin/Cyrillic"), QStringLiteral("ISO-8859-5, cyrillic, iso-ir-144, csISOLatinCyrillic")},
        {OFS_GROUP("Latin/Devanagari"), QStringLiteral("iscii-dev")},
        {OFS_GROUP("Latin/Eastern European"), QStringLiteral("ISO-8859-2, latin2, iso-ir-101, csISOLatin2")},
        {OFS_GROUP("Latin/Greek"), QStringLiteral("ISO-8859-7, ECMA-118, greek, iso-ir-126, csISOLatinGreek")},
        {OFS_GROUP("Latin/Gujarati"), QStringLiteral("iscii-gjr")},
        {OFS_GROUP("Latin/Gurmukhī"), QStringLiteral("iscii-pnj")},
        {OFS_GROUP("Latin/Hebrew"), QStringLiteral("ISO-8859-8, ISO 8859-8-I, iso-ir-138, hebrew, csISOLatinHebrew")},
        {OFS_GROUP("Latin/Kannada"), QStringLiteral("iscii-knd")},
        {OFS_GROUP("Latin/Malayalam"), QStringLiteral("iscii-mlm")},
        {OFS_GROUP("Latin/Nordic"), QStringLiteral("ISO-8859-10, iso-ir-157, latin6, ISO-8859-10:1992, csISOLatin6")},
        {OFS_GROUP("Latin/North European"), QStringLiteral("ISO-8859-4, latin4, iso-ir-110, csISOLatin4")},
        {OFS_GROUP("Latin/Odia"), QStringLiteral("iscii-ori")},
        {OFS_GROUP("Latin/South European"), QStringLiteral("ISO-8859-3, latin3, iso-ir-109, csISOLatin3")},
        {OFS_GROUP("Latin/South-Eastern European"), QStringLiteral("ISO-8859-16, iso-ir-226, latin10")},
        {OFS_GROUP("Latin/Tamil"), QStringLiteral("iscii-tml")},
        {OFS_GROUP("Latin/Telugu"), QStringLiteral("iscii-tlg")},
        {OFS_GROUP("Latin/Thai"), QStringLiteral("TIS-620, ISO 8859-11")},
        {OFS_GROUP("Latin/Thai"), QStringLiteral("IBM874, CP874")},
        {OFS_GROUP("Latin/Turkish"), QStringLiteral("windows-1254, CP1254")},
        {OFS_GROUP("Latin/Turkish"), QStringLiteral("ISO-8859-9, iso-ir-148, latin5, csISOLatin5")},
        {OFS_GROUP("Latin/Western European"),
         QStringLiteral("ISO-8859-1, latin1, CP819, IBM819, iso-ir-100, csISOLatin1")},
        {OFS_GROUP("Latin/Western European"), QStringLiteral("ISO-8859-15, latin9")},
        {OFS_GROUP("Latin/Western European"), QStringLiteral("windows-1252, CP1252")},
        {OFS_GROUP("Modern Greek"), QStringLiteral("windows-1253, CP1253")},
        {OFS_GROUP("Tamil"), QStringLiteral("TSCII")},
        {OFS_GROUP("Unicode"), QStringLiteral("UTF-16BE")},
        {OFS_GROUP("Unicode"), QStringLiteral("UTF-16")},
        {OFS_GROUP("Unicode"), QStringLiteral("UTF-16LE")},
        {OFS_GROUP("Unicode"), QStringLiteral("UTF-8")},
        {OFS_GROUP("Unicode"), QStringLiteral("UTF-32")},
        {OFS_GROUP("Unicode"), QStringLiteral("UTF-32BE")},
        {OFS_GROUP("Unicode"), QStringLiteral("UTF-32LE")},
        {OFS_GROUP("Vietnamese"), QStringLiteral("windows-1258, CP1258")},
        {OFS_GROUP("Western European"), QStringLiteral("hp-roman8, roman8, csHPRoman8")},
        {OFS_GROUP("Western European"), QStringLiteral("IBM850, CP850, csPC850Multilingual")},
        {OFS_GROUP("Western European"), QStringLiteral("macintosh, Apple Roman, MacRoman")},
        {OFS_GROUP("Unknown"), QStringLiteral("WINSAMI2, WS2")},
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

} // namespace OFS

#undef OFS_GROUP

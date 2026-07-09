#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>

namespace OFS {

// A token of a value or expression ("100", "\"name\"", "AnimElem", ">=", "(").
struct CodeToken
{
    enum class Kind
    {
        Number,
        String,
        Identifier,
        Operator,
        Comma,
        OpenParen,
        CloseParen,
        OpenBracket,
        CloseBracket,
        Other,
    };

    Kind kind = Kind::Other;
    int start = 0;
    int length = 0;
};

// The structure of one line of M.U.G.E.N code (definitions, CNS, CMD, AIR, ...):
//
//   [Section name]           ; comment
//   key = value              ; comment
//   key(index) = value
//   Clsn2: 1                 (key: value, used by AIR files)
//   0,0, 0,0, 10             (raw value line, e.g. AIR frames)
//
// Positions refer to the original text so highlighters and validators can map back to it.
struct OFS_CORE_EXPORT CodeLine
{
    enum class Kind
    {
        Blank,
        Comment,
        Section,
        KeyValue, // key = value or key: value
        Raw,      // anything else (value without key)
    };

    Kind kind = Kind::Blank;
    int commentStart = -1;

    // Section
    QString header;
    int headerStart = 0;
    int headerLength = 0;
    bool headerClosed = false;

    // KeyValue
    QString key;
    int keyStart = 0;
    int keyLength = 0;
    QString index; // text between parentheses after the key ("var(5)")
    int indexStart = -1;
    int indexLength = 0;
    int separator = -1;

    // Value (KeyValue and Raw)
    QString value;
    int valueStart = 0;
    QList<CodeToken> tokens;

    static CodeLine parse(const QString& text);
    static QList<CodeToken> tokenize(const QString& text, int offset = 0);
};

} // namespace OFS

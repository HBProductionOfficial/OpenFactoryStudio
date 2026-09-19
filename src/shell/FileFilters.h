#pragma once

#include <QCoreApplication>
#include <QString>

namespace OFS::FileFilters {

// File type lists shared by several editors' file dialogs. The format names stay in English.

inline QString openImages()
{
    return QStringLiteral("All supported (*.pcx *.png *.gif *.bmp);;Personal Computer eXchange (*.pcx);;"
                          "Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;Bitmap (*.bmp)");
}

inline QString saveImages()
{
    return QStringLiteral("Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;"
                          "Personal Computer eXchange (*.pcx);;Bitmap (*.bmp)");
}

inline QString openAudio()
{
    return QStringLiteral("All supported (*.wav *.ogg *.flac *.mp3);;WAVEform audio format (*.wav);;"
                          "Ogg Vorbis (*.ogg);;Free Lossless Audio Codec (*.flac);;MPEG-1/2 Audio Layer 3 (*.mp3)");
}

// Exchange format for sprites and sounds.
inline QString fighterFactoryExport()
{
    return QCoreApplication::translate("OFS::FileFilters", "Fighter Factory Export (*.ffe)");
}

inline QString adobeColorTable()
{
    return QCoreApplication::translate("OFS::FileFilters", "Adobe Color Table (*.act)");
}

inline QString luaScript()
{
    return QCoreApplication::translate("OFS::FileFilters", "Lua Script (*.lua)");
}

} // namespace OFS::FileFilters

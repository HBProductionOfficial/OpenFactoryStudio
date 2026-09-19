#pragma once

#include "formats/util/Bytes.h"

#include <QByteArray>
#include <QString>

#include <memory>

namespace OFS {
class SpriteDocument;
}

namespace OFS::Mugen::Runtime {

// Reading the engine's and the characters' files for the emulator.

QString readText(const QString& path, bool* ok = nullptr);

QByteArray readBytes(const QString& path);

Formats::ByteSpan span(const QByteArray& data);

std::shared_ptr<SpriteDocument> readSprites(const QString& path);

} // namespace OFS::Mugen::Runtime

#pragma once

#include "formats/FormatsExport.h"
#include "formats/codecs/DecodeMode.h"
#include "formats/image/Image.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace OFS::Formats {

enum class GifDisposal : std::uint8_t
{
    Unspecified = 0, // treated like Keep
    Keep = 1,
    Background = 2, // clears to transparent, like browsers do
    Previous = 3,
};

struct GifFrame
{
    Image image;
    int left = 0;
    int top = 0;
    int delay = 0; // hundredths of a second
    GifDisposal disposal = GifDisposal::Unspecified;
    int transparentIndex = -1;
    bool localPalette = false;
    bool interlaced = false;
};

struct GifDocument
{
    int width = 0;
    int height = 0;
    Palette globalPalette;
    std::uint8_t backgroundIndex = 0;
    // NETSCAPE2.0 extension: -1 when absent (plays once), 0 repeats forever.
    int loopCount = -1;
    std::vector<GifFrame> frames;
};

// A 0x0 screen, found in some files, becomes the smallest size that holds every frame.
OFS_FORMATS_EXPORT Result<GifDocument> readGif(ByteSpan data, DecodeMode mode = DecodeMode::Strict,
                                               std::size_t maxFrames = static_cast<std::size_t>(-1));

// The screen grows to hold every frame because decoders disagree about frames that stick out.
OFS_FORMATS_EXPORT Result<Bytes> writeGif(const GifDocument& gif);

OFS_FORMATS_EXPORT GifFrame gifFrameOnScreen(const GifDocument& gif, std::size_t index);

OFS_FORMATS_EXPORT std::vector<Image> renderGifFrames(const GifDocument& gif);

OFS_FORMATS_EXPORT Result<Image> decodeGif(ByteSpan data, DecodeMode mode = DecodeMode::Strict);

} // namespace OFS::Formats

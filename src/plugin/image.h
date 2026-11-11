// JPEG/PNG -> RGBA8 with the Windows Imaging Component (no third-party code). Call from a thread that
// initialised COM (the fetcher thread does); safe to call from several threads.
#pragma once
#include <string>
#include <vector>

namespace lr {

struct Image {
    int w = 0, h = 0;
    std::vector<unsigned char> rgba;  // w*h*4, top-down
    bool ok() const { return w > 0 && h > 0 && rgba.size() == static_cast<size_t>(w) * h * 4; }
};

// Decodes and scales down so the image is at most maxW x maxH (aspect kept). flipVertical for the
// bottom-up thumbnails Nadeo embeds in map files.
bool decodeImage(const std::string& bytes, int maxW, int maxH, bool flipVertical, Image& out, std::string* err = nullptr);

} // namespace lr

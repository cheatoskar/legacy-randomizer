// The TMX sites' own logos (64 px PNGs embedded from assets/sites, origin in assets/sites/SOURCES.md),
// decoded once and turned into textures on first use.
#pragma once
#include "../core/tmx.h"
#include "host.h"
#include "image.h"

namespace lr {

class SiteLogos {
public:
    void decode();                          // all five, on a short-lived COM thread; call once at init
    ImTextureID texture(Host& h, Site s);   // 0 if unavailable; game thread
    void release(Host& h);

private:
    Image img_[5];
    ImTextureID tex_[5] = {};
    bool tried_[5] = {};
};

} // namespace lr

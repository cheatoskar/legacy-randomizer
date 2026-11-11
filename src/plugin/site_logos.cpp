#include "site_logos.h"

#include <cstring>
#include <string>
#include <thread>

#include <windows.h>
#include <objbase.h>

namespace lr {

namespace {
#include "site_logos.inc"
}

void SiteLogos::decode() {
    std::thread t([this] {
        HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        for (int i = 0; i < 5; ++i) {
            std::string bytes(reinterpret_cast<const char*>(kSiteLogos[i].png), kSiteLogos[i].size);
            decodeImage(bytes, 64, 64, false, img_[i]);
        }
        if (SUCCEEDED(co)) CoUninitialize();
    });
    t.join();
}

ImTextureID SiteLogos::texture(Host& h, Site s) {
    const char* host = siteHost(s);
    for (int i = 0; i < 5; ++i) {
        if (std::strcmp(kSiteLogos[i].host, host) != 0) continue;
        if (!tex_[i] && !tried_[i] && img_[i].ok() && h.canCreateTextures()) {
            tried_[i] = true;
            tex_[i] = h.createTextureRGBA(img_[i].w, img_[i].h, img_[i].rgba.data());
        }
        return tex_[i];
    }
    return 0;
}

void SiteLogos::release(Host& h) {
    for (int i = 0; i < 5; ++i)
        if (tex_[i]) {
            h.releaseTexture(tex_[i]);
            tex_[i] = 0;
        }
}

} // namespace lr

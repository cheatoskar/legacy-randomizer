#include "image.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <windows.h>
#include <wincodec.h>

namespace lr {

namespace {

template <class T> struct ComPtr {
    T* p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
    T** operator&() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

bool failed(HRESULT hr, const char* what, std::string* err) {
    if (SUCCEEDED(hr)) return false;
    if (err) {
        char b[96];
        std::snprintf(b, sizeof b, "%s failed (0x%08lx)", what, static_cast<unsigned long>(hr));
        *err = b;
    }
    return true;
}

} // namespace

bool decodeImage(const std::string& bytes, int maxW, int maxH, bool flipVertical, Image& out, std::string* err) {
    out = Image();
    if (bytes.empty() || bytes.size() > 16u * 1024 * 1024) {
        if (err) *err = "no image data";
        return false;
    }
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (failed(hr, "WIC factory", err)) return false;
    ComPtr<IWICStream> stream;
    if (failed(factory->CreateStream(&stream), "WIC stream", err)) return false;
    hr = stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())), static_cast<DWORD>(bytes.size()));
    if (failed(hr, "WIC stream init", err)) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(stream.p, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (failed(hr, "image decoder", err)) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (failed(decoder->GetFrame(0, &frame), "image frame", err)) return false;
    UINT w = 0, h = 0;
    if (failed(frame->GetSize(&w, &h), "image size", err) || w == 0 || h == 0 || w > 8192 || h > 8192) {
        if (err && err->empty()) *err = "bad image size";
        return false;
    }
    IWICBitmapSource* src = frame.p;
    ComPtr<IWICBitmapScaler> scaler;
    UINT tw = w, th = h;
    if (maxW > 0 && maxH > 0 && (w > static_cast<UINT>(maxW) || h > static_cast<UINT>(maxH))) {
        double s = (std::min)(static_cast<double>(maxW) / w, static_cast<double>(maxH) / h);
        tw = (std::max)(1u, static_cast<UINT>(w * s));
        th = (std::max)(1u, static_cast<UINT>(h * s));
        if (failed(factory->CreateBitmapScaler(&scaler), "image scaler", err)) return false;
        if (failed(scaler->Initialize(src, tw, th, WICBitmapInterpolationModeFant), "image scale", err)) return false;
        src = scaler.p;
    }
    ComPtr<IWICFormatConverter> conv;
    if (failed(factory->CreateFormatConverter(&conv), "format converter", err)) return false;
    hr = conv->Initialize(src, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                          WICBitmapPaletteTypeCustom);
    if (failed(hr, "format conversion", err)) return false;
    out.w = static_cast<int>(tw);
    out.h = static_cast<int>(th);
    out.rgba.resize(static_cast<size_t>(tw) * th * 4);
    hr = conv->CopyPixels(nullptr, tw * 4, static_cast<UINT>(out.rgba.size()), out.rgba.data());
    if (failed(hr, "pixel copy", err)) { out = Image(); return false; }
    if (flipVertical) {
        std::vector<unsigned char> row(static_cast<size_t>(tw) * 4);
        for (UINT y = 0; y < th / 2; ++y) {
            unsigned char* a = out.rgba.data() + static_cast<size_t>(y) * tw * 4;
            unsigned char* b = out.rgba.data() + static_cast<size_t>(th - 1 - y) * tw * 4;
            std::memcpy(row.data(), a, row.size());
            std::memcpy(a, b, row.size());
            std::memcpy(b, row.data(), row.size());
        }
    }
    return true;
}

} // namespace lr

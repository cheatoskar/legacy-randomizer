#include "tmtext.h"

#include <cctype>
#include <vector>

#include "imgui_internal.h"  // ImTextCharFromUtf8

namespace lr {

namespace {

struct Span {
    std::string text;
    bool hasColor = false;
    float r = 1, g = 1, b = 1;
    bool bold = false, italic = false, shadow = false, wide = false, narrow = false, upper = false;
};

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::vector<Span> parse(const std::string& s) {
    std::vector<Span> out;
    Span cur;
    auto flush = [&] {
        if (!cur.text.empty()) out.push_back(cur);
        cur.text.clear();
    };
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c != '$') {
            cur.text += cur.upper && c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
            continue;
        }
        if (i + 1 >= s.size()) break;
        char k = s[i + 1];
        if (k == '$') { cur.text += '$'; ++i; continue; }
        if (hexVal(k) >= 0) {
            int d[3] = {0, 0, 0};
            size_t j = i + 1;
            int n = 0;
            for (; n < 3 && j < s.size() && hexVal(s[j]) >= 0; ++n, ++j) d[n] = hexVal(s[j]);
            flush();
            cur.hasColor = true;
            cur.r = d[0] / 15.0f;
            cur.g = d[1] / 15.0f;
            cur.b = d[2] / 15.0f;
            i = j - 1;
            continue;
        }
        flush();
        ++i;
        switch (std::tolower(static_cast<unsigned char>(k))) {
        case 'g': cur.hasColor = false; break;
        case 'o': cur.bold = true; break;
        case 'i': cur.italic = true; break;
        case 's': cur.shadow = true; break;
        case 'w': cur.wide = true; cur.narrow = false; break;
        case 'n': cur.narrow = true; cur.wide = false; break;
        case 'm': cur.wide = cur.narrow = false; break;
        case 't': cur.upper = true; break;
        case 'z': {
            Span fresh;
            cur = fresh;
            break;
        }
        case 'l':
        case 'h':
        case 'p':
            // link: "$l[target]text$l" or "$ltext$l"; keep only the text
            if (i + 1 < s.size() && s[i + 1] == '[') {
                size_t close = s.find(']', i + 1);
                if (close != std::string::npos) i = close;
            }
            break;
        default: break;  // $<, $>, unknown: dropped
        }
    }
    flush();
    return out;
}

ImU32 spanColor(const Span& sp, ImU32 def, bool lift) {
    if (!sp.hasColor) return def;
    float r = sp.r, g = sp.g, b = sp.b;
    if (lift) {
        float l = 0.299f * r + 0.587f * g + 0.114f * b;
        const float minL = 0.38f;
        if (l < minL) {
            float t = (minL - l) / (1.0f - l);
            r += (1 - r) * t;
            g += (1 - g) * t;
            b += (1 - b) * t;
        }
    }
    ImVec4 d = ImGui::ColorConvertU32ToFloat4(def);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, d.w));
}

float widthScale(const Span& sp) { return sp.wide ? 1.25f : (sp.narrow ? 0.8f : 1.0f); }

struct Ctx {
    ImFont* font;
    ImFont* bold;
    float size;
    ImU32 def;
};

Ctx context(const TmTextStyle& s) {
    Ctx c;
    c.font = s.font ? s.font : ImGui::GetFont();
    c.bold = s.boldFont;
    c.size = s.size > 0 ? s.size : (s.font ? s.font->FontSize * ImGui::GetIO().FontGlobalScale : ImGui::GetFontSize());
    c.def = s.defaultColor ? s.defaultColor : ImGui::GetColorU32(ImGuiCol_Text);
    return c;
}

ImFont* spanFont(const Ctx& c, const Span& sp) { return sp.bold && c.bold ? c.bold : c.font; }

float textW(const Ctx& c, const Span& sp, const char* b, const char* e) {
    return spanFont(c, sp)->CalcTextSizeA(c.size, FLT_MAX, 0.0f, b, e).x * widthScale(sp) + (sp.bold && !c.bold ? 1.0f : 0.0f);
}

// Draws one piece; returns its width.
float drawPiece(ImDrawList* dl, const Ctx& c, const Span& sp, ImVec2 p, ImU32 col, const char* b, const char* e) {
    ImFont* f = spanFont(c, sp);
    int vtx0 = dl->VtxBuffer.Size;
    if (sp.shadow) {
        ImU32 sh = IM_COL32(0, 0, 0, static_cast<int>(((col >> IM_COL32_A_SHIFT) & 0xFF) * 0.7f));
        dl->AddText(f, c.size, ImVec2(p.x + 1, p.y + 1), sh, b, e);
    }
    dl->AddText(f, c.size, p, col, b, e);
    if (sp.bold && !c.bold) dl->AddText(f, c.size, ImVec2(p.x + 1, p.y), col, b, e);
    float ws = widthScale(sp);
    if (ws != 1.0f || sp.italic) {
        float baseY = p.y + c.size;
        for (int i = vtx0; i < dl->VtxBuffer.Size; ++i) {
            ImDrawVert& v = dl->VtxBuffer[i];
            v.pos.x = p.x + (v.pos.x - p.x) * ws;
            if (sp.italic) v.pos.x += (baseY - v.pos.y) * 0.2f;
        }
    }
    return textW(c, sp, b, e);
}

} // namespace

std::string tmPlain(const std::string& raw) {
    std::string o;
    for (const Span& sp : parse(raw)) o += sp.text;
    return o;
}

ImVec2 tmTextSize(const std::string& raw, const TmTextStyle& s) {
    Ctx c = context(s);
    float w = 0;
    for (const Span& sp : parse(raw)) w += textW(c, sp, sp.text.data(), sp.text.data() + sp.text.size());
    if (s.maxWidth > 0 && w > s.maxWidth) w = s.maxWidth;
    return ImVec2(w, c.size);
}

void tmTextAt(ImDrawList* dl, ImVec2 pos, const std::string& raw, const TmTextStyle& s) {
    Ctx c = context(s);
    std::vector<Span> spans = parse(raw);
    float total = 0;
    for (const Span& sp : spans) total += textW(c, sp, sp.text.data(), sp.text.data() + sp.text.size());
    bool cut = s.maxWidth > 0 && total > s.maxWidth;
    float limit = cut ? s.maxWidth - c.font->CalcTextSizeA(c.size, FLT_MAX, 0.0f, "...").x : FLT_MAX;
    float x = pos.x;
    ImU32 lastCol = c.def;
    for (const Span& sp : spans) {
        ImU32 col = spanColor(sp, c.def, s.liftDark);
        lastCol = col;
        const char* b = sp.text.data();
        const char* e = b + sp.text.size();
        float w = textW(c, sp, b, e);
        if (x + w - pos.x <= limit) {
            x += drawPiece(dl, c, sp, ImVec2(x, pos.y), col, b, e);
            continue;
        }
        // cut inside this span, at a UTF-8 character boundary
        const char* q = b;
        while (q < e) {
            unsigned int ch = 0;
            int n = ImTextCharFromUtf8(&ch, q, e);
            if (n <= 0) break;
            if (x + textW(c, sp, b, q + n) - pos.x > limit) break;
            q += n;
        }
        if (q > b) x += drawPiece(dl, c, sp, ImVec2(x, pos.y), col, b, q);
        break;
    }
    if (cut) dl->AddText(c.font, c.size, ImVec2(x, pos.y), lastCol, "...");
}

void tmText(const std::string& raw, const TmTextStyle& s) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 sz = tmTextSize(raw, s);
    tmTextAt(ImGui::GetWindowDrawList(), p, raw, s);
    ImGui::Dummy(sz);
}

} // namespace lr

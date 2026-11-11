#include "widgets.h"

#include <vector>

#include "imgui_internal.h"  // FindRenderedTextEnd, CurrentItemFlags

namespace lr {
namespace ui {

namespace {
UiStyle g_style;
struct CardFrame {
    ImVec2 start;
    float width;
};
std::vector<CardFrame> g_cards;
} // namespace

void setStyle(const UiStyle& s) { g_style = s; }
const UiStyle& style() { return g_style; }

ImU32 col(const ImVec4& c, float alphaMul) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * alphaMul)); }

bool segmented(const char* id, int* value, const char* const* labels, int count, float width) {
    ImGui::PushID(id);
    const float sc = g_style.scale;
    const float h = ImGui::GetFrameHeight();
    const float pad = 12.0f * sc;
    float total = 0;
    std::vector<float> w(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) total += (w[static_cast<size_t>(i)] = ImGui::CalcTextSize(labels[i]).x + pad * 2);
    if (width > total) {
        float extra = (width - total) / count;
        for (float& x : w) x += extra;
        total = width;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float r = h * 0.5f;
    dl->AddRectFilled(p, ImVec2(p.x + total, p.y + h), ImGui::GetColorU32(ImGuiCol_FrameBg), r);
    bool changed = false;
    float x = p.x;
    for (int i = 0; i < count; ++i) {
        float wi = w[static_cast<size_t>(i)];
        ImGui::SetCursorScreenPos(ImVec2(x, p.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##seg", ImVec2(wi, h)) && *value != i) {
            *value = i;
            changed = true;
        }
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        bool sel = *value == i;
        if (sel) dl->AddRectFilled(ImVec2(x + 2, p.y + 2), ImVec2(x + wi - 2, p.y + h - 2), col(g_style.accent, 0.85f), r - 2);
        else if (hov) dl->AddRectFilled(ImVec2(x + 2, p.y + 2), ImVec2(x + wi - 2, p.y + h - 2), IM_COL32(255, 255, 255, 18), r - 2);
        ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2(x + (wi - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f),
                    sel ? IM_COL32(20, 16, 12, 255) : col(hov ? g_style.text : g_style.text2), labels[i]);
        x += wi;
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(total, h));
    ImGui::PopID();
    return changed;
}

bool toggle(const char* label, bool* v, const char* tip) {
    const float h = ImGui::GetFrameHeight() * 0.8f;
    const float w = h * 1.8f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineH = ImGui::GetFrameHeight();
    ImGui::PushID(label);
    float labelW = ImGui::CalcTextSize(label, nullptr, true).x;
    bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    bool clicked = ImGui::InvisibleButton("##tg", ImVec2(w + ImGui::GetStyle().ItemInnerSpacing.x * 2 + labelW, lineH));
    bool hov = ImGui::IsItemHovered();
    ImGui::PopID();
    if (clicked) *v = !*v;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float y = p.y + (lineH - h) * 0.5f;
    float a = disabled ? 0.45f : 1.0f;
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), *v ? col(g_style.accent, 0.9f * a) : ImGui::GetColorU32(ImGuiCol_FrameBgHovered, a), h * 0.5f);
    float kx = *v ? p.x + w - h * 0.5f : p.x + h * 0.5f;
    dl->AddCircleFilled(ImVec2(kx, y + h * 0.5f), h * 0.5f - 2.5f, IM_COL32(255, 255, 255, static_cast<int>((hov ? 255 : 235) * a)), 16);
    const char* end = ImGui::FindRenderedTextEnd(label);
    dl->AddText(ImVec2(p.x + w + ImGui::GetStyle().ItemInnerSpacing.x * 2, p.y + (lineH - ImGui::GetTextLineHeight()) * 0.5f),
                col(g_style.text, a), label, end);
    if (tip) infoTip(tip);
    return clicked;
}

bool chip(const char* label, bool on) {
    ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    const float sc = g_style.scale;
    ImVec2 size(ts.x + 20 * sc, ImGui::GetFrameHeight());
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size);
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = size.y * 0.5f;
    if (on) dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col(g_style.accent, 0.85f), r);
    else dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(hov ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), r);
    const char* end = ImGui::FindRenderedTextEnd(label);
    dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), on ? IM_COL32(20, 16, 12, 255) : col(g_style.text2), label, end);
    return clicked;
}

bool triState(const char* label, int* v) {
    static const char* const opts[] = {"Any", "Yes", "No"};
    int idx = *v < 0 ? 0 : (*v == 1 ? 1 : 2);
    bool ch = segmented(label, &idx, opts, 3, 0);
    if (ch) *v = idx == 0 ? -1 : (idx == 1 ? 1 : 0);
    ImGui::SameLine();
    const char* end = ImGui::FindRenderedTextEnd(label);
    ImGui::TextUnformatted(label, end);
    return ch;
}

void infoTip(const char* text) {
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    float s = ImGui::GetTextLineHeight() * 0.85f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lh = ImGui::GetFrameHeight();
    ImGui::Dummy(ImVec2(s, lh));
    bool hov = ImGui::IsItemHovered();
    drawIcon(ImGui::GetWindowDrawList(), Icon::Info, ImVec2(p.x, p.y + (lh - s) * 0.5f), s, col(hov ? g_style.accent : g_style.muted));
    if (hov) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void beginCard(const char* title, Icon ic) {
    const float sc = g_style.scale;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    CardFrame f;
    f.start = ImGui::GetCursorScreenPos();
    f.width = ImGui::GetContentRegionAvail().x;
    g_cards.push_back(f);
    float pad = 14 * sc;
    ImGui::SetCursorScreenPos(ImVec2(f.start.x + pad, f.start.y + pad * 0.8f));
    ImGui::BeginGroup();
    ImGui::PushItemWidth((f.width - pad * 2) * 0.55f);
    icon(ic, col(g_style.accent));
    ImGui::SameLine();
    if (g_style.title) ImGui::PushFont(g_style.title);
    ImGui::SetWindowFontScale(0.8f);
    ImGui::TextUnformatted(title);
    ImGui::SetWindowFontScale(1.0f);
    if (g_style.title) ImGui::PopFont();
    ImGui::Dummy(ImVec2(f.width - pad * 2, 2 * sc));
}

void endCard() {
    const float sc = g_style.scale;
    ImGui::PopItemWidth();
    ImGui::EndGroup();
    CardFrame f = g_cards.back();
    g_cards.pop_back();
    float pad = 14 * sc;
    ImVec2 end(f.start.x + f.width, ImGui::GetItemRectMax().y + pad * 0.8f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(f.start, end, IM_COL32(255, 255, 255, 10), 8 * sc);
    dl->AddRect(f.start, end, IM_COL32(255, 255, 255, 22), 8 * sc);
    dl->ChannelsMerge();
    ImGui::SetCursorScreenPos(f.start);
    ImGui::Dummy(ImVec2(f.width, end.y - f.start.y));
    ImGui::Dummy(ImVec2(0, 6 * sc));
}

bool bigButton(const char* label, Icon ic, ImVec2 size, bool enabled) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size) && enabled;
    bool hov = ImGui::IsItemHovered() && enabled;
    bool held = ImGui::IsItemActive() && enabled;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float a = enabled ? (held ? 1.0f : hov ? 0.95f : 0.82f) : 0.3f;
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col(g_style.accent, a), 8 * g_style.scale);
    if (g_style.title) ImGui::PushFont(g_style.title);
    ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    float is = ts.y * 0.8f, gap = 10 * g_style.scale;
    float x = p.x + (size.x - ts.x - is - gap) * 0.5f;
    ImU32 ink = IM_COL32(20, 16, 12, enabled ? 255 : 160);
    drawIcon(dl, ic, ImVec2(x, p.y + (size.y - is) * 0.5f), is, ink);
    dl->AddText(ImVec2(x + is + gap, p.y + (size.y - ts.y) * 0.5f), ink, label, ImGui::FindRenderedTextEnd(label));
    if (g_style.title) ImGui::PopFont();
    return clicked;
}

void label(const char* text) { ImGui::TextColored(g_style.muted, "%s", text); }

void textRight(const char* text, float rightX, const ImVec4* c) {
    float w = ImGui::CalcTextSize(text).x;
    ImGui::SameLine(rightX - w);
    if (c) ImGui::TextColored(*c, "%s", text);
    else ImGui::TextUnformatted(text);
}

void medalDot(const ImVec4& c) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    float r = h * 0.22f;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + r + 1, p.y + h * 0.5f), r, ImGui::ColorConvertFloat4ToU32(c), 16);
    ImGui::Dummy(ImVec2(r * 2 + 2, h));
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
}

} // namespace ui
} // namespace lr

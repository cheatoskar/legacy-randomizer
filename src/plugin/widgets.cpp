#include "widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "imgui_internal.h"  // FindRenderedTextEnd, CurrentItemFlags
#include "util.h"
#include "../core/timefmt.h"

namespace lr {
namespace ui {

namespace {
UiStyle g_style;

float sc() { return g_style.scale; }

// the host's card surface (launcher token "surface"), with TMVS's faint border
ImVec4 cardColor() {
    ImVec4 s = g_style.surface;
    return ImVec4(s.x, s.y, s.z, 0.96f);
}
} // namespace

void setStyle(const UiStyle& s) { g_style = s; }
const UiStyle& style() { return g_style; }

ImU32 col(const ImVec4& c, float alphaMul) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * alphaMul)); }

// ------------------------------------------------------------------ cards

void beginCard(const char* title, Icon ic, float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, cardColor());
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.07f));
    // the host's buttons have the card's surface colour: one step lighter on a card, or they vanish
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.07f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f * sc());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * sc(), 12.0f * sc()));
    ImGui::BeginChild(title, ImVec2(width, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    // title: a short accent bar, the icon, the name in the title font
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImFont* f = g_style.title ? g_style.title : ImGui::GetFont();
    float h = f->FontSize * 0.8f;
    dl->AddRectFilled(ImVec2(p.x, p.y + h * 0.1f), ImVec2(p.x + 3.0f * sc(), p.y + h * 0.95f), col(g_style.accent), 2.0f);
    drawIcon(dl, ic, ImVec2(p.x + 10 * sc(), p.y + h * 0.08f), h * 0.84f, col(g_style.text2));
    dl->AddText(f, h, ImVec2(p.x + 10 * sc() + h + 6 * sc(), p.y), col(g_style.text), title, ImGui::FindRenderedTextEnd(title));
    ImGui::Dummy(ImVec2(0, h + 6 * sc()));
}

void endCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::Dummy(ImVec2(0, 4 * sc()));
}

void emptyState(Icon ic, const char* line) {
    float w = ImGui::GetContentRegionAvail().x;
    float is = 34 * sc();
    ImVec2 p = ImGui::GetCursorScreenPos();
    drawIcon(ImGui::GetWindowDrawList(), ic, ImVec2(p.x + (w - is) * 0.5f, p.y + 6 * sc()), is, col(g_style.muted, 0.8f));
    ImGui::Dummy(ImVec2(w, is + 14 * sc()));
    float tw = ImGui::CalcTextSize(line).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (w - tw) * 0.5f));
    ImGui::TextColored(g_style.muted, "%s", line);
    ImGui::Dummy(ImVec2(0, 4 * sc()));
}

// ------------------------------------------------------------------ labels, rows

void fieldLabel(const char* label, const char* tip) {
    ImGui::Dummy(ImVec2(0, 2 * sc()));
    ImGui::TextColored(g_style.muted, "%s", label);
    if (tip) infoTip(tip);
}

bool toggle(const char* id, bool* v) {
    const float h = ImGui::GetFrameHeight() * 0.86f, w = h * 1.8f;
    const float lineH = ImGui::GetFrameHeight();
    bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, lineH));
    if (clicked) *v = !*v;
    // animated knob (TMVS)
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID key = ImGui::GetItemID();
    float t = st->GetFloat(key, *v ? 1.0f : 0.0f);
    t += ((*v ? 1.0f : 0.0f) - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    st->SetFloat(key, t);
    const ImVec4 off(0.25f, 0.26f, 0.31f, 1.0f), on = g_style.accent;
    ImVec4 track(off.x + (on.x - off.x) * t, off.y + (on.y - off.y) * t, off.z + (on.z - off.z) * t, 1.0f);
    float a = ImGui::IsItemHovered() ? 1.0f : 0.85f;
    float y = p.y + (lineH - h) * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (disabled) {
        // locked: an outline only and a grey knob, so a locked "on" never looks like a switch you set
        dl->AddRect(ImVec2(p.x, y), ImVec2(p.x + w, y + h), col(g_style.muted, 0.6f), h * 0.5f, 0, 1.5f * sc());
        dl->AddCircleFilled(ImVec2(p.x + h * 0.5f + t * (w - h), y + h * 0.5f), h * 0.5f - 4.0f, col(g_style.muted, 0.7f), 16);
    } else {
        dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), col(track, a), h * 0.5f);
        dl->AddCircleFilled(ImVec2(p.x + h * 0.5f + t * (w - h), y + h * 0.5f), h * 0.5f - 3.0f, IM_COL32(255, 255, 255, 240), 16);
    }
    return clicked;
}

bool switchRow(const char* label, bool* v, const char* tip) {
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImGui::TextColored(disabled ? g_style.muted : g_style.text, "%s", label);
    if (tip) infoTip(tip);
    float w = ImGui::GetFrameHeight() * 0.86f * 1.8f;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - w));
    bool r = toggle("##switch", v);
    ImGui::PopID();
    return r;
}

void infoTip(const char* text) {
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    float s = ImGui::GetTextLineHeight() * 0.8f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lh = ImGui::GetTextLineHeight();
    float base = ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset;  // text after AlignTextToFramePadding sits lower
    ImGui::Dummy(ImVec2(s, lh));
    bool hov = ImGui::IsItemHovered();
    drawIcon(ImGui::GetWindowDrawList(), Icon::Info, ImVec2(p.x, p.y + base + (lh - s) * 0.5f), s, col(hov ? g_style.accent : g_style.muted, 0.9f));
    if (hov) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ------------------------------------------------------------------ inputs

bool segmented(const char* id, int* value, const char* const* labels, int count, float width) {
    ImGui::PushID(id);
    const float h = ImGui::GetFrameHeight();
    const float pad = 12.0f * sc();
    float total = 0;
    std::vector<float> w(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) total += (w[static_cast<size_t>(i)] = ImGui::CalcTextSize(labels[i]).x + pad * 2);
    if (width < 0) width = ImGui::GetContentRegionAvail().x;
    if (width > total) {
        float extra = (width - total) / count;
        for (float& x : w) x += extra;
        total = width;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float r = 6.0f * sc();
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
        if (sel) dl->AddRectFilled(ImVec2(x + 2, p.y + 2), ImVec2(x + wi - 2, p.y + h - 2), col(g_style.accent, 0.9f), r - 1);
        else if (hov) dl->AddRectFilled(ImVec2(x + 2, p.y + 2), ImVec2(x + wi - 2, p.y + h - 2), IM_COL32(255, 255, 255, 16), r - 1);
        ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2(x + (wi - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), sel ? IM_COL32(20, 16, 12, 255) : col(hov ? g_style.text : g_style.text2),
                    labels[i]);
        x += wi;
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(total, h));
    ImGui::PopID();
    return changed;
}

bool chip(const char* label, bool on, Icon ic, ImU32 iconColor) {
    ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    float is = ic == Icon::None ? 0.0f : ts.y * 0.95f;
    float gap = ic == Icon::None ? 0.0f : 6 * sc();
    ImVec2 size(ts.x + is + gap + 22 * sc(), ImGui::GetFrameHeight());
    // wrap to the next line when the chip doesn't fit
    if (ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x + 1 && ImGui::GetContentRegionAvail().x < size.x) ImGui::NewLine();
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size);
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = size.y * 0.5f;
    if (on) dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col(g_style.accent, 0.9f), r);
    else dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(hov ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), r);
    ImU32 ink = on ? IM_COL32(20, 16, 12, 255) : col(hov ? g_style.text : g_style.text2);
    float x = p.x + 11 * sc();
    if (is > 0) {
        drawIcon(dl, ic, ImVec2(x, p.y + (size.y - is) * 0.5f), is, iconColor ? iconColor : ink);
        x += is + gap;
    }
    dl->AddText(ImVec2(x, p.y + (size.y - ts.y) * 0.5f), ink, label, ImGui::FindRenderedTextEnd(label));
    return clicked;
}

bool triState(const char* label, int* v) {
    static const char* const opts[] = {"Any", "Yes", "No"};
    int idx = *v < 0 ? 0 : (*v == 1 ? 1 : 2);
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
    float segW = 150 * sc();
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - segW));
    bool ch = segmented("tri", &idx, opts, 3, segW);
    ImGui::PopID();
    if (ch) *v = idx == 0 ? -1 : (idx == 1 ? 1 : 0);
    return ch;
}

bool rangeSlider(const char* id, int* loMs, int* hiMs, int maxMs, float width) {
    ImGui::PushID(id);
    const float w = width > 0 ? width : ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetFrameHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##range", ImVec2(w, h));
    bool active = ImGui::IsItemActive(), hov = ImGui::IsItemHovered();
    // lo 0 = open, hi 0 = open (shown at the right end); typed values beyond the scale show clamped
    int a = std::min(*loMs, maxMs), b = *hiMs > 0 ? std::min(*hiMs, maxMs) : maxMs;
    if (b < a) b = a;
    float knob = h * 0.36f;
    float x0 = p.x + knob, x1 = p.x + w - knob;
    // square-root scale: 0:30 to 2:00 (most maps) get a good part of the track instead of a sliver
    auto toX = [&](int v) { return x0 + (x1 - x0) * std::sqrt(static_cast<float>(v) / static_cast<float>(maxMs)); };
    auto toV = [&](float x) {
        float t = std::clamp((x - x0) / (x1 - x0), 0.0f, 1.0f);
        int v = static_cast<int>(t * t * static_cast<float>(maxMs) + 0.5f);
        return (v + 2500) / 5000 * 5000;  // 5 s steps
    };
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID dragKey = ImGui::GetID("##which");
    bool changed = false;
    if (ImGui::IsItemActivated()) {
        float mx = ImGui::GetIO().MousePos.x;
        st->SetInt(dragKey, std::fabs(mx - toX(a)) <= std::fabs(mx - toX(b)) && !(a == b && mx > toX(b)) ? 0 : 1);
    }
    if (active) {
        // only the dragged knob's value is written: a typed value on the other side stays as it is
        int v = toV(ImGui::GetIO().MousePos.x);
        if (st->GetInt(dragKey, 0) == 0) {
            v = std::min(v, b);
            if (v != a) {
                a = v;
                *loMs = a;
                changed = true;
            }
        } else {
            v = std::max(v, a);
            if (v != b) {
                b = v;
                *hiMs = b >= maxMs ? 0 : b;
                changed = true;
            }
        }
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cy = p.y + h * 0.5f;
    dl->AddRectFilled(ImVec2(x0, cy - 2 * sc()), ImVec2(x1, cy + 2 * sc()), ImGui::GetColorU32(ImGuiCol_FrameBg), 2 * sc());
    dl->AddRectFilled(ImVec2(toX(a), cy - 2 * sc()), ImVec2(toX(b), cy + 2 * sc()), col(g_style.accent, 0.9f), 2 * sc());
    for (int k = 0; k < 2; ++k) {
        float x = toX(k == 0 ? a : b);
        bool near = hov && std::fabs(ImGui::GetIO().MousePos.x - x) < knob * 1.5f;
        dl->AddCircleFilled(ImVec2(x, cy), knob, near || active ? IM_COL32(255, 255, 255, 255) : IM_COL32(235, 235, 240, 255), 20);
        dl->AddCircle(ImVec2(x, cy), knob, col(g_style.accent), 20, 2.0f * sc());
    }
    ImGui::PopID();
    return changed;
}

bool durationInput(const char* id, int* ms, const char* hint, float width) {
    ImGui::PushID(id);
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID idBad = ImGui::GetID("##bad"), idLast = ImGui::GetID("##last"), idText = ImGui::GetID("##text");
    static std::map<ImGuiID, std::string> texts;  // what is in the field (kept while it is invalid)
    std::string& text = texts[idText];
    bool bad = st->GetBool(idBad, false);
    // the value changed from outside (slider, defaults): show it and forget an invalid entry
    if (st->GetInt(idLast, -1) != *ms) {
        st->SetInt(idLast, *ms);
        text = formatDurationMs(*ms);
        bad = false;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s", text.c_str());
    if (bad) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(g_style.bad.x, g_style.bad.y, g_style.bad.z, 0.25f));
    ImGui::SetNextItemWidth(width);
    ImGui::InputTextWithHint("##in", hint, buf, sizeof buf, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll);
    if (bad) ImGui::PopStyleColor();
    text = buf;
    bool changed = false;
    if (ImGui::IsItemDeactivated()) {
        int v = 0;
        if (parseDurationMs(text, &v)) {
            bad = false;
            if (v != *ms) {
                *ms = v;
                changed = true;
            }
            st->SetInt(idLast, *ms);
            text = formatDurationMs(*ms);
        } else {
            bad = true;
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(bad ? "Not a time. Use m:ss, m:ss.mmm or h:mm:ss; empty = any." : "m:ss, m:ss.mmm or h:mm:ss; empty = any");
    st->SetBool(idBad, bad);
    ImGui::PopID();
    return changed;
}

bool beginCombo(const char* id, const char* preview) {
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 mn = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();  // the parent's list: still valid once the popup is current
    ImGui::SetNextItemWidth(w);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * sc());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * sc(), ImGui::GetStyle().FramePadding.y));
    const float h = ImGui::GetFrameHeight();
    bool open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge);
    ImGui::PopStyleVar(2);
    // a quiet chevron instead of the arrow box
    float is = h * 0.5f;
    drawIcon(dl, Icon::ChevronDown, ImVec2(mn.x + w - is - 10 * sc(), mn.y + (h - is) * 0.5f), is, col(g_style.text2));
    return open;
}

bool combo(const char* id, int* current, const char* const* items, int count) {
    bool changed = false;
    const char* preview = *current >= 0 && *current < count ? items[*current] : "";
    if (beginCombo(id, preview)) {
        for (int i = 0; i < count; ++i)
            if (ImGui::Selectable(items[i], *current == i)) {
                *current = i;
                changed = true;
            }
        ImGui::EndCombo();
    }
    return changed;
}

bool disclosure(const char* label, bool* open) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetFrameHeight();
    bool clicked = ImGui::InvisibleButton(label, ImVec2(w, h));
    if (clicked) *open = !*open;
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hov) dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 255, 255, 10), 6 * sc());
    float is = h * 0.5f;
    ImU32 c = col(hov ? g_style.text : g_style.text2);
    drawIcon(dl, *open ? Icon::ChevronDown : Icon::ChevronRight, ImVec2(p.x + 4 * sc(), p.y + (h - is) * 0.5f), is, c);
    dl->AddText(ImVec2(p.x + 4 * sc() + is + 8 * sc(), p.y + (h - ImGui::GetTextLineHeight()) * 0.5f), c, label,
                ImGui::FindRenderedTextEnd(label));
    // a hairline to the right of the label
    float tx = p.x + 4 * sc() + is + 8 * sc() + ImGui::CalcTextSize(label, nullptr, true).x + 10 * sc();
    if (tx < p.x + w) dl->AddLine(ImVec2(tx, p.y + h * 0.5f), ImVec2(p.x + w, p.y + h * 0.5f), IM_COL32(255, 255, 255, 18));
    return clicked;
}

bool bigButton(const char* label, Icon ic, ImVec2 size, bool enabled) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size) && enabled;
    bool hov = ImGui::IsItemHovered() && enabled;
    bool held = ImGui::IsItemActive() && enabled;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float a = enabled ? (held ? 1.0f : hov ? 0.95f : 0.85f) : 0.3f;
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col(g_style.accent, a), 8 * sc());
    if (g_style.title) ImGui::PushFont(g_style.title);
    ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    float is = ts.y * 0.75f, gap = 10 * sc();
    float x = p.x + (size.x - ts.x - is - gap) * 0.5f;
    ImU32 ink = IM_COL32(20, 16, 12, enabled ? 255 : 160);
    drawIcon(dl, ic, ImVec2(x, p.y + (size.y - is) * 0.5f), is, ink);
    dl->AddText(ImVec2(x + is + gap, p.y + (size.y - ts.y) * 0.5f), ink, label, ImGui::FindRenderedTextEnd(label));
    if (g_style.title) ImGui::PopFont();
    return clicked;
}

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

#include "panels.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "plugin.h"
#include "widgets.h"

namespace lr {

namespace {

// The table: menu pages first (sidebar order), then the HUD panels.
const PanelDef kPanels[] = {
    {"run", "Run", "Start a run, the run in progress, its result", Icon::Play, false, true, [](Plugin& p) { p.pageRun(); }},
    {"filters", "Filters", "Which maps TMX may pick", Icon::Filter, false, true, [](Plugin& p) { p.pageFilters(); }},
    {"rules", "Rules", "Mode, goal, skips and map choice", Icon::Sliders, false, true, [](Plugin& p) { p.pageRules(); }},
    {"layout", "HUD & layout", "Panels over the race, presets, keys", Icon::Layout, false, true, [](Plugin& p) { p.pageLayout(); }},
    {"history", "History", "Saved runs", Icon::History, false, true, [](Plugin& p) { p.pageHistory(); }},
    {"about", "About", "What it does, where the files are", Icon::Info, false, true, [](Plugin& p) { p.pageAbout(); }},
    {"timer", "Timer", "The run's clock", Icon::Timer, true, false, [](Plugin& p) { p.panelTimer(); }},
    {"map", "Map", "Thumbnail, name, author, TMX id", Icon::Map, true, false, [](Plugin& p) { p.panelMap(); }},
    {"medals", "Medals", "Medal times, personal best, counters", Icon::Medal, true, false, [](Plugin& p) { p.panelMedals(); }},
    {"controls", "Controls", "Skip, gold skip, next map", Icon::Controls, true, true, [](Plugin& p) { p.panelControls(); }},
    {"session", "Session", "The maps of this run", Icon::List, true, false, [](Plugin& p) { p.panelSession(); }},
    {"card", "Compact card", "Everything in one card", Icon::Card, true, false, [](Plugin& p) { p.panelCard(); }},
};
const int kCount = static_cast<int>(sizeof kPanels / sizeof kPanels[0]);

int indexOf(const PanelDef& d) { return static_cast<int>(&d - kPanels); }

struct Place { const char* key; float x, y, w, h; };
const Place kDefaults[] = {
    {"timer", 0.41f, 0.012f, 0.18f, 0.15f},   {"map", 0.008f, 0.012f, 0.17f, 0.315f},
    {"medals", 0.822f, 0.012f, 0.17f, 0.25f}, {"controls", 0.008f, 0.338f, 0.17f, 0.17f},
    {"session", 0.822f, 0.274f, 0.17f, 0.30f}, {"card", 0.008f, 0.012f, 0.185f, 0.56f},
};

void placeDefault(const PanelDef& d, PanelState& w) {
    for (const Place& p : kDefaults)
        if (!std::strcmp(p.key, d.key)) {
            w.x = p.x, w.y = p.y, w.w = p.w, w.h = p.h;
            return;
        }
    w.x = 0.30f, w.y = 0.10f, w.w = 0.34f, w.h = 0.62f;
}

const char* const kSplit[] = {"timer", "map", "medals", "controls", "session"};

} // namespace

const PanelDef* Panels::table(int* count) {
    *count = kCount;
    return kPanels;
}

const PanelDef* Panels::find(const char* key) {
    for (const PanelDef& d : kPanels)
        if (!std::strcmp(d.key, key)) return &d;
    return nullptr;
}

PanelState& Panels::state(const PanelDef& d) { return states_[indexOf(d)]; }

void Panels::load(Host& h) {
    for (const PanelDef& d : kPanels) {
        PanelState& w = state(d);
        std::string v = h.getString((std::string("win_") + d.key).c_str(), "");
        int det = 0, min = 0, keep = 0, op = 100;
        float x = 0, y = 0, ww = 0, hh = 0;
        if (!v.empty() && sscanf_s(v.c_str(), "%d,%d,%d,%f,%f,%f,%f,%d", &det, &min, &keep, &x, &y, &ww, &hh, &op) == 8) {
            w.detached = det != 0, w.minimized = min != 0, w.keepOpen = keep != 0;
            w.x = x, w.y = y, w.w = ww, w.h = hh;
            w.opacity = op < 20 ? 20 : (op > 100 ? 100 : op);
        }
    }
    if (h.getInt("layout_version", 0) == 0) {
        if (plugin_.settings().hudPreset == HudPreset::Split) applySplitPreset();
        else applyCompactPreset();
        h.setInt("layout_version", 1);
    }
}

void Panels::saveIfDirty(Host& h) {
    if (!dirty_ || ImGui::IsMouseDown(0)) return;
    dirty_ = false;
    for (const PanelDef& d : kPanels) {
        const PanelState& w = state(d);
        char b[160];
        std::snprintf(b, sizeof b, "%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%d", w.detached ? 1 : 0, w.minimized ? 1 : 0, w.keepOpen ? 1 : 0, w.x, w.y,
                      w.w, w.h, w.opacity);
        h.setString((std::string("win_") + d.key).c_str(), b);
    }
}

void Panels::detach(const PanelDef& d) {
    PanelState& w = state(d);
    w.detached = true;
    w.minimized = false;
    if (w.w <= 0) placeDefault(d, w);
    if (current_ == d.key) current_ = "run";
    ++placeGen_;
    dirty_ = true;
}

void Panels::attach(const PanelDef& d) {
    state(d).detached = false;
    current_ = d.key;
    dirty_ = true;
}

void Panels::applySplitPreset() {
    for (const char* k : kSplit) {
        const PanelDef* d = find(k);
        PanelState& w = state(*d);
        w.detached = true, w.keepOpen = true, w.minimized = false;
        placeDefault(*d, w);
    }
    state(*find("card")).detached = false;
    ++placeGen_;
    dirty_ = true;
}

void Panels::applyCompactPreset() {
    for (const char* k : kSplit) state(*find(k)).detached = false;
    PanelState& c = state(*find("card"));
    c.detached = true, c.keepOpen = true, c.minimized = false;
    placeDefault(*find("card"), c);
    ++placeGen_;
    dirty_ = true;
}

// ------------------------------------------------------------------ the menu page

void Panels::drawNavigation(bool iconsOnly) {
    const UiStyle& st = ui::style();
    const float sc = st.scale;
    const float h = 34.0f * sc;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool hudHeader = false;
    for (const PanelDef& d : kPanels) {
        if (d.hud && !hudHeader) {
            hudHeader = true;
            ImGui::Dummy(ImVec2(0, 8 * sc));
            if (iconsOnly) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                float wd = ImGui::GetContentRegionAvail().x;
                dl->AddLine(ImVec2(p.x + 6 * sc, p.y), ImVec2(p.x + wd - 6 * sc, p.y), ui::col(st.muted, 0.4f), 1.0f);
                ImGui::Dummy(ImVec2(0, 6 * sc));
            } else {
                ui::textFit("HUD PANELS", 0, &st.muted);
            }
        }
        PanelState& w = state(d);
        const bool active = current_ == d.key && !w.detached;
        ImGui::PushID(d.key);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float wd = ImGui::GetContentRegionAvail().x;
        const bool clicked = ImGui::InvisibleButton("##nav", ImVec2(wd, h));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const float is = 16.0f * sc;
        const ImVec2 popMin(p.x + wd - is - 8 * sc, p.y + (h - is) * 0.5f), popMax(popMin.x + is, popMin.y + is);
        const bool onPop = !iconsOnly && hovered && !w.detached && ImGui::IsMouseHoveringRect(popMin, popMax);
        if (clicked) {
            if (w.detached) {
                w.minimized = false;
                ImGui::SetWindowFocus((std::string(d.title) + "##lrwin_" + d.key).c_str());
            } else if (onPop) {
                detach(d);
            } else {
                current_ = d.key;
            }
        }
        if (hovered) {
            if (w.detached) ImGui::SetTooltip("%s: in its own window", d.title);
            else if (onPop) ImGui::SetTooltip("Pop out %s", d.title);
            else ImGui::SetTooltip("%s", iconsOnly ? d.title : d.hint);
        }
        if (active || hovered)
            dl->AddRectFilled(p, ImVec2(p.x + wd, p.y + h), active ? ui::col(st.accent, 0.16f) : IM_COL32(255, 255, 255, 14), 6 * sc);
        if (active) dl->AddRectFilled(ImVec2(p.x, p.y + h * 0.22f), ImVec2(p.x + 3 * sc, p.y + h * 0.78f), ui::col(st.accent), 2);
        ImU32 tc = active ? ui::col(st.text) : ui::col(st.text2, w.detached ? 0.5f : 0.85f);
        if (iconsOnly) {
            drawIcon(dl, d.icon, ImVec2(p.x + (wd - is) * 0.5f, p.y + (h - is) * 0.5f), is, active ? ui::col(st.accent) : tc);
            continue;
        }
        drawIcon(dl, d.icon, ImVec2(p.x + 10 * sc, p.y + (h - is) * 0.5f), is, active ? ui::col(st.accent) : tc);
        float th = ImGui::GetTextLineHeight();
        // right end: "(detached)" or the pop-out symbol; the title gets the rest
        const char* mark = "(detached)";
        const float markW = ImGui::CalcTextSize(mark).x * 0.8f;
        const float tx = p.x + 34 * sc;
        bool showMark = w.detached && wd - 34 * sc - markW - 16 * sc >= ImGui::CalcTextSize(d.title).x;
        float right = showMark ? markW + 16 * sc : (hovered || active) && !w.detached ? is + 14 * sc : 6 * sc;
        std::string t = ui::ellipsize(d.title, wd - 34 * sc - right);
        dl->AddText(ImVec2(tx, p.y + (h - th) * 0.5f), tc, t.c_str());
        if (showMark) {
            dl->AddText(nullptr, ImGui::GetFontSize() * 0.8f, ImVec2(p.x + wd - markW - 8 * sc, p.y + (h - th * 0.8f) * 0.5f),
                        ui::col(st.muted), mark);
        } else if (!w.detached && (hovered || active)) {
            drawIcon(dl, Icon::PopOut, popMin, is, onPop ? ui::col(st.accent) : ui::col(st.muted));
        }
    }
}

void Panels::drawCurrentPage() {
    const PanelDef* d = find(current_.c_str());
    if (!d) d = &kPanels[0];
    PanelState& w = state(*d);
    if (w.detached) {
        ui::textFit((std::string(d->title) + ": own window").c_str(), 0, &ui::style().muted);
        if (ImGui::Button("Show")) {
            w.minimized = false;
            ImGui::SetWindowFocus((std::string(d->title) + "##lrwin_" + d->key).c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Back here")) attach(*d);
        return;
    }
    d->draw(plugin_);
}

// ------------------------------------------------------------------ windows

void Panels::needLine() {
    const float start = ImGui::GetWindowPos().x + ImGui::GetCursorStartPos().x;
    needWidth(ImGui::GetItemRectMax().x - start);
}

void Panels::drawWindows(bool menuOpen, bool runActive, bool hudVisible, bool editLayout) {
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    bool resized = disp.x != lastDisplay_.x || disp.y != lastDisplay_.y;
    if (resized) ++placeGen_;
    // the widths the HUD panels grew to (drawWindow) start over with a new run, layout or screen size
    if (!runActive || resized || editLayout || neededGen_ != placeGen_) {
        for (float& n : neededW_) n = 0.0f;
        neededGen_ = placeGen_;
    }
    lastDisplay_ = disp;
    drawnCount_ = 0;
    for (const PanelDef& d : kPanels) {
        const PanelState& w = state(d);
        if (!w.detached) continue;
        // HUD panels without a run stay hidden (empty "No map yet" cards), except while arranging the layout.
        bool show = d.hud ? ((runActive && (menuOpen || (w.keepOpen && hudVisible))) || (menuOpen && editLayout))
                          : (menuOpen || w.keepOpen);
        if (show) drawWindow(d, editLayout && menuOpen, false);
    }
}

void Panels::drawWindow(const PanelDef& d, bool editLayout, bool) {
    PanelState& w = state(d);
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    if (w.w <= 0) placeDefault(d, w);
    const bool locked = d.hud && !editLayout;
    ImVec2 size(w.w * disp.x, w.h * disp.y), pos(w.x * disp.x, w.y * disp.y);
    if (size.x < 120) size.x = 120;
    if (size.y < 40) size.y = 40;
    // A locked HUD panel is at least as wide as its content (buttons and counter lines are never cut off):
    // it grows by what the content needed in the last frame; a panel at the right edge grows to the left.
    const int idx = indexOf(d);
    if (locked && neededW_[idx] > size.x) {
        const float grown = std::fmin(neededW_[idx], disp.x * 0.5f);
        const bool rightSide = w.x + w.w * 0.5f > 0.5f;
        if (rightSide) pos.x -= grown - size.x;
        size.x = grown;
        if (pos.x + size.x > disp.x) pos.x = disp.x - size.x;
    }
    if (pos.x > disp.x - 40) pos.x = disp.x - 40;
    if (pos.y > disp.y - 30) pos.y = disp.y - 30;
    if (pos.x < 0) pos.x = 0;
    if (pos.y < 0) pos.y = 0;
    int& applied = applied_[indexOf(d)];
    if (locked) {
        // stacked under another locked panel (saved gap under 3 % of the screen): follow its real bottom
        for (int i = 0; i < drawnCount_; ++i) {
            const Drawn& o = drawn_[i];
            float gap = w.y - o.savedBottom;
            bool overlapX = w.x < o.x1 && w.x + w.w > o.x0;
            if (overlapX && gap > -0.005f && gap < 0.03f) pos.y = o.realBottom + 8.0f * ui::style().scale;
        }
    }
    const ImGuiCond cond = (locked || applied != placeGen_) ? ImGuiCond_Always : ImGuiCond_Appearing;
    applied = placeGen_;
    ImGui::SetNextWindowPos(pos, cond);
    // locked HUD panels fit their content's height; the saved height is used while editing
    ImGui::SetNextWindowSize(locked ? ImVec2(size.x, 0) : size, cond);
    if (!locked) ImGui::SetNextWindowCollapsed(w.minimized, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w * (static_cast<float>(w.opacity) / 100.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (locked) {
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoScrollWithMouse;
        if (!d.inputs) flags |= ImGuiWindowFlags_NoInputs;
    }
    bool open = true;
    const std::string name = std::string(d.title) + "##lrwin_" + d.key;
    // HUD panels in TMVS's overlay look: small radius, tight padding (the host's menu windows are rounder)
    if (d.hud) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * ui::style().scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * ui::style().scale, 10.0f * ui::style().scale));
    }
    const bool visible = ImGui::Begin(name.c_str(), locked ? nullptr : &open, flags);
    if (d.hud) ImGui::PopStyleVar(2);
    const bool collapsed = ImGui::IsWindowCollapsed();
    if (locked && drawnCount_ < 16) {
        float bottom = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y;
        drawn_[drawnCount_++] = {w.x, w.x + w.w, w.y + w.h, bottom};
    }
    if (!locked && !ImGui::IsMouseDown(0)) {
        const ImVec2 at = ImGui::GetWindowPos(), ext = ImGui::GetWindowSize();
        float nx = at.x / disp.x, ny = at.y / disp.y, nw = ext.x / disp.x, nh = ext.y / disp.y;
        bool moved = !collapsed && (std::fabs(nx - w.x) > 0.0005f || std::fabs(ny - w.y) > 0.0005f || std::fabs(nw - w.w) > 0.0005f ||
                                    std::fabs(nh - w.h) > 0.0005f);
        if (collapsed != w.minimized || moved) {
            w.minimized = collapsed;
            if (!collapsed) w.x = nx, w.y = ny, w.w = nw, w.h = nh;
            dirty_ = true;
        }
    }
    if (editLayout && d.hud) {
        ImVec2 a = ImGui::GetWindowPos(), b(a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y);
        ImGui::GetForegroundDrawList()->AddRect(a, b, ui::col(ui::style().accent, 0.9f), ImGui::GetStyle().WindowRounding, 0, 1.5f);
    }
    if (visible && !collapsed) {
        if (!locked) {
            if (ImGui::SmallButton("Window options")) ImGui::OpenPopup("windowOptions");
            if (ImGui::BeginPopup("windowOptions")) {
                if (ui::toggle("Keep open when the menu closes", &w.keepOpen)) dirty_ = true;
                ImGui::SetNextItemWidth(180 * ui::style().scale);
                if (ImGui::SliderInt("Opacity", &w.opacity, 20, 100, "%d%%")) dirty_ = true;
                if (ImGui::Button("Back to the menu")) {
                    open = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::Separator();
        }
        if (d.hud) {
            contentNeed_ = 0;
            // the window's own padding (HUD panels push a smaller one than the style's): window width - content width
            const float frame = ImGui::GetWindowSize().x - ImGui::GetContentRegionAvail().x;
            d.draw(plugin_);
            if (locked && contentNeed_ > 0) {
                // what the buttons and counter lines need (needWidth): the panel is that wide from the next frame
                // on and keeps it while the run goes on (no jumping between two widths); a new run starts over
                const float want = std::ceil(contentNeed_ + frame);
                if (want > ImGui::GetWindowSize().x + 0.5f) neededW_[idx] = std::fmax(neededW_[idx], want);
            }
            contentNeed_ = 0;
        } else {
            ImGui::BeginChild("body", ImVec2(0, 0));
            d.draw(plugin_);
            ImGui::EndChild();
        }
    }
    ImGui::End();
    if (!open) attach(d);
}

} // namespace lr

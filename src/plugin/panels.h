// Pages and HUD panels as one table (the TM Vibrant Shaders "detachable pages" model): every row is a
// page of the plugin's menu (sidebar) that can be popped out into a window of its own. The same draw
// function runs in the menu and in the window. HUD panels are rows too: during a run the HUD is simply
// the popped-out windows that are kept open when the menu closes.
//
// Saved per row (host settings, "win_<key>"): detached, minimized, keepOpen, x, y, w, h (fractions of the
// screen, so they survive resolution changes), opacity. This file is meant to move into the host as a
// shared API later; plugin specifics are only the table in panels.cpp.
#pragma once
#include <string>

#include "host.h"
#include "icons.h"

namespace lr {

class Plugin;

struct PanelDef {
    const char* key;   // saved as win_<key>; never change it
    const char* title;
    const char* hint;
    Icon icon;
    bool hud;          // a HUD panel (shown over the race when kept open)
    bool inputs;       // locked HUD: false = click-through (no buttons in it)
    void (*draw)(Plugin&);
};

struct PanelState {
    bool detached = false, minimized = false, keepOpen = false;
    float x = 0, y = 0, w = 0, h = 0;  // fractions of the screen; w <= 0: never placed
    int opacity = 100;
};

class Panels {
public:
    explicit Panels(Plugin& p) : plugin_(p) {}
    static const PanelDef* table(int* count);
    static const PanelDef* find(const char* key);

    void load(Host& h);
    void saveIfDirty(Host& h);
    PanelState& state(const PanelDef& d);
    void detach(const PanelDef& d);
    void attach(const PanelDef& d);

    // in the plugin's menu page
    void drawNavigation();
    void drawCurrentPage();
    void select(const char* key) { current_ = key; }
    const std::string& current() const { return current_; }
    // every frame from DrawOverlay: the popped-out windows
    void drawWindows(bool menuOpen, bool runActive, bool hudVisible, bool editLayout);

    void applySplitPreset();    // HUD panels out to the corners, kept open; the card back in the menu
    void applyCompactPreset();  // the card out (kept open), the split panels back in the menu
    void markDirty() { dirty_ = true; }

private:
    void drawWindow(const PanelDef& d, bool editLayout, bool forceShow);
    Plugin& plugin_;
    std::string current_ = "run";
    bool dirty_ = false;
    int placeGen_ = 1;  // bumped when a preset places windows: positions are applied once more
    PanelState states_[16];
    int applied_[16] = {};
    ImVec2 lastDisplay_{0, 0};
    // locked HUD panels drawn this frame: where they were meant to end and where they really end (a panel
    // placed right under one of them follows its real height, so no gap opens when content is shorter)
    struct Drawn { float x0, x1, savedBottom, realBottom; };
    Drawn drawn_[16];
    int drawnCount_ = 0;
};

} // namespace lr

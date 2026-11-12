// The randomizer plugin as one object: settings, history, run flow and the UI. Host-agnostic: the
// Horizon adapter (host_horizon.cpp) and the preview harness (test/ui_preview.cpp) both drive it.
#pragma once
#include <memory>
#include <string>

#include "history.h"
#include "host.h"
#include "panels.h"
#include "run.h"
#include "settings.h"
#include "site_logos.h"

#define LR_PLUGIN_VERSION "0.3.0"

namespace lr {

class Plugin {
public:
    explicit Plugin(Host& host);
    ~Plugin();

    void init();       // settings, history, hotkeys, window layout
    void shutdown();   // stops the fetcher, releases textures
    void drawMenu();   // inside the host's page for this plugin (an ImGui child)
    void drawOverlay();// every frame: run update + popped-out windows (the HUD)
    void onEvent(const GameEvent& e);

    Host& host() { return host_; }
    Settings& settings() { return settings_; }
    History& history() { return history_; }
    RunController& run() { return run_; }
    Panels& panels() { return panels_; }
    void selectHistoryRun(int i) { histSel_ = i; }
    void requestStop();  // Stop: at once, or the confirm popup when the run has progress
    void setAdvancedOpen(bool open) { advFilters_ = advRules_ = open; }  // the "Advanced" rows (preview shots)
    void markDirty() { dirty_ = true; }

    // rows of the page/panel table (panels.cpp): the same body in the menu and in a window
    void pageRun();
    void pageFilters();
    void pageRules();
    void pageLayout();
    void pageHistory();
    void pageAbout();
    void panelTimer();
    void panelMap();
    void panelMedals();
    void panelControls(bool withStop = true);  // the Run page has its own Stop next to the timer
    void panelSession();
    void panelCard();

private:
    void drawHeader(bool compact);
    bool stacked() const;  // the current page region is too narrow for two columns of cards
    void siteBadge(Site site, float size);
    void siteBadges(const std::vector<Site>& sites, float size);  // logos side by side
    void drawResults(const RunRecord& r, bool live);
    void drawMapsTable(const RunRecord& r, const char* id, float height);
    void drawMapCard(PreparedMap* m, float width, bool thumb);
    void drawMedalRows(PreparedMap* m, float width);
    void drawControls(float width, bool withStop = true);
    void stopButton(ImVec2 size);  // "Stop": at once, or a confirm when the run has progress
    void stopPopup();              // the confirm, submitted once per frame (drawOverlay)
    void drawResultsToast();
    void ensureTexture(PreparedMap* m);
    ImFont* titleFont();
    std::string hotkey(int handle);

    Host& host_;
    Settings settings_;
    History history_;
    RunController run_;
    Panels panels_;
    SiteLogos logos_;
    UiStyle style_;
    bool dirty_ = false;
    int histSel_ = -1;
    bool toastDismissed_ = true;
    bool stopAsk_ = false;  // open the Stop confirm (stopPopup)
    bool advFilters_ = false, advRules_ = false;  // "Advanced" rows open
    int hkSkip_ = 0, hkGoldSkip_ = 0, hkHud_ = 0, hkNext_ = 0;
};

} // namespace lr

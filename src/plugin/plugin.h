// The randomizer plugin as one object: settings, history, run flow and the UI. Host-agnostic: the
// Horizon adapter (host_horizon.cpp) and the preview harness (test/ui_preview.cpp) both drive it.
#pragma once
#include <memory>
#include <string>

#include "history.h"
#include "host.h"
#include "run.h"
#include "settings.h"

#define LR_PLUGIN_VERSION "0.1.0"

namespace lr {

enum class MenuTab { Run = 0, History = 1, Settings = 2, About = 3 };

class Plugin {
public:
    explicit Plugin(Host& host);
    ~Plugin();

    void init();       // settings, history, hotkeys
    void shutdown();   // stops the fetcher, releases textures
    void drawMenu();   // inside the host's page for this plugin (an ImGui child)
    void drawOverlay();// every frame: run update + HUD
    void onEvent(const GameEvent& e);

    Host& host() { return host_; }
    Settings& settings() { return settings_; }
    History& history() { return history_; }
    RunController& run() { return run_; }
    void selectTab(MenuTab t) { forceTab_ = static_cast<int>(t); }  // next frame
    void selectHistoryRun(int i) { histSel_ = i; }

private:
    // menu pages
    void drawHeader();
    void drawStartPage();
    void drawLivePage();
    void drawResults(const RunRecord& r, bool live);
    void drawHistoryPage();
    void drawSettingsPage();
    void drawAboutPage();
    void drawMapsTable(const RunRecord& r, const char* id, float height);
    // HUD
    void drawHud();
    void drawHudMapCard(PreparedMap* m, float width, bool compact);
    void drawResultsToast();
    // helpers
    void ensureTexture(PreparedMap* m);
    void section(const char* title);
    void markDirty() { dirty_ = true; }
    ImFont* titleFont();
    std::string hotkey(int handle);

    Host& host_;
    Settings settings_;
    History history_;
    RunController run_;
    UiStyle style_;
    bool dirty_ = false;
    int forceTab_ = -1;
    int histSel_ = -1;
    bool toastDismissed_ = true;
    int hkSkip_ = 0, hkGoldSkip_ = 0, hkHud_ = 0, hkNext_ = 0;
};

} // namespace lr

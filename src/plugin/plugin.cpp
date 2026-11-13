#include "plugin.h"

namespace lr {

Plugin::Plugin(Host& host) : host_(host), run_(host, settings_, history_), panels_(*this) {}

Plugin::~Plugin() { shutdown(); }

void Plugin::init() {
    settings_.load(host_);
    history_.setFolder(host_.dataFolder());
    history_.reload();
    panels_.load(host_);
    logos_.decode();
    hkSkip_ = host_.registerHotkey("skip", "Randomizer: skip map", "Ctrl+F9", [this] { run_.skip(); });
    hkGoldSkip_ = host_.registerHotkey("gold-skip", "Randomizer: gold skip", "Ctrl+F10", [this] { run_.goldSkip(); });
    hkHud_ = host_.registerHotkey("toggle-hud", "Randomizer: show/hide HUD", "Ctrl+F11", [this] {
        settings_.hudVisible = !settings_.hudVisible;
        markDirty();
    });
    hkNext_ = host_.registerHotkey("next-map", "Randomizer: next map (after a goal)", "", [this] { run_.nextNow(); });
    host_.log(LogInfo, std::string("Legacy Randomizer " LR_PLUGIN_VERSION " ready, ") + std::to_string(history_.runs().size()) +
                           " saved runs, data in " + host_.dataFolder());
}

// The randomizer shows the map with its picture and the medals itself; the Clock and Map Info script widgets of
// Horizon Scripts would sit on top of its panels. While a run goes on they are hidden through the host
// (suspend_widgets: nothing saved, the host shows them again if we stop, are switched off or crash).
void Plugin::syncScriptWidgets() {
    const bool want = settings_.hideScriptWidgets && run_.active();
    if (want == scriptWidgetsHidden_) return;
    // script widget keys: "w-<script id>-<widget id>-<hash>" (Horizon Scripts)
    static const char* const kPrefixes[] = {"w-clock-clock-", "w-map-info-map-info-"};
    bool ok = true;
    for (const char* p : kPrefixes) ok = host_.suspendWidgets("scripts", p, want) && ok;
    scriptWidgetsHidden_ = want;
    if (want && !ok) host_.log(LogInfo, "this Horizon Client can't hide script widgets (needs 0.3.1 or newer)");
}

void Plugin::shutdown() {
    if (scriptWidgetsHidden_) {
        static const char* const kPrefixes[] = {"w-clock-clock-", "w-map-info-map-info-"};
        for (const char* p : kPrefixes) host_.suspendWidgets("scripts", p, false);
        scriptWidgetsHidden_ = false;
    }
    run_.shutdown();
    logos_.release(host_);
}

void Plugin::onEvent(const GameEvent& e) { run_.onEvent(e); }

std::string Plugin::hotkey(int handle) {
    if (handle <= 0) return std::string();
    return host_.hotkeyText(handle);
}

} // namespace lr

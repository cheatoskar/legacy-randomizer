#include "plugin.h"

namespace lr {

Plugin::Plugin(Host& host) : host_(host), run_(host, settings_, history_), panels_(*this) {}

Plugin::~Plugin() { shutdown(); }

void Plugin::init() {
    settings_.load(host_);
    history_.setFolder(host_.dataFolder());
    history_.reload();
    panels_.load(host_);
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

void Plugin::shutdown() { run_.shutdown(); }

void Plugin::onEvent(const GameEvent& e) { run_.onEvent(e); }

std::string Plugin::hotkey(int handle) {
    if (handle <= 0) return std::string();
    return host_.hotkeyText(handle);
}

} // namespace lr

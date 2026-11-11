// All user settings of the randomizer, stored through the host's settings store (one key per field).
#pragma once
#include <string>

#include "../core/session.h"
#include "../core/tmx.h"
#include "host.h"

namespace lr {

enum class SiteChoice { Auto = 0, Tmnf, Tmuf, Nations, Sunrise, Original };
enum class ThumbSource { Tmx = 0, MapFile = 1, Off = 2 };
enum class HudCorner { TopLeft = 0, TopRight = 1 };

struct Settings {
    // run
    Mode mode = Mode::RMC;
    Goal goal = Goal::Author;
    int rmcMinutes = 60;
    int rmsStartMinutes = 15;
    int rmsAuthorBonusS = 180;
    int rmsGoldBonusS = 60;
    int rmsSkipPenaltyS = 60;
    int freeSkips = 1;        // RMC/RMS: skips that cost nothing (-1 unlimited)
    bool goldSkip = true;
    // map filters
    SiteChoice site = SiteChoice::Auto;
    int atMinS = 0, atMaxS = 0;  // 0 = no bound
    int environment = 0;         // TMX environment id, 0 any
    int difficulty = -1;
    int tag = -1;
    bool excludeUnlimiter = true;
    bool requireAuthorTime = true;
    bool excludePlayed = true;
    // flow
    bool autoAdvance = true;
    int advanceDelayS = 3;
    bool replayAutosave = false;  // off: Horizon ABI 1.0 has no replay save for plugins yet
    // HUD
    bool hudVisible = true;
    bool hudCompact = false;
    HudCorner hudCorner = HudCorner::TopLeft;
    ThumbSource thumbSource = ThumbSource::Tmx;

    void load(Host& h);
    void save(Host& h) const;
    void resetToDefaults() { *this = Settings(); }

    SessionConfig sessionConfig() const;
    TmxFilter filter() const;
};

Site siteFor(SiteChoice c, HostGame g);
const char* hostGameShortName(HostGame g);

} // namespace lr

// All user settings of the randomizer, stored through the host's settings store (one key per field).
// Window/panel state is stored by panels.cpp (keys "win_<panel>").
#pragma once
#include <string>
#include <vector>

#include "../core/session.h"
#include "../core/tmx.h"
#include "host.h"

namespace lr {

enum class SiteChoice { Auto = 0, Tmnf, Tmuf, Nations, Sunrise, Original };
enum class ThumbSource { Tmx = 0, MapFile = 1, Off = 2 };
enum class HudCorner { TopLeft = 0, TopRight = 1 };
enum class HudPreset { CompactCard = 0, Split = 1 };

struct Settings {
    // rules
    Mode mode = Mode::RMC;
    Goal goal = Goal::Author;  // RandomizerTMF's auto-skip mode
    int rmcMinutes = 60;
    int rmsStartMinutes = 15;
    int rmsAuthorBonusS = 180;
    int rmsGoldBonusS = 60;
    int rmsSkipPenaltyS = 60;
    int freeSkips = 1;        // RMC/RMS: skips that cost nothing (-1 unlimited)
    bool goldSkip = true;
    bool equalEnvironments = false;  // pick the environment first, evenly
    bool excludePlayed = true;       // maps of saved runs
    bool avoidSkipped = false;       // maps skipped in saved runs
    bool excludeFinished = false;    // maps with a replay in the game's Autosaves folder
    std::string banned;              // TMX ids, comma separated (stored per site: banned_<site>)
    // map filters (TMX)
    SiteChoice site = SiteChoice::Auto;  // ESWC / Sunrise / Original (Forever uses foreverSource)
    // Forever: 0 the game's own exchange, 1 the other one (TMNF game: TMUF-X, Stadium only), 2 both
    // (a site per map). TMUF offers 0 and 2 (TMNF-X as an extra source).
    int foreverSource = 0;
    bool evenSites = false;      // both sites: keep the count of maps per site even instead of a coin flip
    int atMinMs = 0, atMaxMs = 0;  // author time window, 0 = no bound
    int envMask = 0;             // bit (id-1) per TMX environment id, 0 any
    int difficulty = -1;
    int tag = -1;
    int moodMask = 0, routesMask = 0;
    int lbType = -1;
    int vehicle = 0;             // TMUF only
    std::string name, author, uploadedAfter, uploadedBefore;
    int flags[FlagCount] = {-1, -1, -1, -1, -1, -1};
    bool excludeUnlimiter = true;
    bool requireAuthorTime = true;
    // flow
    bool autoAdvance = true;
    int advanceDelayS = 3;
    bool replayAutosave = false;  // off: Horizon ABI 1.0 has no replay save for plugins yet
    // HUD
    bool hudVisible = true;
    HudPreset hudPreset = HudPreset::Split;
    bool layoutEdit = false;
    bool hudCompact = false;      // compact card: smaller variant
    HudCorner hudCorner = HudCorner::TopLeft;
    ThumbSource thumbSource = ThumbSource::Tmx;

    void load(Host& h);
    void save(Host& h) const;
    void resetToDefaults() { *this = Settings(); }

    SessionConfig sessionConfig() const;
    TmxFilter filter(Site site) const;  // without the environment (the fetcher picks it per request)
    std::string summary(const std::vector<Site>& sites, HostGame g) const;  // "RMC · 60 min · Author · nations.tm-exchange.com · Stadium"
};

Site siteFor(SiteChoice c, HostGame g);
bool isForever(HostGame g);
std::vector<Site> sourceSites(const Settings& s, HostGame g);  // the TMX sites a run picks from (1 or 2)
std::string sitesLabel(const std::vector<Site>& sites);        // "tmnf.exchange + tmuf.exchange"
// The environments the environment chips offer for these sources (TMNF: Stadium only).
int sourceEnvironments(const std::vector<Site>& sites, HostGame g, const int** ids);
const char* hostGameShortName(HostGame g);
const char* goalTitle(Goal g);      // "Author", "Gold", ..., "Finished"
int siteEnvironments(Site s, const int** ids);  // TMX environment ids (1..7) of a site; returns count
const char* environmentName(int id);
std::string siteKey(Site s);         // "tmnf", "nations", ...

} // namespace lr

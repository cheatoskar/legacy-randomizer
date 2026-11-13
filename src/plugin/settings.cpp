#include "settings.h"

#include "../core/timefmt.h"

namespace lr {

namespace {

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

const char* modeKey(Mode m) { return m == Mode::RMC ? "rmc" : m == Mode::RMS ? "rms" : "free"; }
Mode modeFrom(const std::string& s) { return s == "rms" ? Mode::RMS : s == "free" ? Mode::FreePlay : Mode::RMC; }

bool validDate(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
        if (d[static_cast<size_t>(i)] < '0' || d[static_cast<size_t>(i)] > '9') return false;
    return true;
}

} // namespace

void Settings::load(Host& h) {
    Settings d;
    mode = modeFrom(h.getString("mode", modeKey(d.mode)));
    goal = Session::goalFromKey(h.getString("goal", "author").c_str());
    rmcMinutes = clampi(h.getInt("rmc_minutes", d.rmcMinutes), 1, 600);
    rmsStartMinutes = clampi(h.getInt("rms_start_minutes", d.rmsStartMinutes), 1, 120);
    rmsAuthorBonusS = clampi(h.getInt("rms_author_bonus_s", d.rmsAuthorBonusS), 0, 3600);
    rmsGoldBonusS = clampi(h.getInt("rms_gold_bonus_s", d.rmsGoldBonusS), 0, 3600);
    rmsSkipPenaltyS = clampi(h.getInt("rms_skip_penalty_s", d.rmsSkipPenaltyS), 0, 3600);
    freeSkips = clampi(h.getInt("free_skips", d.freeSkips), -1, 99);
    goldSkip = h.getInt("gold_skip", d.goldSkip) != 0;
    equalEnvironments = h.getInt("equal_environments", d.equalEnvironments) != 0;
    excludePlayed = h.getInt("exclude_played", d.excludePlayed) != 0;
    avoidSkipped = h.getInt("avoid_skipped", d.avoidSkipped) != 0;
    excludeFinished = h.getInt("exclude_finished", d.excludeFinished) != 0;
    banned = h.getString("banned", "");
    site = static_cast<SiteChoice>(clampi(h.getInt("site", static_cast<int>(d.site)), 0, 5));
    // milliseconds since 0.3 (typed values like 1:30.250); the old whole-second keys as fallback
    atMinMs = clampi(h.getInt("at_min_ms", clampi(h.getInt("at_min_s", 0), 0, 36000) * 1000), 0, 36000000);
    atMaxMs = clampi(h.getInt("at_max_ms", clampi(h.getInt("at_max_s", 0), 0, 36000) * 1000), 0, 36000000);
    foreverSource = clampi(h.getInt("forever_source", d.foreverSource), 0, 2);
    evenSites = h.getInt("even_sites", d.evenSites) != 0;
    envMask = clampi(h.getInt("environments", d.envMask), 0, 127);
    difficulty = clampi(h.getInt("difficulty", d.difficulty), -1, 3);
    tag = clampi(h.getInt("tag", d.tag), -1, 99);
    moodMask = clampi(h.getInt("moods", d.moodMask), 0, 15);
    routesMask = clampi(h.getInt("routes", d.routesMask), 0, 7);
    lbType = clampi(h.getInt("lb_type", d.lbType), -1, 5);
    vehicle = clampi(h.getInt("vehicle", d.vehicle), 0, 7);
    name = h.getString("name", "");
    author = h.getString("author", "");
    uploadedAfter = h.getString("uploaded_after", "");
    uploadedBefore = h.getString("uploaded_before", "");
    for (int i = 0; i < FlagCount; ++i) flags[i] = clampi(h.getInt((std::string("flag_") + tmxFlagQuery(i)).c_str(), -1), -1, 1);
    excludeUnlimiter = h.getInt("exclude_unlimiter", d.excludeUnlimiter) != 0;
    requireAuthorTime = h.getInt("require_author_time", d.requireAuthorTime) != 0;
    autoAdvance = h.getInt("auto_advance", d.autoAdvance) != 0;
    advanceDelayS = clampi(h.getInt("advance_delay_s", d.advanceDelayS), 0, 30);
    replayAutosave = h.getInt("replay_autosave", d.replayAutosave) != 0;
    hudVisible = h.getInt("hud_visible", d.hudVisible) != 0;
    hudPreset = static_cast<HudPreset>(clampi(h.getInt("hud_preset", static_cast<int>(d.hudPreset)), 0, 1));
    layoutEdit = h.getInt("layout_edit", d.layoutEdit) != 0;
    hudCompact = h.getInt("hud_compact", d.hudCompact) != 0;
    hideScriptWidgets = h.getInt("hide_script_widgets", d.hideScriptWidgets) != 0;
    hudCorner = static_cast<HudCorner>(clampi(h.getInt("hud_corner", static_cast<int>(d.hudCorner)), 0, 1));
    thumbSource = static_cast<ThumbSource>(clampi(h.getInt("thumb_source", static_cast<int>(d.thumbSource)), 0, 2));
}

void Settings::save(Host& h) const {
    h.setString("mode", modeKey(mode));
    h.setString("goal", Session::goalKey(goal));
    h.setInt("rmc_minutes", rmcMinutes);
    h.setInt("rms_start_minutes", rmsStartMinutes);
    h.setInt("rms_author_bonus_s", rmsAuthorBonusS);
    h.setInt("rms_gold_bonus_s", rmsGoldBonusS);
    h.setInt("rms_skip_penalty_s", rmsSkipPenaltyS);
    h.setInt("free_skips", freeSkips);
    h.setInt("gold_skip", goldSkip);
    h.setInt("equal_environments", equalEnvironments);
    h.setInt("exclude_played", excludePlayed);
    h.setInt("avoid_skipped", avoidSkipped);
    h.setInt("exclude_finished", excludeFinished);
    h.setString("banned", banned);
    h.setInt("site", static_cast<int>(site));
    h.setInt("at_min_ms", atMinMs);
    h.setInt("at_max_ms", atMaxMs);
    h.setInt("forever_source", foreverSource);
    h.setInt("even_sites", evenSites);
    h.setInt("environments", envMask);
    h.setInt("difficulty", difficulty);
    h.setInt("tag", tag);
    h.setInt("moods", moodMask);
    h.setInt("routes", routesMask);
    h.setInt("lb_type", lbType);
    h.setInt("vehicle", vehicle);
    h.setString("name", name);
    h.setString("author", author);
    h.setString("uploaded_after", uploadedAfter);
    h.setString("uploaded_before", uploadedBefore);
    for (int i = 0; i < FlagCount; ++i) h.setInt((std::string("flag_") + tmxFlagQuery(i)).c_str(), flags[i]);
    h.setInt("exclude_unlimiter", excludeUnlimiter);
    h.setInt("require_author_time", requireAuthorTime);
    h.setInt("auto_advance", autoAdvance);
    h.setInt("advance_delay_s", advanceDelayS);
    h.setInt("replay_autosave", replayAutosave);
    h.setInt("hud_visible", hudVisible);
    h.setInt("hud_preset", static_cast<int>(hudPreset));
    h.setInt("layout_edit", layoutEdit);
    h.setInt("hud_compact", hudCompact);
    h.setInt("hide_script_widgets", hideScriptWidgets);
    h.setInt("hud_corner", static_cast<int>(hudCorner));
    h.setInt("thumb_source", static_cast<int>(thumbSource));
}

SessionConfig Settings::sessionConfig() const {
    SessionConfig c;
    switch (mode) {
    case Mode::RMC:
        c = SessionConfig::rmc(goal);
        c.startMs = rmcMinutes * 60 * 1000;
        c.freeSkips = freeSkips;
        break;
    case Mode::RMS:
        c = SessionConfig::rms();
        c.goal = goal;
        c.startMs = rmsStartMinutes * 60 * 1000;
        c.bonusAuthorMs = rmsAuthorBonusS * 1000;
        c.bonusGoldMs = rmsGoldBonusS * 1000;
        c.skipPenaltyMs = rmsSkipPenaltyS * 1000;
        c.freeSkips = freeSkips;
        break;
    case Mode::FreePlay:
        c = SessionConfig::freePlay();
        c.goal = goal;
        break;
    }
    c.allowGoldSkip = goldSkip;
    return c;
}

TmxFilter Settings::filter(Site s) const {
    TmxFilter f;
    f.authorTimeMinMs = atMinMs;
    f.authorTimeMaxMs = atMaxMs;
    f.difficulty = difficulty;
    f.tag = tag;
    f.excludeUnlimiter = excludeUnlimiter;
    // goal Author needs an author time to be reachable at all
    f.requireAuthorTime = requireAuthorTime || goal == Goal::Author;
    f.name = name;
    f.author = author;
    f.moodMask = moodMask;
    f.routesMask = routesMask;
    f.lbType = lbType;
    f.vehicle = s == Site::TMUF ? vehicle : 0;
    if (validDate(uploadedAfter)) f.uploadedAfter = uploadedAfter;
    if (validDate(uploadedBefore)) f.uploadedBefore = uploadedBefore;
    for (int i = 0; i < FlagCount; ++i) f.flags[i] = static_cast<signed char>(flags[i]);
    return f;
}

std::string Settings::summary(const std::vector<Site>& sites, HostGame g) const {
    std::string o = mode == Mode::RMC ? "RMC \xC2\xB7 " + std::to_string(rmcMinutes) + " min"
                    : mode == Mode::RMS ? "RMS \xC2\xB7 " + std::to_string(rmsStartMinutes) + " min start"
                                        : std::string("Free play");
    o += std::string(" \xC2\xB7 ") + goalTitle(goal);
    o += " \xC2\xB7 " + sitesLabel(sites);
    const int* ids = nullptr;
    int n = sourceEnvironments(sites, g, &ids);
    std::string env;
    for (int i = 0; i < n; ++i)
        if (envMask & (1 << (ids[i] - 1))) env += (env.empty() ? "" : "/") + std::string(environmentName(ids[i]));
    if (env.empty()) env = n == 1 ? environmentName(ids[0]) : "all environments";
    o += " \xC2\xB7 " + env;
    if (atMinMs > 0 || atMaxMs > 0)
        o += " \xC2\xB7 AT " + (atMinMs > 0 ? formatDurationMs(atMinMs) : std::string("any")) + "\xE2\x80\x93" +
             (atMaxMs > 0 ? formatDurationMs(atMaxMs) : std::string("any"));
    return o;
}

Site siteFor(SiteChoice c, HostGame g) {
    switch (c) {
    case SiteChoice::Tmnf: return Site::TMNF;
    case SiteChoice::Tmuf: return Site::TMUF;
    case SiteChoice::Nations: return Site::TMN_ESWC;
    case SiteChoice::Sunrise: return Site::TMS;
    case SiteChoice::Original: return Site::TMO;
    case SiteChoice::Auto: break;
    }
    switch (g) {
    case HostGame::Eswc: return Site::TMN_ESWC;
    case HostGame::Sunrise: return Site::TMS;
    case HostGame::Original: return Site::TMO;
    case HostGame::Tmuf: return Site::TMUF;
    case HostGame::Tmnf: return Site::TMNF;
    case HostGame::Unknown: break;
    }
    return Site::TMNF;
}

bool isForever(HostGame g) { return g == HostGame::Tmnf || g == HostGame::Tmuf; }

std::vector<Site> sourceSites(const Settings& s, HostGame g) {
    if (g == HostGame::Tmnf) {
        if (s.foreverSource == 1) return {Site::TMUF};
        if (s.foreverSource == 2) return {Site::TMNF, Site::TMUF};
        return {Site::TMNF};
    }
    if (g == HostGame::Tmuf) {
        if (s.foreverSource == 2) return {Site::TMUF, Site::TMNF};
        return {Site::TMUF};
    }
    return {siteFor(s.site, g)};
}

std::string sitesLabel(const std::vector<Site>& sites) {
    std::string o;
    for (Site x : sites) o += (o.empty() ? "" : " + ") + std::string(siteHost(x));
    return o;
}

int sourceEnvironments(const std::vector<Site>& sites, HostGame g, const int** ids) {
    if (g == HostGame::Tmnf || sites.empty()) return siteEnvironments(Site::TMNF, ids);  // Stadium
    for (Site x : sites)
        if (x == Site::TMUF) return siteEnvironments(Site::TMUF, ids);
    return siteEnvironments(sites[0], ids);
}

const char* hostGameShortName(HostGame g) {
    switch (g) {
    case HostGame::Eswc: return "TMN ESWC";
    case HostGame::Sunrise: return "TM Sunrise";
    case HostGame::Original: return "TM Original";
    case HostGame::Tmnf: return "TMNF";
    case HostGame::Tmuf: return "TMUF";
    case HostGame::Unknown: break;
    }
    return "unknown game";
}

const char* goalTitle(Goal g) {
    switch (g) {
    case Goal::Author: return "Author";
    case Goal::Gold: return "Gold";
    case Goal::Silver: return "Silver";
    case Goal::Bronze: return "Bronze";
    case Goal::Finished: return "Finished";
    }
    return "Author";
}

int siteEnvironments(Site s, const int** ids) {
    static const int all[] = {1, 2, 3, 4, 5, 6, 7};
    static const int sunrise[] = {4, 5, 6};
    static const int original[] = {1, 2, 3};
    static const int stadium[] = {7};
    switch (s) {
    case Site::TMUF: *ids = all; return 7;
    case Site::TMS: *ids = sunrise; return 3;
    case Site::TMO: *ids = original; return 3;
    case Site::TMNF:
    case Site::TMN_ESWC: break;
    }
    *ids = stadium;
    return 1;
}

const char* environmentName(int id) {
    static const char* const n[] = {"Any", "Snow", "Desert", "Rally", "Island", "Coast", "Bay", "Stadium"};
    return id >= 0 && id <= 7 ? n[id] : "?";
}

std::string siteKey(Site s) {
    switch (s) {
    case Site::TMNF: return "tmnf";
    case Site::TMUF: return "tmuf";
    case Site::TMN_ESWC: return "nations";
    case Site::TMS: return "sunrise";
    case Site::TMO: return "original";
    }
    return "tmnf";
}

} // namespace lr

#include "settings.h"

namespace lr {

namespace {

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

const char* modeKey(Mode m) { return m == Mode::RMC ? "rmc" : m == Mode::RMS ? "rms" : "free"; }
Mode modeFrom(const std::string& s) { return s == "rms" ? Mode::RMS : s == "free" ? Mode::FreePlay : Mode::RMC; }

} // namespace

void Settings::load(Host& h) {
    Settings d;
    mode = modeFrom(h.getString("mode", modeKey(d.mode)));
    goal = h.getString("goal", "author") == "gold" ? Goal::Gold : Goal::Author;
    rmcMinutes = clampi(h.getInt("rmc_minutes", d.rmcMinutes), 1, 600);
    rmsStartMinutes = clampi(h.getInt("rms_start_minutes", d.rmsStartMinutes), 1, 120);
    rmsAuthorBonusS = clampi(h.getInt("rms_author_bonus_s", d.rmsAuthorBonusS), 0, 3600);
    rmsGoldBonusS = clampi(h.getInt("rms_gold_bonus_s", d.rmsGoldBonusS), 0, 3600);
    rmsSkipPenaltyS = clampi(h.getInt("rms_skip_penalty_s", d.rmsSkipPenaltyS), 0, 3600);
    freeSkips = clampi(h.getInt("free_skips", d.freeSkips), -1, 99);
    goldSkip = h.getInt("gold_skip", d.goldSkip) != 0;
    site = static_cast<SiteChoice>(clampi(h.getInt("site", static_cast<int>(d.site)), 0, 5));
    atMinS = clampi(h.getInt("at_min_s", d.atMinS), 0, 3600);
    atMaxS = clampi(h.getInt("at_max_s", d.atMaxS), 0, 3600);
    environment = clampi(h.getInt("environment", d.environment), 0, 7);
    difficulty = clampi(h.getInt("difficulty", d.difficulty), -1, 3);
    tag = clampi(h.getInt("tag", d.tag), -1, 99);
    excludeUnlimiter = h.getInt("exclude_unlimiter", d.excludeUnlimiter) != 0;
    requireAuthorTime = h.getInt("require_author_time", d.requireAuthorTime) != 0;
    excludePlayed = h.getInt("exclude_played", d.excludePlayed) != 0;
    autoAdvance = h.getInt("auto_advance", d.autoAdvance) != 0;
    advanceDelayS = clampi(h.getInt("advance_delay_s", d.advanceDelayS), 0, 30);
    replayAutosave = h.getInt("replay_autosave", d.replayAutosave) != 0;
    hudVisible = h.getInt("hud_visible", d.hudVisible) != 0;
    hudCompact = h.getInt("hud_compact", d.hudCompact) != 0;
    hudCorner = static_cast<HudCorner>(clampi(h.getInt("hud_corner", static_cast<int>(d.hudCorner)), 0, 1));
    thumbSource = static_cast<ThumbSource>(clampi(h.getInt("thumb_source", static_cast<int>(d.thumbSource)), 0, 2));
}

void Settings::save(Host& h) const {
    h.setString("mode", modeKey(mode));
    h.setString("goal", goal == Goal::Gold ? "gold" : "author");
    h.setInt("rmc_minutes", rmcMinutes);
    h.setInt("rms_start_minutes", rmsStartMinutes);
    h.setInt("rms_author_bonus_s", rmsAuthorBonusS);
    h.setInt("rms_gold_bonus_s", rmsGoldBonusS);
    h.setInt("rms_skip_penalty_s", rmsSkipPenaltyS);
    h.setInt("free_skips", freeSkips);
    h.setInt("gold_skip", goldSkip);
    h.setInt("site", static_cast<int>(site));
    h.setInt("at_min_s", atMinS);
    h.setInt("at_max_s", atMaxS);
    h.setInt("environment", environment);
    h.setInt("difficulty", difficulty);
    h.setInt("tag", tag);
    h.setInt("exclude_unlimiter", excludeUnlimiter);
    h.setInt("require_author_time", requireAuthorTime);
    h.setInt("exclude_played", excludePlayed);
    h.setInt("auto_advance", autoAdvance);
    h.setInt("advance_delay_s", advanceDelayS);
    h.setInt("replay_autosave", replayAutosave);
    h.setInt("hud_visible", hudVisible);
    h.setInt("hud_compact", hudCompact);
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

TmxFilter Settings::filter() const {
    TmxFilter f;
    f.authorTimeMinMs = atMinS * 1000;
    f.authorTimeMaxMs = atMaxS * 1000;
    f.environment = environment;
    f.difficulty = difficulty;
    f.tag = tag;
    f.excludeUnlimiter = excludeUnlimiter;
    // goal Author needs an author time to be reachable at all
    f.requireAuthorTime = requireAuthorTime || goal == Goal::Author;
    return f;
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
    case HostGame::Tmnf:
    case HostGame::Unknown: break;
    }
    return Site::TMNF;
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

} // namespace lr

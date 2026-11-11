#include "run.h"

#include "util.h"

namespace lr {

const char* phaseName(Phase p) {
    switch (p) {
    case Phase::Idle: return "idle";
    case Phase::Preparing: return "preparing";
    case Phase::Loading: return "loading";
    case Phase::Playing: return "playing";
    case Phase::Leaving: return "leaving";
    case Phase::WaitingMap: return "waiting for map";
    case Phase::Over: return "over";
    }
    return "";
}

namespace {

const char* modeKey(Mode m) { return m == Mode::RMC ? "rmc" : m == Mode::RMS ? "rms" : "free"; }

bool sameUid(const std::string& a, const std::string& b) { return a.empty() || b.empty() || a == b; }

const char* resultText(HostResult r) {
    switch (r) {
    case HostResult::Ok: return "ok";
    case HostResult::NotSupported: return "not supported for this game";
    case HostResult::WrongState: return "wrong game state";
    case HostResult::BlockedOnline: return "blocked (online or Competition Patch run)";
    case HostResult::InvalidArg: return "invalid argument";
    case HostResult::Failed: return "failed";
    case HostResult::Busy: return "busy";
    }
    return "?";
}

} // namespace

RunController::RunController(Host& host, Settings& settings, History& history)
    : host_(host), settings_(settings), history_(history),
      fetcher_([&host](LogLevel l, const std::string& m) { host.log(l, m); }) {}

RunController::~RunController() { shutdown(); }

void RunController::shutdown() {
    *alive_ = false;
    fetcher_.stop();
    releaseMap(cur_);
    releaseMap(pending_);
}

bool RunController::forever() const { return host_.game() == HostGame::Tmnf || host_.game() == HostGame::Tmuf; }

bool RunController::start(std::string* why) {
    auto no = [why](const char* m) { if (why) *why = m; return false; };
    if (active()) return no("A run is already going on.");
    if (host_.isOnline() || !host_.actionsAllowed()) return no("Not available in net play.");
    sites_ = sourceSites(settings_, host_.game());
    site_ = sites_[0];
    mapSites_.clear();
    cfg_ = settings_.sessionConfig();
    session_.reset(new Session(cfg_));
    session_->start();
    releaseMap(cur_);
    releaseMap(pending_);
    replays_.clear();
    curReplay_.clear();
    onOurMap_ = userLeft_ = goalReached_ = waitUser_ = raceEnded_ = leaveSent_ = reloading_ = manualLeave_ = false;
    loadFailures_ = 0;
    retryAt_ = 0;
    notice_.clear();
    error_.clear();
    started_ = isoLocalTime();
    lastTick_ = host_.nowMs();
    tickCarry_ = 0;
    if (injected_.empty()) {
        Fetcher::Job job;
        job.folder = host_.dataFolder() + "maps\\";
        job.thumbs = settings_.thumbSource;
        job.evenSites = settings_.evenSites;
        const std::set<long long> banned = [&] {
            std::set<long long> b;
            for (long long id : parseIdList(settings_.banned)) b.insert(id);
            return b;
        }();
        for (Site s : sites_) {
            Fetcher::Source src;
            src.site = s;
            src.filter = settings_.filter(s);
            // past runs per site + TMX id (the same id on two sites is two different maps)
            if (settings_.excludePlayed) src.exclude = history_.playedIds(siteHost(s));
            if (settings_.avoidSkipped)
                for (long long id : history_.skippedIds(siteHost(s))) src.exclude.insert(id);
            src.exclude.insert(banned.begin(), banned.end());
            if (host_.game() == HostGame::Tmnf && s == Site::TMUF) {
                // TMNF plays only Stadium maps driven with the Stadium car
                src.environments = {7};
                src.filter.vehicle = 7;
            } else {
                // environments: the chosen ones, or with "equal distribution" all of the site's, one per request
                const int* envs = nullptr;
                int nEnv = siteEnvironments(s, &envs);
                for (int i = 0; i < nEnv; ++i)
                    if (settings_.envMask & (1 << (envs[i] - 1))) src.environments.push_back(envs[i]);
                // chosen environments this site doesn't have (TMNF-X next to TMUF-X with only Snow): leave it out
                if (src.environments.empty() && settings_.envMask != 0 && sites_.size() > 1) continue;
                if (src.environments.empty() && settings_.equalEnvironments && nEnv > 1) src.environments.assign(envs, envs + nEnv);
            }
            job.sources.push_back(src);
        }
        if (job.sources.empty()) return no("None of the TMX sites has the chosen environments.");
        if (settings_.excludeFinished) {
            std::string user = host_.userFolder(), game = host_.gameFolder();
            // Forever / Sunrise: the user folder (Documents\TrackMania...); ESWC: <exe dir>\GameData\...
            if (!user.empty()) job.autosaveDirs.push_back(user + "Tracks\\Replays\\Autosaves\\");
            if (!game.empty()) job.autosaveDirs.push_back(game + "GameData\\Tracks\\Replays\\Autosaves\\");
            job.autosaveCache = host_.dataFolder() + "autosaves_cache.txt";
        }
        fetcher_.start(job);
    }
    phase_ = Phase::Preparing;
    host_.log(LogInfo, std::string("run started: ") + modeKey(cfg_.mode) + ", goal " +
                           Session::goalKey(cfg_.goal) + ", site " + sitesLabel(sites_));
    return true;
}

void RunController::stop() {
    if (active()) endRun("user");
}

void RunController::newRun() {
    if (phase_ == Phase::Over) phase_ = Phase::Idle;
}

std::shared_ptr<PreparedMap> RunController::takeNext() {
    if (!injected_.empty()) {
        std::shared_ptr<PreparedMap> m = injected_.front();
        injected_.erase(injected_.begin());
        return m;
    }
    return fetcher_.take();
}

void RunController::releaseMap(std::shared_ptr<PreparedMap>& m) {
    if (m && m->tex) {
        host_.releaseTexture(m->tex);
        m->tex = 0;
    }
    m.reset();
}

void RunController::setError(const std::string& e) {
    error_ = e;
    host_.log(LogWarn, e);
}

int RunController::pendingLoadAgeMs() const {
    if (phase_ != Phase::Loading) return 0;
    return static_cast<int>(host_.nowMs() - loadSentAt_);
}

std::string RunController::fetchStatus() { return injected_.empty() ? fetcher_.status() : "Preview map ready"; }
bool RunController::fetchFailing() { return injected_.empty() && fetcher_.lastFailed(); }

int RunController::bestMs() const {
    if (!session_) return 0;
    if (session_->state() == State::Playing) return session_->currentBestMs();
    if ((phase_ == Phase::Leaving || phase_ == Phase::Over) && !session_->history().empty())
        return session_->history().back().bestMs;
    return 0;
}

Medal RunController::bestMedal() const {
    if (!session_) return Medal::None;
    if (session_->state() == State::Playing) return session_->currentMedal();
    if ((phase_ == Phase::Leaving || phase_ == Phase::Over) && !session_->history().empty())
        return session_->history().back().medal;
    return Medal::None;
}

bool RunController::canSkip() const { return phase_ == Phase::Playing && session_ && session_->state() == State::Playing; }
bool RunController::canGoldSkip() const { return canSkip() && session_->canGoldSkip(); }

// ------------------------------------------------------------------ user actions

void RunController::skip() {
    if (!canSkip()) return;
    SkipKind k = session_->skip();
    if (k == SkipKind::NotAllowed) return;
    host_.log(LogInfo, k == SkipKind::Free ? "free skip" : "skip");
    mapResolved(false);
}

void RunController::goldSkip() {
    if (!canGoldSkip()) return;
    if (!session_->goldSkip()) return;
    host_.log(LogInfo, "gold skip");
    mapResolved(false);
}

void RunController::nextNow() {
    if (phase_ != Phase::Leaving) return;
    waitUser_ = false;
    advanceAt_ = 0;
    goalAt_ = -1e12;  // don't wait for the replay any more
    pump();
}

void RunController::resume() {
    if (phase_ != Phase::Playing || !userLeft_ || !cur_ || callQueued_) return;
    if (host_.gameState() != GameState::MainMenu || !host_.actionsAllowed()) return;
    callQueued_ = true;
    std::shared_ptr<bool> alive = alive_;
    std::shared_ptr<PreparedMap> m = cur_;
    host_.runOnGameThread([this, alive, m] {
        if (!*alive) return;
        callQueued_ = false;
        if (cur_ != m || phase_ != Phase::Playing) return;
        HostResult r = host_.playMapFile(m->path);
        if (r == HostResult::Ok) {
            reloading_ = true;
            notice_ = "Loading the map again";
        } else {
            setError(std::string("Could not reload the map: ") + resultText(r));
        }
    });
}

void RunController::dropPendingMap() {
    if (!pending_ || (phase_ != Phase::Loading && phase_ != Phase::Preparing && phase_ != Phase::WaitingMap)) return;
    host_.log(LogWarn, "dropping map " + std::to_string(pending_->tmx.id) + " (did not start)");
    fetcher_.exclude(pending_->site, pending_->tmx.id);
    releaseMap(pending_);
    phase_ = session_ && session_->history().empty() && !cur_ ? Phase::Preparing : Phase::WaitingMap;
    notice_.clear();
}

// ------------------------------------------------------------------ frame

void RunController::update() {
    double now = host_.nowMs();
    double dt = now - lastTick_;
    lastTick_ = now;
    if (!active() || !session_) return;
    GameState gs = host_.gameState();
    if (phase_ == Phase::Playing && onOurMap_ && !userLeft_ && (gs == GameState::SoloRace || gs == GameState::EndRace) &&
        !host_.isOnline() && dt > 0) {
        double total = dt + tickCarry_;
        int ms = static_cast<int>(total);
        tickCarry_ = total - ms;
        session_->tick(ms);
        if (session_->state() == State::Over) {
            endRun("timer");
            return;
        }
    }
    pump();
}

void RunController::pump() {
    if (!active()) return;
    if (host_.isOnline() || !host_.actionsAllowed()) {
        notice_ = "Paused: the randomizer does nothing in net play.";
        return;
    }
    double now = host_.nowMs();
    GameState gs = host_.gameState();
    switch (phase_) {
    case Phase::Preparing:
    case Phase::WaitingMap:
        if (callQueued_ || now < retryAt_) return;
        if (!pending_) pending_ = takeNext();
        if (!pending_) return;  // the fetch status is shown by the UI
        if (gs == GameState::MainMenu) tryPlayNext();
        else if (gs == GameState::SoloRace || gs == GameState::EndRace) leaveMap();
        else notice_ = "Waiting for the main menu";
        return;
    case Phase::Loading: {
        double age = now - loadSentAt_;
        // hosts without race events (Sunrise in host 0.1.0): the map counts as started once it is
        // loaded and the game is in the solo race for 2 s
        if (!hostHasRaceEvents_ && pending_ && mapLoadedAt_ > loadSentAt_ && gs == GameState::SoloRace && now - mapLoadedAt_ > 2000 &&
            (loadedUid_.empty() || pending_->uid.empty() || loadedUid_ == pending_->uid)) {
            host_.log(LogInfo, "no RACE_START from the host: starting the map from MAP_LOADED");
            beginMap();
            return;
        }
        if ((gs == GameState::MainMenu && age > 15000) || age > 45000) {
            if (error_.empty()) setError("The map did not start. Take another map, or wait.");
        }
        return;
    }
    case Phase::Leaving:
        if (gs == GameState::MainMenu) {
            phase_ = Phase::WaitingMap;
            leaveSent_ = false;
            manualLeave_ = false;
            notice_.clear();
            retryAt_ = 0;
            pump();
            return;
        }
        if (waitUser_) {
            notice_ = "Goal reached. Press \"Next map\" when you are ready.";
            return;
        }
        // after a goal, wait for RACE_END (the replay is complete there), at most 5 s
        if (goalReached_ && !raceEnded_ && now - goalAt_ < 5000) return;
        if (now >= advanceAt_) leaveMap();
        return;
    case Phase::Playing:
        if (userLeft_ && gs == GameState::MainMenu && !reloading_)
            notice_ = "You left the map. Resume it or skip it.";
        // hosts that deliver race events on Sunrise (host `next`) need no hint
        else if (host_.game() == HostGame::Sunrise && !hostHasRaceEvents_ && notice_.empty() &&
                 host_.nowMs() - mapLoadedAt_ > 5000)
            notice_ = "Sunrise: finish detection not available yet, skip manually.";
        else if (hostHasRaceEvents_ && notice_ == "Sunrise: finish detection not available yet, skip manually.")
            notice_.clear();
        return;
    case Phase::Idle:
    case Phase::Over: return;
    }
}

void RunController::tryPlayNext() {
    if (callQueued_ || !pending_) return;
    callQueued_ = true;
    std::shared_ptr<bool> alive = alive_;
    std::shared_ptr<PreparedMap> m = pending_;
    host_.runOnGameThread([this, alive, m] {
        if (!*alive) return;
        callQueued_ = false;
        if (pending_ != m || !active() || (phase_ != Phase::Preparing && phase_ != Phase::WaitingMap)) return;
        double now = host_.nowMs();
        if (host_.gameState() != GameState::MainMenu) { retryAt_ = now + 500; return; }
        HostResult r = host_.playMapFile(m->path);
        switch (r) {
        case HostResult::Ok:
            phase_ = Phase::Loading;
            loadSentAt_ = now;
            loadedUid_.clear();
            error_.clear();
            notice_ = "Loading map";
            host_.log(LogInfo, "play map " + std::to_string(m->tmx.id) + ": " + m->path);
            return;
        case HostResult::WrongState:
        case HostResult::Busy:
            retryAt_ = now + 1000;
            return;
        case HostResult::BlockedOnline:
            retryAt_ = now + 2000;
            notice_ = "Blocked by the host (online or Competition Patch run).";
            return;
        case HostResult::NotSupported:
            setError("This game can't start maps yet (host: not supported).");
            endRun("error");
            return;
        case HostResult::InvalidArg:
        case HostResult::Failed:
            break;
        }
        ++loadFailures_;
        setError("Map " + std::to_string(m->tmx.id) + " failed to load (" + resultText(r) + "), taking the next one.");
        fetcher_.exclude(m->site, m->tmx.id);
        deleteFile(m->path);
        releaseMap(pending_);
        retryAt_ = now + 1000;
        if (loadFailures_ >= 5) {
            setError("Five maps in a row failed to load; run stopped.");
            endRun("error");
        }
    });
}

void RunController::leaveMap() {
    double now = host_.nowMs();
    if (callQueued_ || manualLeave_ || (leaveSent_ && now - leaveSentAt_ < 8000)) return;
    GameState gs = host_.gameState();
    bool allowed = gs == GameState::EndRace || (gs == GameState::SoloRace && !forever());
    if (!allowed) {
        if (gs == GameState::SoloRace)
            notice_ = forever() ? "Quit the map yourself (Esc > Quit). The next map starts from the menu."
                                : "Waiting to leave the map";
        return;
    }
    callQueued_ = true;
    std::shared_ptr<bool> alive = alive_;
    host_.runOnGameThread([this, alive] {
        if (!*alive) return;
        callQueued_ = false;
        if (!active()) return;
        GameState g = host_.gameState();
        bool ok = g == GameState::EndRace || (g == GameState::SoloRace && !forever());
        if (!ok) return;
        HostResult r = host_.returnToMenu();
        if (r == HostResult::Ok) {
            leaveSent_ = true;
            leaveSentAt_ = host_.nowMs();
        } else if (r == HostResult::NotSupported) {
            manualLeave_ = true;
            notice_ = "Quit the map yourself (Esc > Quit). The next map starts from the menu.";
        } else {
            retryAt_ = host_.nowMs() + 1000;
            host_.log(LogWarn, std::string("return to menu: ") + resultText(r));
        }
    });
}

void RunController::beginMap() {
    releaseMap(cur_);
    cur_ = pending_;
    pending_.reset();
    SessionMap sm;
    sm.id = cur_->tmx.id;
    sm.uid = cur_->uid;
    sm.name = cur_->name;
    sm.author = cur_->author;
    sm.medals = cur_->medals;
    session_->mapStarted(sm);
    mapSites_.push_back(cur_->site);
    phase_ = Phase::Playing;
    onOurMap_ = true;
    userLeft_ = goalReached_ = waitUser_ = raceEnded_ = reloading_ = false;
    curReplay_.clear();
    notice_.clear();
    error_.clear();
    loadFailures_ = 0;
    host_.log(LogInfo, "map started: " + std::to_string(sm.id) + " " + stripTmCodes(sm.name));
}

void RunController::mapResolved(bool goal) {
    double now = host_.nowMs();
    replays_.resize(session_->history().size());
    if (!replays_.empty()) replays_.back() = curReplay_;
    curReplay_.clear();
    goalReached_ = goal;
    goalAt_ = now;
    replayTried_ = false;
    raceEnded_ = false;
    leaveSent_ = false;
    phase_ = Phase::Leaving;
    if (session_->state() == State::Over) {
        endRun("timer");
        return;
    }
    waitUser_ = goal && !settings_.autoAdvance;
    advanceAt_ = goal ? now + settings_.advanceDelayS * 1000.0 : now;
    pump();
}

RunRecord RunController::snapshot(const char* by) const {
    RunRecord r;
    r.game = host_.gameName();
    r.site = sitesLabel(sites_);
    r.mode = modeKey(cfg_.mode);
    r.goal = Session::goalKey(cfg_.goal);
    r.timeLimitMs = cfg_.mode == Mode::FreePlay ? 0 : cfg_.startMs;
    r.started = started_;
    r.ended = by[0] ? isoLocalTime() : std::string();
    r.endedBy = by;
    if (!session_) return r;
    r.elapsedMs = session_->elapsedMs();
    r.authors = session_->authorCount();
    r.golds = session_->goldCount();
    r.skips = session_->skipCount();
    const std::vector<HistoryEntry>& h = session_->history();
    for (size_t i = 0; i < h.size(); ++i) {
        RunMapRecord m;
        m.tmxId = h[i].map.id;
        m.uid = h[i].map.uid;
        m.name = h[i].map.name;
        m.author = h[i].map.author;
        m.result = resultKey(h[i].result);
        m.medal = Session::medalName(h[i].medal);
        m.bestMs = h[i].bestMs;
        m.spentMs = h[i].spentMs;
        m.medals = h[i].map.medals;
        if (i < replays_.size()) m.replay = replays_[i];
        m.site = siteHost(i < mapSites_.size() ? mapSites_[i] : site_);
        r.maps.push_back(m);
    }
    return r;
}

void RunController::endRun(const char* by) {
    if (session_ && session_->state() != State::Over) session_->stop();
    fetcher_.requestStop();
    if (pending_) deleteFile(pending_->path);
    releaseMap(pending_);
    releaseMap(cur_);
    injected_.clear();
    phase_ = Phase::Over;
    notice_.clear();
    if (!session_) return;

    last_ = snapshot(by);
    lastPath_.clear();
    if (history_.save(last_, &lastPath_)) host_.log(LogInfo, "run saved: " + lastPath_);
    else setError("Could not save the run file.");
    host_.log(LogInfo, std::string("run ended (") + by + "): AT " + std::to_string(last_.authors) + ", gold " +
                           std::to_string(last_.golds) + ", skips " + std::to_string(last_.skips));
}

// ------------------------------------------------------------------ game events

void RunController::onEvent(const GameEvent& e) {
    if (e.type == EventType::RaceStart || e.type == EventType::Checkpoint || e.type == EventType::Finish)
        hostHasRaceEvents_ = true;
    if (!active()) return;
    switch (e.type) {
    case EventType::MapLoaded:
        loadedUid_ = e.mapUid;
        mapLoadedAt_ = host_.nowMs();
        if (phase_ == Phase::Playing && cur_) onOurMap_ = sameUid(e.mapUid, cur_->uid);
        return;
    case EventType::RaceStart: {
        std::string uid = e.hasMap ? e.mapUid : loadedUid_;
        raceEnded_ = false;
        if (phase_ == Phase::Loading && pending_) {
            if (sameUid(uid, pending_->uid)) beginMap();
            else setError("A different map is running; waiting for the randomizer map.");
        } else if (phase_ == Phase::Playing && cur_) {
            onOurMap_ = sameUid(uid, cur_->uid);
            if (onOurMap_) {
                userLeft_ = reloading_ = false;
                notice_.clear();
            }
        }
        return;
    }
    case EventType::Finish:
        if (phase_ != Phase::Playing || !onOurMap_ || userLeft_ || !e.isLocalPlayer || !e.solo) return;
        {
            FinishOutcome o = session_->finish(e.timeMs);
            if (o.accepted)
                host_.log(LogInfo, "finish " + formatMs(e.timeMs) + " (" + Session::medalName(o.medal) + ")" +
                                       (o.counted ? ", goal" : ""));
            if (o.counted) mapResolved(true);
        }
        return;
    case EventType::RaceEnd:
        raceEnded_ = true;
        if (phase_ == Phase::Leaving && goalReached_ && settings_.replayAutosave && host_.canSaveReplay() && !replayTried_ &&
            cur_) {
            replayTried_ = true;
            std::string name = "LegacyRandomizer\\" + fileStampLocal() + "_" + std::to_string(cur_->tmx.id) + "_" +
                               Session::medalName(bestMedal()) + ".Replay.Gbx";
            HostResult r = host_.saveReplay(name);
            if (r == HostResult::Ok && !replays_.empty()) replays_.back() = name;
            else if (r != HostResult::Ok) host_.log(LogWarn, std::string("replay save: ") + resultText(r));
        }
        return;
    case EventType::MenuEntered:
        if (phase_ == Phase::Playing && !reloading_) {
            userLeft_ = true;
            onOurMap_ = false;
        }
        return;
    case EventType::ReplaySaved:
        if (phase_ == Phase::Leaving && !replays_.empty()) replays_.back() = e.replayPath;
        else if (phase_ == Phase::Playing) curReplay_ = e.replayPath;
        return;
    case EventType::OnlineChanged:
        if (e.online) notice_ = "Paused: the randomizer does nothing in net play.";
        else notice_.clear();
        return;
    case EventType::Respawn:
    case EventType::Checkpoint:
    case EventType::GameStateChanged: return;
    }
}

} // namespace lr

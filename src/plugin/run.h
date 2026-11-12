// The run flow on top of lr::Session: fetch -> play -> finish/skip -> leave the map -> play the next
// (prefetched) map, until the timer runs out or the user ends the run. Lives on the game thread; only
// the Fetcher works in the background.
//
// Game-control rules:
//  - Nothing at all while the host reports online / gated (actionsAllowed() == false).
//  - PlayMapFile only in the main menu.
//  - ReturnToMenu: ESWC/Sunrise/Original in a solo race or on the end screen; TMNF/TMUF (Competition
//    Patch 2.0) ONLY on the end screen. A skip during a Forever run asks the player to quit the map
//    themselves (Esc > Quit); the next map starts from the menu.
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../core/session.h"
#include "fetcher.h"
#include "history.h"
#include "host.h"
#include "settings.h"

namespace lr {

enum class Phase {
    Idle,       // no run
    Preparing,  // run started, first map being fetched
    Loading,    // PlayMapFile sent, waiting for the race to start
    Playing,    // the session's current map is being played
    Leaving,    // map done (goal / skip), leaving it
    WaitingMap, // in the menu, waiting for the next map (fetch) or for the game
    Over        // run ended, results shown
};

class RunController {
public:
    RunController(Host& host, Settings& settings, History& history);
    ~RunController();

    // --- user actions (UI / hotkeys) ---
    bool start(std::string* why);  // false + reason when not possible now
    void stop();                   // end the run now (results are saved)
    void skip();
    void goldSkip();
    void nextNow();                // leave now (auto-advance off, or don't want to wait)
    void resume();                 // replay the current map after the player left it to the menu
    void dropPendingMap();         // the pending map did not start: take another one
    void newRun();                 // Over -> Idle

    // --- driven by the plugin ---
    void update();                 // every frame, game thread
    void onEvent(const GameEvent& e);
    void shutdown();

    // --- state for the UI ---
    Phase phase() const { return phase_; }
    bool active() const { return phase_ != Phase::Idle && phase_ != Phase::Over; }
    const Session* session() const { return session_.get(); }
    std::shared_ptr<PreparedMap> displayMap() const { return cur_ ? cur_ : pending_; }
    bool onOurMap() const { return onOurMap_; }
    int bestMs() const;
    Medal bestMedal() const;
    bool canSkip() const;
    bool canGoldSkip() const;
    bool goalJustReached() const { return phase_ == Phase::Leaving && goalReached_; }
    bool waitingForUser() const { return phase_ == Phase::Leaving && waitUser_; }
    bool leftMap() const { return phase_ == Phase::Playing && userLeft_; }
    std::string notice() const { return notice_; }  // instruction for the player ("" none)
    std::string error() const { return error_; }
    std::string fetchStatus();
    bool fetchFailing();
    Site site() const { return site_; }
    const std::vector<Site>& sites() const { return sites_; }  // the run's TMX sites (Forever "both": two)
    bool forever() const;
    const RunRecord& lastRun() const { return last_; }
    RunRecord snapshot(const char* endedBy = "") const;  // the run so far, as it would be saved
    std::string lastRunPath() const { return lastPath_; }
    Fetcher& fetcher() { return fetcher_; }
    int pendingLoadAgeMs() const;

    // preview/test hook: a run built from given maps, without TMX
    void debugInjectMap(std::shared_ptr<PreparedMap> m) { injected_.push_back(std::move(m)); }

private:
    void pump();
    void tryPlayNext();
    void leaveMap();
    void beginMap();
    void mapResolved(bool goal);
    void endRun(const char* by);
    void setError(const std::string& e);
    void releaseMap(std::shared_ptr<PreparedMap>& m);
    std::shared_ptr<PreparedMap> takeNext();

    Host& host_;
    Settings& settings_;
    History& history_;
    Fetcher fetcher_;
    std::unique_ptr<Session> session_;
    Phase phase_ = Phase::Idle;
    Site site_ = Site::TMNF;            // the first of sites_
    std::vector<Site> sites_;
    std::vector<Site> mapSites_;        // per started map (history entry), for the run file
    SessionConfig cfg_;
    std::string started_;

    std::shared_ptr<PreparedMap> cur_;      // map of the session (playing / just resolved)
    std::shared_ptr<PreparedMap> pending_;  // map being loaded
    std::vector<std::shared_ptr<PreparedMap>> injected_;
    std::vector<std::string> replays_;      // per session history entry
    std::string curReplay_;

    bool onOurMap_ = false;
    bool userLeft_ = false;
    bool goalReached_ = false;
    bool waitUser_ = false;
    bool raceEnded_ = false;
    bool callQueued_ = false;   // a RunOnGameThread call of ours is still pending
    bool leaveSent_ = false;
    bool reloading_ = false;
    bool manualLeave_ = false;  // the host can't leave the map: the player does it
    bool replayTried_ = false;
    bool hostHasRaceEvents_ = false;  // seen RACE_START/CHECKPOINT/FINISH from this host at least once
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);  // guards queued game-thread calls
    std::string loadedUid_;
    double lastTick_ = 0, advanceAt_ = 0, loadSentAt_ = 0, leaveSentAt_ = 0, retryAt_ = 0, goalAt_ = 0;
    double tickCarry_ = 0;
    int waitLoggedState_ = -1;  // game state last logged while waiting to start a map (logged once per state)
    double mapLoadedAt_ = 0;  // MAP_LOADED of the pending map (fallback when RACE_START never comes)
    int loadFailures_ = 0;
    std::string notice_, error_;

    RunRecord last_;
    std::string lastPath_;

    // Textures of maps let go during a frame: the draw data of that frame may still use them, so they are
    // released at the next update() (start of the next frame), never mid-frame.
    std::vector<ImTextureID> deadTex_;
    void releaseDeadTextures();
    // the run file is written by a small thread (no file I/O on the game thread when a run ends)
    std::thread saver_;
    std::atomic<int> saveState_{0};  // 0 idle/writing, 1 written, 2 failed
};

const char* phaseName(Phase p);

} // namespace lr

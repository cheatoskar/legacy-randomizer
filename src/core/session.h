// Random Map Challenge session logic: a pure state machine, no I/O, no clock. The host feeds events
// (map started, finish, skip, gold skip, tick) and reads the state.
//
// Rules implemented (written from the publicly documented RMC / RMS rules; the exact numbers are
// all in SessionConfig so they can be adjusted):
//  RMC  60 min countdown. Goal medal Author (default) or Gold. Reaching the goal counts and the host
//       loads the next map. Skips cost no extra time (the time on the map is simply lost), except the
//       first `freeSkips` ones, which are for broken maps: they refund the time spent on that map.
//       Gold skip: with goal Author, a map you have gold (or better) on may be skipped and counts
//       as a gold medal (tie-break counter).
//  RMS  Survival. Start 15 min, +3 min per author medal, +1 min per gold medal / gold skip, -1 min per
//       skip; the first skip is free (no penalty). Run ends when the timer reaches 0.
//  FreePlay  No limit, counts up, nothing is penalised.
// The timer only runs while a map is being played (not between a map ending and mapStarted()).
#pragma once
#include <string>
#include <vector>

namespace lr {

enum class Mode { RMC, RMS, FreePlay };
enum class Goal { Author, Gold };
enum class Medal { None = 0, Bronze, Silver, Gold, Author };
enum class State { Idle, WaitingForMap, Playing, Over };
enum class SkipKind { NotAllowed, Free, Normal };
enum class EntryResult { Author, Gold, GoldSkip, Skipped, FreeSkipped, Unfinished };

struct MapMedals {
    int authorMs = 0, goldMs = 0, silverMs = 0, bronzeMs = 0;  // 0 = unknown
};

struct SessionMap {
    long long id = 0;
    std::string uid, name, author;
    MapMedals medals;
};

struct SessionConfig {
    Mode mode = Mode::RMC;
    Goal goal = Goal::Author;
    int startMs = 60 * 60 * 1000;  // countdown start (ignored for FreePlay)
    int bonusAuthorMs = 0;         // added on an author medal
    int bonusGoldMs = 0;           // added on a gold finish (goal Gold) or gold skip
    int skipPenaltyMs = 0;         // taken per non-free skip
    int freeSkips = 1;             // skips that cost nothing (-1 = unlimited)
    bool freeSkipRefundsTime = true;  // a free skip gives back the time spent on that map
    bool allowGoldSkip = true;     // gold skip rule (only meaningful with goal Author)

    static SessionConfig rmc(Goal g = Goal::Author);
    static SessionConfig rms();
    static SessionConfig freePlay();
};

struct HistoryEntry {
    SessionMap map;
    EntryResult result = EntryResult::Skipped;
    Medal medal = Medal::None;  // best medal reached on the map
    int bestMs = 0;             // best finish time on the map, 0 = none
    int spentMs = 0;            // time spent playing it (before any refund)
};

struct FinishOutcome {
    Medal medal = Medal::None;
    bool counted = false;  // the finish reached the goal: map is done, host should load the next one
    bool accepted = false; // false when no map was being played
};

class Session {
public:
    explicit Session(const SessionConfig& cfg);

    static Medal medalFor(int timeMs, const MapMedals& m);
    static const char* medalName(Medal m);
    static std::string formatTime(int ms);  // "12:34.567" / "1:02:03.004"

    // --- events in ---
    void start();                       // Idle -> WaitingForMap
    void mapStarted(const SessionMap& m);  // map is loaded and the player can drive
    FinishOutcome finish(int timeMs);
    SkipKind skip();                    // plain/free skip; NotAllowed when no map is being played
    bool goldSkip();                    // only when allowed by the rules and gold was reached on this map
    void tick(int deltaMs);             // time passed (counts only while Playing)
    void stop();                        // end the run manually

    // --- state out ---
    State state() const { return state_; }
    const SessionConfig& config() const { return cfg_; }
    bool countsDown() const { return cfg_.mode != Mode::FreePlay; }
    int remainingMs() const { return remainingMs_; }  // 0 for FreePlay
    int elapsedMs() const { return elapsedMs_; }      // total played time
    int authorCount() const { return authors_; }
    int goldCount() const { return golds_; }          // gold finishes + gold skips
    int skipCount() const { return skips_; }          // all skips incl. free ones, not gold skips
    int freeSkipsLeft() const;                        // -1 = unlimited
    const SessionMap* currentMap() const { return state_ == State::Playing ? &cur_ : nullptr; }
    int currentSpentMs() const { return spentMs_; }
    int currentBestMs() const { return bestMs_; }
    Medal currentMedal() const { return medal_; }
    bool canGoldSkip() const;
    const std::vector<HistoryEntry>& history() const { return history_; }

private:
    void resolve(EntryResult r);
    void checkTimeout();

    SessionConfig cfg_;
    State state_ = State::Idle;
    SessionMap cur_;
    int remainingMs_ = 0, elapsedMs_ = 0, spentMs_ = 0, bestMs_ = 0;
    Medal medal_ = Medal::None;
    int authors_ = 0, golds_ = 0, skips_ = 0, freeUsed_ = 0;
    std::vector<HistoryEntry> history_;
};

} // namespace lr

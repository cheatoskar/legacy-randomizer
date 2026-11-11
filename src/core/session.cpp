#include "session.h"

#include <cstdio>

namespace lr {

SessionConfig SessionConfig::rmc(Goal g) {
    SessionConfig c;
    c.mode = Mode::RMC;
    c.goal = g;
    c.startMs = 60 * 60 * 1000;
    c.freeSkips = 1;
    c.freeSkipRefundsTime = true;
    c.allowGoldSkip = true;
    return c;
}

SessionConfig SessionConfig::rms() {
    SessionConfig c;
    c.mode = Mode::RMS;
    c.goal = Goal::Author;
    c.startMs = 15 * 60 * 1000;
    c.bonusAuthorMs = 3 * 60 * 1000;
    c.bonusGoldMs = 1 * 60 * 1000;
    c.skipPenaltyMs = 1 * 60 * 1000;
    c.freeSkips = 1;
    c.freeSkipRefundsTime = false;
    c.allowGoldSkip = true;
    return c;
}

SessionConfig SessionConfig::freePlay() {
    SessionConfig c;
    c.mode = Mode::FreePlay;
    c.startMs = 0;
    c.freeSkips = -1;
    c.freeSkipRefundsTime = false;
    c.allowGoldSkip = true;
    return c;
}

Session::Session(const SessionConfig& cfg) : cfg_(cfg) {
    remainingMs_ = cfg.mode == Mode::FreePlay ? 0 : cfg.startMs;
}

Medal Session::medalFor(int t, const MapMedals& m) {
    if (t <= 0) return Medal::None;
    if (m.authorMs > 0 && t <= m.authorMs) return Medal::Author;
    if (m.goldMs > 0 && t <= m.goldMs) return Medal::Gold;
    if (m.silverMs > 0 && t <= m.silverMs) return Medal::Silver;
    if (m.bronzeMs > 0 && t <= m.bronzeMs) return Medal::Bronze;
    return Medal::None;
}

const char* Session::medalName(Medal m) {
    switch (m) {
    case Medal::None: return "none";
    case Medal::Bronze: return "bronze";
    case Medal::Silver: return "silver";
    case Medal::Gold: return "gold";
    case Medal::Author: return "author";
    }
    return "";
}

std::string Session::formatTime(int ms) {
    if (ms < 0) ms = 0;
    int h = ms / 3600000, m = (ms / 60000) % 60, s = (ms / 1000) % 60, f = ms % 1000;
    char b[40];
    if (h > 0) std::snprintf(b, sizeof b, "%d:%02d:%02d.%03d", h, m, s, f);
    else std::snprintf(b, sizeof b, "%d:%02d.%03d", m, s, f);
    return b;
}

int Session::freeSkipsLeft() const {
    if (cfg_.freeSkips < 0) return -1;
    return cfg_.freeSkips > freeUsed_ ? cfg_.freeSkips - freeUsed_ : 0;
}

bool Session::canGoldSkip() const {
    if (state_ != State::Playing || !cfg_.allowGoldSkip || cfg_.goal != Goal::Author) return false;
    return medal_ == Medal::Gold || medal_ == Medal::Author;
}

void Session::start() {
    if (state_ == State::Idle) state_ = State::WaitingForMap;
}

void Session::mapStarted(const SessionMap& m) {
    if (state_ == State::Idle || state_ == State::Over) return;
    // a new map while one is still being played: the old one counts as unfinished
    if (state_ == State::Playing) resolve(EntryResult::Unfinished);
    cur_ = m;
    spentMs_ = 0;
    bestMs_ = 0;
    medal_ = Medal::None;
    state_ = State::Playing;
}

void Session::resolve(EntryResult r) {
    HistoryEntry e;
    e.map = cur_;
    e.result = r;
    e.medal = medal_;
    e.bestMs = bestMs_;
    e.spentMs = spentMs_;
    history_.push_back(e);
    state_ = State::WaitingForMap;
}

FinishOutcome Session::finish(int timeMs) {
    FinishOutcome o;
    if (state_ != State::Playing || timeMs <= 0) return o;
    o.accepted = true;
    o.medal = Session::medalFor(timeMs, cur_.medals);
    if (bestMs_ == 0 || timeMs < bestMs_) bestMs_ = timeMs;
    if (o.medal > medal_) medal_ = o.medal;
    bool atGoal = cfg_.goal == Goal::Author ? o.medal == Medal::Author
                                            : (o.medal == Medal::Author || o.medal == Medal::Gold);
    if (!atGoal) return o;
    o.counted = true;
    if (o.medal == Medal::Author) {
        ++authors_;
        remainingMs_ += cfg_.bonusAuthorMs;
        resolve(EntryResult::Author);
    } else {
        ++golds_;
        remainingMs_ += cfg_.bonusGoldMs;
        resolve(EntryResult::Gold);
    }
    return o;
}

SkipKind Session::skip() {
    if (state_ != State::Playing) return SkipKind::NotAllowed;
    ++skips_;
    if (freeSkipsLeft() != 0) {
        ++freeUsed_;
        if (cfg_.freeSkipRefundsTime && cfg_.mode != Mode::FreePlay) {
            remainingMs_ += spentMs_;
            elapsedMs_ -= spentMs_;
        }
        resolve(EntryResult::FreeSkipped);
        return SkipKind::Free;
    }
    remainingMs_ -= cfg_.skipPenaltyMs;
    resolve(EntryResult::Skipped);
    checkTimeout();
    return SkipKind::Normal;
}

bool Session::goldSkip() {
    if (!canGoldSkip()) return false;
    ++golds_;
    remainingMs_ += cfg_.bonusGoldMs;
    resolve(EntryResult::GoldSkip);
    return true;
}

void Session::tick(int dt) {
    if (state_ != State::Playing || dt <= 0) return;
    spentMs_ += dt;
    elapsedMs_ += dt;
    if (cfg_.mode == Mode::FreePlay) return;
    remainingMs_ -= dt;
    checkTimeout();
}

void Session::checkTimeout() {
    if (cfg_.mode == Mode::FreePlay || remainingMs_ > 0) return;
    remainingMs_ = 0;
    if (state_ == State::Playing) resolve(EntryResult::Unfinished);
    state_ = State::Over;
}

void Session::stop() {
    if (state_ == State::Idle || state_ == State::Over) return;
    if (state_ == State::Playing) resolve(EntryResult::Unfinished);
    state_ = State::Over;
}

} // namespace lr

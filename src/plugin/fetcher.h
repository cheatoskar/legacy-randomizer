// Background map fetcher: keeps one random map ready (picked on TMX, downloaded into the plugin's
// data folder, header read, thumbnail decoded), so the next map can start without waiting.
// One worker thread; everything public is thread-safe.
#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "../core/gbx.h"
#include "../core/session.h"
#include "../core/tmx.h"
#include "image.h"
#include "settings.h"

namespace lr {

struct PreparedMap {
    Site site = Site::TMNF;
    MapInfo tmx;               // TMX data (id, name, uploader, times)
    std::string path;          // UTF-8, the downloaded .Challenge.Gbx
    std::string uid;
    std::string name;          // raw, with $ codes (from the map file, else TMX)
    std::string author;
    MapMedals medals;          // from the map header, TMX as fallback
    Image thumb;               // decoded RGBA (may be empty)
    std::string thumbSource;   // "TMX", "map file" or ""
    ImTextureID tex = 0;       // created on the game thread by the UI, released by the owner
    bool texTried = false;
};

// One TMX site to pick from. A run has one, on Forever with "both" two: then a site is picked per map
// (a coin flip, or with evenSites the site with fewer maps so far).
struct FetchSource {
    Site site = Site::TMNF;
    TmxFilter filter;
    std::vector<int> environments;    // pick one of these per request (equal distribution); empty: any
    std::set<long long> exclude;      // TMX ids of this site not to pick (played before, banned, skipped)
};
struct FetchJob {
    std::vector<FetchSource> sources;
    bool evenSites = false;
    std::string folder;               // UTF-8, trailing backslash; maps go here
    ThumbSource thumbs = ThumbSource::Tmx;
    std::vector<std::string> autosaveDirs;  // "never a map you finished before": replays in these folders
    std::string autosaveCache;        // cache file for the replay scan ("" none)
};

// One worker thread for one run. Stopping it cancels its HTTP request (the handles are closed), so it
// ends within a moment; nobody on the game thread waits for it (see Fetcher).
class FetchWorker {
public:
    using Source = FetchSource;
    using Job = FetchJob;
    using LogFn = std::function<void(LogLevel, const std::string&)>;

    explicit FetchWorker(LogFn log) : log_(std::move(log)), cancel_(std::make_shared<HttpCancel>()) {}
    ~FetchWorker() { stop(); }

    void start(const Job& job);
    void requestStop();          // returns at once: cancels the running request, the thread ends by itself
    void stop();                 // requestStop + join (quick: the request is cancelled)
    bool finished() const { return done_; }  // the thread has ended (join is instant)

    std::shared_ptr<PreparedMap> take();  // the ready map or null; the worker starts the next one
    bool hasReady();
    void exclude(Site site, long long id);
    void retryNow();                       // skip the back-off wait after an error

    std::string status();      // what the worker is doing / last error
    bool lastFailed();
    int failures();            // consecutive failures

private:
    void loop();
    bool fetchOne(PreparedMap& out, std::string& err);
    bool excluded(Site site, long long id);
    Source* source(Site site);  // under mu_
    std::vector<int> taken_;     // maps handed out per source (even site mix)
    void scanAutosaves(const Job& job);
    std::set<std::string> finished_;  // map uids with an autosave replay (worker thread only)

    LogFn log_;
    std::shared_ptr<HttpCancel> cancel_;
    std::atomic<bool> done_{false};
    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool stop_ = false, wake_ = false;
    Job job_;
    std::shared_ptr<PreparedMap> ready_;
    std::string status_;
    bool lastFailed_ = false;
    int failures_ = 0;
};

// Keeps the next map ready for the current run. Game-thread object; nothing here blocks the game
// thread: stopping hands the worker over to a list of stopped ones (their request cancelled), which
// are joined once they have ended, at the next start, and at shutdown (stop()).
class Fetcher {
public:
    using Source = FetchSource;
    using Job = FetchJob;
    using LogFn = FetchWorker::LogFn;

    explicit Fetcher(LogFn log) : log_(std::move(log)) {}
    ~Fetcher() { stop(); }

    void start(const Job& job);  // a new worker for a new run (a previous one is stopped, not waited for)
    void requestStop();          // returns at once
    void stop();                 // stops and joins every worker (shutdown; quick, requests are cancelled)
    bool running() const { return cur_ != nullptr; }

    std::shared_ptr<PreparedMap> take() { return cur_ ? cur_->take() : nullptr; }
    bool hasReady() { return cur_ && cur_->hasReady(); }
    void exclude(Site site, long long id) { if (cur_) cur_->exclude(site, id); }
    void retryNow() { if (cur_) cur_->retryNow(); }
    std::string status() { return cur_ ? cur_->status() : std::string(); }
    bool lastFailed() { return cur_ && cur_->lastFailed(); }
    int failures() { return cur_ ? cur_->failures() : 0; }

private:
    void retire();  // current worker -> stopped list
    void reap();    // join the stopped workers whose thread has ended

    LogFn log_;
    std::unique_ptr<FetchWorker> cur_;
    std::vector<std::unique_ptr<FetchWorker>> old_;
};

} // namespace lr

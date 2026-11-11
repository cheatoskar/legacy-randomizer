// Background map fetcher: keeps one random map ready (picked on TMX, downloaded into the plugin's
// data folder, header read, thumbnail decoded), so the next map can start without waiting.
// One worker thread; everything public is thread-safe.
#pragma once
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

class Fetcher {
public:
    struct Job {
        Site site = Site::TMNF;
        TmxFilter filter;
        std::string folder;               // UTF-8, trailing backslash; maps go here
        ThumbSource thumbs = ThumbSource::Tmx;
        std::set<long long> exclude;      // TMX ids not to pick (played before, banned, skipped)
        std::vector<int> environments;    // pick one of these per request (equal distribution); empty: any
        std::vector<std::string> autosaveDirs;  // "never a map you finished before": replays in these folders
        std::string autosaveCache;        // cache file for the replay scan ("" none)
    };
    using LogFn = std::function<void(LogLevel, const std::string&)>;

    explicit Fetcher(LogFn log) : log_(std::move(log)) {}
    ~Fetcher() { stop(); }

    void start(const Job& job);  // (re)starts the worker; it keeps one map ready
    void requestStop();          // returns at once; the worker ends after its current request
    void stop();                 // requestStop + join (waits for a running HTTP request, max ~20 s)
    bool running() const { return thread_.joinable(); }

    std::shared_ptr<PreparedMap> take();  // the ready map or null; the worker starts the next one
    bool hasReady();
    void exclude(long long id);
    void retryNow();                       // skip the back-off wait after an error

    std::string status();      // what the worker is doing / last error
    bool lastFailed();
    int failures();            // consecutive failures

private:
    void loop();
    bool fetchOne(PreparedMap& out, std::string& err);
    bool excluded(long long id);
    void scanAutosaves(const Job& job);
    std::set<std::string> finished_;  // map uids with an autosave replay (worker thread only)

    LogFn log_;
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

} // namespace lr

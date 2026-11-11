#include "fetcher.h"

#include <chrono>
#include <cstdio>
#include <map>

#include <windows.h>
#include <objbase.h>

#include "util.h"

#include <random>
#include <vector>

namespace lr {

void Fetcher::start(const Job& job) {
    stop();
    {
        std::lock_guard<std::mutex> l(mu_);
        job_ = job;
        taken_.assign(job_.sources.size(), 0);
        stop_ = false;
        wake_ = false;
        ready_.reset();
        status_ = "Starting";
        lastFailed_ = false;
        failures_ = 0;
    }
    ensureDir(job.folder);
    thread_ = std::thread([this] { loop(); });
}

void Fetcher::requestStop() {
    {
        std::lock_guard<std::mutex> l(mu_);
        stop_ = true;
        // a prefetched map that will never be played
        if (ready_) deleteFile(ready_->path);
        ready_.reset();
    }
    cv_.notify_all();
}

void Fetcher::stop() {
    requestStop();
    if (thread_.joinable()) thread_.join();
}

std::shared_ptr<PreparedMap> Fetcher::take() {
    std::shared_ptr<PreparedMap> m;
    {
        std::lock_guard<std::mutex> l(mu_);
        m = std::move(ready_);
        ready_.reset();
        if (m) {
            if (Source* s = source(m->site)) s->exclude.insert(m->tmx.id);
            for (size_t i = 0; i < job_.sources.size(); ++i)
                if (job_.sources[i].site == m->site) ++taken_[i];
        }
    }
    cv_.notify_all();
    return m;
}

bool Fetcher::hasReady() {
    std::lock_guard<std::mutex> l(mu_);
    return ready_ != nullptr;
}

Fetcher::Source* Fetcher::source(Site site) {
    for (Source& s : job_.sources)
        if (s.site == site) return &s;
    return nullptr;
}

void Fetcher::exclude(Site site, long long id) {
    std::lock_guard<std::mutex> l(mu_);
    if (Source* s = source(site)) s->exclude.insert(id);
}

void Fetcher::retryNow() {
    {
        std::lock_guard<std::mutex> l(mu_);
        wake_ = true;
    }
    cv_.notify_all();
}

std::string Fetcher::status() {
    std::lock_guard<std::mutex> l(mu_);
    return status_;
}

bool Fetcher::lastFailed() {
    std::lock_guard<std::mutex> l(mu_);
    return lastFailed_;
}

int Fetcher::failures() {
    std::lock_guard<std::mutex> l(mu_);
    return failures_;
}

bool Fetcher::excluded(Site site, long long id) {
    std::lock_guard<std::mutex> l(mu_);
    Source* s = source(site);
    return s && s->exclude.count(id) != 0;
}

void Fetcher::loop() {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    {
        Job copy;
        {
            std::lock_guard<std::mutex> l(mu_);
            copy.autosaveDirs = job_.autosaveDirs;
            copy.autosaveCache = job_.autosaveCache;
        }
        scanAutosaves(copy);
    }
    for (;;) {
        {
            std::unique_lock<std::mutex> l(mu_);
            cv_.wait(l, [this] { return stop_ || !ready_; });
            if (stop_) break;
        }
        PreparedMap m;
        std::string err;
        bool ok = fetchOne(m, err);
        std::unique_lock<std::mutex> l(mu_);
        if (stop_) {
            if (ok) deleteFile(m.path);
            break;
        }
        if (ok) {
            ready_ = std::make_shared<PreparedMap>(std::move(m));
            status_ = "ready";
            lastFailed_ = false;
            failures_ = 0;
            continue;
        }
        ++failures_;
        lastFailed_ = true;
        // back-off: 2, 5, 10, 20, 30 s ...
        static const int kWait[] = {2, 5, 10, 20, 30};
        int wait = kWait[failures_ - 1 < 4 ? failures_ - 1 : 4];
        status_ = err + " (retry in " + std::to_string(wait) + " s)";
        if (log_) log_(LogWarn, "fetch failed: " + err);
        wake_ = false;
        cv_.wait_for(l, std::chrono::seconds(wait), [this] { return stop_ || wake_; });
        if (stop_) break;
    }
    if (SUCCEEDED(co)) CoUninitialize();
}

bool Fetcher::fetchOne(PreparedMap& out, std::string& err) {
    static std::mt19937 rng(static_cast<unsigned>(GetTickCount()));
    // the site of this map: the only one, or of two a coin flip / the one with fewer maps so far
    std::vector<Source> order;
    Job job;
    {
        std::lock_guard<std::mutex> l(mu_);
        if (job_.sources.empty()) {
            err = "no TMX site to pick from";
            return false;
        }
        size_t first = 0;
        if (job_.sources.size() > 1) {
            first = std::uniform_int_distribution<size_t>(0, job_.sources.size() - 1)(rng);
            if (job_.evenSites)
                for (size_t i = 0; i < job_.sources.size(); ++i)
                    if (taken_[i] < taken_[first]) first = i;
        }
        for (size_t k = 0; k < job_.sources.size(); ++k) order.push_back(job_.sources[(first + k) % job_.sources.size()]);
        for (Source& s : order) s.exclude.clear();  // checked under the lock (excluded())
        job.folder = job_.folder;
        job.thumbs = job_.thumbs;
        status_ = std::string("Picking a random map on ") + siteHost(order[0].site);
    }
    HttpOptions http;
    http.timeoutMs = 10000;
    http.retries = 1;
    int rejected = 0;
    size_t which = 0;  // index into order; the other site is tried when this one fails
    for (int attempt = 0; attempt < 8; ++attempt) {
        Source& src = order[which];
        // environment first (chosen ones / equal distribution), then a random map of it
        if (!src.environments.empty())
            src.filter.environment = src.environments[std::uniform_int_distribution<size_t>(0, src.environments.size() - 1)(rng)];
        MapInfo info;
        TmxError e = getRandomMap(src.site, src.filter, info, http);
        if (!e.ok()) {
            if (which + 1 < order.size()) {
                if (log_) log_(LogWarn, std::string(siteHost(src.site)) + ": " + e.message + ", trying " + siteHost(order[which + 1].site));
                ++which;
                continue;
            }
            err = e.message;
            return false;
        }
        if (excluded(src.site, info.id)) { ++rejected; continue; }
        if (!info.uid.empty() && finished_.count(info.uid)) {
            if (log_) log_(LogInfo, "map " + std::to_string(info.id) + " was finished before (autosave), next one");
            exclude(src.site, info.id);
            ++rejected;
            continue;
        }
        {
            std::lock_guard<std::mutex> l(mu_);
            status_ = "Downloading map " + std::to_string(info.id);
        }
        std::string path = job.folder + siteName(src.site) + std::string("_") + std::to_string(info.id) + ".Challenge.Gbx";
        for (char& c : path)
            if (c == ' ') c = '_';
        std::string bytes;
        e = downloadMap(src.site, info.id, path, &bytes, http);
        if (!e.ok()) { err = "download of map " + std::to_string(info.id) + ": " + e.message; return false; }
        GbxMapHeader h = parseChallengeHeader(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
        if (!h.ok) {
            if (log_) log_(LogWarn, "map " + std::to_string(info.id) + " has an unreadable header (" + h.error + "), next one");
            exclude(src.site, info.id);
            deleteFile(path);
            continue;
        }
        PreparedMap m;
        m.site = src.site;
        m.tmx = info;
        m.path = path;
        m.uid = !h.uid.empty() ? h.uid : info.uid;
        m.name = !h.name.empty() ? h.name : info.name;
        m.author = !h.author.empty() ? h.author : info.author;
        m.medals.authorMs = h.authorMs > 0 ? h.authorMs : info.authorMs;
        m.medals.goldMs = h.goldMs > 0 ? h.goldMs : info.goldMs;
        m.medals.silverMs = h.silverMs > 0 ? h.silverMs : info.silverMs;
        m.medals.bronzeMs = h.bronzeMs > 0 ? h.bronzeMs : info.bronzeMs;
        if (src.filter.requireAuthorTime && m.medals.authorMs <= 0) {
            if (log_) log_(LogInfo, "map " + std::to_string(info.id) + " has no author time, next one");
            exclude(src.site, info.id);
            deleteFile(path);
            continue;
        }
        if (m.medals.goldMs <= 0) {
            if (log_) log_(LogInfo, "map " + std::to_string(info.id) + " has no medal times, next one");
            exclude(src.site, info.id);
            deleteFile(path);
            continue;
        }
        // thumbnail: TMX screenshot, else the JPEG embedded in the map file (works offline too)
        std::string ierr;
        if (job.thumbs == ThumbSource::Tmx) {
            std::string img;
            if (downloadThumbnail(src.site, info.id, img, http).ok() && decodeImage(img, 512, 512, false, m.thumb, &ierr))
                m.thumbSource = "TMX";
        }
        if (job.thumbs != ThumbSource::Off && !m.thumb.ok() && !h.thumbnailJpeg.empty() &&
            decodeImage(h.thumbnailJpeg, 512, 512, true, m.thumb, &ierr))
            m.thumbSource = "map file";
        out = std::move(m);
        return true;
    }
    err = rejected ? "TMX keeps returning maps you already played; loosen the filter" : "no usable map found";
    return false;
}

} // namespace lr

namespace lr {

// Map uids of every replay in the Autosaves folders. A cache (file name, size, write time -> uid) keeps
// later starts fast; only new or changed replays are read (their first 64 KiB).
void Fetcher::scanAutosaves(const Job& job) {
    finished_.clear();
    if (job.autosaveDirs.empty()) return;
    {
        std::lock_guard<std::mutex> l(mu_);
        status_ = "Scanning your autosave replays";
    }
    std::map<std::string, std::string> cache, fresh;
    std::string text;
    if (!job.autosaveCache.empty() && readFile(job.autosaveCache, text)) {
        size_t p = 0;
        while (p < text.size()) {
            size_t e = text.find('\n', p);
            if (e == std::string::npos) e = text.size();
            std::string line = text.substr(p, e - p);
            size_t bar = line.rfind('|');
            if (bar != std::string::npos) cache[line.substr(0, bar)] = line.substr(bar + 1);
            p = e + 1;
        }
    }
    int files = 0, read = 0;
    for (const std::string& dir : job.autosaveDirs) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(toWide(dir + "*.gbx").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ++files;
            std::string name = toUtf8(fd.cFileName);
            std::string key = dir + name + "|" + std::to_string(fd.nFileSizeLow) + "|" +
                              std::to_string((static_cast<unsigned long long>(fd.ftLastWriteTime.dwHighDateTime) << 32) | fd.ftLastWriteTime.dwLowDateTime);
            std::string uid;
            auto it = cache.find(key);
            if (it != cache.end()) {
                uid = it->second;
            } else {
                FILE* fp = nullptr;
                if (_wfopen_s(&fp, toWide(dir + name).c_str(), L"rb") == 0 && fp) {
                    std::vector<unsigned char> buf(64 * 1024);
                    size_t n = std::fread(buf.data(), 1, buf.size(), fp);
                    std::fclose(fp);
                    uid = replayMapUid(buf.data(), n);
                    ++read;
                }
            }
            fresh[key] = uid;
            if (!uid.empty()) finished_.insert(uid);
            {
                std::lock_guard<std::mutex> l(mu_);
                if (stop_) { FindClose(h); return; }
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (!job.autosaveCache.empty() && read > 0) {
        std::string out;
        for (const auto& kv : fresh) out += kv.first + "|" + kv.second + "\n";
        writeFileAtomic(job.autosaveCache, out);
    }
    if (log_)
        log_(LogInfo, "autosaves: " + std::to_string(files) + " replays, " + std::to_string(finished_.size()) + " finished maps (" +
                          std::to_string(read) + " read, rest from cache)");
}

} // namespace lr

#include "fetcher.h"

#include <chrono>

#include <windows.h>
#include <objbase.h>

#include "util.h"

namespace lr {

void Fetcher::start(const Job& job) {
    stop();
    {
        std::lock_guard<std::mutex> l(mu_);
        job_ = job;
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
        if (m) job_.exclude.insert(m->tmx.id);
    }
    cv_.notify_all();
    return m;
}

bool Fetcher::hasReady() {
    std::lock_guard<std::mutex> l(mu_);
    return ready_ != nullptr;
}

void Fetcher::exclude(long long id) {
    std::lock_guard<std::mutex> l(mu_);
    job_.exclude.insert(id);
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

bool Fetcher::excluded(long long id) {
    std::lock_guard<std::mutex> l(mu_);
    return job_.exclude.count(id) != 0;
}

void Fetcher::loop() {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
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
            status_ = "Next map ready";
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
    Job job;
    {
        std::lock_guard<std::mutex> l(mu_);
        job.site = job_.site;
        job.filter = job_.filter;
        job.folder = job_.folder;
        job.thumbs = job_.thumbs;
        status_ = std::string("Picking a random map on ") + siteHost(job.site);
    }
    HttpOptions http;
    http.timeoutMs = 10000;
    http.retries = 1;
    int rejected = 0;
    for (int attempt = 0; attempt < 8; ++attempt) {
        MapInfo info;
        TmxError e = getRandomMap(job.site, job.filter, info, http);
        if (!e.ok()) { err = e.message; return false; }
        if (excluded(info.id)) { ++rejected; continue; }
        {
            std::lock_guard<std::mutex> l(mu_);
            status_ = "Downloading map " + std::to_string(info.id);
        }
        std::string path = job.folder + siteName(job.site) + std::string("_") + std::to_string(info.id) + ".Challenge.Gbx";
        for (char& c : path)
            if (c == ' ') c = '_';
        std::string bytes;
        e = downloadMap(job.site, info.id, path, &bytes, http);
        if (!e.ok()) { err = "download of map " + std::to_string(info.id) + ": " + e.message; return false; }
        GbxMapHeader h = parseChallengeHeader(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
        if (!h.ok) {
            if (log_) log_(LogWarn, "map " + std::to_string(info.id) + " has an unreadable header (" + h.error + "), next one");
            exclude(info.id);
            deleteFile(path);
            continue;
        }
        PreparedMap m;
        m.site = job.site;
        m.tmx = info;
        m.path = path;
        m.uid = !h.uid.empty() ? h.uid : info.uid;
        m.name = !h.name.empty() ? h.name : info.name;
        m.author = !h.author.empty() ? h.author : info.author;
        m.medals.authorMs = h.authorMs > 0 ? h.authorMs : info.authorMs;
        m.medals.goldMs = h.goldMs > 0 ? h.goldMs : info.goldMs;
        m.medals.silverMs = h.silverMs > 0 ? h.silverMs : info.silverMs;
        m.medals.bronzeMs = h.bronzeMs > 0 ? h.bronzeMs : info.bronzeMs;
        if (job.filter.requireAuthorTime && m.medals.authorMs <= 0) {
            if (log_) log_(LogInfo, "map " + std::to_string(info.id) + " has no author time, next one");
            exclude(info.id);
            deleteFile(path);
            continue;
        }
        if (m.medals.goldMs <= 0) {
            if (log_) log_(LogInfo, "map " + std::to_string(info.id) + " has no medal times, next one");
            exclude(info.id);
            deleteFile(path);
            continue;
        }
        // thumbnail: TMX screenshot, else the JPEG embedded in the map file (works offline too)
        std::string ierr;
        if (job.thumbs == ThumbSource::Tmx) {
            std::string img;
            if (downloadThumbnail(job.site, info.id, img, http).ok() && decodeImage(img, 512, 512, false, m.thumb, &ierr))
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

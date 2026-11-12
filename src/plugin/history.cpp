#include "history.h"

#include <algorithm>

#include "../core/json.h"
#include "util.h"

namespace lr {

const char* resultKey(EntryResult r) {
    switch (r) {
    case EntryResult::Author: return "author";
    case EntryResult::Gold: return "gold";
    case EntryResult::GoldSkip: return "gold_skip";
    case EntryResult::Skipped: return "skipped";
    case EntryResult::FreeSkipped: return "free_skip";
    case EntryResult::Unfinished: return "unfinished";
    case EntryResult::Finished: return "finished";
    }
    return "unfinished";
}

namespace {

void kv(std::string& o, const char* k, const std::string& v, bool comma = true) {
    o += "\"";
    o += k;
    o += "\": \"" + jsonEscape(v) + "\"";
    if (comma) o += ", ";
}
void kv(std::string& o, const char* k, long long v, bool comma = true) {
    o += "\"";
    o += k;
    o += "\": " + std::to_string(v);
    if (comma) o += ", ";
}

std::string str(const Json& j, const char* k) {
    const Json* v = j.get(k);
    return v ? v->asString() : std::string();
}
long long num(const Json& j, const char* k) {
    const Json* v = j.get(k);
    return v ? v->asInt() : 0;
}

} // namespace

std::string runToJson(const RunRecord& r) {
    std::string o = "{\n  ";
    kv(o, "format", 1);
    kv(o, "game", r.game);
    kv(o, "site", r.site);
    o += "\n  ";
    kv(o, "mode", r.mode);
    kv(o, "goal", r.goal);
    kv(o, "time_limit_ms", r.timeLimitMs);
    o += "\n  ";
    kv(o, "started", r.started);
    kv(o, "ended", r.ended);
    kv(o, "ended_by", r.endedBy);
    kv(o, "elapsed_ms", r.elapsedMs);
    o += "\n  ";
    kv(o, "authors", r.authors);
    kv(o, "golds", r.golds);
    kv(o, "skips", r.skips);
    o += "\n  \"maps\": [";
    for (size_t i = 0; i < r.maps.size(); ++i) {
        const RunMapRecord& m = r.maps[i];
        o += i ? ",\n    {" : "\n    {";
        kv(o, "tmx_id", m.tmxId);
        if (!m.site.empty()) kv(o, "site", m.site);
        kv(o, "uid", m.uid);
        kv(o, "name", m.name);
        kv(o, "author", m.author);
        kv(o, "result", m.result);
        kv(o, "medal", m.medal);
        kv(o, "best_ms", m.bestMs);
        kv(o, "spent_ms", m.spentMs);
        kv(o, "author_ms", m.medals.authorMs);
        kv(o, "gold_ms", m.medals.goldMs);
        kv(o, "silver_ms", m.medals.silverMs);
        kv(o, "bronze_ms", m.medals.bronzeMs);
        kv(o, "replay", m.replay, false);
        o += "}";
    }
    o += r.maps.empty() ? "]\n}\n" : "\n  ]\n}\n";
    return o;
}

bool runFromJson(const std::string& text, RunRecord& out) {
    Json j;
    std::string err;
    if (!JsonParser::parse(text, j, err) || j.type != Json::Object) return false;
    RunRecord r;
    r.game = str(j, "game");
    r.site = str(j, "site");
    r.mode = str(j, "mode");
    r.goal = str(j, "goal");
    r.timeLimitMs = static_cast<int>(num(j, "time_limit_ms"));
    r.started = str(j, "started");
    r.ended = str(j, "ended");
    r.endedBy = str(j, "ended_by");
    r.elapsedMs = static_cast<int>(num(j, "elapsed_ms"));
    r.authors = static_cast<int>(num(j, "authors"));
    r.golds = static_cast<int>(num(j, "golds"));
    r.skips = static_cast<int>(num(j, "skips"));
    if (const Json* maps = j.get("maps"))
        if (maps->type == Json::Array)
            for (const Json& m : maps->arr) {
                RunMapRecord e;
                e.tmxId = num(m, "tmx_id");
                e.site = str(m, "site");
                e.uid = str(m, "uid");
                e.name = str(m, "name");
                e.author = str(m, "author");
                e.result = str(m, "result");
                e.medal = str(m, "medal");
                e.bestMs = static_cast<int>(num(m, "best_ms"));
                e.spentMs = static_cast<int>(num(m, "spent_ms"));
                e.medals.authorMs = static_cast<int>(num(m, "author_ms"));
                e.medals.goldMs = static_cast<int>(num(m, "gold_ms"));
                e.medals.silverMs = static_cast<int>(num(m, "silver_ms"));
                e.medals.bronzeMs = static_cast<int>(num(m, "bronze_ms"));
                e.replay = str(m, "replay");
                r.maps.push_back(e);
            }
    out = r;
    return true;
}

void History::setFolder(const std::string& dataFolder) { folder_ = dataFolder + "runs\\"; }

void History::reload() {
    runs_.clear();
    if (folder_.empty()) return;
    std::vector<std::string> files = listFiles(folder_, "run-*.json");
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) { return a > b; });
    for (const std::string& f : files) {
        std::string text;
        RunRecord r;
        if (readFile(folder_ + f, text) && runFromJson(text, r)) {
            r.file = f;
            runs_.push_back(r);
        }
    }
}

bool History::save(RunRecord& r, std::string* pathOut) {
    if (folder_.empty() || !ensureDir(folder_)) return false;
    std::string name = "run-" + fileStampLocal() + ".json";
    std::string path = folder_ + name;
    if (!writeFileAtomic(path, runToJson(r))) return false;
    r.file = name;
    if (pathOut) *pathOut = path;
    runs_.insert(runs_.begin(), r);
    return true;
}

std::string History::add(RunRecord& r) {
    if (folder_.empty()) return std::string();
    std::string name = "run-" + fileStampLocal() + ".json";
    r.file = name;
    runs_.insert(runs_.begin(), r);
    return folder_ + name;
}

bool History::writeRun(const std::string& path, const std::string& json) {
    size_t slash = path.find_last_of('\\');
    if (slash != std::string::npos && !ensureDir(path.substr(0, slash + 1))) return false;
    return writeFileAtomic(path, json);
}

std::set<long long> History::playedIds(const std::string& site) const {
    std::set<long long> ids;
    for (const RunRecord& r : runs_)
        for (const RunMapRecord& m : r.maps)
            if (m.tmxId > 0 && (m.site.empty() ? r.site : m.site) == site) ids.insert(m.tmxId);
    return ids;
}

std::set<long long> History::skippedIds(const std::string& site) const {
    std::set<long long> ids;
    for (const RunRecord& r : runs_)
        for (const RunMapRecord& m : r.maps)
            if (m.tmxId > 0 && (m.site.empty() ? r.site : m.site) == site && (m.result == "skipped" || m.result == "free_skip"))
                ids.insert(m.tmxId);
    return ids;
}

} // namespace lr
